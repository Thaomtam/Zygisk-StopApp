#include <fstream>
#include <string>
#include <vector>
#include "json.hpp" // Include a JSON library like nlohmann/json

std::vector<std::string> loadJsonArray(const std::string &filePath) {
    std::ifstream file(filePath);
    Json::Value root;
    file >> root;

    std::vector<std::string> result;
    for (const auto &entry : root["apps"]) {
        result.push_back(entry.asString());
    }
    return result;
}

bool shouldForceStop(const std::string &packageName) {
    std::vector<std::string> blacklist = loadJsonArray("/data/adb/modules/KTAify-STOPAPP/blacklist.json");
    std::vector<std::string> whitelist = loadJsonArray("/data/adb/modules/KTAify-STOPAPP/whitelist.json");

    // If package is in whitelist, don't stop it
    if (std::find(whitelist.begin(), whitelist.end(), packageName) != whitelist.end()) {
        return false;
    }

    // If package is in blacklist, stop it
    return std::find(blacklist.begin(), blacklist.end(), packageName) != blacklist.end();
}

void forceStopApp(const std::string &packageName) {
    std::string command = "su -c \"am force-stop --user 0 " + packageName + "\"";
    system(command.c_str());
}
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
