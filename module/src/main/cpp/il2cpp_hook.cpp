#include "stdinclude.hpp"
#include "il2cpp_hook.h"
#include "il2cpp/il2cpp_symbols.h"
#include "localify/localify.h"
#include "logger/logger.h"
#include "notifier/notifier.h"
#include <codecvt>
#include <thread>
#include <rapidjson/rapidjson.h>
#include <rapidjson/prettywriter.h>
#include <sstream>
#include <regex>
#include <SQLiteCpp/SQLiteCpp.h>
#include "jwt/jwt.hpp"

#include <list>

struct HookInfo {
    string image;
    string namespace_;
    string clazz;
    string method;
    int paramCount;
    void *address;
    void *replace;
    void **orig;
    function<bool(const MethodInfo *)> predict;
};

list<HookInfo> hookList;

#define HOOK_METHOD(image_, namespaceName, className, method_, paramCount_, ret, fn, ...) \
  void* addr_##className##_##method_;                                                     \
  ret (*orig_##className##_##method_)(__VA_ARGS__);                                       \
  ret new_##className##_##method_(__VA_ARGS__)fn                                          \
  HookInfo hookInfo_##className##_##method_ = []{ /* NOLINT(cert-err58-cpp) */            \
  auto info = HookInfo{                                                                   \
    .image = image_,                                                                      \
    .namespace_ = namespaceName,                                                          \
    .clazz = #className,                                                                  \
    .method = #method_,                                                                   \
    .paramCount = paramCount_,                                                            \
    .replace = reinterpret_cast<void *>(new_##className##_##method_)                      \
  };                                                                                      \
  hookList.emplace_back(info);                                                            \
  return info;                                                                            \
  }();

#define FIND_HOOK_METHOD(image_, namespaceName, className, method_, predictFn, ret, fn, ...) \
  void* addr_##className##_##method_;                                                        \
  ret (*orig_##className##_##method_)(__VA_ARGS__);                                          \
  ret new_##className##_##method_(__VA_ARGS__)fn                                             \
  HookInfo hookInfo_##className##_##method_ = []{ /* NOLINT(cert-err58-cpp) */               \
  auto info = HookInfo{                                                                      \
    .image = image_,                                                                         \
    .namespace_ = namespaceName,                                                             \
    .clazz = #className,                                                                     \
    .method = #method_,                                                                      \
    .replace = reinterpret_cast<void *>(new_##className##_##method_),                        \
    .predict = predictFn                                                                     \
  };                                                                                         \
  hookList.emplace_back(info);                                                               \
  return info;                                                                               \
  }();

using namespace il2cpp_symbols;
using namespace localify;
using namespace logger;

static void *il2cpp_handle = nullptr;
static uint64_t il2cpp_base = 0;

Il2CppObject *assets = nullptr;

Il2CppObject *replaceAssets = nullptr;

Il2CppObject *(*load_from_file)(Il2CppString *path);

Il2CppObject *(*load_assets)(Il2CppObject *thisObj, Il2CppString *name, Il2CppObject *type);

Il2CppArray *(*get_all_asset_names)(Il2CppObject *thisObj);

Il2CppString *(*uobject_get_name)(Il2CppObject *uObject);

bool (*uobject_IsNativeObjectAlive)(Il2CppObject *uObject);

Il2CppString *(*get_unityVersion)();

vector<string> replaceAssetNames;

Il2CppObject *
GetRuntimeType(const char *assemblyName, const char *namespaze, const char *klassName) {
    auto dummyObj = (Il2CppObject *) il2cpp_object_new(
            il2cpp_symbols::get_class(assemblyName, namespaze, klassName));
    auto (*get_type)(Il2CppObject *thisObj) = reinterpret_cast<Il2CppObject *(*)(
            Il2CppObject *thisObj)>(il2cpp_symbols::get_method_pointer("mscorlib.dll", "System",
                                                                       "Object", "GetType", 0));
    return get_type(dummyObj);
}

template<typename... T, typename R>
Il2CppDelegate *CreateDelegate(Il2CppObject *target, R (*fn)(Il2CppObject *, T...)) {
    auto delegate = reinterpret_cast<MulticastDelegate *>(il2cpp_object_new(
            il2cpp_defaults.multicastdelegate_class));
    auto delegateClass = il2cpp_defaults.delegate_class;
    delegate->delegates = il2cpp_array_new(delegateClass, 1);
    il2cpp_array_set(delegate->delegates, Il2CppDelegate *, 0, delegate);
    delegate->method_ptr = reinterpret_cast<Il2CppMethodPointer>(fn);

    auto methodInfo = reinterpret_cast<MethodInfo *>(il2cpp_object_new(
            il2cpp_defaults.method_info_class));
    methodInfo->methodPointer = delegate->method_ptr;
    methodInfo->klass = il2cpp_defaults.method_info_class;
    delegate->method = methodInfo;
    delegate->target = target;
    return delegate;
}

template<typename... T>
Il2CppDelegate *CreateUnityAction(Il2CppObject *target, void (*fn)(Il2CppObject *, T...)) {
    auto delegate = reinterpret_cast<MulticastDelegate *>(
            il2cpp_object_new(
                    il2cpp_symbols::get_class("UnityEngine.CoreModule.dll", "UnityEngine.Events",
                                              "UnityAction")));
    auto delegateClass = il2cpp_defaults.delegate_class;
    delegate->delegates = il2cpp_array_new(delegateClass, 1);
    il2cpp_array_set(delegate->delegates, Il2CppDelegate *, 0, delegate);
    delegate->method_ptr = reinterpret_cast<Il2CppMethodPointer>(fn);

    auto methodInfo = reinterpret_cast<MethodInfo *>(il2cpp_object_new(
            il2cpp_defaults.method_info_class));
    methodInfo->methodPointer = delegate->method_ptr;
    methodInfo->klass = il2cpp_defaults.method_info_class;
    delegate->method = methodInfo;
    delegate->target = target;
    return delegate;
}

Boolean GetBoolean(bool value) {
    return reinterpret_cast<Boolean (*)(Il2CppString *value)>(il2cpp_symbols::get_method_pointer(
            "mscorlib.dll", "System", "Boolean", "Parse", 1))(
            il2cpp_string_new(value ? "true" : "false"));
}

Int32Object *GetInt32Instance(int value) {
    return reinterpret_cast<Int32Object *>(il2cpp_value_box(il2cpp_defaults.int32_class, &value));
}

Il2CppObject *ParseEnum(Il2CppObject *runtimeType, const string &name) {
    return reinterpret_cast<Il2CppObject *(*)(Il2CppObject *,
                                              Il2CppString *)>(il2cpp_symbols::get_method_pointer(
            "mscorlib.dll", "System", "Enum", "Parse", 2))(runtimeType,
                                                           il2cpp_string_new(name.data()));
}

Il2CppString *GetEnumName(Il2CppObject *runtimeType, u_int id) {
    return reinterpret_cast<Il2CppString *(*)(Il2CppObject *,
                                              Int32Object *)>(il2cpp_symbols::get_method_pointer(
            "mscorlib.dll", "System", "Enum", "GetName", 2))(runtimeType, GetInt32Instance(
            static_cast<int>(id)));
}

unsigned long GetEnumValue(Il2CppObject *runtimeEnum) {
    return reinterpret_cast<unsigned long (*)(Il2CppObject *)>(il2cpp_symbols::get_method_pointer(
            "mscorlib.dll", "System", "Enum", "ToUInt64", 1))(runtimeEnum);
}

Il2CppObject *GetCustomTMPFont() {
    if (!assets) return nullptr;
    if (!g_tmpro_font_asset_name.empty()) {
        auto tmpFont = load_assets(assets, il2cpp_string_new(g_tmpro_font_asset_name.data()),
                                   GetRuntimeType("Unity.TextMeshPro.dll", "TMPro",
                                                  "TMP_FontAsset"));
        return tmpFont;
    }
    return nullptr;
}

string GetUnityVersion() {
    return string(localify::u16_u8(get_unityVersion()->start_char));
}

Il2CppObject *GetSingletonInstance(Il2CppClass *klass) {
    if (!klass || !klass->parent) {
        return nullptr;
    }
    if (string(klass->parent->name).find("Singleton`1") == string::npos) {
        return nullptr;
    }
    auto instanceField = il2cpp_class_get_field_from_name(klass, "_instance");
    Il2CppObject *instance;
    il2cpp_field_static_get_value(instanceField, &instance);
    return instance;
}

void *update_orig = nullptr;

void *update_hook(Il2CppObject *thisObj, void *updateType, float deltaTime, float independentTime) {
    return reinterpret_cast<decltype(update_hook) * > (update_orig)(thisObj, updateType, deltaTime *
                                                                                         g_ui_animation_scale,
                                                                    independentTime *
                                                                    g_ui_animation_scale);
}

void *CySpringController_set_UpdateMode_orig = nullptr;

void CySpringController_set_UpdateMode_hook(Il2CppObject *thisObj, int  /*value*/) {
    reinterpret_cast<decltype(CySpringController_set_UpdateMode_hook) *>(CySpringController_set_UpdateMode_orig)(
            thisObj, g_cyspring_update_mode);
}

void *CySpringController_get_UpdateMode_orig = nullptr;

int CySpringController_get_UpdateMode_hook(Il2CppObject *thisObj) {
    CySpringController_set_UpdateMode_hook(thisObj, g_cyspring_update_mode);
    return reinterpret_cast<decltype(CySpringController_get_UpdateMode_hook) *>(CySpringController_get_UpdateMode_orig)(
            thisObj);
}

void *CySpringModelController_set_SpringUpdateMode_orig = nullptr;

void CySpringModelController_set_SpringUpdateMode_hook(Il2CppObject *thisObj, int  /*value*/) {
    reinterpret_cast<decltype(CySpringModelController_set_SpringUpdateMode_hook) *>(CySpringModelController_set_SpringUpdateMode_orig)(
            thisObj, g_cyspring_update_mode);
}

void *CySpringModelController_get_SpringUpdateMode_orig = nullptr;

int CySpringModelController_get_SpringUpdateMode_hook(Il2CppObject *thisObj) {
    CySpringModelController_set_SpringUpdateMode_hook(thisObj, g_cyspring_update_mode);
    return reinterpret_cast<decltype(CySpringModelController_get_SpringUpdateMode_hook) *>(CySpringModelController_get_SpringUpdateMode_orig)(
            thisObj);
}

void *TMP_Text_set_text_orig = nullptr;

void TMP_Text_set_text_hook(Il2CppObject *thisObj, Il2CppString *text) {
    reinterpret_cast<decltype(TMP_Text_set_text_hook) *>(TMP_Text_set_text_orig)(thisObj,
                                                                                 localify::get_localized_string(
                                                                                         text));
}


Il2CppString *(*tmp_text_get_text)(Il2CppObject *);

void (*tmp_text_set_text)(Il2CppObject *, Il2CppString *);

void *TextMeshPro_Awake_orig = nullptr;

void TextMeshPro_Awake_hook(Il2CppObject *thisObj) {
    auto text = localify::get_localized_string(tmp_text_get_text(thisObj));
    tmp_text_set_text(thisObj, text);

    // Allow overflow for custom font
    reinterpret_cast<void (*)(Il2CppObject *, int)>(il2cpp_class_get_method_from_name(
            thisObj->klass, "set_overflowMode", 1)->methodPointer)(thisObj, 0);
    reinterpret_cast<decltype(TextMeshPro_Awake_hook) *>(TextMeshPro_Awake_orig)(thisObj);
}

void *set_fps_orig = nullptr;

void set_fps_hook([[maybe_unused]] int value) {
    return reinterpret_cast<decltype(set_fps_hook) * > (set_fps_orig)(g_max_fps);
}

void *set_anti_aliasing_orig = nullptr;

void set_anti_aliasing_hook(int  /*level*/) {
    reinterpret_cast<decltype(set_anti_aliasing_hook) * > (set_anti_aliasing_orig)(g_anti_aliasing);
}

void *set_shadowResolution_orig = nullptr;

void set_shadowResolution_hook(int  /*level*/) {
    reinterpret_cast<decltype(set_shadowResolution_hook) *>(set_shadowResolution_orig)(3);
}

void *set_anisotropicFiltering_orig = nullptr;

void set_anisotropicFiltering_hook(int  /*mode*/) {
    reinterpret_cast<decltype(set_anisotropicFiltering_hook) *>(set_anisotropicFiltering_orig)(2);
}

void *set_shadows_orig = nullptr;

void set_shadows_hook(int  /*quality*/) {
    reinterpret_cast<decltype(set_shadows_hook) *>(set_shadows_orig)(2);
}

void *set_softVegetation_orig = nullptr;

void set_softVegetation_hook(bool  /*enable*/) {
    reinterpret_cast<decltype(set_softVegetation_hook) *>(set_softVegetation_orig)(true);
}

void *set_realtimeReflectionProbes_orig = nullptr;

void set_realtimeReflectionProbes_hook(bool  /*enable*/) {
    reinterpret_cast<decltype(set_realtimeReflectionProbes_hook) *>(set_realtimeReflectionProbes_orig)(
            true);
}

void *Light_set_shadowResolution_orig = nullptr;

void Light_set_shadowResolution_hook(Il2CppObject *thisObj, int  /*level*/) {
    reinterpret_cast<decltype(Light_set_shadowResolution_hook) *>(Light_set_shadowResolution_orig)(
            thisObj, 3);
}

Il2CppObject *(*display_get_main)();

int (*get_system_width)(Il2CppObject *thisObj);

int (*get_system_height)(Il2CppObject *thisObj);

void *set_resolution_orig = nullptr;

void set_resolution_hook(int width, int height, bool fullscreen) {
    const int systemWidth = get_system_width(display_get_main());
    const int systemHeight = get_system_height(display_get_main());
    if (g_ui_use_system_resolution) {
        reinterpret_cast<decltype(set_resolution_hook) * > (set_resolution_orig)(systemWidth,
                                                                                 systemHeight,
                                                                                 fullscreen);
    } else {
        reinterpret_cast<decltype(set_resolution_hook) * > (set_resolution_orig)(width, height,
                                                                                 fullscreen);
    }
}

void *GraphicSettings_GetVirtualResolution_orig = nullptr;

Vector2Int_t GraphicSettings_GetVirtualResolution_hook(Il2CppObject *thisObj) {
    auto res = reinterpret_cast<decltype(GraphicSettings_GetVirtualResolution_hook) *>(
            GraphicSettings_GetVirtualResolution_orig
    )(thisObj);
    // LOGD("GraphicSettings_GetVirtualResolution %d %d", res.x, res.y);
    return res;
}

void *GraphicSettings_GetVirtualResolution3D_orig = nullptr;

Vector2Int_t
GraphicSettings_GetVirtualResolution3D_hook(Il2CppObject *thisObj, bool isForcedWideAspect) {
    auto resolution = reinterpret_cast<decltype(GraphicSettings_GetVirtualResolution3D_hook) *>(GraphicSettings_GetVirtualResolution3D_orig)(
            thisObj, isForcedWideAspect);
    resolution.x = static_cast<int>(roundf(
            static_cast<float>(resolution.x) * g_resolution_3d_scale));
    resolution.y = static_cast<int>(roundf(
            static_cast<float>(resolution.y) * g_resolution_3d_scale));
    return resolution;
}

void *PathResolver_GetLocalPath_orig = nullptr;

Il2CppString *PathResolver_GetLocalPath_hook(Il2CppObject *thisObj, int kind, Il2CppString *hname) {
    auto hnameU8 = localify::u16_u8(hname->start_char);
    if (g_replace_assets.find(hnameU8) != g_replace_assets.end()) {
        auto &replaceAsset = g_replace_assets.at(hnameU8);
        return il2cpp_string_new(replaceAsset.path.data());
    }
    return reinterpret_cast<decltype(PathResolver_GetLocalPath_hook) *>(PathResolver_GetLocalPath_orig)(
            thisObj, kind, hname);
}

void *apply_graphics_quality_orig = nullptr;

void apply_graphics_quality_hook(Il2CppObject *thisObj, int  /*quality*/, bool  /*force*/) {
    LOGD("apply_graphics_quality");
    reinterpret_cast<decltype(apply_graphics_quality_hook) * >
    (apply_graphics_quality_orig)(thisObj, g_graphics_quality, false);
}

void *assetbundle_LoadFromFile_orig = nullptr;

Il2CppObject *assetbundle_LoadFromFile_hook(Il2CppString *path) {
    stringstream pathStream(localify::u16_u8(path->start_char));
    string segment;
    vector<string> split;
    while (getline(pathStream, segment, '/')) {
        split.push_back(segment);
    }
    if (g_replace_assets.find(split[split.size() - 1]) != g_replace_assets.end()) {
        auto &replaceAsset = g_replace_assets.at(split[split.size() - 1]);
        replaceAsset.asset = reinterpret_cast<decltype(assetbundle_LoadFromFile_hook) *>(assetbundle_LoadFromFile_orig)(
                il2cpp_string_new(replaceAsset.path.data()));
        return replaceAsset.asset;
    }
    return reinterpret_cast<decltype(assetbundle_LoadFromFile_hook) *>(assetbundle_LoadFromFile_orig)(
            path);
}

void *assetbundle_load_asset_orig = nullptr;

Il2CppObject *
assetbundle_load_asset_hook(Il2CppObject *thisObj, Il2CppString *name, const Il2CppType *type) {
    stringstream pathStream(localify::u16_u8(name->start_char));
    string segment;
    vector<string> split;
    while (getline(pathStream, segment, '/')) {
        split.emplace_back(segment);
    }
    auto &fileName = split.back();
    if (find_if(replaceAssetNames.begin(), replaceAssetNames.end(), [fileName](const string &item) {
        return item.find(fileName) != string::npos;
    }) != replaceAssetNames.end()) {
        return reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
                replaceAssets, il2cpp_string_new(fileName.data()), type);
    }
    auto *asset = reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
            thisObj, name, type);
    return asset;
}

