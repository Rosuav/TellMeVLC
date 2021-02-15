Tell Me, VLC
============

Simple VLC extension for push notifications of various sorts.

Basic socket interface. Connect, get told when things change.
Each notification is a single line, terminated with "\r\n".
If a line would have contained a newline (eg if there is one
inside a file name), it will be escaped in some way.

* https://wiki.videolan.org/OutOfTreeCompile/
* sudo apt install libvlc-dev libvlccore-dev
