#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "kgfw/kgfw.h"
#include "kgfw/ktga/ktga.h"
#include "kgfw/koml/koml.h"
#include "kgfw/kobj/kobj.h"
#include "kgfw/kgfw_sys_ui.h"
#ifndef KGFW_WINDOWS
#include <unistd.h>
#endif
#include <linmath.h>

struct {
	kgfw_window_t window;
	kgfw_camera_t camera;
	unsigned char input;
	unsigned char exit;
	struct {
		float movement;
		float arrow_speed;
		float mouse_speed;
		float gamepad_sensitivity;
		float jump_force;
		float gravity;
		float fov;
	} settings;

	kgfw_gamepad_t * gamepad;

	struct {
		unsigned int width;
		unsigned int height;
	} board;
	struct sudoku * selected;
	kgfw_graphics_mesh_node_t * selector;
	kgfw_graphics_texture_t textures[10];
} static state = {
	{ 0 },
	{
		{ 0, 0, 5 },
		{ 0, 0, 0 },
		{ 1, 1 },
		90, 0.01f, 1000.0f, 1.3333f, 1, 0,
		{ 0, 0, 0 },
	},
	1, 0,
	{
		3.0f,
		90.0f,
		0.15f,
		10.0f,
		10.0f,
		25.0f,
		90.0f,
	},

	.gamepad = NULL,

	.board = {
		.width = 9,
		.height = 9,
	},
	.selected = NULL,
	.selector = NULL,
};

struct {
	vec3 pos;
	vec3 rot;
	vec3 vel;
} static player = {
	{ 0, 0, 0 },
	{ 0, 0, 0 },
	{ 0, 0, 0 },
};

#define STORAGE_MAX_TEXTURES 64
#define STORAGE_MAX_MESHES 64
#define EVALUATION_MAX_CYCLES 100

typedef struct sudoku {
	kgfw_entity_t * entity;
	kgfw_sys_ui_component_t * ui_comp;
	unsigned int number;

	struct sudoku * right;
	struct sudoku * left;
	struct sudoku * up;
	struct sudoku * down;
} sudoku_t;

struct {
	ktga_t textures[STORAGE_MAX_TEXTURES];
	unsigned long long int textures_count;
	kgfw_hash_t texture_hashes[STORAGE_MAX_TEXTURES];
	kgfw_graphics_mesh_t meshes[STORAGE_MAX_MESHES];
	unsigned long long int meshes_count;
	kgfw_hash_t mesh_hashes[STORAGE_MAX_MESHES];
	sudoku_t * sudokus;
	unsigned int * solutions;
} static storage = {
	{ 0 },
	0,
	{ 0 },
	{ 0 },
	0,
	{ 0 },
};

static int kgfw_log_handler(kgfw_log_severity_enum severity, char * string);
static int kgfw_logc_handler(kgfw_log_severity_enum severity, char character);
static void kgfw_key_handler(kgfw_input_key_enum key, unsigned char action);
static void kgfw_mouse_button_handle(kgfw_input_mouse_button_enum button, unsigned char action);
static void kgfw_gamepad_handle(kgfw_gamepad_t * gamepad);
static ktga_t * texture_get(char * name);
static int textures_load(void);
static void textures_cleanup(void);
static kgfw_graphics_mesh_t * mesh_get(char * name);
static int meshes_load(void);
static void meshes_cleanup(void);

static int exit_command(int argc, char ** argv);
static int game_command(int argc, char ** argv);

/* components */
static void click(kgfw_sys_ui_component_t * self, float x, float y);

