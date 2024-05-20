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
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#else
#include <windows.h>
#endif

#include <linmath.h>
#include <jni.h>

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
		JavaVM* jvm;
		JNIEnv* env;
		kgfw_uuid_t system_id;
		
		struct {
			jclass class;
			jmethodID init;
			jmethodID deinit;
		} jstatic;
	} java;
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
};

#define STORAGE_MAX_TEXTURES 64
#define STORAGE_MAX_MESHES 64

struct {
	ktga_t textures[STORAGE_MAX_TEXTURES];
	unsigned long long int textures_count;
	kgfw_hash_t texture_hashes[STORAGE_MAX_TEXTURES];
	kgfw_graphics_mesh_t meshes[STORAGE_MAX_MESHES];
	unsigned long long int meshes_count;
	kgfw_hash_t mesh_hashes[STORAGE_MAX_MESHES];
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
static void kgfw_mouse_button_handle(kgfw_window_t* window, kgfw_input_mouse_button_enum button, unsigned char action);
static void kgfw_gamepad_handle(kgfw_gamepad_t * gamepad);
static ktga_t * texture_get(char * name);
static int textures_load(void);
static void textures_cleanup(void);
static kgfw_graphics_mesh_t * mesh_get(char * name);
static int meshes_load(void);
static void meshes_cleanup(void);

static int exit_command(int argc, char ** argv);

typedef struct java_script_component {
	kgfw_component_update_f update;
	kgfw_component_start_f start;
	kgfw_component_destroy_f destroy;
	/* identifier for component instance */
	kgfw_uuid_t instance_id;
	/* id for the component type */
	kgfw_uuid_t type_id;
	kgfw_entity_t * entity;

	jclass script_class;
	jobject script_object;
	jmethodID script_update;
	jmethodID script_start;
	jmethodID script_destroy;
} java_script_component_t;

static void java_scripts_component_start(kgfw_component_t * self);
static void java_scripts_component_update(kgfw_component_t * self);
static void java_scripts_component_destroy(kgfw_component_t * self);

static void java_scripts_system_start(kgfw_system_t * self, kgfw_component_node_t * components);
static void java_scripts_system_update(kgfw_system_t * self, kgfw_component_node_t * components);
static void java_scripts_system_destroy(kgfw_system_t * self);

static int scripts_load(void);
static void scripts_cleanup(void);
static void scripts_get_files(unsigned int * out_count, char *** out_files);

static int jni_check_exception(void) {
	if ((*state.java.env)->ExceptionOccurred(state.java.env)) {
		(*state.java.env)->ExceptionDescribe(state.java.env);
		return 1;
	}

	return 0;
}

int engine_main(int argc, char ** argv) {
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

	/* load java script ecs system */
	kgfw_system_t java_system = {
		.update = &java_scripts_system_update,
		.start = &java_scripts_system_start,
		.destroy = &java_scripts_system_destroy,
	};

	state.java.system_id = kgfw_system_construct("java scripts", sizeof(java_system), &java_system);
	if (state.java.system_id == KGFW_ECS_INVALID_ID) {
		kgfw_log(KGFW_LOG_SEVERITY_ERROR, "Failed to setup java scripts ecs system");
		return 7;
	}

	{
		unsigned int script_count = 0;
		char ** script_files = NULL;
		scripts_get_files(&script_count, &script_files);

		JavaVMOption options[2];
		char * path = "./assets/scripts/";
		options[0].optionString = "-Djava.class.path=./assets/scripts/";
		//options[1].optionString = "-verbose:class";

		if (script_count != 0) {
			unsigned int final_size = strlen(options[0].optionString) + script_count + strlen(path) * script_count;
			char * final = NULL;

			for (unsigned int i = 0; i < script_count; ++i) {
				final_size += strlen(script_files[i]);
			}

			final = malloc(final_size);
			if (final == NULL) {
				return 10;
			}

			memcpy(final, options[0].optionString, strlen(options[0].optionString));

			unsigned int final_index = strlen(options[0].optionString);
			for (unsigned int i = 0; i < script_count; ++i) {
			#ifdef KGFW_WINDOWS
				final[final_index] = ';';
			#else
				final[final_index] = ':';
			#endif
				++final_index;

				memcpy(&final[final_index], path, strlen(path));
				final_index += strlen(path);

				memcpy(&final[final_index], script_files[i], strlen(script_files[i]));
				final_index += strlen(script_files[i]);

				free(script_files[i]);
			}

			free(script_files);

			final[final_index] = '\0';
			options[0].optionString = final;
		}

		kgfw_logf(KGFW_LOG_SEVERITY_DEBUG, "JVM options: %s", options[0].optionString);

		JavaVMInitArgs jvm_args = {
			.version = JNI_VERSION_1_8,
			.nOptions = 1,
			.options = options,
			.ignoreUnrecognized = 0,
		};

		if (JNI_CreateJavaVM(&state.java.jvm, (void**) &state.java.env, &jvm_args) != JNI_OK) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to init java jvm");
			return 8;
		}
		
		if (script_count != 0) {
			free(options[0].optionString);
		}
	}

