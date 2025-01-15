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
std::unordered_set<std::string> lastRecentTasks; // Lưu danh sách app gần đây

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

    // Nếu ứng dụng nằm trong whitelist, không dừng
    if (std::find(whitelist.begin(), whitelist.end(), packageName) != whitelist.end()) {
        return false;
    }

    // Nếu ứng dụng nằm trong blacklist, dừng
    return std::find(blacklist.begin(), blacklist.end(), packageName) != blacklist.end();
}

// Thực hiện force-stop ứng dụng
void forceStopApp(const std::string &packageName) {
    std::string command = "su -c \"am force-stop --user 0 " + packageName + "\"";
    system(command.c_str());
    LOGI("Force-stopped app: %s", packageName.c_str());
}

// Hàm hook vào `getRecentTasks`
void *hooked_get_recent_tasks(JNIEnv *env, jobject thiz, jint maxNum) {
    LOGI("Hooked getRecentTasks() called");

    // Gọi hàm gốc
    void *result = ((decltype(&hooked_get_recent_tasks))original_get_recent_tasks)(env, thiz, maxNum);

    // Lấy danh sách các ứng dụng hiện tại
    std::unordered_set<std::string> currentRecentTasks;
    jobjectArray recentTasks = (jobjectArray)result;
    jsize taskCount = env->GetArrayLength(recentTasks);
    LOGI("Found %d recent tasks", taskCount);

    for (int i = 0; i < taskCount; ++i) {
        jobject taskInfo = env->GetObjectArrayElement(recentTasks, i);
        if (!taskInfo) {
            LOGE("Failed to get taskInfo at index %d", i);
            continue;
        }

        jclass taskInfoClass = env->GetObjectClass(taskInfo);
        if (!taskInfoClass) {
            LOGE("Failed to get taskInfo class");
            env->DeleteLocalRef(taskInfo);
            continue;
        }

        jmethodID getPackageName = env->GetMethodID(taskInfoClass, "getPackageName", "()Ljava/lang/String;");
        if (!getPackageName) {
            LOGE("Failed to find method getPackageName");
            env->DeleteLocalRef(taskInfoClass);
            env->DeleteLocalRef(taskInfo);
            continue;
        }

        jstring packageName = (jstring)env->CallObjectMethod(taskInfo, getPackageName);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            LOGE("Exception occurred while calling getPackageName");
            env->DeleteLocalRef(taskInfoClass);
            env->DeleteLocalRef(taskInfo);
            continue;
        }

        const char *packageCStr = env->GetStringUTFChars(packageName, nullptr);
        if (packageCStr) {
            currentRecentTasks.insert(packageCStr);
            env->ReleaseStringUTFChars(packageName, packageCStr);
        }

        // Giải phóng tham chiếu cục bộ
        env->DeleteLocalRef(packageName);
        env->DeleteLocalRef(taskInfoClass);
        env->DeleteLocalRef(taskInfo);
    }

    // Phát hiện ứng dụng bị thoát (có trong danh sách cũ nhưng không còn trong danh sách mới)
    for (const auto &app : lastRecentTasks) {
        if (currentRecentTasks.find(app) == currentRecentTasks.end()) {
            LOGI("App removed from recent tasks: %s", app.c_str());
            if (shouldForceStop(app)) {
                forceStopApp(app); // Buộc dừng nếu ứng dụng trong blacklist
            }
        }
    }

    // Cập nhật danh sách ứng dụng hiện tại
    lastRecentTasks = currentRecentTasks;

    return result;
}

// Cài đặt Shadowhook
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

// Zygisk module
class KTASTOPAPP : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        LOGI("Zygisk module loaded");
        install_hooks(); // Cài đặt hook
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        LOGI("Zygisk: Pre App Specialize");
    }
};

// Đăng ký module với Zygisk
REGISTER_ZYGISK_MODULE(KTASTOPAPP)
