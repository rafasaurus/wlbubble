#define _POSIX_C_SOURCE 200809L

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "wayland_surface.h"

#define BUFFER_COUNT 2

typedef struct {
    struct wl_buffer *buffer;
    uint32_t *data;
    bool busy;
} Buffer;

static struct wl_display *display;
static struct wl_registry *registry;
static struct wl_compositor *compositor;
static struct wl_shm *shm;
static struct wl_seat *seat;
static struct wl_pointer *pointer;
static struct xdg_wm_base *xdg_wm_base;
static struct wl_surface *surface;
static struct xdg_surface *xdg_surface;
static struct xdg_toplevel *xdg_toplevel;

static Buffer buffers[BUFFER_COUNT];
static int current_buffer;
static bool configured;
static bool visible = true;
static bool should_close;
static bool pointer_in_surface;
static int pointer_x, pointer_y;
static struct wl_callback *frame_cb;
static bool can_render = true;

static WaylandEvent pending_events[16];
static int event_read_pos;
static int event_write_pos;

static void push_event(WaylandEventType type, uint32_t button) {
    int next = (event_write_pos + 1) % 16;
    if (next == event_read_pos)
        return;
    pending_events[event_write_pos].type = type;
    pending_events[event_write_pos].button = button;
    pending_events[event_write_pos].pointer_x = pointer_x;
    pending_events[event_write_pos].pointer_y = pointer_y;
    event_write_pos = next;
}

bool wayland_get_event(WaylandEvent *event) {
    if (event_read_pos == event_write_pos)
        return false;
    *event = pending_events[event_read_pos];
    event_read_pos = (event_read_pos + 1) % 16;
    return true;
}

static int create_shm_file(size_t size) {
    int fd = -1;
#ifdef __linux__
    fd = memfd_create("wmbubble-shm", MFD_CLOEXEC);
#endif
    if (fd < 0) {
        char name[] = "/wmbubble-XXXXXX";
        fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) shm_unlink(name);
    }
    if (fd < 0) return -1;
    if (ftruncate(fd, size) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void buffer_release(void *data, struct wl_buffer *buffer) {
    Buffer *buf = data;
    buf->busy = false;
}

static const struct wl_buffer_listener buffer_listener = {
    .release = buffer_release
};

static int create_buffers(void) {
    size_t stride = BOX_SIZE * 4;
    size_t size = stride * BOX_SIZE;
    size_t pool_size = size * BUFFER_COUNT;

    int fd = create_shm_file(pool_size);
    if (fd < 0) {
        fprintf(stderr, "Failed to create shm file\n");
        return -1;
    }

    uint8_t *pool_data = mmap(NULL, pool_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pool_data == MAP_FAILED) {
        close(fd);
        fprintf(stderr, "Failed to mmap shm\n");
        return -1;
    }

    struct wl_shm_pool *pool = wl_shm_create_pool(shm, fd, pool_size);
    close(fd);

    for (int i = 0; i < BUFFER_COUNT; i++) {
        buffers[i].data = (uint32_t *)(pool_data + i * size);
        buffers[i].buffer = wl_shm_pool_create_buffer(pool, i * size,
            BOX_SIZE, BOX_SIZE, stride, WL_SHM_FORMAT_XRGB8888);
        buffers[i].busy = false;
        wl_buffer_add_listener(buffers[i].buffer, &buffer_listener, &buffers[i]);
    }

    wl_shm_pool_destroy(pool);
    return 0;
}

static void xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial) {
    (void)data;
    xdg_surface_ack_configure(xdg_surface, serial);
    configured = true;
}

static const struct xdg_surface_listener xdg_surface_listener = {
    .configure = xdg_surface_configure
};

static void xdg_toplevel_configure(void *data, struct xdg_toplevel *toplevel,
                                   int32_t width, int32_t height,
                                   struct wl_array *states) {
    (void)data;
    (void)toplevel;
    (void)width;
    (void)height;
    (void)states;
}

static void xdg_toplevel_close(void *data, struct xdg_toplevel *toplevel) {
    (void)data;
    (void)toplevel;
    should_close = true;
    push_event(WL_EVENT_CLOSE, 0);
}

static void xdg_toplevel_configure_bounds(void *data, struct xdg_toplevel *toplevel,
                                          int32_t width, int32_t height) {
    (void)data;
    (void)toplevel;
    (void)width;
    (void)height;
}

static void xdg_toplevel_wm_capabilities(void *data, struct xdg_toplevel *toplevel,
                                         struct wl_array *capabilities) {
    (void)data;
    (void)toplevel;
    (void)capabilities;
}

static const struct xdg_toplevel_listener xdg_toplevel_listener = {
    .configure = xdg_toplevel_configure,
    .close = xdg_toplevel_close,
    .configure_bounds = xdg_toplevel_configure_bounds,
    .wm_capabilities = xdg_toplevel_wm_capabilities
};