void *assetbundle_unload_orig = nullptr;

void assetbundle_unload_hook(Il2CppObject *thisObj, bool unloadAllLoadedObjects) {
    for (auto &pair: g_replace_assets) {
        if (pair.second.asset == thisObj) {
            pair.second.asset = nullptr;
        }
    }
    reinterpret_cast<decltype(assetbundle_unload_hook) * > (assetbundle_unload_orig)(thisObj,
                                                                                     unloadAllLoadedObjects);
}

void *resources_load_orig = nullptr;

Il2CppObject *resources_load_hook(Il2CppString *path, Il2CppType *type) {
    stringstream pathStream(localify::u16_u8(path->start_char));
    LOGD("resources_load_asset: %s", pathStream.str().data());
    string segment;
    vector<string> split;
    while (getline(pathStream, segment, '/')) {
        split.emplace_back(segment);
    }
    auto &fileName = split.back();
    if (find_if(replaceAssetNames.begin(), replaceAssetNames.end(), [fileName](const string &item) {
        return item.find(fileName) != string::npos;
    }) != replaceAssetNames.end()) {
        return reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
                replaceAssets, il2cpp_string_new(fileName.data()), type);
    }
    auto object = reinterpret_cast<decltype(resources_load_hook) *>(resources_load_orig)(path,
                                                                                         type);
    if (object->klass->name == "TMP_Settings"s) {
        auto fontAssetField = il2cpp_class_get_field_from_name(object->klass, "m_defaultFontAsset");
        il2cpp_field_set_value(object, fontAssetField, GetCustomTMPFont());
    }
    return object;

}

