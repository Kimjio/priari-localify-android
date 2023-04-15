#include <sstream>
#include <thread>
#include "stdinclude.hpp"
#include "hook.h"
#include "il2cpp_hook.h"
#include "localify/localify.h"
#include "logger/logger.h"
#include "config.hpp"
#include "native_bridge_itf.h"

using namespace std;
using namespace localify;
using namespace logger;

bool g_enable_logger = false;
int g_max_fps = -1;
float g_ui_animation_scale = 1.0f;
bool g_ui_use_system_resolution = false;
float g_resolution_3d_scale = 1.0f;
bool g_replace_to_custom_font = false;
string g_font_assetbundle_path;
string g_tmpro_font_asset_name;
int g_graphics_quality = -1;
int g_anti_aliasing = -1;
unordered_map<string, ReplaceAsset> g_replace_assets;
string g_replace_assetbundle_file_path;
int g_cyspring_update_mode = -1;
bool g_dump_msgpack = false;
bool g_dump_msgpack_request = false;
string g_packet_notifier;

string g_configuration_json_url_base;

string text_id_dict;

bool isGame(const char *pkgNm) {
    if (!pkgNm)
        return false;
    if (Game::IsPackageNameEqualsByGameRegion(pkgNm, Game::Region::JAP)) {
        LOGI("detect package: %s", pkgNm);
        return true;
    }
    return false;
}

bool isCriWareInit = false;

enum class ProcessStatus {
    INIT,
    DONE,
    NONE,
};

ProcessStatus dlopen_process(const char *name, void *handle) {
    if (!il2cpp_handle) {
        if (name != nullptr && strstr(name, "libil2cpp.so")) {
            il2cpp_handle = handle;
            LOGI("Got il2cpp handle!");
            return ProcessStatus::INIT;
        }
    }
    if (name != nullptr && strstr(name, "libcri_ware_unity.so") && !isCriWareInit) {
        isCriWareInit = true;
        // il2cpp_load_assetbundle();
        return ProcessStatus::DONE;
    }
    return ProcessStatus::NONE;
}

HOOK_DEF(void*, do_dlopen, const char *name, int flags) {
    void *handle = orig_do_dlopen(name, flags);
    if (dlopen_process(name, handle) == ProcessStatus::DONE) {
        DobbyDestroy(addr_do_dlopen);
    }
    return handle;
}

HOOK_DEF(void*, __loader_dlopen, const char *filename, int flags, const void *caller_addr) {
    void *handle = orig___loader_dlopen(filename, flags, caller_addr);
    if (dlopen_process(filename, handle) == ProcessStatus::DONE) {
        DobbyDestroy(addr___loader_dlopen);
    }
    return handle;
}

HOOK_DEF(void*, do_dlopen_V24, const char *name, int flags, const void *extinfo [[maybe_unused]],
         void *caller_addr) {
    void *handle = orig_do_dlopen_V24(name, flags, extinfo, caller_addr);
    if (dlopen_process(name, handle) == ProcessStatus::DONE) {
        DobbyDestroy(addr_do_dlopen_V24);
    }
    return handle;
}

HOOK_DEF(void*, do_dlopen_V19, const char *name, int flags, const void *extinfo [[maybe_unused]]) {
    void *handle = orig_do_dlopen_V19(name, flags, extinfo);
    if (dlopen_process(name, handle) == ProcessStatus::DONE) {
        DobbyDestroy(addr_do_dlopen_V19);
    }
    return handle;
}

HOOK_DEF(void*, NativeBridgeLoadLibrary_V21, const char *filename, int flag) {
    if (string(filename).find(string("libmain.so")) != string::npos) {
        auto *NativeBridgeError = reinterpret_cast<bool (*)()>(DobbySymbolResolver(nullptr,
                                                                                   "_ZN7android17NativeBridgeErrorEv"));

        stringstream path_armV8;
        path_armV8 << "/data/data/" << Game::GetCurrentPackageName().data() << "/arm64-v8a.so";
        stringstream path_armV7;
        path_armV7 << "/data/data/" << Game::GetCurrentPackageName().data() << "/armeabi-v7a.so";

        string path;

        if (access(path_armV8.str().data(), F_OK) != -1) {
            path = path_armV8.str();
        } else if (access(path_armV7.str().data(), F_OK) != -1) {
            path = path_armV7.str();
        }

        if (!path.empty()) {
            thread load_thread([path, NativeBridgeError]() {
                void *lib = orig_NativeBridgeLoadLibrary_V21(path.data(), RTLD_NOW);
                LOGI("%s: %p", path.data(), lib);
                if (NativeBridgeError()) {
                    LOGW("LoadLibrary failed");
                }
                DobbyDestroy(addr_NativeBridgeLoadLibrary_V21);
            });
            load_thread.detach();
        }
    }

    return orig_NativeBridgeLoadLibrary_V21(filename, flag);
}