static void xdg_wm_base_ping(void *data, struct xdg_wm_base *xdg_wm_base, uint32_t serial) {
    (void)data;
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping
};

static void pointer_enter(void *data, struct wl_pointer *pointer,
                          uint32_t serial, struct wl_surface *surface,
                          wl_fixed_t sx, wl_fixed_t sy) {
    (void)data;
    (void)pointer;
    (void)serial;
    (void)surface;
    pointer_x = wl_fixed_to_int(sx);
    pointer_y = wl_fixed_to_int(sy);
    pointer_in_surface = true;
    push_event(WL_EVENT_ENTER, 0);
}

static void pointer_leave(void *data, struct wl_pointer *pointer,
                          uint32_t serial, struct wl_surface *surface) {
    (void)data;
    (void)pointer;
    (void)serial;
    (void)surface;
    pointer_in_surface = false;
    push_event(WL_EVENT_LEAVE, 0);
}

static void pointer_motion(void *data, struct wl_pointer *pointer,
                           uint32_t time, wl_fixed_t sx, wl_fixed_t sy) {
    (void)data;
    (void)pointer;
    (void)time;
    pointer_x = wl_fixed_to_int(sx);
    pointer_y = wl_fixed_to_int(sy);
}

static void pointer_button(void *data, struct wl_pointer *pointer,
                           uint32_t serial, uint32_t time, uint32_t button,
                           uint32_t state) {
    (void)data;
    (void)pointer;
    (void)serial;
    (void)time;
    if (state == WL_POINTER_BUTTON_STATE_PRESSED) {
        uint32_t btn = 0;
        if (button == 0x110) btn = 1;
        else if (button == 0x111) btn = 3;
        else if (button == 0x112) btn = 2;
        push_event(WL_EVENT_BUTTON, btn);
    }
}

static void pointer_axis(void *data, struct wl_pointer *pointer,
                         uint32_t time, uint32_t axis, wl_fixed_t value) {
    (void)data;
    (void)pointer;
    (void)time;
    (void)axis;
    (void)value;
}

static void pointer_frame(void *data, struct wl_pointer *pointer) {
    (void)data;
    (void)pointer;
}

static void pointer_axis_source(void *data, struct wl_pointer *pointer, uint32_t source) {
    (void)data;
    (void)pointer;
    (void)source;
}

static void pointer_axis_stop(void *data, struct wl_pointer *pointer,
                              uint32_t time, uint32_t axis) {
    (void)data;
    (void)pointer;
    (void)time;
    (void)axis;
}

static void pointer_axis_discrete(void *data, struct wl_pointer *pointer,
                                  uint32_t axis, int32_t discrete) {
    (void)data;
    (void)pointer;
    (void)axis;
    (void)discrete;
}

static void pointer_axis_value120(void *data, struct wl_pointer *pointer,
                                  uint32_t axis, int32_t value120) {
    (void)data;
    (void)pointer;
    (void)axis;
    (void)value120;
}

static void pointer_axis_relative_direction(void *data, struct wl_pointer *pointer,
                                            uint32_t axis, uint32_t direction) {
    (void)data;
    (void)pointer;
    (void)axis;
    (void)direction;
}

static const struct wl_pointer_listener pointer_listener = {
    .enter = pointer_enter,
    .leave = pointer_leave,
    .motion = pointer_motion,
    .button = pointer_button,
    .axis = pointer_axis,
    .frame = pointer_frame,
    .axis_source = pointer_axis_source,
    .axis_stop = pointer_axis_stop,
    .axis_discrete = pointer_axis_discrete,
    .axis_value120 = pointer_axis_value120,
    .axis_relative_direction = pointer_axis_relative_direction
};

static void seat_capabilities(void *data, struct wl_seat *seat, uint32_t caps) {
    (void)data;
    if ((caps & WL_SEAT_CAPABILITY_POINTER) && !pointer) {
        pointer = wl_seat_get_pointer(seat);
        wl_pointer_add_listener(pointer, &pointer_listener, NULL);
    } else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && pointer) {
        wl_pointer_destroy(pointer);
        pointer = NULL;
    }
}

static void seat_name(void *data, struct wl_seat *seat, const char *name) {
    (void)data;
    (void)seat;
    (void)name;
}

static const struct wl_seat_listener seat_listener = {
    .capabilities = seat_capabilities,
    .name = seat_name
};

