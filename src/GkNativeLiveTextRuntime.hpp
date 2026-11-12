#pragma once
// Included inside the reflection namespace after Require and string metadata.
    inline std::optional<std::string> TryText(void* text, size_t limit = 512)
    {
        if (!text || !managedStringClass || !g_faceRuntime.objectClass ||
            g_faceRuntime.objectClass(text) != managedStringClass) return {};
        const auto value = static_cast<const Il2CppString*>(text);
        if (value->length < 0 || static_cast<size_t>(value->length) > limit) return {};
        return Il2CppStringToUtf8(text);
    }
    inline std::string Text(void* text, size_t limit = 512)
    {
        Require(text != nullptr, "native string result missing");
        Require(managedStringClass && g_faceRuntime.objectClass && g_faceRuntime.objectClass(text) == managedStringClass,
            "native string result has unexpected type");
        const auto value = static_cast<const Il2CppString*>(text);
        if (value->length < 0 || static_cast<size_t>(value->length) > limit)
            throw Failure("native string exceeds limit (length=" + std::to_string(value->length) + ", limit=" + std::to_string(limit) + ")");
        return Il2CppStringToUtf8(text);
    }
