#ifdef ANDROID
extern "C" {
#include "libavcodec/jni.h"
}

#include "util/log.h"
#include "util/utilxx.h"
#include <jni.h>

extern "C" JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    av_jni_set_java_vm(vm, nullptr);
    LXX_DEBEG("JNI_OnLoad");
    return JNI_VERSION_1_6;
}

extern "C" JNIEXPORT void JNICALL
    Java_run_bool_mediaxxandroidhelper_MediaxxAndroidHelper_setApplicationContextNative(
        JNIEnv* env,
        jobject thiz,
        jobject context
    ) {
    LXX_DEBEG("Java_run_bool_mediaxx_MediaxxAndroidHelper_setApplicationContextNative");

    /*
     * 创建全局引用防止被GC回收
     * note: 阅读源码libavcodec/jni.c可以发现:
     * av_jni_set_android_app_ctx函数要求context是全局引用
     */
    // 一直保存
    jobject global_ctx = env->NewGlobalRef(context);
    av_jni_set_android_app_ctx(global_ctx, nullptr);
    // 删除全局引用，否则会造成内存泄漏
    // env->DeleteGlobalRef(global_ctx);
}

#endif