int main(int argc, char ** argv) {
	kgfw_log_register_callback(kgfw_log_handler);
	kgfw_logc_register_callback(kgfw_logc_handler);
	if (kgfw_init() != 0) {
		return 1;
	}

	kgfw_time_init();

	/* work-around for pylauncher bug */
	#ifndef KGFW_WINDOWS
	if (argc > 1) {
		if (chdir(argv[1]) != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to chdir to %s", argv[1]);
		}
	}
	#endif

	if (kgfw_window_create(&state.window, 800, 600, "hello, worl") != 0) {
		kgfw_deinit();
		return 2;
	}

	if (kgfw_audio_init() != 0) {
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 2;
	}

	if (kgfw_graphics_init(&state.window, &state.camera) != 0) {
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 3;
	}

	if (textures_load() != 0) {
		textures_cleanup();
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 4;
	}

	if (meshes_load() != 0) {
		meshes_cleanup();
		textures_cleanup();
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 4;
	}

	if (kgfw_console_init() != 0) {
		textures_cleanup();
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 4;
	}

	kgfw_console_register_command("exit", exit_command);
	kgfw_console_register_command("quit", exit_command);
	kgfw_console_register_command("game", game_command);

	if (kgfw_commands_init() != 0) {
		kgfw_console_deinit();
		textures_cleanup();
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 5;
	}

	if (kgfw_ecs_init() != 0) {
		kgfw_console_deinit();
		textures_cleanup();
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 6;
	}

	if (kgfw_sys_ui_init(&state.camera) == 0) {
		kgfw_ecs_deinit();
		kgfw_console_deinit();
		textures_cleanup();
		kgfw_graphics_deinit();
		kgfw_audio_deinit();
		kgfw_window_destroy(&state.window);
		kgfw_deinit();
		return 7;
	}

	{
		ktga_t * tga = texture_get("blank");
		kgfw_graphics_texture_t texture = {
			.bitmap = tga->bitmap,
			.width = tga->header.img_w,
			.height = tga->header.img_h,

			.fmt = KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
			.u_wrap = KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
			.v_wrap = KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
			.filtering = KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
		};
		state.textures[0] = texture;

		tga = texture_get("one");
		texture.bitmap = tga->bitmap;
		texture.width = tga->header.img_w;
		texture.height = tga->header.img_h;
		state.textures[1] = texture;

		tga = texture_get("two");
		texture.bitmap = tga->bitmap;
		texture.width = tga->header.img_w;
		texture.height = tga->header.img_h;
		state.textures[2] = texture;

		tga = texture_get("three");
		texture.bitmap = tga->bitmap;
		texture.width = tga->header.img_w;
		texture.height = tga->header.img_h;
		state.textures[3] = texture;

		tga = texture_get("four");
		texture.bitmap = tga->bitmap;
		texture.width = tga->header.img_w;
		texture.height = tga->header.img_h;
		state.textures[4] = texture;

		tga = texture_get("five");
		texture.bitmap = tga->bitmap;
		texture.width = tga->header.img_w;
		texture.height = tga->header.img_h;
		state.textures[5] = texture;

		tga = texture_get("six");
		texture.bitmap = tga->bitmap;
		texture.width = tga->header.img_w;
		texture.height = tga->header.img_h;
		state.textures[6] = texture;

		tga = texture_get("seven");
		texture.bitmap = tga->bitmap;
		texture.width = tga->header.img_w;
		texture.height = tga->header.img_h;
		state.textures[7] = texture;

		tga = texture_get("eight");
		texture.bitmap = tga->bitmap;
		texture.width = tga->header.img_w;
		texture.height = tga->header.img_h;
		state.textures[8] = texture;

		tga = texture_get("nine");
		texture.bitmap = tga->bitmap;
		texture.width = tga->header.img_w;
		texture.height = tga->header.img_h;
		state.textures[9] = texture;
	}

	kgfw_input_key_register_callback(kgfw_key_handler);
	kgfw_input_mouse_button_register_callback(kgfw_mouse_button_handle);
	kgfw_input_gamepad_register_callback(kgfw_gamepad_handle);

	kgfw_entity_t * player = kgfw_entity_new("player");
	if (player == NULL) {
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "player entity creation failure");
		return 99;
	}

	storage.sudokus = malloc(sizeof(sudoku_t) * state.board.width * state.board.height);
	if (storage.sudokus == NULL) {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to malloc sudoku board");
		return 99;
	}

	storage.solutions = malloc(sizeof(unsigned int) * state.board.width * state.board.height);
	if (storage.solutions == NULL) {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to malloc sudoku solutions");
		return 99;
	}
	memset(storage.solutions, 0, sizeof(unsigned int) * state.board.width * state.board.height);

	kgfw_graphics_mesh_t mesh = {
		.vertices = (kgfw_graphics_vertex_t[]) {
			{
				.x = -1, .y = -1, .z = 0,
				.r = 1, .g = 1, .b = 1,
				.nx = 0, .ny = 0, .nz = 0,
				.u = 0, .v = 0,
			},
			{
				.x = -1, .y = 1, .z = 0,
				.r = 1, .g = 1, .b = 1,
				.nx = 0, .ny = 0, .nz = 0,
				.u = 0, .v = 1,
			},
			{
				.x = 1, .y = -1, .z = 0,
				.r = 1, .g = 1, .b = 1,
				.nx = 0, .ny = 0, .nz = 0,
				.u = 1, .v = 0,
			},
			{
				.x = 1, .y = 1, .z = 0,
				.r = 1, .g = 1, .b = 1,
				.nx = 0, .ny = 0, .nz = 0,
				.u = 1, .v = 1,
			},
		},
		.vertices_count = 4,

		.indices = (unsigned int[]) {
			1, 0, 2,
			1, 2, 3,
		},
		.indices_count = 6,

		.pos = {
			0, 0, 0,
		},
		.rot = {
			0, 0, 0,
		},
		.scale = {
			0, 1, 1
		},
	};

	state.selector = kgfw_graphics_mesh_new(&mesh, NULL);
	if (state.selector == NULL) {
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "selector creation failure");
		return 99;
	}
	state.selector->transform.scale[0] = 0;

	{
		ktga_t * tga = texture_get("selector");
		if (tga == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "failed to find selector texture");
			return 99;
		}

		kgfw_graphics_texture_t texture = {
			.bitmap = tga->bitmap,
			.width = tga->header.img_w,
			.height = tga->header.img_h,
			.fmt = KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
			.u_wrap = KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
			.v_wrap = KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
			.filtering = KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
		};

		kgfw_graphics_mesh_texture(state.selector, &texture, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
	}

	for (unsigned int i = 0; i < state.board.width * state.board.height; ++i) {
		storage.sudokus[i].entity = kgfw_entity_new(NULL);
		if (storage.sudokus[i].entity == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "sudoku %u creation failure", i);
			return 100;
		}

		storage.sudokus[i].ui_comp = kgfw_entity_attach_component(storage.sudokus[i].entity, kgfw_sys_ui_get_uuid());
		if (storage.sudokus[i].ui_comp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_INFO, "sudoku %u ui component attachment failure", i);
			return 100;
		}
		storage.sudokus[i].number = i % state.board.width + 1;

		storage.sudokus[i].ui_comp->mesh = kgfw_graphics_mesh_new(&mesh, NULL);
		kgfw_graphics_mesh_texture(storage.sudokus[i].ui_comp->mesh, &state.textures[storage.sudokus[i].number], KGFW_GRAPHICS_TEXTURE_USE_COLOR);

		float w = state.board.width;
		float h = state.board.height;
		float nx = fmod(i, state.board.width) * 2 / state.board.width - 1;
		float ny = (i / state.board.width + 1) / w * 2.0f - 1;

		storage.sudokus[i].ui_comp->rect = (kgfw_sys_ui_rect_t) {
			.width = 1.0f / w,
			.height = 1.0f / h,
			.x = nx,
			.y = ny,

			.origin = {
				.x = -1,
				.y = 1,
			},
		};
		storage.sudokus[i].ui_comp->click = click;

		if (i < state.board.width * state.board.height - 1 && i % state.board.width != state.board.width - 1) {
			storage.sudokus[i].right = &storage.sudokus[i + 1];
		} else {
			storage.sudokus[i].right = NULL;
		}
		if (i >= state.board.width) {
			storage.sudokus[i].down = &storage.sudokus[i - state.board.width];
		} else {
			storage.sudokus[i].down = NULL;
		}
		if (i >= 1 && i % state.board.width != 0) {
			storage.sudokus[i].left = &storage.sudokus[i - 1];
		} else {
			storage.sudokus[i].left = NULL;
		}
		if (i < state.board.width * (state.board.height - 1)) {
			storage.sudokus[i].up = &storage.sudokus[i + state.board.width];
		} else {
			storage.sudokus[i].up = NULL;
		}
	}

	sudoku_t * piece = &storage.sudokus[0];
	while (piece != NULL) {
		while (piece->right != NULL) {
			piece = piece->right;
		}
		piece = piece->down;
		if (piece == NULL) {
			break;
		}
		while (piece->left != NULL) {
			piece = piece->left;
		}
		piece = piece->down;
	}

	while (!state.window.closed && !state.exit) {
		kgfw_time_start();
		if (kgfw_graphics_draw() != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to draw");
			break;
		}

		if (kgfw_window_update(&state.window) != 0) {
			state.exit = 1;
			break;
		}

		{
			unsigned int w = state.window.width;
			unsigned int h = state.window.height;
			if (kgfw_update() != 0) {
				state.exit = 1;
				break;
			}
			if (w != state.window.width || h != state.window.height) {
				kgfw_graphics_viewport(state.window.width, state.window.height);
				state.camera.ratio = state.window.width / (float)state.window.height;
			}
		}

		kgfw_time_end();
		kgfw_ecs_update();
		if (state.selected == NULL) {
			state.selector->transform.scale[0] = 0;
		} else {
			state.selector->transform.pos[0] = state.selected->ui_comp->mesh->transform.pos[0];
			state.selector->transform.pos[1] = state.selected->ui_comp->mesh->transform.pos[1];
			state.selector->transform.scale[0] = state.selected->ui_comp->mesh->transform.scale[0];
			state.selector->transform.scale[1] = state.selected->ui_comp->mesh->transform.scale[1];
		}

		kgfw_input_update();

		kgfw_audio_update();
		kgfw_time_end();
		if (state.input) {
			//kgfw_logf(KGFW_LOG_SEVERITY_INFO, "abs time: %f    frame time: %f    fps: %f", kgfw_time_get(), kgfw_time_delta(), 1 / kgfw_time_delta());
		}
	}

	kgfw_ecs_deinit();
	kgfw_console_deinit();
	meshes_cleanup();
	textures_cleanup();
	kgfw_graphics_deinit();
	kgfw_audio_deinit();
	kgfw_window_destroy(&state.window);
	kgfw_deinit();

	return 0;
}

