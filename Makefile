libtellmevlc_plugin.so: tellmevlc.c
	gcc -shared -g -O2 -Wall -Wextra `pkg-config --cflags vlc-plugin` `pkg-config --libs vlc-plugin` -o $@ $^