HOOK_DEF(void*, NativeBridgeLoadLibraryExt_V26, const char *filename, int flag,
         struct native_bridge_namespace_t *ns) {
    if (string(filename).find(string("libmain.so")) != string::npos) {
        auto *NativeBridgeError = reinterpret_cast<bool (*)()>(DobbySymbolResolver(nullptr,
                                                                                   "_ZN7android17NativeBridgeErrorEv"));
        auto *NativeBridgeGetError = reinterpret_cast<char *(*)()>(DobbySymbolResolver(nullptr,
                                                                                       "_ZN7android20NativeBridgeGetErrorEv"));

        stringstream path_armV8;
        path_armV8 << "/data/data/" << Game::GetCurrentPackageName().data() << "/arm64-v8a.so";
        stringstream path_armV7;
        path_armV7 << "/data/data/" << Game::GetCurrentPackageName().data() << "/armeabi-v7a.so";

        string path;

        if (access(path_armV8.str().data(), F_OK) != -1) {
            path = path_armV8.str();
        } else if (access(path_armV7.str().data(), F_OK) != -1) {
            path = path_armV7.str();
        }

        if (!path.empty()) {
            thread load_thread([path, ns, NativeBridgeError, NativeBridgeGetError]() {
                void *lib = orig_NativeBridgeLoadLibraryExt_V26(path.data(), RTLD_NOW, ns);
                LOGI("%s: %p", path.data(), lib);
                if (NativeBridgeError()) {
                    char *error_bridge = NativeBridgeGetError();
                    if (error_bridge) {
                        LOGW("error_bridge: %s", error_bridge);
                    }
                }
                DobbyDestroy(addr_NativeBridgeLoadLibraryExt_V26);
            });
            load_thread.detach();
        }
    }

    return orig_NativeBridgeLoadLibraryExt_V26(filename, flag, ns);
}

HOOK_DEF(void*, NativeBridgeLoadLibraryExt_V30, const char *filename, int flag,
         struct native_bridge_namespace_t *ns) {
    if (string(filename).find(string("libmain.so")) != string::npos) {
        auto *NativeBridgeError = reinterpret_cast<bool (*)()>(DobbySymbolResolver(nullptr,
                                                                                   "NativeBridgeError"));
        auto *NativeBridgeGetError = reinterpret_cast<char *(*)()>(DobbySymbolResolver(nullptr,
                                                                                       "NativeBridgeGetError"));

        stringstream path_armV8;
        path_armV8 << "/data/data/" << Game::GetCurrentPackageName().data() << "/arm64-v8a.so";
        stringstream path_armV7;
        path_armV7 << "/data/data/" << Game::GetCurrentPackageName().data() << "/armeabi-v7a.so";

        string path;

        if (access(path_armV8.str().data(), F_OK) != -1) {
            path = path_armV8.str();
        } else if (access(path_armV7.str().data(), F_OK) != -1) {
            path = path_armV7.str();
        }

        if (!path.empty()) {
            thread load_thread([path, ns, NativeBridgeError, NativeBridgeGetError]() {
                void *lib = orig_NativeBridgeLoadLibraryExt_V30(path.data(), RTLD_NOW, ns);
                LOGI("%s: %p", path.data(), lib);
                if (NativeBridgeError()) {
                    char *error_bridge = NativeBridgeGetError();
                    if ((error_bridge) !=
                        nullptr) {
                        LOGW("error_bridge: %s", error_bridge);
                    }
                }
                DobbyDestroy(addr_NativeBridgeLoadLibraryExt_V30);
            });
            load_thread.detach();
        }
    }

    return orig_NativeBridgeLoadLibraryExt_V30(filename, flag, ns);
}