static int kgfw_log_handler(kgfw_log_severity_enum severity, char * string) {
	char * severity_strings[] = { "CONSOLE", "TRACE", "DEBUG", "INFO", "WARN", "ERROR" };
	printf("[%s] %s\n", severity_strings[severity % 6], string);

	return 0;
}

static int kgfw_logc_handler(kgfw_log_severity_enum severity, char character) {
	putc(character, stdout);
	fflush(stdout);

	return 0;
}

static void kgfw_key_handler(kgfw_input_key_enum key, unsigned char action) {
	if (key == KGFW_KEY_GRAVE && action == 1) {
		unsigned char enabled = kgfw_console_is_input_enabled();
		state.input = enabled;
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "console input %sabled", !enabled ? "en" : "dis");
		kgfw_console_input_enable(!enabled);
	}

	if (key == KGFW_KEY_ESCAPE) {
		state.exit = 1;
	}

	if (state.input) {
		if (key == KGFW_KEY_R && action == 1) {
			char * argv[3] = { "gfx", "reload", "shaders" };
			kgfw_console_run(3, argv);
		}
	}

	if (state.selected != NULL) {
		if (key >= KGFW_KEY_0 && key <= KGFW_KEY_9) {
			state.selected->number = key - KGFW_KEY_0;
			kgfw_graphics_mesh_texture_detach(state.selected->ui_comp->mesh, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
			kgfw_graphics_mesh_texture(state.selected->ui_comp->mesh, &state.textures[state.selected->number], KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}

		if (action == 1) {
			if (key == KGFW_KEY_LEFT && state.selected->left != NULL) {
				state.selected = state.selected->left;
			}
			if (key == KGFW_KEY_RIGHT && state.selected->right != NULL) {
				state.selected = state.selected->right;
			}
			if (key == KGFW_KEY_DOWN && state.selected->down != NULL) {
				state.selected = state.selected->down;
			}
			if (key == KGFW_KEY_UP && state.selected->up != NULL) {
				state.selected = state.selected->up;
			}
		}
	} else {
		if (key == KGFW_KEY_LEFT || key == KGFW_KEY_RIGHT || key == KGFW_KEY_DOWN || key == KGFW_KEY_UP) {
			state.selected = &storage.sudokus[(state.board.width * state.board.height) / 2];
		}
	}
}

static void kgfw_mouse_button_handle(kgfw_input_mouse_button_enum button, unsigned char action) {
	return;
}

static void kgfw_gamepad_handle(kgfw_gamepad_t * gamepad) {
	if (!gamepad->status.connected) {
		kgfw_logf(KGFW_LOG_SEVERITY_INFO, "controller %u disconnected", gamepad->id);
	}

	if (kgfw_input_gamepad_pressed(gamepad, KGFW_GAMEPAD_A)) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "a pressed");
	}
	if (kgfw_input_gamepad_pressed(gamepad, KGFW_GAMEPAD_B)) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "b pressed");
	}
	if (kgfw_input_gamepad_pressed(gamepad, KGFW_GAMEPAD_X)) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "x pressed");
	}
	if (kgfw_input_gamepad_pressed(gamepad, KGFW_GAMEPAD_Y)) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "y pressed");
	}
	if (kgfw_input_gamepad_pressed(gamepad, KGFW_GAMEPAD_START)) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "start pressed");
		state.input = !state.input;
	}
	if (kgfw_input_gamepad_pressed(gamepad, KGFW_GAMEPAD_BACK)) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "back pressed");
	}
	if (kgfw_input_gamepad_pressed(gamepad, KGFW_GAMEPAD_LTHUMB)) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "lthumb pressed");
	}
	if (kgfw_input_gamepad_pressed(gamepad, KGFW_GAMEPAD_RTHUMB)) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "rthumb pressed");
	}
	if (kgfw_input_gamepad_pressed(gamepad, KGFW_GAMEPAD_LBUMPER)) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "lbumper pressed");
	}
	if (kgfw_input_gamepad_pressed(gamepad, KGFW_GAMEPAD_RBUMPER)) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "rbumper pressed");
	}
	if (kgfw_input_gamepad_pressed(gamepad, KGFW_GAMEPAD_DPAD_UP)) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "dpad up pressed");
	}
	if (kgfw_input_gamepad_pressed(gamepad, KGFW_GAMEPAD_DPAD_DOWN)) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "dpad down pressed");
	}
	if (kgfw_input_gamepad_pressed(gamepad, KGFW_GAMEPAD_DPAD_LEFT)) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "dpad left pressed");
	}
	if (kgfw_input_gamepad_pressed(gamepad, KGFW_GAMEPAD_DPAD_RIGHT)) {
		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "dpad right pressed");
	}

	//kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "rt %f lt %f", gamepad->left_trigger, gamepad->right_trigger);
}