void *Sprite_get_texture_orig = nullptr;

Il2CppObject *Sprite_get_texture_hook(Il2CppObject *thisObj) {
    auto texture2D = reinterpret_cast<decltype(Sprite_get_texture_hook) *>(Sprite_get_texture_orig)(
            thisObj);
    auto uobject_name = uobject_get_name(texture2D);
    if (!localify::u16_u8(uobject_name->start_char).empty()) {
        auto newTexture = reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
                replaceAssets, uobject_name,
                reinterpret_cast<Il2CppType *>(GetRuntimeType("UnityEngine.CoreModule.dll",
                                                              "UnityEngine", "Texture2D")));
        if (newTexture) {
            reinterpret_cast<void (*)(Il2CppObject *, int)>(
                    il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine",
                                                       "Object", "set_hideFlags", 1))(newTexture,
                                                                                      32);
            return newTexture;
        }
    }
    return texture2D;
}

void *Renderer_get_material_orig = nullptr;

Il2CppObject *Renderer_get_material_hook(Il2CppObject *thisObj) {
    auto material = reinterpret_cast<decltype(Renderer_get_material_hook) *>(Renderer_get_material_orig)(
            thisObj);
    if (material) {
        auto get_mainTexture = reinterpret_cast<Il2CppObject *(*)(
                Il2CppObject *)>(il2cpp_class_get_method_from_name(material->klass,
                                                                   "get_mainTexture",
                                                                   0)->methodPointer);
        auto set_mainTexture = reinterpret_cast<Il2CppObject *(*)(Il2CppObject *,
                                                                  Il2CppObject *)>(il2cpp_class_get_method_from_name(
                material->klass, "set_mainTexture", 1)->methodPointer);
        auto mainTexture = get_mainTexture(material);
        if (mainTexture) {
            auto uobject_name = uobject_get_name(mainTexture);
            if (!localify::u16_u8(uobject_name->start_char).empty()) {
                auto newTexture = reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
                        replaceAssets, uobject_name,
                        reinterpret_cast<Il2CppType *>(GetRuntimeType("UnityEngine.CoreModule.dll",
                                                                      "UnityEngine", "Texture2D")));
                if (newTexture) {
                    reinterpret_cast<void (*)(Il2CppObject *, int)>(
                            il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll",
                                                               "UnityEngine", "Object",
                                                               "set_hideFlags", 1))(newTexture, 32);
                    set_mainTexture(material, newTexture);
                }
            }
        }
    }
    return material;
}

void *Renderer_get_materials_orig = nullptr;

Il2CppArray *Renderer_get_materials_hook(Il2CppObject *thisObj) {
    auto materials = reinterpret_cast<decltype(Renderer_get_materials_hook) *>(Renderer_get_materials_orig)(
            thisObj);
    for (int i = 0; i < materials->max_length; i++) {
        auto material = reinterpret_cast<Il2CppObject *>(materials->vector[i]);
        if (material) {
            auto get_mainTexture = reinterpret_cast<Il2CppObject *(*)(
                    Il2CppObject *)>(il2cpp_class_get_method_from_name(material->klass,
                                                                       "get_mainTexture",
                                                                       0)->methodPointer);
            auto set_mainTexture = reinterpret_cast<Il2CppObject *(*)(Il2CppObject *,
                                                                      Il2CppObject *)>(il2cpp_class_get_method_from_name(
                    material->klass, "set_mainTexture", 1)->methodPointer);
            auto mainTexture = get_mainTexture(material);
            if (mainTexture) {
                auto uobject_name = uobject_get_name(mainTexture);
                if (!localify::u16_u8(uobject_name->start_char).empty()) {
                    auto newTexture = reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
                            replaceAssets, uobject_name,
                            reinterpret_cast<Il2CppType *>(GetRuntimeType(
                                    "UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D")));
                    if (newTexture) {
                        reinterpret_cast<void (*)(Il2CppObject *, int)>(
                                il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll",
                                                                   "UnityEngine", "Object",
                                                                   "set_hideFlags", 1))(newTexture,
                                                                                        32);
                        set_mainTexture(material, newTexture);
                    }
                }
            }
        }
    }
    return materials;
}

void *Renderer_get_sharedMaterial_orig = nullptr;

Il2CppObject *Renderer_get_sharedMaterial_hook(Il2CppObject *thisObj) {
    auto material = reinterpret_cast<decltype(Renderer_get_sharedMaterial_hook) *>(Renderer_get_sharedMaterial_orig)(
            thisObj);
    if (material) {
        auto get_mainTexture = reinterpret_cast<Il2CppObject *(*)(
                Il2CppObject *)>(il2cpp_class_get_method_from_name(material->klass,
                                                                   "get_mainTexture",
                                                                   0)->methodPointer);
        auto set_mainTexture = reinterpret_cast<Il2CppObject *(*)(Il2CppObject *,
                                                                  Il2CppObject *)>(il2cpp_class_get_method_from_name(
                material->klass, "set_mainTexture", 1)->methodPointer);
        auto mainTexture = get_mainTexture(material);
        if (mainTexture) {
            auto uobject_name = uobject_get_name(mainTexture);
            if (!localify::u16_u8(uobject_name->start_char).empty()) {
                auto newTexture = reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
                        replaceAssets, uobject_name,
                        reinterpret_cast<Il2CppType *>(GetRuntimeType("UnityEngine.CoreModule.dll",
                                                                      "UnityEngine", "Texture2D")));
                if (newTexture) {
                    reinterpret_cast<void (*)(Il2CppObject *, int)>(
                            il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll",
                                                               "UnityEngine", "Object",
                                                               "set_hideFlags", 1))(newTexture, 32);
                    set_mainTexture(material, newTexture);
                }
            }
        }
    }
    return material;
}

void *Renderer_get_sharedMaterials_orig = nullptr;

Il2CppArray *Renderer_get_sharedMaterials_hook(Il2CppObject *thisObj) {
    auto materials = reinterpret_cast<decltype(Renderer_get_sharedMaterials_hook) *>(Renderer_get_sharedMaterials_orig)(
            thisObj);
    for (int i = 0; i < materials->max_length; i++) {
        auto material = reinterpret_cast<Il2CppObject *>(materials->vector[i]);
        if (material) {
            auto get_mainTexture = reinterpret_cast<Il2CppObject *(*)(
                    Il2CppObject *)>(il2cpp_class_get_method_from_name(material->klass,
                                                                       "get_mainTexture",
                                                                       0)->methodPointer);
            auto set_mainTexture = reinterpret_cast<Il2CppObject *(*)(Il2CppObject *,
                                                                      Il2CppObject *)>(il2cpp_class_get_method_from_name(
                    material->klass, "set_mainTexture", 1)->methodPointer);
            auto mainTexture = get_mainTexture(material);
            if (mainTexture) {
                auto uobject_name = uobject_get_name(mainTexture);
                if (!localify::u16_u8(uobject_name->start_char).empty()) {
                    auto newTexture = reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
                            replaceAssets, uobject_name,
                            reinterpret_cast<Il2CppType *>(GetRuntimeType(
                                    "UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D")));
                    if (newTexture) {
                        reinterpret_cast<void (*)(Il2CppObject *, int)>(
                                il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll",
                                                                   "UnityEngine", "Object",
                                                                   "set_hideFlags", 1))(newTexture,
                                                                                        32);
                        set_mainTexture(material, newTexture);
                    }
                }
            }
        }
    }
    return materials;
}

