#include "kgfw_sys_ui.h"
#include "kgfw_log.h"
#include "kgfw_input.h"

struct {
	kgfw_uuid_t comp_uuid;
	struct {
		float x;
		float y;
		unsigned char click;
	} mouse;

	kgfw_camera_t * camera;
	kgfw_window_t * window;
} static state = {
	.comp_uuid = 0,

	.mouse = {
		.x = 0,
		.y = 0,
		.click = 0,
	},

	.camera = NULL,
	.window = NULL,
};

static void comp_ui_update(kgfw_sys_ui_component_t * self);
static void comp_ui_start(kgfw_sys_ui_component_t * self);
static void comp_ui_destroy(kgfw_sys_ui_component_t * self);
static void ui_mouse_callback(kgfw_window_t * window, kgfw_input_mouse_button_enum button, unsigned char action);

kgfw_uuid_t kgfw_sys_ui_init(kgfw_camera_t * camera) {
	kgfw_sys_ui_component_t component = {
		{
			.update = (kgfw_component_update_f) comp_ui_update,
			.start = (kgfw_component_start_f) comp_ui_start,
			.destroy = (kgfw_component_destroy_f) comp_ui_destroy,

			.instance_id = 0,
			.type_id = 0,
			.entity = NULL,
		},
		.click = NULL,

		.rect = {
			1, 1,
			0, 0,

			.origin = {
				.x = 0,
				.y = 0,
			},
		},
		.is_clipspace = 1,
	};

	kgfw_uuid_t uuid = kgfw_component_construct("ui", sizeof(kgfw_sys_ui_component_t), &component, 0);
	if (uuid == 0) {
		kgfw_logf(KGFW_LOG_SEVERITY_WARN, "kgfw UI component construction failed");
		return 0;
	}

	state.comp_uuid = uuid;
	kgfw_input_mouse_button_register_callback(ui_mouse_callback);
	state.camera = camera;

	return state.comp_uuid;
}

kgfw_uuid_t kgfw_sys_ui_get_uuid(void) {
	if (state.comp_uuid == 0) {
		kgfw_logf(KGFW_LOG_SEVERITY_WARN, "kgfw UI component system never initialized");
	}

	return state.comp_uuid;
}

static void comp_ui_update(kgfw_sys_ui_component_t * self) {
	float xs = 1;
	float ys = 1;

	if (state.camera->ratio < 1) {
		ys = 1.0f / state.camera->ratio;
	}
	else {
		xs = state.camera->ratio;
	}

	kgfw_input_mouse_pos(&state.mouse.x, &state.mouse.y);
	float mw = self->rect.width / ys;
	float mh = self->rect.height / ys;
	float mx = (self->rect.x - (self->rect.width * self->rect.origin.x)) / ys;
	float my = (self->rect.y - (self->rect.height * self->rect.origin.y)) / ys;
	self->mesh->transform.pos[0] = mx;
	self->mesh->transform.pos[1] = my;
	self->mesh->transform.scale[0] = mw;// - (mw * self->rect.origin.x);
	self->mesh->transform.scale[1] = mh;// + (mh * self->rect.origin.y);

	//kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "comp update 0x%llx", self->base.instance_id);

	if (state.mouse.click && self->click != NULL) {
		float x = ((state.mouse.x / (float) state.window->width) * 2 - 1) * xs + (self->rect.width * self->rect.origin.x);
		float y = -((state.mouse.y / (float) state.window->height) * 2 - 1) * ys + (self->rect.height * self->rect.origin.y);
		//kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "-%f %f-", xs, ys);

		float l = self->rect.x - self->rect.width;
		float r = self->rect.x + self->rect.width;
		float b = self->rect.y - self->rect.height;
		float t = self->rect.y + self->rect.height;
		//kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "tl (%f, %f) br (%f, %f)", l, t, r, b);
		if (x > l && x < r && y > b && y < t) {
			self->click(self, x, y);
			state.mouse.click = 0;
		}
	}
}

static void comp_ui_start(kgfw_sys_ui_component_t * self) {
	kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "comp start 0x%llx", self->base.instance_id);
}

static void comp_ui_destroy(kgfw_sys_ui_component_t * self) {
	kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "comp destroy 0x%llx", self->base.instance_id);
}

static void ui_mouse_callback(kgfw_window_t * window, kgfw_input_mouse_button_enum button, unsigned char action) {
	if (action == 1) {
		state.mouse.click = 1;
	} else {
		state.mouse.click = 0;
	}

	kgfw_input_mouse_pos(&state.mouse.x, &state.mouse.y);
	state.window = window;
}