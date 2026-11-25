#include <jni.h>

#include "Ps1System.hpp"

#include <cstdint>
#include <memory>
#include <vector>

namespace {
std::unique_ptr<nexus::ps1::Ps1System> gSystem;
std::uint16_t gControllerButtons = 0;
std::uint8_t gAnalogX = 128;
std::uint8_t gAnalogY = 128;
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
    candidate->memory().controller().setButtons(gControllerButtons);
    candidate->memory().controller().setAnalogMode(true);
    candidate->memory().controller().setAnalogAxes(gAnalogX, gAnalogY);
    gSystem = std::move(candidate);
    return JNI_TRUE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_sultanmiem_nexusps_MainActivity_nativeBiosLoaded(JNIEnv*, jobject) {
    return (gSystem != nullptr && gSystem->memory().bios().loaded())
        ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_sultanmiem_nexusps_MainActivity_nativeRunForCycles(
    JNIEnv*, jobject, jlong cycleBudget) {
    if (gSystem == nullptr || cycleBudget <= 0) return 0;
    const auto result = gSystem->runForCycles(static_cast<std::uint64_t>(cycleBudget));
    return static_cast<jlong>(result.cyclesExecuted);
}

extern "C" JNIEXPORT void JNICALL
Java_com_sultanmiem_nexusps_MainActivity_nativeSetControllerButtons(
    JNIEnv*, jobject, jint pressedMask) {
    gControllerButtons = static_cast<std::uint16_t>(pressedMask & 0xFFFF);
    if (gSystem != nullptr) {
        gSystem->memory().controller().setButtons(gControllerButtons);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_sultanmiem_nexusps_MainActivity_nativeSetControllerAnalogAxes(
    JNIEnv*, jobject, jint x, jint y) {
    const auto clampAxis = [](jint value) -> std::uint8_t {
        return static_cast<std::uint8_t>(value < 0 ? 0 : (value > 255 ? 255 : value));
    };
    gAnalogX = clampAxis(x);
    gAnalogY = clampAxis(y);
    if (gSystem != nullptr) {
        gSystem->memory().controller().setAnalogAxes(gAnalogX, gAnalogY);
    }
}
