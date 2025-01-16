#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include "zygisk.hpp"
#include "json.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include <string_view>
#include <filesystem>
#include <unordered_map>
#include <algorithm>

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "KTASTOPAPP", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "KTASTOPAPP", __VA_ARGS__)

#define MODULE_NAME "KTASTOPAPP"
#define IFY_JSON_PATH "/data/adb/modules/KTASTOPAPP/ify.json"

class KTASTOPAPP : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
        LOGD("%s onLoad => module loaded!", MODULE_NAME);

        // Đọc cấu hình từ ify.json
        parseGlobalJson();
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        // Không xử lý logic ở preAppSpecialize, chỉ giữ module
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        // Lấy danh sách tác vụ gần đây
        std::vector<std::string> recentTasks = getRecentTaskPackages();

        // Kiểm tra từng packageName trong ify.json
        for (const auto &pkg : appConfigs) {
            if (std::find(recentTasks.begin(), recentTasks.end(), pkg) == recentTasks.end()) {
                // Nếu packageName không có trong danh sách gần đây => buộc dừng
                LOGD("Forcing stop for package: %s", pkg.first.c_str());
                forceStopApp(pkg.first);
            }
        }
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::unordered_map<std::string, nlohmann::json> appConfigs;

    void parseGlobalJson() {
        // Đọc ify.json và lưu danh sách app
        std::ifstream jsonFile(IFY_JSON_PATH);
        if (!jsonFile) {
            LOGE("Could not open %s", IFY_JSON_PATH);
            return;
        }

        nlohmann::json config;
        jsonFile >> config;

        if (!config.contains("app") || !config["app"].is_array()) {
            LOGE("Invalid or missing 'app' array in ify.json");
            return;
        }

        for (const auto &app : config["app"]) {
            if (app.is_string()) {
                appConfigs[app.get<std::string>()] = app;
            }
        }

        LOGD("Loaded %zu app configurations from %s", appConfigs.size(), IFY_JSON_PATH);
    }

    std::vector<std::string> getRecentTaskPackages() {
        std::vector<std::string> recentPackages;
        jclass activityManagerClass = env->FindClass("android/app/ActivityManager");
        jobject activityManager = getSystemService("activity");

        jmethodID getRecentTasks = env->GetMethodID(activityManagerClass, "getRecentTasks",
                                                    "(IILjava/lang/String;)Ljava/util/List;");
        jobject recentTasks = env->CallObjectMethod(activityManager, getRecentTasks, 100, 0, nullptr);

        if (!recentTasks) {
            LOGE("Failed to retrieve recent tasks");
            return recentPackages;
        }

        jclass listClass = env->FindClass("java/util/List");
        jmethodID sizeMethod = env->GetMethodID(listClass, "size", "()I");
        jmethodID getMethod = env->GetMethodID(listClass, "get", "(I)Ljava/lang/Object;");

        int size = env->CallIntMethod(recentTasks, sizeMethod);
        for (int i = 0; i < size; ++i) {
            jobject taskInfo = env->CallObjectMethod(recentTasks, getMethod, i);
            jclass taskInfoClass = env->FindClass("android/app/ActivityManager$RecentTaskInfo");

            jfieldID baseIntentField = env->GetFieldID(taskInfoClass, "baseIntent", "Landroid/content/Intent;");
            jobject intent = env->GetObjectField(taskInfo, baseIntentField);

            jclass intentClass = env->FindClass("android/content/Intent");
            jmethodID getPackageMethod = env->GetMethodID(intentClass, "getPackage", "()Ljava/lang/String;");
            jstring packageName = (jstring) env->CallObjectMethod(intent, getPackageMethod);

            const char *pkgName = env->GetStringUTFChars(packageName, nullptr);
            recentPackages.emplace_back(pkgName);
            env->ReleaseStringUTFChars(packageName, pkgName);

            // Giải phóng tham chiếu JNI
            env->DeleteLocalRef(packageName);
            env->DeleteLocalRef(intent);
            env->DeleteLocalRef(intentClass);
            env->DeleteLocalRef(taskInfo);
            env->DeleteLocalRef(taskInfoClass);
        }

        // Giải phóng tham chiếu JNI
        env->DeleteLocalRef(recentTasks);
        env->DeleteLocalRef(listClass);
        env->DeleteLocalRef(activityManagerClass);
        env->DeleteLocalRef(activityManager);

        return recentPackages;
    }

    void forceStopApp(const std::string &packageName) {
        jclass activityManagerClass = env->FindClass("android/app/ActivityManager");
        jobject activityManager = getSystemService("activity");

        jmethodID forceStopPackage = env->GetMethodID(activityManagerClass, "forceStopPackage", "(Ljava/lang/String;)V");
        jstring pkgName = env->NewStringUTF(packageName.c_str());

        env->CallVoidMethod(activityManager, forceStopPackage, pkgName);
        if (env->ExceptionCheck()) {
            env->ExceptionDescribe();
            env->ExceptionClear();
        }

        // Giải phóng tham chiếu JNI
        env->DeleteLocalRef(pkgName);
        env->DeleteLocalRef(activityManagerClass);
        env->DeleteLocalRef(activityManager);
    }

    jobject getSystemService(const char *serviceName) {
        jclass contextClass = env->FindClass("android/content/Context");
        jmethodID getSystemService = env->GetMethodID(contextClass, "getSystemService",
                                                      "(Ljava/lang/String;)Ljava/lang/Object;");

        jstring serviceStr = env->NewStringUTF(serviceName);
        jobject service = env->CallObjectMethod(env->NewGlobalRef(contextClass), getSystemService, serviceStr);
        
        // Giải phóng tham chiếu JNI
        env->DeleteLocalRef(serviceStr);
        env->DeleteLocalRef(contextClass);

        return service;
    }
};

// Đăng ký module và companion
REGISTER_ZYGISK_MODULE(KTASTOPAPP)
