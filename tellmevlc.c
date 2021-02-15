#define MODULE_STRING "tellmevlc"
#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_interface.h>
#include <stdio.h>

#define MAX_SOCKETS 256
struct intf_sys_t {
	int nsock; //Will generally be at least 1 (the main socket)
	int sockets[MAX_SOCKETS];
};

static int Open(vlc_object_t *this) {
	printf("Hello, world!\n");
	intf_thread_t *intf = (intf_thread_t *)this;
	intf_sys_t *sys = malloc(sizeof(intf_sys_t));
	if (!sys) return VLC_ENOMEM;
	//if (thing failed) {free(sys); return VLC_EGENERIC;}
	intf->p_sys = sys;
	return VLC_SUCCESS;
}

static void Close(vlc_object_t *this) {
	intf_thread_t *intf = (intf_thread_t *)this;
	intf_sys_t *sys = intf->p_sys;
	free(sys);
}

vlc_module_begin()
	set_shortname("TellMeVLC")
	set_description("Notifications interface")
	set_capability("interface", 0)
	set_category(CAT_INTERFACE)
	set_subcategory(SUBCAT_INTERFACE_CONTROL)
	set_callbacks(Open, Close)
vlc_module_end()