/*
static int player_ortho_movement_update(player_movement_t * self, player_movement_state_t * mstate) {
	if (kgfw_input_mouse_button(KGFW_MOUSE_LBUTTON)) {
		float sx, sy;

		kgfw_input_mouse_pos(&sx, &sy);
		vec4 mouse = { (sx * 2 / state.window.width) - 1, (-sy * 2 / state.window.height) + 1, state.camera.pos[2], 1 };

		mat4x4 vp;
		mat4x4 ivp;
		mat4x4 v;
		{
			kgfw_camera_perspective(&state.camera, vp);
			kgfw_camera_view(&state.camera, v);
			mat4x4_mul(vp, vp, v);
			mat4x4_invert(ivp, vp);
		}

		vec4 pos = { 0, 0, 0, 1 };
		mat4x4_mul_vec4(pos, ivp, mouse);
	}
	
	if (kgfw_input_key(KGFW_KEY_LSHIFT)) {
		float x, y;
		kgfw_input_mouse_scroll(&x, &y);
		state.camera.pos[0] += y * state.camera.scale[0] / 5;
	} else {
		float x, y;
		kgfw_input_mouse_scroll(&x, &y);
		state.camera.pos[0] -= x * state.camera.scale[0] / 5;
		state.camera.pos[1] += y * state.camera.scale[1] / 5;
	}

	if (kgfw_input_key(KGFW_KEY_LEFT)) {
		state.camera.pos[0] -= state.camera.scale[0] / 5;
	}
	if (kgfw_input_key(KGFW_KEY_RIGHT)) {
		state.camera.pos[0] += state.camera.scale[0] / 5;
	}
	if (kgfw_input_key(KGFW_KEY_DOWN)) {
		state.camera.pos[1] -= state.camera.scale[1] / 5;
	}
	if (kgfw_input_key(KGFW_KEY_UP)) {
		state.camera.pos[1] += state.camera.scale[1] / 5;
	}

	if (kgfw_input_key(KGFW_KEY_EQUAL)) {
		state.camera.scale[0] /= 1.05f;
		state.camera.scale[1] /= 1.05f;
	} else if (kgfw_input_key(KGFW_KEY_MINUS)) {
		state.camera.scale[0] *= 1.05f;
		state.camera.scale[1] *= 1.05f;
	}

	return 0;
}*/

