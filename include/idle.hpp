#pragma once

#include <cstdint>

#include <wayland-client.h>

#include "idle-inhibit-unstable-v1-client-protocol.h"

#ifdef HAVE_SYSTEMD
#include <systemd/sd-bus.h>
#endif
#ifdef HAVE_ELOGIND
#include <elogind/sd-bus.h>
#endif

class Idle {
	struct wl_display *display = nullptr;
	struct wl_registry *registry = nullptr;
	struct wl_compositor *compositor = nullptr;
	struct wl_surface *surface = nullptr;
	struct zwp_idle_inhibit_manager_v1 *inhibitManager = nullptr;
	struct zwp_idle_inhibitor_v1 *inhibitor = nullptr;
	bool waylandUnsupported = false;

	struct sd_bus *bus = nullptr;
	int logindFd = -1;
	bool logindUnavailable = false;

	static void global_add(void *data, struct wl_registry *registry,
						   uint32_t name, const char *interface,
						   uint32_t version);

	static void global_remove(void *data, struct wl_registry *registry,
							  uint32_t name);

	bool connectWayland();
	void logindBlock();
	void logindRelease();

	void block();
	void release_block();

  public:
	Idle();

	void update(bool isRunning);
};
