#ifndef KRISVERS_KGFW_SYS_UI_H
#define KRISVERS_KGFW_SYS_UI_H

#include "kgfw_ecs.h"
#include "kgfw_window.h"
#include "kgfw_graphics.h"

typedef void (*kgfw_sys_ui_click_f)(struct kgfw_sys_ui_component * self, float x, float y);

typedef struct kgfw_sys_ui_rect {
	float width;
	float height;
	float x;
	float y;

	struct {
		float x;
		float y;
	} origin;
} kgfw_sys_ui_rect_t;

typedef struct kgfw_sys_ui_component {
	kgfw_component_t base;
	kgfw_sys_ui_click_f click;

	kgfw_sys_ui_rect_t rect;
	unsigned char is_clipspace;

	kgfw_graphics_mesh_node_t * mesh;
} kgfw_sys_ui_component_t;

kgfw_uuid_t kgfw_sys_ui_init(kgfw_camera_t * camera);
kgfw_uuid_t kgfw_sys_ui_get_uuid(void);

#endif
