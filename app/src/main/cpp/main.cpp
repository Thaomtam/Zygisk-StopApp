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

// Biến toàn cục để lưu blacklist và whitelist
std::vector<std::string> blacklist;
std::vector<std::string> whitelist;

// Biến toàn cục để lưu danh sách các ứng dụng gần đây
std::unordered_set<std::string> lastRecentTasks;

// Khai báo kiểu hàm gốc getRecentTasks
typedef jobject (*getRecentTasks_t)(JNIEnv *, jobject, jint, jint);
getRecentTasks_t original_get_recent_tasks = nullptr;

// Hàm load JSON array từ file với kiểm tra lỗi
std::vector<std::string> loadJsonArray(const std::string &filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        LOGE("Không thể mở file: %s", filePath.c_str());
        return {}; // Trả về danh sách rỗng nếu không mở được file
    }

    json root;
    try {
        file >> root;
    } catch (json::parse_error &e) {
        LOGE("Lỗi phân tích JSON trong file %s: %s", filePath.c_str(), e.what());
        return {};
    }

    if (!root.contains("apps") || !root["apps"].is_array()) {
        LOGE("JSON không chứa mảng 'apps' trong file %s", filePath.c_str());
        return {};
    }

    std::vector<std::string> result;
    for (const auto &entry : root["apps"]) {
        if (entry.is_string()) {
            result.push_back(entry.get<std::string>());
        } else {
            LOGE("Mục không hợp lệ trong mảng 'apps': không phải là chuỗi");
        }
    }
    return result;
}

// Hàm tải và cache JSON
bool loadAndCacheJson() {
    blacklist = loadJsonArray("/data/adb/modules/KTAify-STOPAPP/blacklist.json");
    if (blacklist.empty()) {
        LOGE("Blacklist trống hoặc không thể tải");
        return false;
    }

    whitelist = loadJsonArray("/data/adb/modules/KTAify-STOPAPP/whitelist.json");
    if (whitelist.empty()) {
        LOGE("Whitelist trống hoặc không thể tải");
        return false;
    }

    LOGI("Đã tải thành công blacklist và whitelist");
    return true;
}

// Kiểm tra ứng dụng có cần dừng không
bool shouldForceStop(const std::string &packageName) {
    if (std::find(whitelist.begin(), whitelist.end(), packageName) != whitelist.end()) {
        return false;
    }
    return std::find(blacklist.begin(), blacklist.end(), packageName) != blacklist.end();
}

// Hàm lấy Context từ ActivityThread
jobject get_context(JNIEnv *env) {
    jclass activityThreadClass = env->FindClass("android/app/ActivityThread");
    if (!activityThreadClass) {
        LOGE("Không thể tìm thấy lớp ActivityThread");
        return nullptr;
    }

    jmethodID currentActivityThreadMethod = env->GetStaticMethodID(activityThreadClass, "currentActivityThread", "()Landroid/app/ActivityThread;");
    if (!currentActivityThreadMethod) {
        LOGE("Không thể tìm thấy phương thức currentActivityThread");
        env->DeleteLocalRef(activityThreadClass);
        return nullptr;
    }

    jobject activityThread = env->CallStaticObjectMethod(activityThreadClass, currentActivityThreadMethod);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("Đã xảy ra ngoại lệ khi gọi currentActivityThread");
        env->DeleteLocalRef(activityThreadClass);
        return nullptr;
    }

    jmethodID getApplicationMethod = env->GetMethodID(activityThreadClass, "getApplication", "()Landroid/app/Application;");
    if (!getApplicationMethod) {
        LOGE("Không thể tìm thấy phương thức getApplication");
        env->DeleteLocalRef(activityThreadClass);
        env->DeleteLocalRef(activityThread);
        return nullptr;
    }

    jobject context = env->CallObjectMethod(activityThread, getApplicationMethod);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("Đã xảy ra ngoại lệ khi gọi getApplication");
        env->DeleteLocalRef(activityThreadClass);
        env->DeleteLocalRef(activityThread);
        return nullptr;
    }

    env->DeleteLocalRef(activityThreadClass);
    env->DeleteLocalRef(activityThread);
    return context;
}