void *Renderer_set_material_orig = nullptr;

void Renderer_set_material_hook(Il2CppObject *thisObj, Il2CppObject *material) {
    if (material) {
        auto get_mainTexture = reinterpret_cast<Il2CppObject *(*)(
                Il2CppObject *)>(il2cpp_class_get_method_from_name(material->klass,
                                                                   "get_mainTexture",
                                                                   0)->methodPointer);
        auto set_mainTexture = reinterpret_cast<Il2CppObject *(*)(Il2CppObject *,
                                                                  Il2CppObject *)>(il2cpp_class_get_method_from_name(
                material->klass, "set_mainTexture", 1)->methodPointer);
        auto mainTexture = get_mainTexture(material);
        if (mainTexture) {
            auto uobject_name = uobject_get_name(mainTexture);
            if (!localify::u16_u8(uobject_name->start_char).empty()) {
                auto newTexture = reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
                        replaceAssets, uobject_name,
                        reinterpret_cast<Il2CppType *>(GetRuntimeType("UnityEngine.CoreModule.dll",
                                                                      "UnityEngine", "Texture2D")));
                if (newTexture) {
                    reinterpret_cast<void (*)(Il2CppObject *, int)>(
                            il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll",
                                                               "UnityEngine", "Object",
                                                               "set_hideFlags", 1))(newTexture, 32);
                    set_mainTexture(material, newTexture);
                }
            }
        }
    }
    reinterpret_cast<decltype(Renderer_set_material_hook) *>(Renderer_set_material_orig)(thisObj,
                                                                                         material);
}

void *Renderer_set_materials_orig = nullptr;

void Renderer_set_materials_hook(Il2CppObject *thisObj, Il2CppArray *materials) {
    for (int i = 0; i < materials->max_length; i++) {
        auto material = reinterpret_cast<Il2CppObject *>(materials->vector[i]);
        if (material) {
            auto get_mainTexture = reinterpret_cast<Il2CppObject *(*)(
                    Il2CppObject *)>(il2cpp_class_get_method_from_name(material->klass,
                                                                       "get_mainTexture",
                                                                       0)->methodPointer);
            auto set_mainTexture = reinterpret_cast<Il2CppObject *(*)(Il2CppObject *,
                                                                      Il2CppObject *)>(il2cpp_class_get_method_from_name(
                    material->klass, "set_mainTexture", 1)->methodPointer);
            auto mainTexture = get_mainTexture(material);
            if (mainTexture) {
                auto uobject_name = uobject_get_name(mainTexture);
                if (!localify::u16_u8(uobject_name->start_char).empty()) {
                    auto newTexture = reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
                            replaceAssets, uobject_name,
                            reinterpret_cast<Il2CppType *>(GetRuntimeType(
                                    "UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D")));
                    if (newTexture) {
                        reinterpret_cast<void (*)(Il2CppObject *, int)>(
                                il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll",
                                                                   "UnityEngine", "Object",
                                                                   "set_hideFlags", 1))(newTexture,
                                                                                        32);
                        set_mainTexture(material, newTexture);
                    }
                }
            }
        }
    }
    reinterpret_cast<decltype(Renderer_set_materials_hook) *>(Renderer_set_materials_orig)(thisObj,
                                                                                           materials);
}

void *Renderer_set_sharedMaterial_orig = nullptr;

void Renderer_set_sharedMaterial_hook(Il2CppObject *thisObj, Il2CppObject *material) {
    if (material) {
        auto get_mainTexture = reinterpret_cast<Il2CppObject *(*)(
                Il2CppObject *)>(il2cpp_class_get_method_from_name(material->klass,
                                                                   "get_mainTexture",
                                                                   0)->methodPointer);
        auto set_mainTexture = reinterpret_cast<Il2CppObject *(*)(Il2CppObject *,
                                                                  Il2CppObject *)>(il2cpp_class_get_method_from_name(
                material->klass, "set_mainTexture", 1)->methodPointer);
        auto mainTexture = get_mainTexture(material);
        if (mainTexture) {
            auto uobject_name = uobject_get_name(mainTexture);
            if (!localify::u16_u8(uobject_name->start_char).empty()) {
                auto newTexture = reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
                        replaceAssets, uobject_name,
                        reinterpret_cast<Il2CppType *>(GetRuntimeType("UnityEngine.CoreModule.dll",
                                                                      "UnityEngine", "Texture2D")));
                if (newTexture) {
                    reinterpret_cast<void (*)(Il2CppObject *, int)>(
                            il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll",
                                                               "UnityEngine", "Object",
                                                               "set_hideFlags", 1))(newTexture, 32);
                    set_mainTexture(material, newTexture);
                }
            }
        }
    }
    reinterpret_cast<decltype(Renderer_set_sharedMaterial_hook) *>(Renderer_set_sharedMaterial_orig)(
            thisObj, material);
}

void *Renderer_set_sharedMaterials_orig = nullptr;

void Renderer_set_sharedMaterials_hook(Il2CppObject *thisObj, Il2CppArray *materials) {
    for (int i = 0; i < materials->max_length; i++) {
        auto material = reinterpret_cast<Il2CppObject *>(materials->vector[i]);
        if (material) {
            auto get_mainTexture = reinterpret_cast<Il2CppObject *(*)(
                    Il2CppObject *)>(il2cpp_class_get_method_from_name(material->klass,
                                                                       "get_mainTexture",
                                                                       0)->methodPointer);
            auto set_mainTexture = reinterpret_cast<Il2CppObject *(*)(Il2CppObject *,
                                                                      Il2CppObject *)>(il2cpp_class_get_method_from_name(
                    material->klass, "set_mainTexture", 1)->methodPointer);
            auto mainTexture = get_mainTexture(material);
            if (mainTexture) {
                auto uobject_name = uobject_get_name(mainTexture);
                if (!localify::u16_u8(uobject_name->start_char).empty()) {
                    auto newTexture = reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
                            replaceAssets, uobject_name,
                            reinterpret_cast<Il2CppType *>(GetRuntimeType(
                                    "UnityEngine.CoreModule.dll", "UnityEngine", "Texture2D")));
                    if (newTexture) {
                        reinterpret_cast<void (*)(Il2CppObject *, int)>(
                                il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll",
                                                                   "UnityEngine", "Object",
                                                                   "set_hideFlags", 1))(newTexture,
                                                                                        32);
                        set_mainTexture(material, newTexture);
                    }
                }
            }
        }
    }
    reinterpret_cast<decltype(Renderer_set_sharedMaterials_hook) *>(Renderer_set_sharedMaterials_orig)(
            thisObj, materials);
}

void *Material_set_mainTexture_orig = nullptr;

void Material_set_mainTexture_hook(Il2CppObject *thisObj, Il2CppObject *texture) {
    if (texture) {
        if (!localify::u16_u8(uobject_get_name(texture)->start_char).empty()) {
            auto newTexture = reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
                    replaceAssets, uobject_get_name(texture),
                    reinterpret_cast<Il2CppType *>(GetRuntimeType("UnityEngine.CoreModule.dll",
                                                                  "UnityEngine", "Texture2D")));
            if (newTexture) {
                reinterpret_cast<void (*)(Il2CppObject *, int)>(
                        il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll",
                                                           "UnityEngine", "Object", "set_hideFlags",
                                                           1))(newTexture, 32);
                reinterpret_cast<decltype(Material_set_mainTexture_hook) *>(Material_set_mainTexture_orig)(
                        thisObj, newTexture);
                return;
            }
        }
    }
    reinterpret_cast<decltype(Material_set_mainTexture_hook) *>(Material_set_mainTexture_orig)(
            thisObj, texture);
}

void *Material_get_mainTexture_orig = nullptr;

Il2CppObject *Material_get_mainTexture_hook(Il2CppObject *thisObj) {
    auto texture = reinterpret_cast<decltype(Material_get_mainTexture_hook) *>(Material_get_mainTexture_orig)(
            thisObj);
    if (texture) {
        auto uobject_name = uobject_get_name(texture);
        if (!localify::u16_u8(uobject_name->start_char).empty()) {
            auto newTexture = reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
                    replaceAssets, uobject_name,
                    reinterpret_cast<Il2CppType *>(GetRuntimeType("UnityEngine.CoreModule.dll",
                                                                  "UnityEngine", "Texture2D")));
            if (newTexture) {
                reinterpret_cast<void (*)(Il2CppObject *, int)>(
                        il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll",
                                                           "UnityEngine", "Object", "set_hideFlags",
                                                           1))(newTexture, 32);
                return newTexture;
            }
        }
    }
    return texture;
}

void *Material_SetTextureI4_orig = nullptr;

