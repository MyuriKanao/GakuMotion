#pragma once

// Included after StageLoadOverride, inside the plugin's private namespace.
// No song timeline, actor, shared material or global RenderSettings is changed.
namespace StageLightingRuntime
{
    namespace lighting = GakuMotion::stage_light;
    inline lighting::Controller controller;
    inline void *lightClass{}, *beamClass{}, *colorClass{}, *colorModeClass{};
    inline const void *getColor{}, *setColor{}, *getIntensity{}, *setIntensity{};
    inline const void *getEnabled{}, *setEnabled{}, *activeInHierarchy{}, *updateBeam{};
    inline void* (*fieldByName)(void*, const char*){};
    inline void* (*fieldType)(void*){};
    inline uint32_t (*fieldFlags)(void*){};
    inline void (*fieldGet)(void*, void*, void*){};
    inline void (*fieldSet)(void*, void*, void*){};
    inline void (*staticGet)(void*, void*){};
    inline void* beamFields[6]{};
    inline bool lightReady{}, beamReady{};
    inline std::vector<StageLoader::Handle> pins;
    inline GakuMotion::injection::OwnerSnapshot owner;

    template<class T> bool Read(const void* method, void* target, T& value)
    {
        if (!method || !g_faceRuntime.invoke || !g_safeBones.unbox) return false;
        void* exception = nullptr;
        void* box = g_faceRuntime.invoke(method, target, nullptr, &exception);
        void* data = box && !exception ? g_safeBones.unbox(box) : nullptr;
        if (!data) return false;
        std::memcpy(&value, data, sizeof(T));
        return true;
    }
    inline bool Call(const void* method, void* target, void** args = nullptr)
    {
        if (!method || !g_faceRuntime.invoke) return false;
        void* exception = nullptr;
        g_faceRuntime.invoke(method, target, args, &exception);
        return !exception;
    }
    template<class T> bool Set(const void* method, void* target, T& value)
    {
        void* args[]{&value};
        return Call(method, target, args);
    }
    inline void End()
    {
        for (auto handle : pins) StageLoader::freeHandle(handle);
        pins.clear();
    }
    inline bool Enumerate(void* klass, lighting::Kind kind, std::vector<lighting::Target>& targets)
    {
        void* nativeType = il2cpp_class_get_type(klass);
        void* type = nativeType ? il2cpp_type_get_object(nativeType) : nullptr;
        if (!type) return false;
        bool includeInactive = true;
        void* args[]{type, &includeInactive};
        void* array = StageLoader::Invoke(StageLoader::findAll, nullptr, args);
        auto handle = array ? StageLoader::newHandle(array, false) : nullptr;
        if (!handle) return false;
        pins.push_back(handle);
        const auto count = StageLoader::arrayLength(array);
        constexpr uintptr_t kMaxComponents = 4096;
        if (count > kMaxComponents) return false;
        auto items = reinterpret_cast<void**>(static_cast<char*>(array) + 32);
        for (uintptr_t i = 0; i < count; ++i)
        {
            if (!items[i]) continue;
            void* go = StageLoader::Invoke(StageLoader::componentGo, items[i]);
            lighting::Identity id{kind};
            bool active = false;
            if (!go || !Read(StageLoader::instanceId, items[i], id.component) ||
                !Read(StageLoader::instanceId, go, id.gameObject) || !Read(StageLoader::goScene, go, id.scene) ||
                !Read(activeInHierarchy, go, active)) return false;
            if (id.component && id.gameObject && id.scene) targets.push_back({id, items[i], active});
        }
        return true;
    }
    inline bool Begin(std::vector<lighting::Target>& targets)
    {
        if (!ProtectionOnUnityThread() || !pins.empty() || !StageLoader::findAll ||
            !StageLoader::componentGo || !StageLoader::goScene || !StageLoader::instanceId ||
            !StageLoader::newHandle || !StageLoader::freeHandle || !StageLoader::arrayLength ||
            !il2cpp_class_get_type || !il2cpp_type_get_object || !activeInHierarchy) return false;
        if ((!lightReady || Enumerate(lightClass, lighting::Kind::Light, targets)) &&
            (!beamReady || Enumerate(beamClass, lighting::Kind::Beam, targets))) return true;
        End();
        targets.clear();
        return false;
    }
    inline bool ReadValues(const lighting::Target& target, lighting::Values& value)
    {
        if (!Read(getEnabled, target.token, value.enabled)) return false;
        if (target.id.kind == lighting::Kind::Light)
        {
            if (!Read(getColor, target.token, value.color) || !Read(getIntensity, target.token, value.intensity)) return false;
        }
        else
        {
            fieldGet(target.token, beamFields[0], &value.color);
            fieldGet(target.token, beamFields[1], &value.intensity);
            fieldGet(target.token, beamFields[2], &value.outside);
            fieldGet(target.token, beamFields[3], &value.colorFromLight);
            fieldGet(target.token, beamFields[4], &value.intensityFromLight);
            fieldGet(target.token, beamFields[5], &value.colorMode);
        }
        for (float component : value.color) if (!std::isfinite(component)) return false;
        return std::isfinite(value.intensity) && value.intensity >= 0 &&
            std::isfinite(value.outside) && value.outside >= 0;
    }
    inline bool Near(float a, float b)
    { return std::fabs(a - b) <= 1e-4f * (std::max)(1.0f, std::fabs(b)); }
    inline bool WriteValues(const lighting::Target& target, const lighting::Values& source)
    {
        auto value = source;
        bool ok = true;
        if (target.id.kind == lighting::Kind::Light)
        {
            ok = Set(setColor, target.token, value.color);
            ok = Set(setIntensity, target.token, value.intensity) && ok;
        }
        else
        {
            fieldSet(target.token, beamFields[0], &value.color);
            fieldSet(target.token, beamFields[1], &value.intensity);
            fieldSet(target.token, beamFields[2], &value.outside);
            fieldSet(target.token, beamFields[3], &value.colorFromLight);
            fieldSet(target.token, beamFields[4], &value.intensityFromLight);
            fieldSet(target.token, beamFields[5], &value.colorMode);
        }
        ok = Set(setEnabled, target.token, value.enabled) && ok;
        if (target.id.kind == lighting::Kind::Beam) ok = Call(updateBeam, target.token) && ok;
        lighting::Values actual;
        if (!ReadValues(target, actual)) return false;
        for (int i = 0; i < 4; ++i) ok = Near(actual.color[i], value.color[i]) && ok;
        return ok && Near(actual.intensity, value.intensity) && Near(actual.outside, value.outside) &&
            actual.enabled == value.enabled && actual.colorFromLight == value.colorFromLight &&
            actual.intensityFromLight == value.intensityFromLight && actual.colorMode == value.colorMode;
    }
    inline void* Field(const char* name, GakuMotion::ExactMethodMetadata::Type expected)
    {
        if (!beamClass || !fieldByName || !fieldType || !fieldFlags || !fieldGet || !fieldSet ||
            !StageLoader::metadata.byRef || !StageLoader::metadata.typeCode || !StageLoader::metadata.classFromType) return nullptr;
        void* field = fieldByName(beamClass, name);
        return field && !(fieldFlags(field) & 0x10) && StageLoader::metadata.Matches(fieldType(field), expected) ? field : nullptr;
    }
    inline void Resolve(HMODULE module)
    {
        using Type = GakuMotion::ExactMethodMetadata::Type;
        auto& metadata = StageLoader::metadata;
        auto unity = [](const char* name) { return FindClass("UnityEngine.CoreModule.dll", "UnityEngine", name); };
        lightClass = unity("Light"); colorClass = unity("Color");
        void* behaviour = unity("Behaviour");
        constexpr const char* beamAssembly = "quaunity-volumetriclightbeam.Runtime.dll";
        beamClass = FindClass(beamAssembly, "VLB", "VolumetricLightBeamSD");
        colorModeClass = FindClass(beamAssembly, "VLB", "ColorMode");
#define LIGHT_EXPORT(member, symbol) member = reinterpret_cast<decltype(member)>(GetProcAddress(module, symbol))
        LIGHT_EXPORT(fieldByName, "il2cpp_class_get_field_from_name");
        LIGHT_EXPORT(fieldType, "il2cpp_field_get_type");
        LIGHT_EXPORT(fieldFlags, "il2cpp_field_get_flags");
        LIGHT_EXPORT(fieldGet, "il2cpp_field_get_value");
        LIGHT_EXPORT(fieldSet, "il2cpp_field_set_value");
        LIGHT_EXPORT(staticGet, "il2cpp_field_static_get_value");
#undef LIGHT_EXPORT
        uint32_t alignment = 0;
        const bool colorLayout = colorClass && StageLoader::valueSize &&
            StageLoader::valueSize(colorClass, &alignment) == sizeof(lighting::Color) && alignment == alignof(lighting::Color);
        auto find = [&](void* klass, const char* name, Type result, std::initializer_list<Type> args)
        { return metadata.Find(klass, name, false, result, args); };
        getEnabled = find(behaviour, "get_enabled", {0x02}, {});
        setEnabled = find(behaviour, "set_enabled", {0x01}, {{0x02}});
        activeInHierarchy = find(StageLoader::goClass, "get_activeInHierarchy", {0x02}, {});
        getColor = colorLayout ? find(lightClass, "get_color", {0x11, colorClass}, {}) : nullptr;
        setColor = colorLayout ? find(lightClass, "set_color", {0x01}, {{0x11, colorClass}}) : nullptr;
        getIntensity = find(lightClass, "get_intensity", {0x0c}, {});
        setIntensity = find(lightClass, "set_intensity", {0x01}, {{0x0c}});
        const bool lightBehaviour = behaviour && lightClass && StageLoader::assignable && StageLoader::assignable(behaviour, lightClass);
        const bool beamBehaviour = behaviour && beamClass && StageLoader::assignable && StageLoader::assignable(behaviour, beamClass);
        lightReady = lightBehaviour && colorLayout && getEnabled && setEnabled && getColor && setColor && getIntensity && setIntensity;
        beamFields[0] = colorLayout ? Field("color", {0x11, colorClass}) : nullptr;
        beamFields[1] = Field("intensityInside", {0x0c});
        beamFields[2] = Field("intensityOutside", {0x0c});
        beamFields[3] = Field("colorFromLight", {0x02});
        beamFields[4] = Field("intensityFromLight", {0x02});
        int flat = -1;
        void* flatField = colorModeClass && fieldByName ? fieldByName(colorModeClass, "Flat") : nullptr;
        const bool enumLayout = colorModeClass && StageLoader::valueSize &&
            StageLoader::valueSize(colorModeClass, &alignment) == sizeof(int) && alignment == alignof(int);
        if (enumLayout && flatField && staticGet && fieldFlags && fieldType && metadata.byRef &&
            metadata.typeCode && metadata.classFromType && (fieldFlags(flatField) & 0x40) &&
            metadata.Matches(fieldType(flatField), {0x11, colorModeClass})) staticGet(flatField, &flat);
        beamFields[5] = enumLayout && flat == 0 ? Field("colorMode", {0x11, colorModeClass}) : nullptr;
        updateBeam = find(beamClass, "UpdateAfterManualPropertyChange", {0x01}, {});
        beamReady = beamBehaviour && colorLayout && getEnabled && setEnabled && updateBeam;
        for (void* field : beamFields) beamReady = beamReady && field;
        controller.api = {Begin, End, ReadValues, WriteValues};
        GakuMotion::host::Info("[GakuMotion][StageLight] metadata ready lights=%d beams=%d (Photography receipt only; visual acceptance pending)",
            lightReady, beamReady);
    }
    inline bool Restore()
    {
        if (controller.HasChanges() && !ProtectionOnUnityThread()) return false;
        const bool ok = controller.Restore();
        if (ok) owner = {};
        return ok;
    }
    inline int AdmittedScene()
    {
        const auto* stage = GakuMotion::stage_load::FindStage(SceneOwnerRuntime::cachedActiveScene);
        return g_runtimeReady.load(std::memory_order_acquire) && g_activeSceneVerified &&
            stage && stage->concertLighting && GakuMotion::stage_load::receipt.Context(
                SceneOwnerRuntime::cachedActiveScene, g_verifiedOwnerSceneName, g_sceneOwner) ==
                    GakuMotion::injection::Context::Photography ? g_verifiedActiveSceneHandle : 0;
    }
    inline void PollRestoration(bool performanceAllowed)
    {
        // Host ticks continue even if an unloaded stage has no actor callbacks.
        if (controller.HasChanges() && (!performanceAllowed || !AdmittedScene() || !owner.SameOwner(g_sceneOwner))) Restore();
    }
    inline bool Update(const lighting::Settings& settings, double seconds, std::string& notice)
    {
        if (!ProtectionOnUnityThread()) return false;
        const int scene = AdmittedScene();
        if (owner.Valid() && !owner.SameOwner(g_sceneOwner) && !Restore())
        { notice = "灯光原状态恢复待重试"; return false; }
        if (scene) owner = g_sceneOwner;
        const bool ok = controller.Update(settings, seconds, scene);
        if (!ok) notice = "灯光访问或恢复未完成，保留原状态并等待重试";
        else if (settings.mode == lighting::Mode::Native) notice = "原生灯光（未接管）";
        else if (!scene) notice = "灯光待机：请载入通用 Live、初星或试镜舞台";
        else if (!lightReady && !beamReady) notice = "当前客户端灯光接口未匹配，保留原生灯光";
        else notice = "已驱动真实灯 " + std::to_string(controller.lights) + " / 体积光束 " +
            std::to_string(controller.beams) + "（程序化效果，非歌曲原生灯光时间轴）";
        return ok;
    }
}
