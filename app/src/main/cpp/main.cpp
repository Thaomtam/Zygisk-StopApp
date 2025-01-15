#include <jni.h>
#include <android/log.h>
#include <string>
#include <vector>
#include <unordered_set>
#include <fstream>
#include "json.hpp"
#include "zygisk.hpp"
#include "shadowhook.h"

using json = nlohmann::json;

// Macro log
#define LOG_TAG "KTASTOPAPP"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Biến toàn cục
void *original_get_recent_tasks = nullptr;
std::unordered_set<std::string> lastRecentTasks;

// Hàm load JSON array từ file
std::vector<std::string> loadJsonArray(const std::string &filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file: " + filePath);
    }

    json root;
    file >> root;

    std::vector<std::string> result;
    for (const auto &entry : root["apps"]) {
        result.push_back(entry);
    }
    return result;
}

// Kiểm tra ứng dụng có cần dừng không
bool shouldForceStop(const std::string &packageName) {
    std::vector<std::string> blacklist = loadJsonArray("/data/adb/modules/KTAify-STOPAPP/blacklist.json");
    std::vector<std::string> whitelist = loadJsonArray("/data/adb/modules/KTAify-STOPAPP/whitelist.json");

    if (std::find(whitelist.begin(), whitelist.end(), packageName) != whitelist.end()) {
        return false;
    }
    return std::find(blacklist.begin(), blacklist.end(), packageName) != blacklist.end();
}

// Thực hiện force-stop ứng dụng bằng JNI
void forceStopApp(JNIEnv *env, const std::string &packageName) {
    jclass contextClass = env->FindClass("android/content/Context");
    if (!contextClass) {
        LOGE("Failed to find Context class");
        return;
    }

    jclass activityManagerClass = env->FindClass("android/app/ActivityManager");
    if (!activityManagerClass) {
        LOGE("Failed to find ActivityManager class");
        env->DeleteLocalRef(contextClass);
        return;
    }

    jfieldID activityServiceField = env->GetStaticFieldID(contextClass, "ACTIVITY_SERVICE", "Ljava/lang/String;");
    if (!activityServiceField) {
        LOGE("Failed to find ACTIVITY_SERVICE field");
        env->DeleteLocalRef(contextClass);
        env->DeleteLocalRef(activityManagerClass);
        return;
    }

    jstring activityService = (jstring)env->GetStaticObjectField(contextClass, activityServiceField);
    jmethodID getSystemServiceMethod = env->GetMethodID(contextClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    if (!getSystemServiceMethod) {
        LOGE("Failed to find getSystemService method");
        env->DeleteLocalRef(contextClass);
        env->DeleteLocalRef(activityManagerClass);
        env->DeleteLocalRef(activityService);
        return;
    }

    jobject activityManager = env->CallObjectMethod(env->NewGlobalRef(contextClass), getSystemServiceMethod, activityService);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("Exception occurred while calling getSystemService");
        env->DeleteLocalRef(contextClass);
        env->DeleteLocalRef(activityManagerClass);
        env->DeleteLocalRef(activityService);
        return;
    }

    jmethodID forceStopPackageMethod = env->GetMethodID(activityManagerClass, "forceStopPackage", "(Ljava/lang/String;)V");
    if (!forceStopPackageMethod) {
        LOGE("Failed to find forceStopPackage method");
        env->DeleteLocalRef(contextClass);
        env->DeleteLocalRef(activityManagerClass);
        env->DeleteLocalRef(activityService);
        env->DeleteLocalRef(activityManager);
        return;
    }

    jstring packageNameJString = env->NewStringUTF(packageName.c_str());
    env->CallVoidMethod(activityManager, forceStopPackageMethod, packageNameJString);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("Exception occurred while calling forceStopPackage");
    }

    // Giải phóng tham chiếu JNI
    env->DeleteLocalRef(contextClass);
    env->DeleteLocalRef(activityManagerClass);
    env->DeleteLocalRef(activityService);
    env->DeleteLocalRef(activityManager);
    env->DeleteLocalRef(packageNameJString);

    LOGI("Force-stopped app: %s", packageName.c_str());
}

// Hàm hook vào getRecentTasks
void *hooked_get_recent_tasks(JNIEnv *env, jobject thiz, jint maxNum) {
    LOGI("Hooked getRecentTasks() called");

    void *result = ((decltype(&hooked_get_recent_tasks))original_get_recent_tasks)(env, thiz, maxNum);

    std::unordered_set<std::string> currentRecentTasks;
    jobjectArray recentTasks = (jobjectArray)result;
    jsize taskCount = env->GetArrayLength(recentTasks);

    for (int i = 0; i < taskCount; ++i) {
        jobject taskInfo = env->GetObjectArrayElement(recentTasks, i);
        jclass taskInfoClass = env->GetObjectClass(taskInfo);
        jmethodID getPackageName = env->GetMethodID(taskInfoClass, "getPackageName", "()Ljava/lang/String;");
        jstring packageName = (jstring)env->CallObjectMethod(taskInfo, getPackageName);

        const char *packageCStr = env->GetStringUTFChars(packageName, nullptr);
        if (packageCStr) {
            currentRecentTasks.insert(packageCStr);
            env->ReleaseStringUTFChars(packageName, packageCStr);
        }
        env->DeleteLocalRef(packageName);
        env->DeleteLocalRef(taskInfoClass);
        env->DeleteLocalRef(taskInfo);
    }

    for (const auto &app : lastRecentTasks) {
        if (currentRecentTasks.find(app) == currentRecentTasks.end()) {
            if (shouldForceStop(app)) {
                forceStopApp(env, app);
            }
        }
    }

    lastRecentTasks = currentRecentTasks;

    return result;
}

// Cài đặt hook
void install_hooks() {
    original_get_recent_tasks = shadowhook_hook_sym_name(
        "libandroid_runtime.so",
        "android.app.ActivityManager.getRecentTasks",
        (void *)hooked_get_recent_tasks,
        (void **)&original_get_recent_tasks
    );
    if (!original_get_recent_tasks) {
        LOGE("Failed to hook getRecentTasks");
    } else {
        LOGI("Successfully hooked getRecentTasks");
    }
}

// Zygisk module
class KTASTOPAPP : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        LOGI("Zygisk module loaded");
        install_hooks();
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        LOGI("Zygisk: Pre App Specialize");
    }
};

// Đăng ký module với Zygisk
REGISTER_ZYGISK_MODULE(KTASTOPAPP)