#define _GNU_SOURCE
#define MODULE_STRING "tellmevlc"
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <vlc_playlist.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <fcntl.h>

/* NOTE ON NONBLOCKING WRITES

The socket subsystem here uses a somewhat naive write system that makes the
following assumptions:

* Any individual attempted write will be smaller than the socket buffer, so it
  will not block if the buffer is empty.
* Well-behaved clients will keep the buffer mostly empty.
* Stuck clients will let the buffer fill, and won't survive long
* Notifications may spin rapidly, but not for long.

If a partial write occurs, the remainder will be retained in a costly way. When
a write is attempted on a socket that already has a pending write, it will be
queued, again in a costly way. If too many writes are queued, the socket will be
disconnected.

Similarly, nonblocking reads use a very small buffer. Commands must be shorter
than READ_BUFFER_SIZE to be readable.
*/

struct queuedwrite {
	struct queuedwrite *next;
	size_t len;
	char data[1]; //expand as needed
};

#define MAX_SOCKETS 256
#define READ_BUFFER_SIZE 256
struct intf_sys_t {
	vlc_thread_t thread;
	int nsock; //Will generally be at least 1 (the main socket)
	struct pollfd sockets[MAX_SOCKETS];
	struct queuedwrite *writebuf[MAX_SOCKETS];
	char readbuf[MAX_SOCKETS][READ_BUFFER_SIZE];
	int read_pending[MAX_SOCKETS];
};

void handle_command(intf_thread_t *intf, const char *cmd, const char *param) {
	if (!strcasecmp(cmd, "volume")) {
		if (*param) {
			//Set volume
			int vol = atoi(param);
			msg_Info(intf, "GOT VOLUME: >>%d<<", vol);
			playlist_VolumeSet(pl_Get(intf), vol / 100.0);
			playlist_MuteSet(pl_Get(intf), !vol);
		} else {
			//TODO: Get volume (good for startup - format will be same as a unilateral message)
		}
		return;
	}
	//TODO: Respond with an error message
	msg_Info(intf, "GOT LINE: >>%s %s<<", cmd, param);
}

int handle_read(intf_thread_t *intf, int idx) {
	intf_sys_t *sys = intf->p_sys;
	struct pollfd *sock = sys->sockets + idx;
	if (sys->read_pending[idx] >= READ_BUFFER_SIZE) {msg_Err(intf, "Line too long on fd %d", sock->fd); return 1;}
	char *const readbuf = sys->readbuf[idx];
	ssize_t nread = read(sock->fd, readbuf + sys->read_pending[idx], READ_BUFFER_SIZE - sys->read_pending[idx]);
	if (nread < 0) {msg_Err(intf, "%s reading from socket %d", vlc_strerror(errno), sock->fd); return 1;}
	if (!nread) return 0; //Nothing read. Not sure why it showed up in poll(), maybe a race.
	sys->read_pending[idx] += nread;
	while (1) {
		char *const nl = memchr(readbuf, '\n', sys->read_pending[idx]);
		if (!nl) return 0; //No newline. Wait for more text.
		*nl = 0;
		if (nl > readbuf && nl[-1] == '\r') nl[-1] = 0;
		//The line will either be a single word (the command), or a command, space, parameter(s)
		char *param = strchr(readbuf, ' ');
		if (param) *param++ = 0; else param = "";
		if (!strcasecmp(readbuf, "quit")) return 1; //Let the socket get closed. (TODO: make sure it doesn't feel like an error)
		handle_command(intf, readbuf, param);
		memmove(readbuf, nl + 1, sys->read_pending[idx] -= (nl - readbuf + 1));
	}
}

static void *Run(void *this) {
	intf_thread_t *intf = (intf_thread_t *)this;
	intf_sys_t *sys = intf->p_sys;
	msg_Info(intf, "Hello, world!");
	while (sys->nsock) {
		poll(sys->sockets, sys->nsock, 5000);
		if (sys->sockets[0].revents & POLLIN) {
			int newsock = accept4(sys->sockets[0].fd, 0, 0, SOCK_NONBLOCK); //TODO: Log the source IPs?
			msg_Info(intf, "Accepted new socket [%d]", newsock);
			if (sys->nsock >= MAX_SOCKETS) {close(newsock); msg_Warn(intf, "Too many connections - discarding"); continue;}
			sys->writebuf[sys->nsock] = 0;
			sys->read_pending[sys->nsock] = 0;
			struct pollfd *s = sys->sockets + sys->nsock++;
			s->fd = newsock;
			s->events = POLLIN; s->revents = 0;
		}
		struct pollfd *sock = sys->sockets + 1; int n = sys->nsock - 1;
		while (n--) {
			if (!sock->revents) {++sock; continue;}
			if (sock->revents & POLLOUT) {
				struct queuedwrite *q = sys->writebuf[sock - sys->sockets];
				if (q) {
					ssize_t written = write(sock->fd, q->data, q->len);
					if (written < 0) {++sock; continue;} //Or close the socket??
					if (written < (ssize_t)q->len) {
						//This is a fairly unlikely case - a partial write after a
						//partial write. I don't mind if it's costly.
						memmove(q->data, q->data + written, q->len - written);
						q->len -= written;
					}
					struct queuedwrite *next = q->next;
					free(q);
					sys->writebuf[sock - sys->sockets] = next;
					if (next) {++sock; continue;} //Still more queued.
				}
				sock->events &= ~POLLOUT;
				++sock;
				continue;
			}
			if (sock->revents & POLLIN) {
				if (!handle_read(intf, sock - sys->sockets)) { //or should this index be fundamental and sock not??
					++sock;
					continue;
				}
			}
			//Otherwise, assume it has an error.
			msg_Err(intf, "Probable socket error on fd %d - revents = %X", sock->fd, sock->revents);
			close(sock->fd);
			//Dispose of any pending write buffer
			struct queuedwrite *q = sys->writebuf[sock - sys->sockets];
			while (q) {
				struct queuedwrite *next = q->next;
				free(q);
				q = next;
			}
			*sock = sys->sockets[--sys->nsock];
			//And don't advance the pointer (but continue decrementing the count)
		}
	}
	return 0;
}

