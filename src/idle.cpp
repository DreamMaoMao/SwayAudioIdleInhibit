#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <unistd.h>

#include "idle.hpp"

using namespace std;

void Idle::global_add(void *data, struct wl_registry *registry, uint32_t name,
					  const char *interface, uint32_t version) {
	(void)version;
	Idle *idle = (Idle *)data;
	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		idle->compositor = (struct wl_compositor *)wl_registry_bind(
			registry, name, &wl_compositor_interface, 1);
	} else if (strcmp(interface,
					  zwp_idle_inhibit_manager_v1_interface.name) == 0) {
		idle->inhibitManager =
			(struct zwp_idle_inhibit_manager_v1 *)wl_registry_bind(
				registry, name, &zwp_idle_inhibit_manager_v1_interface, 1);
	}
}

void Idle::global_remove(void *, struct wl_registry *, uint32_t) {}

bool Idle::connectWayland() {
	if (display && inhibitManager) {
		return true;
	}
	// The compositor either doesn't support the protocol or we already failed
	// to connect to this display, so there's no point in retrying
	if (waylandUnsupported) {
		return false;
	}

	display = wl_display_connect(nullptr);
	if (!display) {
		fprintf(stderr, "Could not connect to the Wayland display. Will "
						"retry when audio starts playing.\n");
		return false;
	}

	static const struct wl_registry_listener registryListener = {
		.global = global_add,
		.global_remove = global_remove,
	};

	registry = wl_display_get_registry(display);
	wl_registry_add_listener(registry, &registryListener, this);
	if (wl_display_roundtrip(display) < 0) {
		fprintf(stderr, "Could not query the Wayland registry\n");
		wl_display_disconnect(display);
		display = nullptr;
		registry = nullptr;
		return false;
	}

	if (!compositor || !inhibitManager) {
		fprintf(stderr,
				"Compositor doesn't support the idle-inhibit protocol\n");
		waylandUnsupported = true;
		wl_display_disconnect(display);
		display = nullptr;
		registry = nullptr;
		compositor = nullptr;
		inhibitManager = nullptr;
		return false;
	}

	surface = wl_compositor_create_surface(compositor);
	if (wl_display_roundtrip(display) < 0) {
		fprintf(stderr, "Could not create an idle inhibitor surface\n");
		wl_display_disconnect(display);
		display = nullptr;
		registry = nullptr;
		compositor = nullptr;
		inhibitManager = nullptr;
		surface = nullptr;
		return false;
	}

	return true;
}

void Idle::logindBlock() {
#if defined(HAVE_SYSTEMD) || defined(HAVE_ELOGIND)
	if (logindFd >= 0 || logindUnavailable) {
		return;
	}

	if (!bus) {
		int ret = sd_bus_default_system(&bus);
		if (ret < 0) {
			fprintf(stderr, "Could not connect to the system bus: %s\n",
					strerror(-ret));
			logindUnavailable = true;
			return;
		}
	}

	sd_bus_message *message = nullptr;
	sd_bus_error error = SD_BUS_ERROR_NULL;
	int ret = sd_bus_call_method(
		bus, "org.freedesktop.login1", "/org/freedesktop/login1",
		"org.freedesktop.login1.Manager", "Inhibit", &error, &message, "ssss",
		"idle", "sway-audio-idle-inhibit", "Audio is playing", "block");
	if (ret < 0) {
		fprintf(stderr, "Could not send inhibit signal! %s: %s\n", error.name,
				error.message);
		sd_bus_error_free(&error);
		return;
	}

	ret = sd_bus_message_read(message, "h", &logindFd);
	if (ret < 0) {
		fprintf(stderr, "Could not get DBus response: %s\n", strerror(-ret));
		sd_bus_error_free(&error);
		sd_bus_message_unref(message);
		return;
	}

	// Clone the FD (will be invalid once we unref the message)
	logindFd = fcntl(logindFd, F_DUPFD_CLOEXEC, 3);
	if (logindFd < 0) {
		fprintf(stderr, "Could not copy lock fd: %s\n", strerror(errno));
	}

	sd_bus_error_free(&error);
	sd_bus_message_unref(message);
#endif
}

void Idle::logindRelease() {
	if (logindFd >= 0) {
		close(logindFd);
		logindFd = -1;
	}
}

void Idle::block() {
	if (!inhibitor && connectWayland()) {
		inhibitor = zwp_idle_inhibit_manager_v1_create_inhibitor(
			inhibitManager, surface);
		if (!inhibitor || wl_display_roundtrip(display) < 0) {
			fprintf(stderr, "Could not inhibit idle through Wayland\n");
			inhibitor = nullptr;
			wl_display_disconnect(display);
			display = nullptr;
			registry = nullptr;
			compositor = nullptr;
			inhibitManager = nullptr;
			surface = nullptr;
		}
	}
	logindBlock();
}

void Idle::release_block() {
	if (inhibitor) {
		zwp_idle_inhibitor_v1_destroy(inhibitor);
		wl_display_flush(display);
		inhibitor = nullptr;
	}
	logindRelease();
}

Idle::Idle() {
	// Connect early so that a missing compositor protocol is reported at
	// startup instead of when audio starts playing
	connectWayland();
}

void Idle::update(bool isRunning) {
	if (isRunning) {
		block();
		cout << "IDLE INHIBITED" << endl;
	} else {
		release_block();
		cout << "NOT IDLE INHIBITED" << endl;
	}
}