optional<vector<string>> read_config() {
    ifstream config_stream{
            string("/sdcard/Android/data/").append(Game::GetCurrentPackageName()).append(
                    "/config.json")};
    vector<string> dicts{};

    if (!config_stream.is_open()) {
        LOGW("config.json not loaded.");
        return nullopt;
    }

    LOGI("config.json loaded.");

    rapidjson::IStreamWrapper wrapper{config_stream};
    rapidjson::Document document;

    document.ParseStream(wrapper);

    if (!document.HasParseError()) {
        if (document.HasMember("enableLogger")) {
            g_enable_logger = document["enableLogger"].GetBool();
        }
        if (document.HasMember("maxFps")) {
            g_max_fps = document["maxFps"].GetInt();
        }
        if (document.HasMember("uiAnimationScale")) {
            g_ui_animation_scale = document["uiAnimationScale"].GetFloat();
        }
        if (document.HasMember("uiUseSystemResolution")) {
            g_ui_use_system_resolution = document["uiUseSystemResolution"].GetBool();
        }
        if (document.HasMember("resolution3dScale")) {
            g_resolution_3d_scale = document["resolution3dScale"].GetFloat();
        }
        if (document.HasMember("replaceToCustomFont")) {
            g_replace_to_custom_font = document["replaceToCustomFont"].GetBool();
        }
        if (document.HasMember("fontAssetBundlePath")) {
            g_font_assetbundle_path = string(document["fontAssetBundlePath"].GetString());
        }
        if (document.HasMember("tmproFontAssetName")) {
            g_tmpro_font_asset_name = string(document["tmproFontAssetName"].GetString());
        }
        if (document.HasMember("graphicsQuality")) {
            g_graphics_quality = document["graphicsQuality"].GetInt();
            if (g_graphics_quality < -1) {
                g_graphics_quality = -1;
            }
            if (g_graphics_quality > 4) {
                g_graphics_quality = 3;
            }
        }
        if (document.HasMember("antiAliasing")) {
            g_anti_aliasing = document["antiAliasing"].GetInt();
            vector<int> options = {0, 2, 4, 8, -1};
            g_anti_aliasing =
                    find(options.begin(), options.end(), g_anti_aliasing) - options.begin();
        }

        if (document.HasMember("replaceAssetsPath")) {
            auto replaceAssetsPath = localify::u8_u16(document["replaceAssetsPath"].GetString());
            if (!replaceAssetsPath.starts_with(u"/")) {
                replaceAssetsPath.insert(0, u16string(u"/sdcard/Android/data/").append(
                        localify::u8_u16(Game::GetCurrentPackageName())).append(u"/"));
            }
            if (filesystem::exists(replaceAssetsPath) &&
                filesystem::is_directory(replaceAssetsPath)) {
                for (auto &file: filesystem::directory_iterator(replaceAssetsPath)) {
                    if (file.is_regular_file()) {
                        g_replace_assets.emplace(file.path().filename().string(),
                                                 ReplaceAsset{file.path().string(), nullptr});
                    }
                }
            }
        }

        if (document.HasMember("replaceAssetBundleFilePath")) {
            auto replaceAssetBundleFilePath = localify::u8_u16(
                    document["replaceAssetBundleFilePath"].GetString());
            if (!replaceAssetBundleFilePath.starts_with(u"/")) {
                replaceAssetBundleFilePath.insert(0, u16string(u"/sdcard/Android/data/").append(
                        localify::u8_u16(Game::GetCurrentPackageName())).append(u"/"));
            }
            if (filesystem::exists(replaceAssetBundleFilePath) &&
                filesystem::is_regular_file(replaceAssetBundleFilePath)) {
                g_replace_assetbundle_file_path = localify::u16_u8(replaceAssetBundleFilePath);
            }
        }

        if (document.HasMember("cySpringUpdateMode")) {
            g_cyspring_update_mode = document["cySpringUpdateMode"].GetInt();
            vector<int> options = {0, 1, 2, 3, -1};
            g_cyspring_update_mode =
                    find(options.begin(), options.end(), g_cyspring_update_mode) - options.begin();
        }

        if (document.HasMember("dicts")) {
            auto &dicts_arr = document["dicts"];
            auto len = dicts_arr.Size();

            for (size_t i = 0; i < len; ++i) {
                auto dict = dicts_arr[i].GetString();

                dicts.emplace_back(dict);
            }
        }

        if (document.HasMember("dumpMsgPack")) {
            g_dump_msgpack = document["dumpMsgPack"].GetBool();
        }

        if (document.HasMember("dumpMsgPackRequest")) {
            g_dump_msgpack_request = document["dumpMsgPackRequest"].GetBool();
        }

        if (document.HasMember("packetNotifier")) {
            g_packet_notifier = document["packetNotifier"].GetString();
        }

        if (document.HasMember("configurationJsonUrlBase")) {
            g_configuration_json_url_base = document["configurationJsonUrlBase"].GetString();
            if (!g_configuration_json_url_base.ends_with("/")) {
                g_configuration_json_url_base.append("/");
            }
        }
    }

    config_stream.close();
    return dicts;
}