struct queuedwrite *queuewrite(const char *data, size_t len) {
	struct queuedwrite *q = malloc(sizeof(struct queuedwrite) + len);
	q->next = 0;
	q->len = len;
	strcpy(q->data, data);
	return q;
}

void send_to(vlc_object_t *this, intf_sys_t *sys, int sockidx, const char *data) {
	//TODO: Semaphore this properly
	if (sockidx < 0 || sockidx >= MAX_SOCKETS) return;
	size_t len = strlen(data);
	struct queuedwrite *q = sys->writebuf[sockidx];
	if (q) {
		//Something already queued? Queue this after it.
		int limit = 10; //Arbitrary.
		while (limit > 0 && q->next) {--limit; q = q->next;}
		if (q->next) {close(sys->sockets[sockidx].fd); msg_Err(this, "Too many queued writes [fd=%d]", sys->sockets[sockidx].fd); return;}
		q->next = queuewrite(data, len);
		return;
	}
	struct pollfd *sock = sys->sockets + sockidx;
	ssize_t written = write(sock->fd, data, len); //And what is it that we're meant to have wrote? ... Written.
	if (written < 0) {msg_Err(this, "%s writing to socket %d", vlc_strerror(errno), sock->fd); close(sock->fd); return;}
	if (written == (ssize_t)len) return;
	sys->writebuf[sockidx] = queuewrite(data + written, len - written);
	sock->events |= POLLOUT;
}

static int VolumeChanged(vlc_object_t *this, char const *psz_cmd,
	vlc_value_t oldval, vlc_value_t newval, void *data)
{
	(void)this; VLC_UNUSED(psz_cmd); VLC_UNUSED(oldval); VLC_UNUSED(newval);
	intf_thread_t *intf = (intf_thread_t*)data;
	int volume = (int)(newval.f_float * 100 + 0.5);
	if (volume < 0) return VLC_SUCCESS; //Seems to give us a -1.0 on shutdown??

	char buf[64]; snprintf(buf, sizeof(buf), "volume: %d\r\n", volume);
	intf_sys_t *sys = intf->p_sys;
	int n = sys->nsock;
	for (int i = 1; i < n; ++i) send_to(this, sys, i, buf);
	return VLC_SUCCESS;
}

static void Close(vlc_object_t *this) {
	intf_thread_t *intf = (intf_thread_t *)this;
	intf_sys_t *sys = intf->p_sys;
	struct pollfd *sock = sys->sockets; int n = sys->nsock;
	while (n--) {close(sock->fd); ++sock;}
	free(sys);
}

static int Open(vlc_object_t *this) {
	intf_thread_t *intf = (intf_thread_t *)this;
	intf_sys_t *sys = malloc(sizeof(intf_sys_t));
	if (!sys) return VLC_ENOMEM;
	int mainsock = socket(AF_INET6, SOCK_STREAM, 0);
	setsockopt(mainsock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
	int port = var_InheritInteger(this, "tellmevlc-port");
	struct sockaddr_in6 bindto = {AF_INET6, htons(port), 0, in6addr_any, 0};
	if (bind(mainsock, (struct sockaddr *)&bindto, sizeof(bindto))) {
		msg_Err(this, "%s on port %d", vlc_strerror(errno), port);
		close(mainsock);
		free(sys);
		return VLC_EGENERIC;
	}
	listen(mainsock, 5);
	fcntl(mainsock, F_SETFL, O_NONBLOCK);
	sys->sockets[0].fd = mainsock;
	sys->sockets[0].events = POLLIN;
	sys->nsock = 1;
	msg_Info(this, "Listening on %d.", port);
	intf->p_sys = sys;
	var_AddCallback(pl_Get(intf), "volume", VolumeChanged, intf);
	if (vlc_clone(&sys->thread, Run, intf, VLC_THREAD_PRIORITY_LOW)) {
		Close(this);
		return VLC_EGENERIC;
	}
	return VLC_SUCCESS;
}

vlc_module_begin()
	set_shortname("TellMeVLC")
	set_description("Notifications interface")
	set_capability("interface", 0)
	set_category(CAT_INTERFACE)
	set_subcategory(SUBCAT_INTERFACE_CONTROL)
	set_callbacks(Open, Close)
	add_integer_with_range("tellmevlc-port", 4221, 1, 65535, "TellMeVLC port", "TCP/IP port number to listen on for notifications", 1)
vlc_module_end()
