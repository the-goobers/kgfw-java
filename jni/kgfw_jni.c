#include "../kgfw/kgfw.h"
#include <jni.h>

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
