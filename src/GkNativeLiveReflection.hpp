#pragma once
// Included inside the plugin namespace, after StageLightingRuntime.
namespace NativeLiveRuntime::reflection
{
    using Type = GakuMotion::ExactMethodMetadata::Type;
    inline void* (*handleTarget)(StageLoader::Handle){};
    inline void* (*objectNew)(void*){};
    inline void* (*arrayClass)(void*, uint32_t){};
    inline void* (*nextField)(void*, void**){};
    inline const char* (*fieldName)(void*){};
    inline const char* (*className)(void*){};
    inline const char* (*classNamespace)(void*){};
    inline void* (*nestedType)(void*, void**){};
    inline void* (*parentClass)(void*){};
    inline const void* alive{};
    inline void* managedStringClass{};
    inline int32_t (*stringLength)(void*){};
    inline const char16_t* (*stringChars)(void*){};
    inline size_t (*arrayHeaderSize)(){};
    inline uint32_t (*arrayElementSize)(void*){};
    struct Failure : std::runtime_error { using std::runtime_error::runtime_error; };
    inline void Require(bool value, const char* message) { if (!value) throw Failure(message); }

    struct Root
    {
        StageLoader::Handle handle{};
        Root() = default;
        explicit Root(void* value)
        {
            Require(value && StageLoader::newHandle && StageLoader::freeHandle && handleTarget, "managed root API unavailable");
            handle = StageLoader::newHandle(value, false);
            Require(handle != nullptr, "managed root allocation failed");
        }
        Root(const Root&) = delete;
        Root& operator=(const Root&) = delete;
        Root(Root&& other) noexcept : handle(std::exchange(other.handle, nullptr)) {}
        Root& operator=(Root&& other) noexcept
        { Reset(); handle = std::exchange(other.handle, nullptr); return *this; }
        ~Root() { Reset(); }
        void Reset() noexcept
        {
            if (handle && ProtectionOnUnityThread() && StageLoader::freeHandle) StageLoader::freeHandle(handle);
            handle = nullptr;
        }
        void* Get() const
        { void* value = handle && handleTarget ? handleTarget(handle) : nullptr; Require(value != nullptr, "managed root expired"); return value; }
        explicit operator bool() const { return handle != nullptr; }
    };
    struct Arena
    {
        std::vector<Root> roots;
        void* Keep(void* value) { if (value) roots.emplace_back(value); return value; }
    };
    inline bool MetadataReady()
    {
        const auto& m = StageLoader::metadata;
        return m.methods && m.name && m.count && m.flags && m.generic && m.inflated && m.declaringClass &&
            m.parameter && m.result && m.typeCode && m.byRef && m.classFromType;
    }
    inline const void* Find(void* owner, const char* name, bool stat, Type result,
        std::initializer_list<Type> args, bool closedList = false)
    {
        if (!owner || !MetadataReady()) return nullptr;
        for (auto type : args)
            if ((type.code == 0x12 || type.code == 0x11 || type.code == 0x1d) && !type.klass) return nullptr;
        if ((result.code == 0x12 || result.code == 0x11 || result.code == 0x1d) && !result.klass) return nullptr;
        if (!closedList) return StageLoader::metadata.Find(owner, name, stat, result, args);
        // Only List<T>'s non-generic accessors may be inflated. This does not
        // relax the plugin-wide exact-method resolver or admit generic methods.
        if (!className || !classNamespace || std::strcmp(className(owner), "List`1") ||
            std::strcmp(classNamespace(owner), "System.Collections.Generic")) return nullptr;
        const auto& m = StageLoader::metadata;
        const void* match = nullptr; void* iterator = nullptr;
        while (const void* method = m.methods(owner, &iterator))
        {
            uint32_t impl{}; const auto flags = m.flags(method, &impl);
            const char* candidate = m.name(method);
            if (!candidate || std::strcmp(candidate, name) || m.generic(method) || m.declaringClass(method) != owner ||
                m.count(method) != args.size() || ((flags & 0x10) != 0) != stat || (flags & 0x400) || !m.Matches(m.result(method), result)) continue;
            size_t index{}; bool valid = true;
            for (auto type : args) valid = m.Matches(m.parameter(method, static_cast<uint32_t>(index++)), type) && valid;
            if (valid) { if (match) return nullptr; match = method; }
        }
        return match;
    }
    inline void* Class(const char* image, const char* ns, const char* name)
    { return FindClass(image, ns, name); }
    inline void* Unity(const char* name) { return Class("UnityEngine.CoreModule.dll", "UnityEngine", name); }
    inline void* Nested(void* owner, const char* name)
    {
        if (!owner || !nestedType || !className) return nullptr;
        void* iterator = nullptr; void* match = nullptr;
        while (void* type = nestedType(owner, &iterator))
            if (!std::strcmp(className(type), name)) { if (match) return nullptr; match = type; }
        return match;
    }
    inline bool Layout(void* klass, size_t size, size_t align)
    {
        uint32_t actual{};
        return klass && StageLoader::valueSize && StageLoader::valueSize(klass, &actual) == size && actual == align;
    }
    inline void* Field(void* klass, const char* name, Type expected)
    {
        using namespace StageLightingRuntime;
        if (!klass || !fieldByName || !fieldType || !fieldFlags || !fieldGet || !fieldSet || !MetadataReady()) return nullptr;
        void* field = fieldByName(klass, name);
        return field && !(fieldFlags(field) & 0x10) && StageLoader::metadata.Matches(fieldType(field), expected) ? field : nullptr;
    }
    inline void* ReferenceField(void* klass, const char* name)
    {
        using namespace StageLightingRuntime;
        void* field = klass && fieldByName ? fieldByName(klass, name) : nullptr;
        if (!field || !fieldType || !fieldFlags || !fieldGet || !fieldSet || !MetadataReady() || (fieldFlags(field) & 0x10)) return nullptr;
        void* type = fieldType(field);
        const int code = StageLoader::metadata.typeCode(type);
        return !StageLoader::metadata.byRef(type) && (code == 0x12 || code == 0x15 || code == 0x1d) ? field : nullptr;
    }
    inline int Enum(void* klass, const char* name)
    {
        using namespace StageLightingRuntime;
        Require(Layout(klass, sizeof(int), alignof(int)) && fieldByName && fieldFlags && fieldType && staticGet, "enum layout unavailable");
        void* field = fieldByName(klass, name);
        Require(field && (fieldFlags(field) & 0x40) && StageLoader::metadata.Matches(fieldType(field), {0x11, klass}), "enum constant unavailable");
        int value{}; staticGet(field, &value); return value;
    }
    inline void* Call(const void* method, void* target = nullptr, void** args = nullptr)
    {
        Require(method && g_faceRuntime.invoke, "exact native method unavailable");
        void* exception{}; void* value = g_faceRuntime.invoke(method, target, args, &exception);
        Require(!exception, "native managed invocation raised an exception"); return value;
    }
    template<class T> T Read(const void* method, void* target = nullptr, void** args = nullptr)
    {
        void* boxed = Call(method, target, args);
        void* value = boxed && g_safeBones.unbox ? g_safeBones.unbox(boxed) : nullptr;
        Require(value != nullptr, "native value result unavailable");
        T result; std::memcpy(&result, value, sizeof(T)); return result;
    }
    template<class T> void Set(const void* method, void* target, const T& value)
    { void* args[]{const_cast<T*>(&value)}; Call(method, target, args); }
    inline void* FieldObject(void* target, void* field, Arena& roots)
    {
        Require(target && field && StageLightingRuntime::fieldGet, "native reference field unavailable");
        void* value{}; StageLightingRuntime::fieldGet(target, field, &value); return roots.Keep(value);
    }
    inline void FieldObjectSet(void* target, void* field, void* value)
    {
        Require(target && field && StageLightingRuntime::fieldSet, "native reference field setter unavailable");
        // il2cpp_field_set_value takes the reference value itself for reference fields.
        StageLightingRuntime::fieldSet(target, field, value);
    }
    inline void* TypeObject(void* klass, Arena& roots)
    {
        Require(klass && il2cpp_class_get_type && il2cpp_type_get_object, "native reflection type unavailable");
        void* type = il2cpp_class_get_type(klass);
        return roots.Keep(type ? il2cpp_type_get_object(type) : nullptr);
    }
    inline bool Is(void* object, void* klass)
    { return object && klass && g_faceRuntime.objectClass && StageLoader::assignable && StageLoader::assignable(klass, g_faceRuntime.objectClass(object)); }
    inline bool Alive(void* object) { if (!object) return false; void* args[]{object}; return Read<bool>(alive, nullptr, args); }
#include "GkNativeLiveTextRuntime.hpp"
    inline void* String(const std::string& value, Arena& roots)
    { Require(StageLoader::stringNew != nullptr, "native string allocator unavailable"); return roots.Keep(StageLoader::stringNew(value.c_str())); }
    inline std::vector<void*> Items(void* array, size_t limit = 16384)
    {
        Require(array && StageLoader::arrayLength, "native array unavailable");
        const auto count = StageLoader::arrayLength(array);
        Require(count <= limit, "native array exceeds limit");
        Require(arrayHeaderSize && arrayElementSize && g_faceRuntime.objectClass, "native array layout API unavailable");
        const size_t header=arrayHeaderSize();
        Require(header>=sizeof(void*)*2 && header<=256 && header%alignof(void*)==0 &&
            arrayElementSize(g_faceRuntime.objectClass(array))==sizeof(void*), "native reference array layout mismatch");
        auto values = reinterpret_cast<void**>(static_cast<char*>(array) + header);
        return GakuMotion::native_live::ArrayElements(values, count);
    }
    inline void Resolve(HMODULE module)
    {
#define LIVE_EXPORT(member, name) member = reinterpret_cast<decltype(member)>(GetProcAddress(module, name))
        LIVE_EXPORT(handleTarget, "il2cpp_gchandle_get_target"); LIVE_EXPORT(objectNew, "il2cpp_object_new");
        LIVE_EXPORT(arrayClass, "il2cpp_array_class_get"); LIVE_EXPORT(nextField, "il2cpp_class_get_fields");
        LIVE_EXPORT(fieldName, "il2cpp_field_get_name"); LIVE_EXPORT(className, "il2cpp_class_get_name");
        LIVE_EXPORT(classNamespace, "il2cpp_class_get_namespace"); LIVE_EXPORT(nestedType, "il2cpp_class_get_nested_types");
        LIVE_EXPORT(parentClass, "il2cpp_class_get_parent");
        LIVE_EXPORT(stringLength, "il2cpp_string_length"); LIVE_EXPORT(stringChars, "il2cpp_string_chars");
        LIVE_EXPORT(arrayHeaderSize, "il2cpp_array_object_header_size"); LIVE_EXPORT(arrayElementSize, "il2cpp_array_element_size");
#undef LIVE_EXPORT
        alive = Find(Unity("Object"), "op_Implicit", true, {0x02}, {{0x12, Unity("Object")}});
        managedStringClass = Class("mscorlib.dll", "System", "String");
    }
}
