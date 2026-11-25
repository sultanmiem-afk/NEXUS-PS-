#include <jni.h>

extern "C" JNIEXPORT jstring JNICALL
Java_com_sultanmiem_nexusps_MainActivity_nativeCoreVersion(JNIEnv* env, jobject) {
    return env->NewStringUTF("C++ PS1 core linked (preview)");
}