	if (scripts_load() != 0) {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to load scripts");
		return 9;
	}

	while (!state.window.closed && !state.exit) {
		kgfw_time_start();
		if (kgfw_graphics_draw() != 0) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to draw");
			break;
		}

		{
			unsigned int w = state.window.width;
			unsigned int h = state.window.height;
			if (kgfw_update() != 0) {
				state.exit = 1;
				break;
			}
			if (kgfw_window_update(&state.window) != 0) {
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
		kgfw_input_update();
		kgfw_audio_update();
		kgfw_time_end();
	}

	scripts_cleanup();

	(*state.java.jvm)->DestroyJavaVM(state.java.jvm);

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

static int scripts_load(void) {
	struct {
		void * buffer;
		unsigned long long int size;
	} file = {
		NULL, 0
	};
	{
		FILE * fp = fopen("./assets/scripts/scripts.koml", "rb");
		if (fp == NULL) {
			return 0;
		}

		fseek(fp, 0L, SEEK_END);
		file.size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		file.buffer = malloc(file.size);
		if (file.buffer == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to alloc buffer for \"scripts.koml\"");
			return 2;
		}

		if (fread(file.buffer, 1, file.size, fp) != file.size) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to read from \"scripts.koml\"");
			return 3;
		}

		fclose(fp);
	}

	koml_table_t ktable;
	if (koml_table_load(&ktable, file.buffer, file.size) != 0) {
		kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "failed to load koml table from \"scripts.koml\"");
		free(file.buffer);
		return 0;
	}
	
	koml_symbol_t * classpaths = koml_table_symbol(&ktable, "scripts:classpaths");
	if (classpaths == NULL) {
		goto load_failure;
	}

	if (classpaths->type != KOML_TYPE_ARRAY) {
		goto load_failure;
	}

	if (classpaths->data.array.type == KOML_TYPE_STRING) {
		goto load_failure;
	}

	java_script_component_t comp;
	for (unsigned long long int i = 0; i < classpaths->data.array.length; ++i) {
		char * path = classpaths->data.array.elements.string[i];
		char * component_name = malloc(strlen(path) + 6);
		if (component_name == NULL) {
			kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Failed to allocate string for script \"%s\"", path);
			return 1;
		}

		component_name = "Java ";
		memcpy(component_name + 5, path, strlen(path));
		component_name[strlen(path) + 5] = '\0';

		comp.update = java_scripts_component_update;
		comp.start = java_scripts_component_start;
		comp.destroy = java_scripts_component_destroy;
		comp.instance_id = KGFW_ECS_INVALID_ID;
		comp.type_id = KGFW_ECS_INVALID_ID;
		comp.entity = NULL;
		comp.script_class = (*state.java.env)->FindClass(state.java.env, path);
		if (comp.script_class == NULL) {
			if (!jni_check_exception()) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "Class \"%s\" not found in jars when loading script specified in \"scripts.koml\"");
			}
			goto element_load_failure;
		}

		comp.script_object = NULL;
		comp.script_update = (*state.java.env)->GetMethodID(state.java.env, comp.script_class, "update", "()");
		comp.script_start = (*state.java.env)->GetMethodID(state.java.env, comp.script_class, "start", "()");
		comp.script_destroy = (*state.java.env)->GetMethodID(state.java.env, comp.script_class, "destroy", "()");

		kgfw_uuid_t component_type = kgfw_component_construct(component_name, sizeof(comp), &comp, state.java.system_id);
		if (component_type == KGFW_ECS_INVALID_ID) {
			goto element_load_failure;
		}

	element_load_failure:
		free(component_name);
	}