static int exit_command(int argc, char ** argv) {
	state.exit = 1;

	return 0;
}

static int textures_load(void) {
	struct {
		void * buffer;
		unsigned long long int size;
	} file = {
		NULL, 0
	};
	{
		FILE * fp = fopen("./assets/config.koml", "rb");
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to open \"config.koml\"");
			return 1;
		}

		fseek(fp, 0L, SEEK_END);
		file.size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		file.buffer = malloc(file.size);
		if (file.buffer == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to alloc buffer for \"config.koml\"");
			return 2;
		}

		if (fread(file.buffer, 1, file.size, fp) != file.size) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to read from \"config.koml\"");
			return 3;
		}

		fclose(fp);
	}

	koml_table_t ktable;
	if (koml_table_load(&ktable, file.buffer, file.size) != 0) {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to load koml table from \"config.koml\"");
		return 4;
	}

	koml_symbol_t * files = koml_table_symbol(&ktable, "textures:files");
	koml_symbol_t * names = koml_table_symbol(&ktable, "textures:names");
	if (files == NULL) {
		goto skip_tga_load;
	}
	storage.textures_count = files->data.array.length;

	for (unsigned long long int i = 0; i < storage.textures_count; ++i) {
		FILE * fp = fopen(files->data.array.elements.string[i], "rb");
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to open \"%s\"", files->data.array.elements.string[i]);
			return 5;
		}

		fseek(fp, 0L, SEEK_END);
		unsigned long long int size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		void * buffer = malloc(size);
		if (buffer == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to alloc buffer for \"%s\"", files->data.array.elements.string[i]);
			return 6;
		}

		if (fread(buffer, 1, size, fp) != size) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to read from \"%s\" %llu", files->data.array.elements.string[i], size);
			return 7;
		}

		fclose(fp);
		ktga_load(&storage.textures[i], buffer, size);
		free(buffer);

		if (names == NULL) {
			unsigned long long int len = strlen(files->data.array.elements.string[i]);
			storage.texture_hashes[i] = kgfw_hash(files->data.array.elements.string[i]);
		}
		else {
			unsigned long long int len = strlen(names->data.array.elements.string[i]);
			storage.texture_hashes[i] = kgfw_hash(names->data.array.elements.string[i]);
		}
	}

