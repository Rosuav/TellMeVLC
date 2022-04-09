libtellmevlc_plugin.so: tellmevlc.c
	gcc -shared -g -O2 -Wall -Wextra -fPIC -DPIC `pkg-config --cflags vlc-plugin` `pkg-config --libs vlc-plugin` -o $@ $^

install: libtellmevlc_plugin.so
	cp $^ `pkg-config vlc-plugin --variable=pluginsdir`/misc/
	`pkg-config vlc-plugin --variable=pluginsdir`/../vlc-cache-gen `pkg-config vlc-plugin --variable=pluginsdir`
	