load_failure:;
	koml_symbol_t * jstatic = koml_table_symbol(&ktable, "scripts:static");
	if (jstatic == NULL) {
		goto skip_jstatic;
	}

	if (jstatic->type != KOML_TYPE_STRING) {
		kgfw_log(KGFW_LOG_SEVERITY_WARN, "\"scripts:static\" is not a string to static init class path");
		goto skip_jstatic;
	}

	state.java.jstatic.class = (*state.java.env)->FindClass(state.java.env, jstatic->data.string);
	if (state.java.jstatic.class == NULL) {
		if (!jni_check_exception()) {
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "\"%s\" (value from \"scripts:static\") is not a valid class path", jstatic->data.string);
		}
		goto skip_jstatic;
	}

	state.java.jstatic.init = (*state.java.env)->GetStaticMethodID(state.java.env, state.java.jstatic.class, "init", "()Z");
	if (state.java.jstatic.init != NULL) {
		if (!(*state.java.env)->CallStaticBooleanMethod(state.java.env, state.java.jstatic.class, state.java.jstatic.init)) {
			if (!jni_check_exception()) {
				kgfw_logf(KGFW_LOG_SEVERITY_ERROR, "%s::init failed to initialize game", jstatic->data.string);
			}
			return 3;
		}
	} else {
		if (!jni_check_exception()) {
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "\"%s\" (value from \"scripts:static\") class has no static boolean init() method", jstatic->data.string);
		}
	}

	state.java.jstatic.deinit = (*state.java.env)->GetStaticMethodID(state.java.env, state.java.jstatic.class, "deinit", "()V");
	if (state.java.jstatic.deinit == NULL) {
		if (!jni_check_exception()) {
			kgfw_logf(KGFW_LOG_SEVERITY_WARN, "\"%s\" (value from \"scripts:static\") class has no static void deinit() method", jstatic->data.string);
		}
	}

skip_jstatic:
	koml_table_destroy(&ktable);
	free(file.buffer);
	return 0;
}

static void scripts_cleanup(void) {
	if (state.java.jstatic.deinit != NULL) {
		(*state.java.env)->CallStaticVoidMethod(state.java.env, state.java.jstatic.class, state.java.jstatic.deinit);
	}
}

static void scripts_get_files(unsigned int * out_count, char *** out_files) {
	const char * path;
	*out_count = 0;
	*out_files = NULL;
#ifdef KGFW_WINDOWS
	path = ".\\assets\\scripts";
#else
	path = "./assets/scripts";
	struct stat st;
	if (stat(path, &st) != 0) {
		return;
	}

	if (!S_ISDIR(st.st_mode)) {
		return;
	}

	DIR * dir = opendir(path);
	if (access(path, F_OK) == -1 || dir == NULL) {
		return;
	}

	struct dirent * entry = NULL;
	while ((entry = readdir(dir))) {
		if (entry->d_type != DT_REG) {
			continue;
		}

		unsigned int len = strlen(entry->d_name);
		if (len < 5) {
			continue;
		}

		if (strcmp(entry->d_name + (len - 4), ".jar") == 0) {
			++(*out_count);
			if (*out_files == NULL) {
				*out_files = malloc(sizeof(char *) * (*out_count));
				if (*out_files == NULL) {
					*out_files = NULL;
					*out_count = 0;
					return;
				}
			} else {
				char *** f = realloc(*out_files, sizeof(char *) * (*out_count));
				if (f == NULL) {
					free(*out_files);
					*out_files = NULL;
					*out_count = 0;
					return;
				}
			}

			char * p;
			p = malloc(len + 1);
			if (p == NULL) {
				free(*out_files);
				*out_files = NULL;
				*out_count = 0;
				return;
			}
			
			memcpy(p, entry->d_name, len);
			p[len] = '\0';
			(*out_files)[(*out_count) - 1] = p;
		}
	}

	closedir(dir);
#endif
}

