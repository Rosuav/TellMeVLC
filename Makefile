libtellmevlc_plugin.so: tellmevlc.c
	gcc -shared -g -O2 -Wall -Wextra -fPIC -DPIC `pkg-config --cflags vlc-plugin` `pkg-config --libs vlc-plugin` -o $@ $^

install: libtellmevlc_plugin.so
	cp $^ /usr/lib/x86_64-linux-gnu/vlc/plugins/misc/