void Material_SetTextureI4_hook(Il2CppObject *thisObj, int nameID, Il2CppObject *texture) {
    if (texture && !localify::u16_u8(uobject_get_name(texture)->start_char).empty()) {
        auto newTexture = reinterpret_cast<decltype(assetbundle_load_asset_hook) *>(assetbundle_load_asset_orig)(
                replaceAssets, uobject_get_name(texture),
                reinterpret_cast<Il2CppType *>(GetRuntimeType("UnityEngine.CoreModule.dll",
                                                              "UnityEngine", "Texture2D")));
        if (newTexture) {
            reinterpret_cast<void (*)(Il2CppObject *, int)>(
                    il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine",
                                                       "Object", "set_hideFlags", 1))(newTexture,
                                                                                      32);
            reinterpret_cast<decltype(Material_SetTextureI4_hook) *>(Material_SetTextureI4_orig)(
                    thisObj, nameID, newTexture);
            return;
        }
    }
    reinterpret_cast<decltype(Material_SetTextureI4_hook) *>(Material_SetTextureI4_orig)(thisObj,
                                                                                         nameID,
                                                                                         texture);
}

void *CriMana_Player_SetFile_orig = nullptr;

bool
CriMana_Player_SetFile_hook(Il2CppObject *thisObj, Il2CppObject *binder, Il2CppString *moviePath,
                            int setMode) {
    stringstream pathStream(localify::u16_u8(moviePath->start_char));
    string segment;
    vector<string> split;
    while (getline(pathStream, segment, '\\')) {
        split.emplace_back(segment);
    }
    if (g_replace_assets.find(split[split.size() - 1]) != g_replace_assets.end()) {
        auto &replaceAsset = g_replace_assets.at(split[split.size() - 1]);
        moviePath = il2cpp_string_new(replaceAsset.path.data());
    }
    return reinterpret_cast<decltype(CriMana_Player_SetFile_hook) *>(CriMana_Player_SetFile_orig)(
            thisObj, binder, moviePath, setMode);
}

void *TMP_Settings_get_instance_orig = nullptr;

Il2CppObject *TMP_Settings_get_instance_hook() {
    auto tmpSettings = reinterpret_cast<decltype(TMP_Settings_get_instance_hook) *>(TMP_Settings_get_instance_orig)();
    auto fontAssetField = il2cpp_class_get_field_from_name(tmpSettings->klass,
                                                           "m_defaultFontAsset");
    il2cpp_field_set_value(tmpSettings, fontAssetField, GetCustomTMPFont());
    return tmpSettings;
}

void *PriariLocalizationBuilder_UseDefaultLanguage_orig = nullptr;

Il2CppObject *
PriariLocalizationBuilder_UseDefaultLanguage_hook(Il2CppObject *thisObj, Il2CppObject *language) {
    LOGD("PriariLocalizationBuilder_UseDefaultLanguage");
    // TODO: Set language as system language
    auto korean = reinterpret_cast<Il2CppObject *(*)()>(il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari.ServerShared.Globalization",
            "PriariLocalizationLanguage", "get_Korean", -1))();
    reinterpret_cast<decltype(PriariLocalizationBuilder_UseDefaultLanguage_hook) *>(PriariLocalizationBuilder_UseDefaultLanguage_orig)(
            thisObj, korean);
    return thisObj;
}

void *GraphicSettings_Initilaize_orig = nullptr;

void GraphicSettings_Initilaize_hook(Il2CppObject *thisObj) {
    reinterpret_cast<decltype(GraphicSettings_Initilaize_hook) *>(GraphicSettings_Initilaize_orig)(
            thisObj);
    apply_graphics_quality_hook(thisObj, 0, false);
}

void *TitleScene_isInServiceTerm_orig = nullptr;

bool TitleScene_isInServiceTerm_hook(Il2CppObject *thisObj) {
    LOGD("TitleScene_isInServiceTerm: %d",
         reinterpret_cast<decltype(TitleScene_isInServiceTerm_hook) *>(TitleScene_isInServiceTerm_orig)(
                 thisObj));
    return true;
}

void *ResourceManager_Initialize_orig = nullptr;

void ResourceManager_Initialize_hook(Il2CppObject *thisObj) {
    reinterpret_cast<decltype(ResourceManager_Initialize_hook) *>(ResourceManager_Initialize_orig)(
            thisObj);
    auto leaderTalkTableCacheField = il2cpp_class_get_field_from_name(thisObj->klass,
                                                                      "leaderTalkTableCache");
    Il2CppObject *leaderTalkTableCache;
    il2cpp_field_get_value(thisObj, leaderTalkTableCacheField, &leaderTalkTableCache);


    auto talkDataListField = il2cpp_class_get_field_from_name(thisObj->klass, "talkDataList");
    Il2CppObject *talkDataList;
    il2cpp_field_get_value(thisObj, talkDataListField, &talkDataList);

    Il2CppArray *talkDataListArray;
    il2cpp_field_get_value(talkDataList,
                           il2cpp_class_get_field_from_name(talkDataList->klass, "_items"),
                           &talkDataListArray);

    for (int j = 0; j < talkDataListArray->max_length; j++) {
        auto talkData = reinterpret_cast<Il2CppObject *>(talkDataListArray->vector[j]);
        if (talkData) {
            auto messageField = il2cpp_class_get_field_from_name(talkData->klass, "message");
            Il2CppString *message;
            il2cpp_field_get_value(talkData, messageField, &message);
            il2cpp_field_set_value(talkData, messageField, localify::get_localized_string(message));
        }
    }
}

void *TalkTimingTrack_Setup_orig = nullptr;

void TalkTimingTrack_Setup_hook(Il2CppObject *thisObj) {
    reinterpret_cast<decltype(TalkTimingTrack_Setup_hook) *>(TalkTimingTrack_Setup_orig)(thisObj);
    auto clipsField = il2cpp_class_get_field_from_name(thisObj->klass, "talkTimingClips");
    Il2CppObject *clips;
    il2cpp_field_get_value(thisObj, clipsField, &clips);

    Il2CppArray *clipsArray;
    il2cpp_field_get_value(clips, il2cpp_class_get_field_from_name(clips->klass, "_items"),
                           &clipsArray);

    auto dataField = il2cpp_class_get_field_from_name(thisObj->klass, "talkChoiceMessageDatas");
    Il2CppObject *data;
    il2cpp_field_get_value(thisObj, dataField, &data);

    Il2CppArray *dataArray;
    il2cpp_field_get_value(data, il2cpp_class_get_field_from_name(data->klass, "_items"),
                           &dataArray);

    for (int i = 0; i < clipsArray->max_length; i++) {
        auto clipData = reinterpret_cast<Il2CppObject *>(clipsArray->vector[i]);
        if (clipData) {
            auto talkField = il2cpp_class_get_field_from_name(clipData->klass, "talkTiming");
            Il2CppObject *talk;
            il2cpp_field_get_value(clipData, talkField, &talk);
            if (talk) {
                auto charListField = il2cpp_class_get_field_from_name(talk->klass,
                                                                      "characterAssetDatas");
                Il2CppObject *charList;
                il2cpp_field_get_value(talk, charListField, &charList);

                Il2CppArray *charArray;
                il2cpp_field_get_value(charList,
                                       il2cpp_class_get_field_from_name(charList->klass, "_items"),
                                       &charArray);

                for (int j = 0; j < charArray->max_length; j++) {
                    auto charData = reinterpret_cast<Il2CppObject *>(charArray->vector[j]);
                    if (charData) {
                        auto nameField = il2cpp_class_get_field_from_name(charData->klass, "name");
                        Il2CppString *name;
                        il2cpp_field_get_value(charData, nameField, &name);
                        il2cpp_field_set_value(charData, nameField,
                                               localify::get_localized_string(name));
                    }
                }

                auto messageDataField = il2cpp_class_get_field_from_name(talk->klass,
                                                                         "messageData");
                Il2CppObject *messageData;
                il2cpp_field_get_value(talk, messageDataField, &messageData);

                if (messageData) {
                    auto namesField = il2cpp_class_get_field_from_name(messageData->klass, "names");
                    Il2CppObject *names;
                    il2cpp_field_get_value(messageData, namesField, &names);

                    Il2CppArray *nameArray;
                    il2cpp_field_get_value(names,
                                           il2cpp_class_get_field_from_name(names->klass, "_items"),
                                           &nameArray);

                    for (int k = 0; k < nameArray->max_length; k++) {
                        auto text = reinterpret_cast<Il2CppString *>(nameArray->vector[k]);
                        if (text) {
                            il2cpp_array_set(nameArray, Il2CppString*, k,
                                             localify::get_localized_string(text));
                        }
                    }

                    auto messageField = il2cpp_class_get_field_from_name(messageData->klass,
                                                                         "message");
                    Il2CppString *message;
                    il2cpp_field_get_value(messageData, messageField, &message);
                    il2cpp_field_set_value(messageData, messageField,
                                           localify::get_localized_string(message));

                    auto overrideTalkerNameField = il2cpp_class_get_field_from_name(
                            messageData->klass, "overrideTalkerName");
                    Il2CppString *overrideTalkerName;
                    il2cpp_field_get_value(messageData, overrideTalkerNameField,
                                           &overrideTalkerName);
                    il2cpp_field_set_value(messageData, overrideTalkerNameField,
                                           localify::get_localized_string(overrideTalkerName));

                    auto messageWaitDataField = il2cpp_class_get_field_from_name(messageData->klass,
                                                                                 "messageWaitDatas");
                    Il2CppObject *messageWaitData;
                    il2cpp_field_get_value(messageData, messageWaitDataField, &messageWaitData);

                    Il2CppArray *messageWaitDataArray;
                    il2cpp_field_get_value(messageWaitData,
                                           il2cpp_class_get_field_from_name(messageWaitData->klass,
                                                                            "_items"),
                                           &messageWaitDataArray);
                    for (int k = 0; k < messageWaitDataArray->max_length; k++) {
                        auto waitData = reinterpret_cast<MessageWaitData *>(messageWaitDataArray->vector[k]);
                        if (waitData) {
                            waitData->message = localify::get_localized_string(waitData->message);
                        }
                    }
                }
            }
        }
    }

    for (int i = 0; i < dataArray->max_length; i++) {
        auto choiceData = reinterpret_cast<Il2CppObject *>(dataArray->vector[i]);
        if (choiceData) {
            auto messageField = il2cpp_class_get_field_from_name(choiceData->klass, "message");
            Il2CppString *message;
            il2cpp_field_get_value(choiceData, messageField, &message);
            il2cpp_field_set_value(choiceData, messageField,
                                   localify::get_localized_string(message));
        }
    }
}