static void java_scripts_component_start(kgfw_component_t * self) {
	java_script_component_t * s = (java_script_component_t *) self;
	if (s->script_start != NULL) {
		(*state.java.env)->CallVoidMethod(state.java.env, s->script_object, s->script_start);
	}
}

static void java_scripts_component_update(kgfw_component_t * self) {
	java_script_component_t * s = (java_script_component_t *) self;
	if (s->script_start != NULL) {
		(*state.java.env)->CallVoidMethod(state.java.env, s->script_object, s->script_update);
	}
}

static void java_scripts_component_destroy(kgfw_component_t * self) {
	java_script_component_t * s = (java_script_component_t *) self;
	if (s->script_start != NULL) {
		(*state.java.env)->CallVoidMethod(state.java.env, s->script_object, s->script_destroy);
	}
}

static void java_scripts_system_start(kgfw_system_t * self, kgfw_component_node_t * components) {
	for (kgfw_component_node_t * n = components; n != NULL; n = n->next) {
		n->component->update(n->component);
	}
}

static void java_scripts_system_update(kgfw_system_t * self, kgfw_component_node_t * components) {
	kgfw_log(KGFW_LOG_SEVERITY_TRACE, "zaza");
	for (kgfw_component_node_t * n = components; n != NULL; n = n->next) {
		n->component->start(n->component);
	}
}

static void java_scripts_system_destroy(kgfw_system_t * self) {
	return;
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
}

static void kgfw_mouse_button_handle(kgfw_window_t* window, kgfw_input_mouse_button_enum button, unsigned char action) {
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
}

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
	} else {
		kgfw_logf(KGFW_LOG_SEVERITY_CONSOLE, "subcommands: %s", subcommands);
	}

	return 0;
}

static jint throw_jni_exception(JNIEnv* env, const char* exception, const char* message) {
	jclass exception_class = (*env)->FindClass(env, exception);
	if (exception_class == NULL) {
		return 1;
	}

	return (*env)->ThrowNew(env, exception_class, message);
}

/* kgfw_input */
JNIEXPORT jboolean JNICALL Java_kgfw_KGFW_inputGetKey(JNIEnv* env, jobject obj, jint key) {
	if (key >= KGFW_KEY_MAX || key < 0) {
		return JNI_FALSE;
	}

	return (jboolean) kgfw_input_key(key);
}

JNIEXPORT jboolean JNICALL Java_kgfw_KGFW_inputGetKeyDown(JNIEnv* env, jobject obj, jint key) {
	if (key >= KGFW_KEY_MAX || key < 0) {
		return JNI_FALSE;
	}

	return (jboolean) kgfw_input_key_down(key);
}

JNIEXPORT jboolean JNICALL Java_kgfw_KGFW_inputGetKeyUp(JNIEnv* env, jobject obj, jint key) {
	if (key >= KGFW_KEY_MAX || key < 0) {
		return JNI_FALSE;
	}

	return (jboolean) kgfw_input_key_up(key);
}

JNIEXPORT jboolean JNICALL Java_kgfw_KGFW_inputGetMouseButton(JNIEnv* env, jobject obj, jint button) {
	if (button < 0 || button >= KGFW_MOUSE_BUTTON_MAX) {
		return JNI_FALSE;
	}

	return (jboolean) kgfw_input_mouse_button(button);
}