void *GetNativeBridgeLoadLibrary(void *fallbackAddress) {
    void *handle = dlopen(GetNativeBridgeLibrary().data(), RTLD_NOW);
    // clear error
    dlerror();
    if (handle) {
        auto itf = reinterpret_cast<NativeBridgeCallbacks *>(dlsym(handle, "NativeBridgeItf"));
        LOGI("NativeBridgeItf version: %d", itf->version);
        if (GetAndroidApiLevel() >= 26) {
            return reinterpret_cast<void *>(itf->loadLibraryExt);
        }
        return reinterpret_cast<void *>(itf->loadLibrary);
    }
    return fallbackAddress;
}

void hack_thread(void *arg [[maybe_unused]]) {
    LOGI("%s hack thread: %d", ABI, gettid());
    const int api_level = GetAndroidApiLevel();
    LOGI("%s api level: %d", ABI, api_level);
    if (api_level >= 30) {
        if (!IsABIRequiredNativeBridge()) {
            void *addr;
            if (IsRunningOnNativeBridge()) {
                addr = reinterpret_cast<void *>(dlopen);
            } else {
                addr = DobbySymbolResolver(nullptr,
                                           "__dl__Z9do_dlopenPKciPK17android_dlextinfoPKv");
            }
            if (addr) {
                LOGI("%s do_dlopen at: %p", ABI, addr);
                if (IsRunningOnNativeBridge()) {
                    addr_do_dlopen = addr;
                    DobbyHook(addr_do_dlopen, reinterpret_cast<void *>(new_do_dlopen),
                              reinterpret_cast<void **>(&orig_do_dlopen));
                } else {
                    addr_do_dlopen_V24 = addr;
                    DobbyHook(addr_do_dlopen_V24, reinterpret_cast<void *>(new_do_dlopen_V24),
                              reinterpret_cast<void **>(&orig_do_dlopen_V24));
                }
            }
        } else {
            addr_NativeBridgeLoadLibraryExt_V30 = GetNativeBridgeLoadLibrary(
                    DobbySymbolResolver(nullptr,
                                        "NativeBridgeLoadLibraryExt"));
            if (addr_NativeBridgeLoadLibraryExt_V30) {
                LOGI("NativeBridgeLoadLibraryExt at: %p", addr_NativeBridgeLoadLibraryExt_V30);
                DobbyHook(addr_NativeBridgeLoadLibraryExt_V30,
                          reinterpret_cast<void *>(new_NativeBridgeLoadLibraryExt_V30),
                          reinterpret_cast<void **>(&orig_NativeBridgeLoadLibraryExt_V30));
            }
        }
    } else if (api_level >= 26) {
        if (!IsABIRequiredNativeBridge()) {
            void *addr;
            if (IsRunningOnNativeBridge()) {
                addr = reinterpret_cast<void *>(dlopen);
            } else {
                void *libdl_handle = dlopen("libdl.so", RTLD_LAZY);
                addr = dlsym(libdl_handle, "__loader_dlopen");
            }
            if (addr) {
                if (IsRunningOnNativeBridge()) {
                    LOGI("do_dlopen at: %p", addr);
                    addr_do_dlopen = addr;
                    DobbyHook(addr_do_dlopen, reinterpret_cast<void *>(new_do_dlopen),
                              reinterpret_cast<void **>(&orig_do_dlopen));
                } else {
                    LOGI("__loader_dlopen at: %p", addr);
                    addr___loader_dlopen = addr;
                    DobbyHook(addr___loader_dlopen, reinterpret_cast<void *>(new___loader_dlopen),
                              reinterpret_cast<void **>(&orig___loader_dlopen));
                }
            }
        } else {
            addr_NativeBridgeLoadLibraryExt_V26 = GetNativeBridgeLoadLibrary(
                    DobbySymbolResolver(nullptr,
                                        "_ZN7android26NativeBridgeLoadLibraryExtEPKciPNS_25native_bridge_namespace_tE"));
            if (addr_NativeBridgeLoadLibraryExt_V26) {
                LOGI("NativeBridgeLoadLibraryExt at: %p", addr_NativeBridgeLoadLibraryExt_V26);
                DobbyHook(addr_NativeBridgeLoadLibraryExt_V26,
                          reinterpret_cast<void *>(new_NativeBridgeLoadLibraryExt_V26),
                          reinterpret_cast<void **>(&orig_NativeBridgeLoadLibraryExt_V26));
            }
        }
    } else if (api_level >= 24) {
        if (!IsABIRequiredNativeBridge()) {
            void *addr;
            if (IsRunningOnNativeBridge()) {
                addr = reinterpret_cast<void *>(dlopen);
            } else {
                addr = DobbySymbolResolver(nullptr,
                                           "__dl__Z9do_dlopenPKciPK17android_dlextinfoPv");
            }
            if (addr) {
                LOGI("do_dlopen at: %p", addr);
                addr_do_dlopen_V24 = addr;
                DobbyHook(addr_do_dlopen_V24, reinterpret_cast<void *>(new_do_dlopen_V24),
                          reinterpret_cast<void **>(&orig_do_dlopen_V24));
            }
        } else {
            addr_NativeBridgeLoadLibrary_V21 = GetNativeBridgeLoadLibrary(
                    DobbySymbolResolver(nullptr,
                                        "_ZN7android23NativeBridgeLoadLibraryEPKci"));
            if (addr_NativeBridgeLoadLibrary_V21) {
                LOGI("NativeBridgeLoadLibrary at: %p", addr_NativeBridgeLoadLibrary_V21);
                DobbyHook(addr_NativeBridgeLoadLibrary_V21,
                          reinterpret_cast<void *>(new_NativeBridgeLoadLibrary_V21),
                          reinterpret_cast<void **>(&orig_NativeBridgeLoadLibrary_V21));
            }
        }
    } else {
        if (!IsABIRequiredNativeBridge()) {
            void *addr = DobbySymbolResolver(nullptr,
                                             "__dl__Z9do_dlopenPKciPK17android_dlextinfo");
            if (addr) {
                LOGI("do_dlopen at: %p", addr);
                addr_do_dlopen_V19 = addr;
                DobbyHook(addr_do_dlopen_V19, reinterpret_cast<void *>(new_do_dlopen_V19),
                          reinterpret_cast<void **>(&orig_do_dlopen_V19));
            }
        } else {
            addr_NativeBridgeLoadLibrary_V21 = GetNativeBridgeLoadLibrary(
                    DobbySymbolResolver(nullptr,
                                        "_ZN7android23NativeBridgeLoadLibraryEPKci"));
            if (addr_NativeBridgeLoadLibrary_V21) {
                LOGI("NativeBridgeLoadLibrary at: %p", addr_NativeBridgeLoadLibrary_V21);
                DobbyHook(addr_NativeBridgeLoadLibrary_V21,
                          reinterpret_cast<void *>(new_NativeBridgeLoadLibrary_V21),
                          reinterpret_cast<void **>(&orig_NativeBridgeLoadLibrary_V21));
            }
        }
    }
    if (IsABIRequiredNativeBridge()) {
        return;
    }
    while (!il2cpp_handle) {
        sleep(1);
    }
    // prevent crash
    sleep(1);

    auto dict = read_config();

    logger::init_logger();
    il2cpp_hook_init(il2cpp_handle);
    if (dict.has_value()) {
        localify::load_textdb(get_application_version(), &dict.value());
    }
    if (!text_id_dict.empty()) {
        localify::load_textId_textdb(text_id_dict);
    }
    il2cpp_hook();
    il2cpp_load_assetbundle();
}
