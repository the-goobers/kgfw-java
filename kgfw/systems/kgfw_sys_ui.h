#ifndef KRISVERS_KGFW_SYS_UI_H
#define KRISVERS_KGFW_SYS_UI_H

#include "../kgfw_ecs.h"

typedef struct kgfw_sys_ui_rect {
	float width;
	float height;
	float x;
	float y;
} kgfw_sys_ui_rect_t;

typedef struct kgfw_sys_ui_component {
	kgfw_component_t base;

	kgfw_sys_ui_rect_t rect;
	unsigned char is_clipspace;
} kgfw_sys_ui_component_t;

typedef struct kgfw_sys_ui_system {

} kgfw_sys_ui_system_t;

#endif
