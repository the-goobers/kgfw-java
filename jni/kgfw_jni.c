#include "../kgfw/kgfw.h"
#include <jni.h>

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
	if (name == NULL) {
		throw_jni_exception(env, "java/lang/IllegalArgumentException", "New entity name is null");
		return NULL;
	}

	kgfw_entity_t* entity = 
}

JNIEXPORT jobject JNICALL Java_kgfw_KGFW_copyEntity(JNIEnv* env, jobject obj, jstring string, jobject sourceEntity);

JNIEXPORT jobject JNICALL Java_kgfw_KGFW_findEntity__Ljava_lang_String_2(JNIEnv* env, jobject obj, jstring entityName);

JNIEXPORT jobject JNICALL Java_kgfw_KGFW_findEntity__J(JNIEnv* env, jobject obj, jlong entityID);

JNIEXPORT jobject JNICALL Java_kgfw_KGFW_entityGetComponent(JNIEnv* env, jobject obj, jobject entity, jlong componentID);

JNIEXPORT jobject JNICALL Java_kgfw_KGFW_entityAttachComponent__Lkgfw_Entity_2Ljava_lang_String_2(JNIEnv* env, jobject obj, jobject entity, jstring componentName);

JNIEXPORT jobject JNICALL Java_kgfw_KGFW_entityAttachComponent__Lkgfw_Entity_2J(JNIEnv* env, jobject obj, jobject entity, jlong componentID);

JNIEXPORT void JNICALL Java_kgfw_KGFW_destroyComponent(JNIEnv* env, jobject obj, jobject component);