void *TimeUtility_IsTermTime_orig = nullptr;

bool TimeUtility_IsTermTime_hook(Il2CppString *startDate, Il2CppString *endDate, long checkTime) {
    LOGD("TimeUtility_IsTermTime %d",
         reinterpret_cast<decltype(TimeUtility_IsTermTime_hook) *>(TimeUtility_IsTermTime_orig)(
                 startDate, endDate, checkTime));
    return true;
}

void *SystemDefines_get_ConfigurationJsonUrlBase_orig = nullptr;

Il2CppString *SystemDefines_get_ConfigurationJsonUrlBase_hook() {
    LOGD("Orig url: %s", localify::u16_u8(reinterpret_cast<decltype(SystemDefines_get_ConfigurationJsonUrlBase_hook)*>(SystemDefines_get_ConfigurationJsonUrlBase_orig)()->start_char).data());
    return il2cpp_string_new(g_configuration_json_url_base.data());
}

void *SystemDefines_get_Language_orig = nullptr;

Il2CppString *SystemDefines_get_Language_hook() {
    return il2cpp_string_new("Kor");
}

void *SystemDefines_get_MaintenanceJsonUrl_orig = nullptr;

Il2CppString *SystemDefines_get_MaintenanceJsonUrl_hook() {
    return il2cpp_string_new(
            (localify::u16_u8(SystemDefines_get_ConfigurationJsonUrlBase_hook()->start_char) +
             "maintenance-status.json"s).data());
}

void *SystemDefines_get_ClientVersionJsonUrl_orig = nullptr;

Il2CppString *SystemDefines_get_ClientVersionJsonUrl_hook() {
    return il2cpp_string_new(
            (localify::u16_u8(SystemDefines_get_ConfigurationJsonUrlBase_hook()->start_char) +
             "client-version.json"s).data());
}

void *SystemDefines_get_AssetResourceJsonUrl_orig = nullptr;

Il2CppString *SystemDefines_get_AssetResourceJsonUrl_hook() {
    return il2cpp_string_new(
            (localify::u16_u8(SystemDefines_get_ConfigurationJsonUrlBase_hook()->start_char) +
             "asset-resource.json"s).data());
}

void *SystemDefines_get_ServerEndpointJsonUrl_orig = nullptr;

Il2CppString *SystemDefines_get_ServerEndpointJsonUrl_hook() {
    return il2cpp_string_new(
            (localify::u16_u8(SystemDefines_get_ConfigurationJsonUrlBase_hook()->start_char) +
             "server-endpoint.json"s).data());
}

void *ClientServerConfiguration_TryUpdateAssetResourceVersionAsync_orig = nullptr;

Il2CppObject *ClientServerConfiguration_TryUpdateAssetResourceVersionAsync_hook(Il2CppObject* thisObj) {
    LOGD("ClientServerConfiguration_TryUpdateAssetResourceVersionAsync");
    return reinterpret_cast<Il2CppObject *(*)(Il2CppObject*)>(il2cpp_class_get_method_from_name(thisObj->klass, "TryUpdateServerEndpointAsync", 0)->methodPointer)(thisObj);
}

void *ManifestChecker_StartDownload_orig = nullptr;

void ManifestChecker_StartDownload_hook(Il2CppObject* thisObj, Il2CppDelegate *onManifestChecked, Il2CppDelegate onCancel, bool isBackGroundDownloadWindow) {
    LOGD("ManifestChecker_StartDownload");
    reinterpret_cast<void (*)(Il2CppObject *, bool)>(onManifestChecked->method_ptr)(onManifestChecked->target, true);
}

void init_il2cpp_api() {
#define DO_API(r, n, ...) n = (r (*) (__VA_ARGS__))dlsym(il2cpp_handle, #n)

#include "il2cpp/il2cpp-api-functions.h"

#undef DO_API
}

uint64_t get_module_base(const char *module_name) {
    uint64_t addr = 0;
    auto line = array<char, 1024>();
    uint64_t start = 0;
    uint64_t end = 0;
    auto flags = array<char, 5>();
    auto path = array<char, PATH_MAX>();

    FILE *fp = fopen("/proc/self/maps", "r");
    if (fp != nullptr) {
        while (fgets(line.data(), sizeof(line), fp)) {
            strcpy(path.data(), "");
            sscanf(line.data(), "%"
                                PRIx64
                                "-%"
                                PRIx64
                                " %s %*"
                                PRIx64
                                " %*x:%*x %*u %s\n", &start, &end, flags.data(), path.data());
#if defined(__aarch64__)
            if (strstr(flags.data(), "x") == 0)
                continue;
#endif
            if (strstr(path.data(), module_name)) {
                addr = start;
                break;
            }
        }
        fclose(fp);
    }
    return addr;
}

