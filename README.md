Tell Me, VLC
============

Simple VLC extension for push notifications of various sorts.

Basic socket interface. Connect, get told when things change.
Each notification is a single line, terminated with "\r\n".
If a line would have contained a newline (eg if there is one
inside a file name), it will be escaped in some way. The line
starts with the name of the thing changed, then a colon; the
name will always be ASCII, eg `"volume: 65\r\n"`.

* https://wiki.videolan.org/OutOfTreeCompile/
* sudo apt install libvlc-dev libvlccore-dev
* make && sudo make install

In the future, a password may be added. If it is, clients will
need to first send this password before any notifications will
be sent. This will also make this mode compatible with HTTP, as
the client will first send either a password or an HTTP greeting,
and then perhaps establish a websocket.
