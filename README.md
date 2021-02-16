Tell Me, VLC
============

Simple VLC extension for push notifications of various sorts.

Basic socket interface. Connect, get told when things change.
Each notification is a single line, terminated with "\r\n".
If a line would have contained a newline (eg if there is one
inside a file name), it will be escaped in some way. The line
starts with the name of the thing changed, then a colon; the
name will always be ASCII, eg `volume: 65\r\n`.

* https://wiki.videolan.org/OutOfTreeCompile/
* sudo apt install libvlc-dev libvlccore-dev