// Thực hiện force-stop ứng dụng bằng JNI
void forceStopApp(JNIEnv *env, const std::string &packageName) {
    jobject context = get_context(env);
    if (!context) {
        LOGE("Không thể lấy được Context");
        return;
    }

    jclass contextClass = env->GetObjectClass(context);
    if (!contextClass) {
        LOGE("Không thể lấy lớp Context");
        env->DeleteLocalRef(context);
        return;
    }

    jfieldID activityServiceField = env->GetStaticFieldID(contextClass, "ACTIVITY_SERVICE", "Ljava/lang/String;");
    if (!activityServiceField) {
        LOGE("Không thể tìm thấy trường ACTIVITY_SERVICE");
        env->DeleteLocalRef(contextClass);
        env->DeleteLocalRef(context);
        return;
    }

    jstring activityService = (jstring)env->GetStaticObjectField(contextClass, activityServiceField);
    jmethodID getSystemServiceMethod = env->GetMethodID(contextClass, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    if (!getSystemServiceMethod) {
        LOGE("Không thể tìm thấy phương thức getSystemService");
        env->DeleteLocalRef(contextClass);
        env->DeleteLocalRef(activityService);
        env->DeleteLocalRef(context);
        return;
    }

    jobject activityManager = env->CallObjectMethod(context, getSystemServiceMethod, activityService);
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("Đã xảy ra ngoại lệ khi gọi getSystemService");
        env->DeleteLocalRef(contextClass);
        env->DeleteLocalRef(activityService);
        env->DeleteLocalRef(context);
        return;
    }

    jclass activityManagerClass = env->GetObjectClass(activityManager);
    if (!activityManagerClass) {
        LOGE("Không thể lấy lớp ActivityManager");
        env->DeleteLocalRef(contextClass);
        env->DeleteLocalRef(activityService);
        env->DeleteLocalRef(activityManager);
        env->DeleteLocalRef(context);
        return;
    }

    jmethodID forceStopPackageMethod = env->GetMethodID(activityManagerClass, "forceStopPackage", "(Ljava/lang/String;)V");
    if (!forceStopPackageMethod) {
        LOGE("Không thể tìm thấy phương thức forceStopPackage");
        env->DeleteLocalRef(activityManagerClass);
        env->DeleteLocalRef(contextClass);
        env->DeleteLocalRef(activityService);
        env->DeleteLocalRef(activityManager);
        env->DeleteLocalRef(context);
        return;
    }

    jstring packageNameJString = env->NewStringUTF(packageName.c_str());
    env->CallVoidMethod(activityManager, forceStopPackageMethod, packageNameJString);

    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        LOGE("Đã xảy ra ngoại lệ khi gọi forceStopPackage");
    }

    // Giải phóng tham chiếu JNI
    env->DeleteLocalRef(packageNameJString);
    env->DeleteLocalRef(activityManagerClass);
    env->DeleteLocalRef(activityManager);
    env->DeleteLocalRef(activityService);
    env->DeleteLocalRef(contextClass);
    env->DeleteLocalRef(context);

    LOGI("Đã ép buộc dừng ứng dụng: %s", packageName.c_str());
}

