#ifndef WAYLAND_SURFACE_H
#define WAYLAND_SURFACE_H

#include <stdint.h>
#include <stdbool.h>

#ifndef BOX_SIZE
#define BOX_SIZE 58
#endif

typedef enum {
    WL_EVENT_NONE = 0,
    WL_EVENT_ENTER,
    WL_EVENT_LEAVE,
    WL_EVENT_BUTTON,
    WL_EVENT_CLOSE
} WaylandEventType;

typedef struct {
    WaylandEventType type;
    uint32_t button;
    int pointer_x;
    int pointer_y;
} WaylandEvent;

int wayland_init(const char *app_name);
void wayland_cleanup(void);

uint32_t *wayland_get_buffer(void);
void wayland_commit_buffer(void);

int wayland_dispatch(void);
int wayland_dispatch_pending(void);
bool wayland_get_event(WaylandEvent *event);

bool wayland_is_visible(void);
bool wayland_should_close(void);
bool wayland_can_render(void);

int wayland_get_fd(void);

#endif