static void registry_global(void *data, struct wl_registry *registry,
                            uint32_t name, const char *interface, uint32_t version) {
    (void)data;
    (void)version;
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
    } else if (strcmp(interface, wl_shm_interface.name) == 0) {
        shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
    } else if (strcmp(interface, xdg_wm_base_interface.name) == 0) {
        xdg_wm_base = wl_registry_bind(registry, name, &xdg_wm_base_interface, 1);
        xdg_wm_base_add_listener(xdg_wm_base, &xdg_wm_base_listener, NULL);
    } else if (strcmp(interface, wl_seat_interface.name) == 0) {
        seat = wl_registry_bind(registry, name, &wl_seat_interface, 7);
        wl_seat_add_listener(seat, &seat_listener, NULL);
    }
}

static void registry_global_remove(void *data, struct wl_registry *registry, uint32_t name) {
    (void)data;
    (void)registry;
    (void)name;
}

static const struct wl_registry_listener registry_listener = {
    .global = registry_global,
    .global_remove = registry_global_remove
};

int wayland_init(const char *app_name) {
    display = wl_display_connect(NULL);
    if (!display) {
        fprintf(stderr, "Failed to connect to Wayland display\n");
        return -1;
    }

    registry = wl_display_get_registry(display);
    wl_registry_add_listener(registry, &registry_listener, NULL);
    wl_display_roundtrip(display);

    if (!compositor || !shm || !xdg_wm_base) {
        fprintf(stderr, "Missing required Wayland interfaces\n");
        wayland_cleanup();
        return -1;
    }

    if (create_buffers() < 0) {
        wayland_cleanup();
        return -1;
    }

    surface = wl_compositor_create_surface(compositor);
    xdg_surface = xdg_wm_base_get_xdg_surface(xdg_wm_base, surface);
    xdg_surface_add_listener(xdg_surface, &xdg_surface_listener, NULL);

    xdg_toplevel = xdg_surface_get_toplevel(xdg_surface);
    xdg_toplevel_add_listener(xdg_toplevel, &xdg_toplevel_listener, NULL);
    xdg_toplevel_set_title(xdg_toplevel, app_name);
    xdg_toplevel_set_app_id(xdg_toplevel, app_name);
    xdg_toplevel_set_min_size(xdg_toplevel, BOX_SIZE, BOX_SIZE);
    xdg_toplevel_set_max_size(xdg_toplevel, BOX_SIZE, BOX_SIZE);

    wl_surface_commit(surface);
    wl_display_roundtrip(display);

    return 0;
}

void wayland_cleanup(void) {
    for (int i = 0; i < BUFFER_COUNT; i++) {
        if (buffers[i].buffer)
            wl_buffer_destroy(buffers[i].buffer);
    }
    if (pointer)
        wl_pointer_destroy(pointer);
    if (xdg_toplevel)
        xdg_toplevel_destroy(xdg_toplevel);
    if (xdg_surface)
        xdg_surface_destroy(xdg_surface);
    if (surface)
        wl_surface_destroy(surface);
    if (xdg_wm_base)
        xdg_wm_base_destroy(xdg_wm_base);
    if (seat)
        wl_seat_destroy(seat);
    if (shm)
        wl_shm_destroy(shm);
    if (compositor)
        wl_compositor_destroy(compositor);
    if (registry)
        wl_registry_destroy(registry);
    if (display)
        wl_display_disconnect(display);
}

static void frame_done(void *data, struct wl_callback *callback, uint32_t time) {
    (void)data;
    (void)time;
    wl_callback_destroy(callback);
    frame_cb = NULL;
    can_render = true;
}

static const struct wl_callback_listener frame_listener = {
    .done = frame_done
};

uint32_t *wayland_get_buffer(void) {
    for (int i = 0; i < BUFFER_COUNT; i++) {
        int idx = (current_buffer + i) % BUFFER_COUNT;
        if (!buffers[idx].busy) {
            current_buffer = idx;
            return buffers[idx].data;
        }
    }
    return buffers[current_buffer].data;
}

void wayland_commit_buffer(void) {
    if (!configured || !can_render)
        return;
    
    can_render = false;
    buffers[current_buffer].busy = true;
    wl_surface_attach(surface, buffers[current_buffer].buffer, 0, 0);
    wl_surface_damage_buffer(surface, 0, 0, BOX_SIZE, BOX_SIZE);
    
    frame_cb = wl_surface_frame(surface);
    wl_callback_add_listener(frame_cb, &frame_listener, NULL);
    
    wl_surface_commit(surface);
}

int wayland_dispatch(void) {
    return wl_display_dispatch(display);
}

int wayland_dispatch_pending(void) {
    wl_display_flush(display);
    return wl_display_dispatch_pending(display);
}

bool wayland_is_visible(void) {
    return visible && configured;
}

bool wayland_should_close(void) {
    return should_close;
}

bool wayland_can_render(void) {
    return can_render;
}

int wayland_get_fd(void) {
    return wl_display_get_fd(display);
}