skip_tga_load:;
	koml_table_destroy(&ktable);
	return 0;
}

static void textures_cleanup(void) {
	for (unsigned long long int i = 0; i < storage.textures_count; ++i) {
		if (storage.textures[i].bitmap == NULL) {
			continue;
		}

		ktga_destroy(&storage.textures[i]);
		memset(&storage.textures[i], 0, sizeof(ktga_t));
		memset(&storage.texture_hashes[i], 0, sizeof(kgfw_hash_t));
	}
	storage.textures_count = 0;
}

static ktga_t * texture_get(char * name) {
	kgfw_hash_t hash = kgfw_hash(name);
	for (unsigned long long int i = 0; i < storage.textures_count; ++i) {
		if (hash == storage.texture_hashes[i]) {
			return &storage.textures[i];
		}
	}

	return NULL;
}

static int meshes_load(void) {
	struct {
		void * buffer;
		unsigned long long int size;
	} file = {
		NULL, 0
	};
	{
		FILE * fp = fopen("./assets/config.koml", "rb");
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to open \"config.koml\"");
			return 1;
		}

		fseek(fp, 0L, SEEK_END);
		file.size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		file.buffer = malloc(file.size);
		if (file.buffer == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to alloc buffer for \"config.koml\"");
			return 2;
		}

		if (fread(file.buffer, 1, file.size, fp) != file.size) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to read from \"config.koml\"");
			return 3;
		}

		fclose(fp);
	}

	koml_table_t ktable;
	if (koml_table_load(&ktable, file.buffer, file.size) != 0) {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to load koml table from \"config.koml\"");
		return 4;
	}

	koml_symbol_t * files = koml_table_symbol(&ktable, "meshes:files");
	koml_symbol_t * names = koml_table_symbol(&ktable, "meshes:names");
	if (files == NULL) {
		goto skip_mesh_load;
	}
	storage.meshes_count = files->data.array.length;

	for (unsigned long long int mi = 0; mi < storage.meshes_count; ++mi) {
		FILE * fp = fopen(files->data.array.elements.string[mi], "rb");
		if (fp == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to open \"%s\"", files->data.array.elements.string[mi]);
			return 5;
		}

		fseek(fp, 0L, SEEK_END);
		unsigned long long int size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		void * buffer = malloc(size);
		if (buffer == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to alloc buffer for \"%s\"", files->data.array.elements.string[mi]);
			return 6;
		}

		if (fread(buffer, 1, size, fp) != size) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to read from \"%s\" %llu", files->data.array.elements.string[mi], size);
			return 7;
		}

		fclose(fp);

		kobj_t kobj;
		kobj_load(&kobj, buffer, size);

		storage.meshes[mi] = (kgfw_graphics_mesh_t) {
			.pos = { 0, 0, 0 },
			.rot = { 0, 0, 0 },
			.scale = { 1, 1, 1 },
		};

		storage.meshes[mi].vertices = malloc(sizeof(kgfw_graphics_vertex_t) * kobj.vcount);
		if (storage.meshes[mi].vertices == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to allocate mesh vertices buffer");
			return 1;
		}
		storage.meshes[mi].indices = malloc(sizeof(unsigned int) * 3 * kobj.fcount);
		if (storage.meshes[mi].indices == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to allocate mesh indices buffer");
			return 2;
		}
		storage.meshes[mi].vertices_count = kobj.vcount;
		storage.meshes[mi].indices_count = kobj.fcount * 3;

		for (unsigned long long int i = 0; i < storage.meshes[mi].vertices_count; ++i) {
			storage.meshes[mi].vertices[i].x = kobj.vertices[i * 3 + 0];
			storage.meshes[mi].vertices[i].y = kobj.vertices[i * 3 + 1];
			storage.meshes[mi].vertices[i].z = kobj.vertices[i * 3 + 2];
			storage.meshes[mi].vertices[i].r = 1;
			storage.meshes[mi].vertices[i].g = 1;
			storage.meshes[mi].vertices[i].b = 1;
		}

		for (unsigned long long int i = 0; i < kobj.fcount; ++i) {
			if (kobj.faces[i].vn1 != 0) {
				storage.meshes[mi].vertices[kobj.faces[i].v1 - 1].nx = kobj.normals[(kobj.faces[i].vn1 - 1) * 3 + 0];
				storage.meshes[mi].vertices[kobj.faces[i].v1 - 1].ny = kobj.normals[(kobj.faces[i].vn1 - 1) * 3 + 1];
				storage.meshes[mi].vertices[kobj.faces[i].v1 - 1].nz = kobj.normals[(kobj.faces[i].vn1 - 1) * 3 + 2];
			}
			if (kobj.faces[i].vn2 != 0) {
				storage.meshes[mi].vertices[kobj.faces[i].v2 - 1].nx = kobj.normals[(kobj.faces[i].vn2 - 1) * 3 + 0];
				storage.meshes[mi].vertices[kobj.faces[i].v2 - 1].ny = kobj.normals[(kobj.faces[i].vn2 - 1) * 3 + 1];
				storage.meshes[mi].vertices[kobj.faces[i].v2 - 1].nz = kobj.normals[(kobj.faces[i].vn2 - 1) * 3 + 2];
			}
			if (kobj.faces[i].vn3 != 0) {
				storage.meshes[mi].vertices[kobj.faces[i].v3 - 1].nx = kobj.normals[(kobj.faces[i].vn3 - 1) * 3 + 0];
				storage.meshes[mi].vertices[kobj.faces[i].v3 - 1].ny = kobj.normals[(kobj.faces[i].vn3 - 1) * 3 + 1];
				storage.meshes[mi].vertices[kobj.faces[i].v3 - 1].nz = kobj.normals[(kobj.faces[i].vn3 - 1) * 3 + 2];
			}

			if (kobj.faces[i].vt1 != 0) {
				storage.meshes[mi].vertices[kobj.faces[i].v1 - 1].u = kobj.uvs[(kobj.faces[i].vt1 - 1) * 2 + 0];
				storage.meshes[mi].vertices[kobj.faces[i].v1 - 1].v = kobj.uvs[(kobj.faces[i].vt1 - 1) * 2 + 1];
			}
			if (kobj.faces[i].vt2 != 0) {
				storage.meshes[mi].vertices[kobj.faces[i].v2 - 1].u = kobj.uvs[(kobj.faces[i].vt2 - 1) * 2 + 0];
				storage.meshes[mi].vertices[kobj.faces[i].v2 - 1].v = kobj.uvs[(kobj.faces[i].vt2 - 1) * 2 + 1];
			}
			if (kobj.faces[i].vt3 != 0) {
				storage.meshes[mi].vertices[kobj.faces[i].v3 - 1].u = kobj.uvs[(kobj.faces[i].vt3 - 1) * 2 + 0];
				storage.meshes[mi].vertices[kobj.faces[i].v3 - 1].v = kobj.uvs[(kobj.faces[i].vt3 - 1) * 2 + 1];
			}

			if (kobj.faces[i].v1 != 0) {
				storage.meshes[mi].indices[i * 3 + 0] = kobj.faces[i].v1 - 1;
			}
			if (kobj.faces[i].v2 != 0) {
				storage.meshes[mi].indices[i * 3 + 1] = kobj.faces[i].v2 - 1;
			}
			if (kobj.faces[i].v3 != 0) {
				storage.meshes[mi].indices[i * 3 + 2] = kobj.faces[i].v3 - 1;
			}
		}

		kobj_destroy(&kobj);

		free(buffer);

		if (names == NULL) {
			storage.mesh_hashes[mi] = kgfw_hash(files->data.array.elements.string[mi]);
		} else {
			storage.mesh_hashes[mi] = kgfw_hash(names->data.array.elements.string[mi]);
		}
	}