void hookMethods() {
    load_assets = reinterpret_cast<Il2CppObject *(*)(Il2CppObject *thisObj, Il2CppString *name,
                                                     Il2CppObject *type)>(
            il2cpp_symbols::get_method_pointer("UnityEngine.AssetBundleModule.dll", "UnityEngine",
                                               "AssetBundle", "LoadAsset", 2));

    get_all_asset_names = reinterpret_cast<Il2CppArray *(*)(
            Il2CppObject *thisObj)>(il2cpp_resolve_icall(
            "UnityEngine.AssetBundle::GetAllAssetNames()"));

    uobject_get_name = reinterpret_cast<Il2CppString *(*)(Il2CppObject *uObject)>(
            il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine",
                                               "Object", "GetName", -1));

    uobject_IsNativeObjectAlive = reinterpret_cast<bool (*)(Il2CppObject *uObject)>(
            il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine",
                                               "Object", "IsNativeObjectAlive", 1));

    get_unityVersion = reinterpret_cast<Il2CppString *(*)()>(
            il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll", "UnityEngine",
                                               "Application", "get_unityVersion", -1));

    auto update_addr = il2cpp_symbols::get_method_pointer("DOTween.dll", "DG.Tweening.Core",
                                                          "TweenManager", "Update", 3);

    auto CySpringController_set_UpdateMode_addr = il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari", "CySpringController", "set_UpdateMode", 1);

    auto CySpringController_get_UpdateMode_addr = il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari", "CySpringController", "get_UpdateMode", 0);

    auto CySpringModelController_set_SpringUpdateMode_addr = il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari", "CySpringModelController", "set_SpringUpdateMode", 1);

    auto CySpringModelController_get_SpringUpdateMode_addr = il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari", "CySpringModelController", "get_SpringUpdateMode", 0);

    auto TMP_Text_set_text_addr = reinterpret_cast<void (*)(Il2CppObject *, Il2CppString *)>(
            il2cpp_symbols::get_method_pointer("Unity.TextMeshPro.dll", "TMPro", "TMP_Text",
                                               "set_text", 1));

    tmp_text_get_text = reinterpret_cast<Il2CppString *(*)(Il2CppObject *)>(
            il2cpp_symbols::get_method_pointer("Unity.TextMeshPro.dll", "TMPro", "TMP_Text",
                                               "get_text", 0));

    tmp_text_set_text = reinterpret_cast<void (*)(Il2CppObject *, Il2CppString *)>(
            il2cpp_symbols::get_method_pointer("Unity.TextMeshPro.dll", "TMPro", "TMP_Text",
                                               "set_text", 1));

    auto TextMeshPro_Awake_addr = il2cpp_symbols::get_method_pointer("Unity.TextMeshPro.dll",
                                                                     "TMPro", "TextMeshPro",
                                                                     "Awake", 0);

    auto TMP_Settings_get_instance_addr = il2cpp_symbols::get_method_pointer(
            "Unity.TextMeshPro.dll", "TMPro", "TMP_Settings", "get_instance", -1);

    auto set_fps_addr = il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll",
                                                           "UnityEngine", "Application",
                                                           "set_targetFrameRate", 1);

    auto set_anti_aliasing_addr = il2cpp_resolve_icall(
            "UnityEngine.QualitySettings::set_antiAliasing(System.Int32)");

    auto Light_set_shadowResolution_addr = il2cpp_resolve_icall(
            "UnityEngine.Light::set_shadowResolution(UnityEngine.Light,UnityEngine.Rendering.LightShadowResolution)");

    display_get_main = reinterpret_cast<Il2CppObject *(*)()>(il2cpp_symbols::get_method_pointer(
            "UnityEngine.CoreModule.dll", "UnityEngine", "Display", "get_main", -1));

    get_system_width = reinterpret_cast<int (*)(Il2CppObject *)>(il2cpp_symbols::get_method_pointer(
            "UnityEngine.CoreModule.dll", "UnityEngine", "Display", "get_systemWidth", 0));

    get_system_height = reinterpret_cast<int (*)(
            Il2CppObject *)>(il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll",
                                                                "UnityEngine", "Display",
                                                                "get_systemHeight", 0));

    auto set_resolution_addr = il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll",
                                                                  "UnityEngine", "Screen",
                                                                  "SetResolution", 3);

    auto GraphicSettings_Initilaize_addr = il2cpp_symbols::get_method_pointer("Assembly-CSharp.dll",
                                                                              "Priari",
                                                                              "GraphicSettings",
                                                                              "Initilaize", 0);

    auto apply_graphics_quality_addr = il2cpp_symbols::get_method_pointer("Assembly-CSharp.dll",
                                                                          "Priari",
                                                                          "GraphicSettings",
                                                                          "ApplyGraphicsQuality",
                                                                          2);

    auto CriMana_Player_SetFile_addr = il2cpp_symbols::get_method_pointer(
            "CriMw.CriWare.Runtime.dll", "CriWare.CriMana", "Player", "SetFile", 3);

    auto PriariLocalizationBuilder_UseDefaultLanguage_addr = il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari.ServerShared.Globalization", "PriariLocalizationBuilder",
            "UseDefaultLanguage", 1);

    auto TitleScene_isInServiceTerm_addr = il2cpp_symbols::get_method_pointer("Assembly-CSharp.dll",
                                                                              "Priari.Title",
                                                                              "TitleScene",
                                                                              "isInServiceTerm", 0);

    auto ResourceManager_Initialize_addr = il2cpp_symbols::get_method_pointer("Assembly-CSharp.dll",
                                                                              "Priari",
                                                                              "ResourceManager",
                                                                              "Initialize", 0);

    auto TalkTimingTrack_Setup_addr = il2cpp_symbols::get_method_pointer("Assembly-CSharp.dll",
                                                                         "Priari.Adv",
                                                                         "TalkTimingTrack", "Setup",
                                                                         0);

    auto TimeUtility_IsTermTime_addr = il2cpp_symbols::get_method_pointer("Assembly-CSharp.dll",
                                                                          "Priari", "TimeUtility",
                                                                          "IsTermTime", 3);

    auto SystemDefines_get_ConfigurationJsonUrlBase_addr = il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari", "SystemDefines", "get_ConfigurationJsonUrlBase", -1);

    auto SystemDefines_get_Language_addr = il2cpp_symbols::get_method_pointer("Assembly-CSharp.dll",
                                                                              "Priari",
                                                                              "SystemDefines",
                                                                              "get_Language", -1);

    auto SystemDefines_get_MaintenanceJsonUrl_addr = il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari", "SystemDefines", "get_MaintenanceJsonUrl", -1);

    auto SystemDefines_get_ClientVersionJsonUrl_addr = il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari", "SystemDefines", "get_ClientVersionJsonUrl", -1);

    auto SystemDefines_get_AssetResourceJsonUrl_addr = il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari", "SystemDefines", "get_AssetResourceJsonUrl", -1);

    auto SystemDefines_get_ServerEndpointJsonUrl_addr = il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari", "SystemDefines", "get_ServerEndpointJsonUrl", -1);

    auto ClientServerConfiguration_TryUpdateAssetResourceVersionAsync_addr = il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari.Configuration", "ClientServerConfiguration", "TryUpdateAssetResourceVersionAsync", 0);

    auto ManifestChecker_StartDownload_addr = il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari", "ManifestChecker", "StartDownload", 3);

    load_from_file = reinterpret_cast<Il2CppObject *(*)(
            Il2CppString *path)>(il2cpp_symbols::get_method_pointer(
            "UnityEngine.AssetBundleModule.dll", "UnityEngine", "AssetBundle", "LoadFromFile", 1));

    auto PathResolver_GetLocalPath_addr = il2cpp_symbols::get_method_pointer(
            "Cute.AssetBundle.Assembly.dll", "Cute.AssetBundle.LocalFile", "PathResolver",
            "GetLocalPath", 2);

    auto assetbundle_unload_addr = il2cpp_symbols::get_method_pointer(
            "UnityEngine.AssetBundleModule.dll", "UnityEngine", "AssetBundle", "Unload", 1);

    auto assetbundle_LoadFromFile_addr = il2cpp_symbols::get_method_pointer(
            "UnityEngine.AssetBundleModule.dll", "UnityEngine", "AssetBundle", "LoadFromFile", 1);