JNIEXPORT jdouble JNICALL Java_kgfw_KGFW_inputGetMouseDeltaX(JNIEnv* env, jobject obj) {
	float x, y;
	kgfw_input_mouse_delta(&x, &y);
	return (jdouble) x;
}

JNIEXPORT jdouble JNICALL Java_kgfw_KGFW_inputGetMouseDeltaY(JNIEnv* env, jobject obj) {
	float x, y;
	kgfw_input_mouse_delta(&x, &y);
	return (jdouble) y;
}

JNIEXPORT jdouble JNICALL Java_kgfw_KGFW_inputGetMouseScrollX(JNIEnv* env, jobject obj) {
	float x, y;
	kgfw_input_mouse_scroll(&x, &y);
	return (jdouble) x;
}

JNIEXPORT jdouble JNICALL Java_kgfw_KGFW_inputGetMouseScrollY(JNIEnv* env, jobject obj) {
	float x, y;
	kgfw_input_mouse_scroll(&x, &y);
	return (jdouble) y;
}

JNIEXPORT jdouble JNICALL Java_kgfw_KGFW_inputGetMousePosX(JNIEnv* env, jobject obj) {
	float x, y;
	kgfw_input_mouse_pos(&x, &y);
	return (jdouble) x;
}

JNIEXPORT jdouble JNICALL Java_kgfw_KGFW_inputGetMousePosY(JNIEnv* env, jobject obj) {
	float x, y;
	kgfw_input_mouse_pos(&x, &y);
	return (jdouble) y;
}

/* kgfw_log */
JNIEXPORT void JNICALL Java_kgfw_KGFW_log(JNIEnv* env, jobject obj, jint severity, jstring message) {
	const char* nativeString = (*env)->GetStringUTFChars(env, message, 0);
	kgfw_log(severity, (char*) nativeString);
	(*env)->ReleaseStringUTFChars(env, message, nativeString);
}

JNIEXPORT void JNICALL Java_kgfw_KGFW_logc(JNIEnv* env, jobject obj, jint severity, jchar character) {
	kgfw_logc(severity, character);
}

/* kgfw_ecs */
JNIEXPORT jobject JNICALL Java_kgfw_KGFW_newEntity(JNIEnv* env, jobject obj, jstring name) {
	const char* e_name = NULL;
	if (name == NULL) {
		e_name = (*env)->GetStringUTFChars(env, name, 0);
	}

	kgfw_entity_t* kgfw_entity = kgfw_entity_new(e_name);
	if (e_name != NULL) {
		(*env)->ReleaseStringUTFChars(env, name, e_name);
	}

	if (kgfw_entity == NULL) {
		throw_jni_exception(env, "java/lang/RuntimeException", "Failed to create new entity");
		return NULL;
	}

	jobject entity = (*env)->AllocObject(env, );

	return NULL;
}

JNIEXPORT jobject JNICALL Java_kgfw_KGFW_copyEntity(JNIEnv* env, jobject obj, jstring string, jobject sourceEntity);

JNIEXPORT jobject JNICALL Java_kgfw_KGFW_findEntity__Ljava_lang_String_2(JNIEnv* env, jobject obj, jstring entityName);

JNIEXPORT jobject JNICALL Java_kgfw_KGFW_findEntity__J(JNIEnv* env, jobject obj, jlong entityID);

JNIEXPORT jobject JNICALL Java_kgfw_KGFW_entityGetComponent(JNIEnv* env, jobject obj, jobject entity, jlong componentID);

JNIEXPORT jobject JNICALL Java_kgfw_KGFW_entityAttachComponent__Lkgfw_Entity_2Ljava_lang_String_2(JNIEnv* env, jobject obj, jobject entity, jstring componentName) {
	return NULL;
}

JNIEXPORT jobject JNICALL Java_kgfw_KGFW_entityAttachComponent__Lkgfw_Entity_2J(JNIEnv* env, jobject obj, jobject entity, jlong componentID);

JNIEXPORT void JNICALL Java_kgfw_KGFW_destroyComponent(JNIEnv* env, jobject obj, jobject component);