skip_mesh_load:;
	koml_table_destroy(&ktable);
	return 0;
}

static void meshes_cleanup(void) {
	for (unsigned long long int i = 0; i < storage.meshes_count; ++i) {
		if (storage.meshes[i].vertices != NULL) {
			free(storage.meshes[i].vertices);
		}
		if (storage.meshes[i].indices != NULL) {
			free(storage.meshes[i].indices);
		}
	}
	storage.meshes_count = 0;
}

static kgfw_graphics_mesh_t * mesh_get(char * name) {
	kgfw_hash_t hash = kgfw_hash(name);
	for (unsigned long long int i = 0; i < storage.meshes_count; ++i) {
		if (hash == storage.mesh_hashes[i]) {
			return &storage.meshes[i];
		}
	}

	return NULL;
}

static int game_command(int argc, char ** argv) {
	const char * subcommands = "mesh    fov    movement    arrow_speed    mouse_speed    jump_force    gravity    pos";
	if (argc < 2) {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "subcommands: %s", subcommands);
		return 0;
	}

	if (strcmp(argv[1], "mesh") == 0) {
		if (argc < 3) {
			const char * args = "[mesh name] (optional texture name)";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}
		kgfw_graphics_mesh_node_t * node = NULL;
		if (argc >= 3) {
			kgfw_graphics_mesh_t * mesh = mesh_get(argv[2]);
			if (mesh == NULL) {
				kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "mesh does not exist \"%s\"", argv[2]);
				return 0;
			}
			node = kgfw_graphics_mesh_new(mesh, NULL);
			node->transform.pos[0] = state.camera.pos[0];
			node->transform.pos[1] = state.camera.pos[1];
			node->transform.pos[2] = state.camera.pos[2];

			node->transform.rot[1] = state.camera.rot[1];
		}
		if (argc >= 4) {
			ktga_t * tga = texture_get(argv[3]);
			if (tga == NULL) {
				kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "texture does not exist \"%s\"", argv[3]);
				return 0;
			}

			kgfw_graphics_texture_t tex = {
				.bitmap = tga->bitmap,
				.width = tga->header.img_w,
				.height = tga->header.img_h,
				.fmt = KGFW_GRAPHICS_TEXTURE_FORMAT_BGRA,
				.u_wrap = KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				.v_wrap = KGFW_GRAPHICS_TEXTURE_WRAP_CLAMP,
				.filtering = KGFW_GRAPHICS_TEXTURE_FILTERING_NEAREST,
			};
			kgfw_graphics_mesh_texture(node, &tex, KGFW_GRAPHICS_TEXTURE_USE_COLOR);
		}
	} else if (strcmp(argv[1], "fov") == 0) {
		if (argc < 3) {
			const char * args = "[field of view in degrees]";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}

		float f = strtof(argv[2], NULL);
		state.camera.fov = f;
	} else if (strcmp(argv[1], "movement") == 0) {
		if (argc < 3) {
			const char * args = "[movement speed]";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}

		float f = strtof(argv[2], NULL);
		state.settings.movement = f;
	} else if (strcmp(argv[1], "arrow_speed") == 0) {
		if (argc < 3) {
			const char * args = "[arrow speed]";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}

		float f = strtof(argv[2], NULL);
		state.settings.arrow_speed = f;
	} else if (strcmp(argv[1], "mouse_speed") == 0) {
		if (argc < 3) {
			const char * args = "[mouse speed]";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}

		float f = strtof(argv[2], NULL);
		state.settings.mouse_speed = f;
	} else if (strcmp(argv[1], "jump_force") == 0) {
		if (argc < 3) {
			const char * args = "[jump force]";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}

		float f = strtof(argv[2], NULL);
		state.settings.jump_force = f;
	} else if (strcmp(argv[1], "gravity") == 0) {
		if (argc < 3) {
			const char * args = "[gravity]";
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}

		float f = strtof(argv[2], NULL);
		state.settings.gravity = f;
	} else if (strcmp(argv[1], "pos") == 0) {
		if (argc < 5) {
			kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "x: %f, y: %f, z: %f", player.pos[0], player.pos[1], player.pos[2]);
			//const char * args = "[pos]";
			//kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "arguments: %s", args);
			return 0;
		}

		player.pos[0] = strtof(argv[2], NULL);
		player.pos[1] = strtof(argv[3], NULL);
		player.pos[2] = strtof(argv[4], NULL);
		state.camera.pos[0] = player.pos[0];
		state.camera.pos[1] = player.pos[1];
		state.camera.pos[2] = player.pos[2];
	} else {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "subcommands: %s", subcommands);
	}

	return 0;
}


/* components */
static void click(kgfw_sys_ui_component_t * self, float x, float y) {
	for (unsigned int i = 0; i < state.board.width * state.board.height; ++i) {
		if (storage.sudokus[i].ui_comp == self) {
			//kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "selected %u (%f, %f) [%f, %f] {%f, %f}", storage.sudokus[i].number, x, y, self->rect.x, self->rect.y, self->rect.width, self->rect.height);
			if (&storage.sudokus[i] == state.selected) {
				state.selected = NULL;
				return;
			}
			state.selected = &storage.sudokus[i];
			kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "%u", state.selected->number);
			return;
		}
	}
}