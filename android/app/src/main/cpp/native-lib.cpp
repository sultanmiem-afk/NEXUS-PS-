#include <jni.h>

#include "Ps1System.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace {
std::unique_ptr<nexus::ps1::Ps1System> gSystem;
std::mutex gSystemMutex;
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
    std::lock_guard<std::mutex> lock(gSystemMutex);
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
    std::lock_guard<std::mutex> lock(gSystemMutex);
    return (gSystem != nullptr && gSystem->memory().bios().loaded())
        ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_sultanmiem_nexusps_MainActivity_nativeRunForCycles(
    JNIEnv*, jobject, jlong cycleBudget) {
    std::lock_guard<std::mutex> lock(gSystemMutex);
    if (gSystem == nullptr || cycleBudget <= 0) return 0;
    const auto result = gSystem->runForCycles(static_cast<std::uint64_t>(cycleBudget));
    return static_cast<jlong>(result.cyclesExecuted);
}

extern "C" JNIEXPORT void JNICALL
Java_com_sultanmiem_nexusps_MainActivity_nativeSetControllerButtons(
    JNIEnv*, jobject, jint pressedMask) {
    std::lock_guard<std::mutex> lock(gSystemMutex);
    gControllerButtons = static_cast<std::uint16_t>(pressedMask & 0xFFFF);
    if (gSystem != nullptr) {
        gSystem->memory().controller().setButtons(gControllerButtons);
    }
}

extern "C" JNIEXPORT void JNICALL
Java_com_sultanmiem_nexusps_MainActivity_nativeSetControllerAnalogAxes(
    JNIEnv*, jobject, jint x, jint y) {
    std::lock_guard<std::mutex> lock(gSystemMutex);
    const auto clampAxis = [](jint value) -> std::uint8_t {
        return static_cast<std::uint8_t>(value < 0 ? 0 : (value > 255 ? 255 : value));
    };
    gAnalogX = clampAxis(x);
    gAnalogY = clampAxis(y);
    if (gSystem != nullptr) {
        gSystem->memory().controller().setAnalogAxes(gAnalogX, gAnalogY);
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_sultanmiem_nexusps_MainActivity_nativeLoadDiscImage(
    JNIEnv* env, jobject, jbyteArray image) {
    std::lock_guard<std::mutex> lock(gSystemMutex);
    if (image == nullptr || gSystem == nullptr ||
        !gSystem->memory().bios().loaded()) return JNI_FALSE;

    const jsize length = env->GetArrayLength(image);
    if (length < 2048) return JNI_FALSE;

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    env->GetByteArrayRegion(image, 0, length,
        reinterpret_cast<jbyte*>(bytes.data()));
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return JNI_FALSE;
    }
    return gSystem->loadDiscImage(bytes) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_sultanmiem_nexusps_MainActivity_nativeLoadDiscCue(
    JNIEnv* env, jobject, jstring cueText, jstring binFilename, jbyteArray image) {
    std::lock_guard<std::mutex> lock(gSystemMutex);
    if (cueText == nullptr || binFilename == nullptr || image == nullptr ||
        gSystem == nullptr || !gSystem->memory().bios().loaded()) return JNI_FALSE;
    const char* cueChars = env->GetStringUTFChars(cueText, nullptr);
    if (cueChars == nullptr) return JNI_FALSE;
    const char* filenameChars = env->GetStringUTFChars(binFilename, nullptr);
    if (filenameChars == nullptr) {
        env->ReleaseStringUTFChars(cueText, cueChars);
        return JNI_FALSE;
    }
    const jsize length = env->GetArrayLength(image);
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    env->GetByteArrayRegion(image, 0, length, reinterpret_cast<jbyte*>(bytes.data()));
    const bool exception = env->ExceptionCheck();
    if (exception) env->ExceptionClear();
    const bool loaded = !exception && length >= 2048 &&
        gSystem->loadDiscCue(cueChars, filenameChars, bytes);
    env->ReleaseStringUTFChars(cueText, cueChars);
    env->ReleaseStringUTFChars(binFilename, filenameChars);
    return loaded ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jintArray JNICALL
Java_com_sultanmiem_nexusps_MainActivity_nativeCopyDisplayFrame(
    JNIEnv* env, jobject) {
    std::lock_guard<std::mutex> lock(gSystemMutex);
    constexpr jint kWidth = 640;
    constexpr jint kHeight = 512;
    constexpr jsize kHeader = 2;
    constexpr jsize kPixels = kWidth * kHeight;
    if (gSystem == nullptr) return env->NewIntArray(0);

    std::vector<std::uint32_t> pixels(static_cast<std::size_t>(kPixels));
    std::uint32_t width = 0, height = 0;
    if (!gSystem->gpu().copyDisplayToRgba8888(
            pixels.data(), pixels.size(), width, height)) return env->NewIntArray(0);

    jintArray result = env->NewIntArray(kHeader + kPixels);
    if (result == nullptr) return nullptr;
    const jint header[kHeader] = {static_cast<jint>(width), static_cast<jint>(height)};
    env->SetIntArrayRegion(result, 0, kHeader, header);
    env->SetIntArrayRegion(result, kHeader, kPixels,
        reinterpret_cast<const jint*>(pixels.data()));
    return result;
}
