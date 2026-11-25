#include <jni.h>

#include "Ps1System.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace {
std::unique_ptr<nexus::ps1::Ps1System> gSystem;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_sultanmiem_nexusps_MainActivity_nativeCoreVersion(JNIEnv* env, jobject) {
    return env->NewStringUTF("NEXUS-PS native PS1 core");
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_sultanmiem_nexusps_MainActivity_nativeLoadBios(
    JNIEnv* env, jobject, jbyteArray image) {
    if (image == nullptr) return JNI_FALSE;

    const jsize length = env->GetArrayLength(image);
    if (length != static_cast<jsize>(nexus::ps1::BiosRom::kSize)) {
        return JNI_FALSE;
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    env->GetByteArrayRegion(image, 0, length,
        reinterpret_cast<jbyte*>(bytes.data()));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return JNI_FALSE;
    }

    auto candidate = std::make_unique<nexus::ps1::Ps1System>();
    if (!candidate->loadBios(bytes) || !candidate->memory().bios().loaded()) {
        return JNI_FALSE;
    }

    // Keep the BIOS mapped in the native machine. Do not step the CPU here:
    // the Android app UI opens directly and the PS1 BIOS boot animation is not run.
    gSystem = std::move(candidate);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_sultanmiem_nexusps_MainActivity_nativeBiosLoaded(JNIEnv*, jobject) {
    return (gSystem != nullptr && gSystem->memory().bios().loaded())
        ? JNI_TRUE : JNI_FALSE;
}
