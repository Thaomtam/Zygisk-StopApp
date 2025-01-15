#include "shadowhook.h"
#include <jni.h>
#include <android/log.h>
#include <string>
#include <vector>

#define LOG_TAG "ZygiskHook"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

void *original_get_recent_tasks = nullptr;

// Proxy function
void *hooked_get_recent_tasks(JNIEnv *env, jobject thiz, jint maxNum) {
    LOGI("Hooked getRecentTasks() called");

    // Call the original function
    void *result = ((decltype(&hooked_get_recent_tasks))original_get_recent_tasks)(env, thiz, maxNum);

    // Extract recent tasks and print them
    jobjectArray recentTasks = (jobjectArray)result;
    jsize taskCount = env->GetArrayLength(recentTasks);
    LOGI("Found %d recent tasks", taskCount);

    for (int i = 0; i < taskCount; ++i) {
        jobject taskInfo = env->GetObjectArrayElement(recentTasks, i);
        jclass taskInfoClass = env->GetObjectClass(taskInfo);
        jmethodID getPackageName = env->GetMethodID(taskInfoClass, "getPackageName", "()Ljava/lang/String;");
        jstring packageName = (jstring)env->CallObjectMethod(taskInfo, getPackageName);

        const char *packageCStr = env->GetStringUTFChars(packageName, nullptr);
        LOGI("Recent task: %s", packageCStr);

        // Handle Blacklist/Whitelist logic here
        if (shouldForceStop(packageCStr)) {
            forceStopApp(packageCStr);
        }

        env->ReleaseStringUTFChars(packageName, packageCStr);
    }

    return result;
}

void install_hooks() {
    original_get_recent_tasks = shadowhook_hook_sym_name(
        "libandroid_runtime.so",
        "android.app.ActivityManager.getRecentTasks",
        (void *)hooked_get_recent_tasks,
        (void **)&original_get_recent_tasks
    );

    if (original_get_recent_tasks == nullptr) {
        LOGE("Failed to hook getRecentTasks");
    } else {
        LOGI("Successfully hooked getRecentTasks");
    }
}