// Hàm hook vào getRecentTasks
jobject hooked_get_recent_tasks(JNIEnv *env, jobject thiz, jint maxNum, jint flags) {
    LOGI("Đã gọi hàm hook getRecentTasks()");

    // Gọi hàm gốc
    jobject result = original_get_recent_tasks(env, thiz, maxNum, flags);

    if (!result) {
        LOGE("Kết quả từ getRecentTasks() là nullptr");
        return result;
    }

    // Xử lý danh sách Recent Tasks
    jclass listClass = env->FindClass("java/util/List");
    if (!listClass) {
        LOGE("Không thể tìm thấy lớp java.util.List");
        return result;
    }

    jmethodID sizeMethod = env->GetMethodID(listClass, "size", "()I");
    jmethodID getMethod = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");
    if (!sizeMethod || !getMethod) {
        LOGE("Không thể tìm thấy phương thức size hoặc get trong List");
        env->DeleteLocalRef(listClass);
        return result;
    }

    jint taskCount = env->CallIntMethod(result, sizeMethod);
    std::unordered_set<std::string> currentRecentTasks;

    for (int i = 0; i < taskCount; ++i) {
        jobject taskInfo = env->CallObjectMethod(result, getMethod, i);
        if (!taskInfo) {
            LOGE("taskInfo tại vị trí %d là nullptr", i);
            continue;
        }

        jclass taskInfoClass = env->GetObjectClass(taskInfo);
        if (!taskInfoClass) {
            LOGE("Không thể lấy lớp của taskInfo tại vị trí %d", i);
            env->DeleteLocalRef(taskInfo);
            continue;
        }

        jmethodID getPackageName = env->GetMethodID(taskInfoClass, "getPackageName", "()Ljava/lang/String;");
        if (!getPackageName) {
            LOGE("Không thể tìm thấy phương thức getPackageName trong taskInfo tại vị trí %d", i);
            env->DeleteLocalRef(taskInfoClass);
            env->DeleteLocalRef(taskInfo);
            continue;
        }

        jstring packageName = (jstring)env->CallObjectMethod(taskInfo, getPackageName);
        if (!packageName) {
            LOGE("packageName tại vị trí %d là nullptr", i);
            env->DeleteLocalRef(taskInfoClass);
            env->DeleteLocalRef(taskInfo);
            continue;
        }

        const char *packageCStr = env->GetStringUTFChars(packageName, nullptr);
        if (packageCStr) {
            currentRecentTasks.insert(packageCStr);
            env->ReleaseStringUTFChars(packageName, packageCStr);
        }

        // Giải phóng tham chiếu JNI
        env->DeleteLocalRef(packageName);
        env->DeleteLocalRef(taskInfoClass);
        env->DeleteLocalRef(taskInfo);
    }

    // So sánh với danh sách trước đó và force stop nếu cần
    for (const auto &app : lastRecentTasks) {
        if (currentRecentTasks.find(app) == currentRecentTasks.end()) {
            if (shouldForceStop(app)) {
                forceStopApp(env, app);
            }
        }
    }

    lastRecentTasks = currentRecentTasks;

    // Giải phóng tham chiếu JNI
    env->DeleteLocalRef(listClass);

    return result;
}

// Cài đặt hook
void install_hooks() {
    // Chữ ký đúng của getRecentTasks: (I, I)Ljava/util/List;
    // Tên mangled của phương thức getRecentTasksInternal(int, int) trong libandroid_runtime.so
    original_get_recent_tasks = (getRecentTasks_t)shadowhook_hook_sym_name(
        "libandroid_runtime.so",
        "_ZN7android14ActivityManager19getRecentTasksInternalEii", // Tên mangled của getRecentTasksInternal(int, int)
        (void *)hooked_get_recent_tasks,
        (void **)&original_get_recent_tasks
    );

    if (!original_get_recent_tasks) {
        LOGE("Không thể hook getRecentTasks");
    } else {
        LOGI("Đã hook thành công getRecentTasks");
    }
}

// Zygisk module
class KTASTOPAPP : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        LOGI("Zygisk module KTASTOPAPP đã được tải");
        if (!loadAndCacheJson()) {
            LOGE("Không thể tải cấu hình JSON");
        }
        install_hooks();
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        LOGI("Zygisk: Pre App Specialize");
    }
};

// Đăng ký module với Zygisk
REGISTER_ZYGISK_MODULE(KTASTOPAPP)
