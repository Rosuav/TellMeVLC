#define MODULE_STRING "tellmevlc"
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define MAX_SOCKETS 256
struct intf_sys_t {
	vlc_thread_t thread;
	int nsock; //Will generally be at least 1 (the main socket)
	struct pollfd sockets[MAX_SOCKETS];
};

static void *Run(void *this) {
	intf_thread_t *intf = (intf_thread_t *)this;
	intf_sys_t *sys = intf->p_sys;
	while (1) {
		msg_Info(intf, "Hello, world!");
		poll(sys->sockets, sys->nsock, -1);
	}
	return 0;
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
	int port = 4221; //TODO: Allow this to be configured
	struct sockaddr_in6 bindto = {AF_INET6, htons(port), 0, in6addr_any, 0};
	if (bind(mainsock, (struct sockaddr *)&bindto, sizeof(bindto))) {
		msg_Err(this, "%s on port %d", vlc_strerror(errno), port);
		close(mainsock);
		free(sys);
		return VLC_EGENERIC;
	}
	listen(mainsock, 5);
	sys->sockets[0].fd = mainsock;
	sys->sockets[0].events = POLLIN;
	sys->nsock = 1;
	msg_Info(this, "Listening on %d.", port);
	intf->p_sys = sys;
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
vlc_module_end()