#define ADD_HOOK(_name_) \
    LOGI("ADD_HOOK: %s", #_name_); \
    if (_name_##_addr) DobbyHook(reinterpret_cast<void *>(_name_##_addr), reinterpret_cast<void *>(_name_##_hook), reinterpret_cast<void **>(&_name_##_orig)); \
    else LOGW("ADD_HOOK: %s_addr is null", #_name_);

#define ADD_HOOK_NEW(_name_) \
    LOGI("ADD_HOOK_NEW: %s", #_name_); \
    if (addr_##_name_) DobbyHook(reinterpret_cast<void *>(addr_##_name_), reinterpret_cast<void *>(new_##_name_), reinterpret_cast<void **>(&orig_##_name_)); \
    else LOGW("ADD_HOOK_NEW: addr_%s is null", #_name_);

    // TODO: Set language as system language
    auto currentLocalization = reinterpret_cast<Il2CppObject *(*)()>(il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari.ServerShared.Globalization", "PriariLocalization",
            "get_Current", -1))();

    auto korean = reinterpret_cast<Il2CppObject *(*)()>(il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari.ServerShared.Globalization",
            "PriariLocalizationLanguage", "get_Korean", -1))();
    reinterpret_cast<void (*)(Il2CppObject *, Il2CppObject *)>(il2cpp_symbols::get_method_pointer(
            "Assembly-CSharp.dll", "Priari.ServerShared.Globalization", "PriariLocalization",
            "SetDefaultLanguage", 1))(currentLocalization, korean);

    // ADD_HOOK(ClientServerConfiguration_TryUpdateAssetResourceVersionAsync)

    // ADD_HOOK(ManifestChecker_StartDownload)

    ADD_HOOK(SystemDefines_get_Language)

    if (!g_configuration_json_url_base.empty()) {
        ADD_HOOK(SystemDefines_get_ServerEndpointJsonUrl)
        ADD_HOOK(SystemDefines_get_AssetResourceJsonUrl)
        ADD_HOOK(SystemDefines_get_ClientVersionJsonUrl)
        ADD_HOOK(SystemDefines_get_MaintenanceJsonUrl)
        ADD_HOOK(SystemDefines_get_ConfigurationJsonUrlBase)
    }

    ADD_HOOK(TimeUtility_IsTermTime)

    ADD_HOOK(TitleScene_isInServiceTerm)

    ADD_HOOK(ResourceManager_Initialize)

    ADD_HOOK(TalkTimingTrack_Setup)

    ADD_HOOK(PriariLocalizationBuilder_UseDefaultLanguage)

    ADD_HOOK(CriMana_Player_SetFile)

    ADD_HOOK(Light_set_shadowResolution)

    ADD_HOOK(PathResolver_GetLocalPath)

    ADD_HOOK(assetbundle_unload)

    ADD_HOOK(assetbundle_LoadFromFile)

    ADD_HOOK(update)

    if (g_cyspring_update_mode != -1) {
        ADD_HOOK(CySpringController_set_UpdateMode)
        ADD_HOOK(CySpringController_get_UpdateMode)
        ADD_HOOK(CySpringModelController_set_SpringUpdateMode)
        ADD_HOOK(CySpringModelController_get_SpringUpdateMode)
    }

    if (g_ui_use_system_resolution) {
        ADD_HOOK(set_resolution)
    }

    ADD_HOOK(TMP_Text_set_text)

    if (g_replace_to_custom_font) {
        ADD_HOOK(TextMeshPro_Awake)
        ADD_HOOK(TMP_Settings_get_instance)
    }

    if (g_max_fps > -1) {
        ADD_HOOK(set_fps)
    }

    if (g_graphics_quality != -1) {
        ADD_HOOK(GraphicSettings_Initilaize)
        ADD_HOOK(apply_graphics_quality)
        auto graphicSettings = GetSingletonInstance(
                il2cpp_symbols::get_class("Assembly-CSharp.dll", "Priari", "GraphicSettings"));
        if (graphicSettings) {
            apply_graphics_quality_hook(graphicSettings, 0, false);
        }
    }

    if (g_anti_aliasing != -1) {
        ADD_HOOK(set_anti_aliasing)
    }

    auto language = reinterpret_cast<Il2CppObject *(*)(
            Il2CppObject *)>(il2cpp_symbols::get_method_pointer("Assembly-CSharp.dll",
                                                                "Priari.ServerShared.Globalization",
                                                                "PriariLocalization",
                                                                "get_DefaultLanguage", 0))(
            currentLocalization);
    auto displayName = reinterpret_cast<Il2CppString *(*)(
            Il2CppObject *)>(il2cpp_class_get_method_from_name(language->klass, "get_DisplayName",
                                                               0)->methodPointer)(language);

    LOGD("Display Name: %s", localify::u16_u8(displayName->start_char).data());

    LOGI("Unity Version: %s", GetUnityVersion().data());
}

void il2cpp_load_assetbundle() {
    if (!assets && !g_font_assetbundle_path.empty() && g_replace_to_custom_font) {
        auto assetbundlePath = localify::u8_u16(g_font_assetbundle_path);
        if (!assetbundlePath.starts_with(u"/")) {
            assetbundlePath.insert(0, u"/sdcard/Android/data/"s.append(
                    localify::u8_u16(Game::GetCurrentPackageName())).append(u"/"));
        }
        assets = load_from_file(
                il2cpp_string_new_utf16(assetbundlePath.data(), assetbundlePath.length()));

        if (!assets && filesystem::exists(assetbundlePath)) {
            LOGW("Asset founded but not loaded. Maybe Asset BuildTarget is not for Android");
        }

        /* Load from Memory Async

        ifstream infile(localify::u16_u8(assetbundlePath).data(), ios_base::binary);

        vector<char> buffer((istreambuf_iterator<char>(infile)), istreambuf_iterator<char>());

        Il2CppArray* assetBytes = il2cpp_array_new(il2cpp_defaults.byte_class, buffer.size());

        for (int i = 0; i < buffer.size(); i++) {
            il2cpp_array_set(assetBytes, char, i, buffer[i]);
        }
        Il2CppObject* createReq = load_from_memory_async(assetBytes);

        auto get_assetBundle = reinterpret_cast<Il2CppObject *(*)(
                Il2CppObject* thisObj)>(il2cpp_symbols::get_method_pointer(
                "UnityEngine.AssetBundleModule.dll", "UnityEngine", "AssetBundleCreateRequest", "get_assetBundle",
                0));
        auto get_isDone = reinterpret_cast<Il2CppObject *(*)(
                Il2CppObject* thisObj)>(il2cpp_symbols::get_method_pointer(
                "UnityEngine.CoreModule.dll", "UnityEngine", "AsyncOperation", "get_isDone",
                0));

        thread load_thread([createReq, get_assetBundle, get_isDone]() {
            while (!get_isDone(createReq)) {}
            assets = get_assetBundle(createReq);
        });
        load_thread.detach();*/
    }

    if (assets) {
        LOGI("Asset loaded: %p", assets);
    }

    if (!replaceAssets && !g_replace_assetbundle_file_path.empty()) {
        auto assetbundlePath = localify::u8_u16(g_replace_assetbundle_file_path);
        if (!assetbundlePath.starts_with(u"/")) {
            assetbundlePath.insert(0, u"/sdcard/Android/data/"s.append(
                    localify::u8_u16(Game::GetCurrentPackageName())).append(u"/"));
        }
        replaceAssets = load_from_file(
                il2cpp_string_new_utf16(assetbundlePath.data(), assetbundlePath.length()));

        if (!replaceAssets && filesystem::exists(assetbundlePath)) {
            LOGI("Replacement AssetBundle founded but not loaded. Maybe Asset BuildTarget is not for Android");
        }
    }

    if (replaceAssets) {
        LOGI("Replacement AssetBundle loaded: %p", replaceAssets);
        auto names = get_all_asset_names(replaceAssets);
        for (int i = 0; i < names->max_length; i++) {
            auto u8Name = localify::u16_u8(
                    static_cast<Il2CppString *>(names->vector[i])->start_char);
            replaceAssetNames.emplace_back(u8Name);
        }

        auto assetbundle_load_asset_addr = il2cpp_symbols::get_method_pointer(
                "UnityEngine.AssetBundleModule.dll", "UnityEngine", "AssetBundle", "LoadAsset", 2);

        auto resources_load_addr = il2cpp_symbols::get_method_pointer("UnityEngine.CoreModule.dll",
                                                                      "UnityEngine", "Resources",
                                                                      "Load", 2);

        auto Sprite_get_texture_addr = il2cpp_resolve_icall(
                "UnityEngine.Sprite::get_texture(UnityEngine.Sprite)");

        auto Renderer_get_material_addr = il2cpp_symbols::get_method_pointer(
                "UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "get_material", 0);

        auto Renderer_get_materials_addr = il2cpp_symbols::get_method_pointer(
                "UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "get_materials", 0);

        auto Renderer_get_sharedMaterial_addr = il2cpp_symbols::get_method_pointer(
                "UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "get_sharedMaterial", 0);

        auto Renderer_get_sharedMaterials_addr = il2cpp_symbols::get_method_pointer(
                "UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "get_sharedMaterials", 0);

        auto Renderer_set_material_addr = il2cpp_symbols::get_method_pointer(
                "UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "set_material", 1);

        auto Renderer_set_materials_addr = il2cpp_symbols::get_method_pointer(
                "UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "set_materials", 1);

        auto Renderer_set_sharedMaterial_addr = il2cpp_symbols::get_method_pointer(
                "UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "set_sharedMaterial", 1);

        auto Renderer_set_sharedMaterials_addr = il2cpp_symbols::get_method_pointer(
                "UnityEngine.CoreModule.dll", "UnityEngine", "Renderer", "set_sharedMaterials", 1);

        auto Material_get_mainTexture_addr = il2cpp_symbols::get_method_pointer(
                "UnityEngine.CoreModule.dll", "UnityEngine", "Material", "get_mainTexture", 0);

        auto Material_set_mainTexture_addr = il2cpp_symbols::get_method_pointer(
                "UnityEngine.CoreModule.dll", "UnityEngine", "Material", "set_mainTexture", 1);

        auto Material_SetTextureI4_addr = il2cpp_symbols::find_method("UnityEngine.CoreModule.dll",
                                                                      "UnityEngine", "Material",
                                                                      [](const MethodInfo *method) {
                                                                          return method->name ==
                                                                                 "SetTexture"s &&
                                                                                 method->parameters->parameter_type->type ==
                                                                                 IL2CPP_TYPE_I4;
                                                                      });

        ADD_HOOK(assetbundle_load_asset)

        ADD_HOOK(resources_load)

        ADD_HOOK(Sprite_get_texture)

        ADD_HOOK(Renderer_get_material)

        ADD_HOOK(Renderer_get_materials)

        ADD_HOOK(Renderer_get_sharedMaterial)

        ADD_HOOK(Renderer_get_sharedMaterials)

        ADD_HOOK(Renderer_set_material)

        ADD_HOOK(Renderer_set_materials)

        ADD_HOOK(Renderer_set_sharedMaterial)

        ADD_HOOK(Renderer_set_sharedMaterials)

        ADD_HOOK(Material_get_mainTexture)

        ADD_HOOK(Material_set_mainTexture)

        ADD_HOOK(Material_SetTextureI4)
    }
}

void il2cpp_hook_init(void *handle) {
    LOGI("il2cpp_handle: %p", handle);
    il2cpp_handle = handle;
    init_il2cpp_api();
    if (il2cpp_domain_get_assemblies) {
        Dl_info dlInfo;
        if (dladdr(reinterpret_cast<void *>(il2cpp_domain_get_assemblies), &dlInfo)) {
            il2cpp_base = reinterpret_cast<uint64_t>(dlInfo.dli_fbase);
        } else {
            LOGW("dladdr error, using get_module_base.");
            il2cpp_base = get_module_base("libil2cpp.so");
        }
        LOGI("il2cpp_base: %" PRIx64"", il2cpp_base);
    } else {
        LOGE("Failed to initialize il2cpp api.");
        return;
    }
    auto domain = il2cpp_domain_get();
    il2cpp_thread_attach(domain);

    il2cpp_symbols::init(domain);
}

string get_application_version() {
    reinterpret_cast<void (*)()>(
            il2cpp_symbols::get_method_pointer("UnityEngine.AndroidJNIModule.dll", "UnityEngine",
                                               "AndroidJNI", "AttachCurrentThread", -1))();
    auto version = string(localify::u16_u8(reinterpret_cast<Il2CppString *(*)()>(
                                                   il2cpp_symbols::get_method_pointer(
                                                           "Assembly-CSharp.dll", "Cute.Core",
                                                           "Device", "GetAppVersion",
                                                           -1))()->start_char));
    return version;
}

void il2cpp_hook() {
    hookMethods();
}
