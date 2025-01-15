#include "zygisk.hpp"
#include "hook.cpp"

class MyModule : public zygisk::ModuleBase {
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        LOGI("Zygisk module loaded");
        install_hooks();
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        LOGI("Zygisk: Pre App Specialize");
    }
};

REGISTER_ZYGISK_MODULE(MyModule)
