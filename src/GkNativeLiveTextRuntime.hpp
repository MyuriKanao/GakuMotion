#pragma once
// Included inside the reflection namespace after Require and string metadata.
    inline std::optional<std::string> TryText(void* text, size_t limit = 512)
    {
        if (!text || !managedStringClass || !stringLength || !stringChars || !g_faceRuntime.objectClass ||
            g_faceRuntime.objectClass(text) != managedStringClass) return {};
        const int32_t length=stringLength(text);
        if (length<0 || static_cast<size_t>(length)>limit) return {};
        const char16_t* chars=stringChars(text);if (!chars && length) return {};
        std::string result;
        for (int32_t i=0;i<length;++i)
        {
            uint32_t c=chars[i];
            if (c>=0xd800 && c<=0xdbff)
            {
                if (++i>=length || chars[i]<0xdc00 || chars[i]>0xdfff) return {};
                c=0x10000+((c-0xd800)<<10)+(chars[i]-0xdc00);
            }
            else if (c>=0xdc00 && c<=0xdfff) return {};
            if (c<0x80) result+=static_cast<char>(c);
            else if (c<0x800) { result+=static_cast<char>(0xc0|(c>>6));result+=static_cast<char>(0x80|(c&63)); }
            else if (c<0x10000) { result+=static_cast<char>(0xe0|(c>>12));result+=static_cast<char>(0x80|((c>>6)&63));result+=static_cast<char>(0x80|(c&63)); }
            else { result+=static_cast<char>(0xf0|(c>>18));result+=static_cast<char>(0x80|((c>>12)&63));result+=static_cast<char>(0x80|((c>>6)&63));result+=static_cast<char>(0x80|(c&63)); }
        }
        return result;
    }
    // Diagnostic only: "namespace.name" of a managed object's class.
    inline std::string ClassOf(void* value)
    {
        if (!value) return "null";
        void* actual = g_faceRuntime.objectClass ? g_faceRuntime.objectClass(value) : nullptr;
        const char* name = actual && className ? className(actual) : nullptr;
        const char* space = actual && classNamespace ? classNamespace(actual) : nullptr;
        return std::string(space ? space : "?") + "." + (name ? name : "?");
    }
    inline std::string Text(void* text, size_t limit = 512)
    {
        Require(text != nullptr, "native string result missing");
        if (!managedStringClass || !g_faceRuntime.objectClass || g_faceRuntime.objectClass(text) != managedStringClass)
            throw Failure("native string result has unexpected type " + ClassOf(text));
        Require(stringLength && stringChars, "native string API unavailable");
        if (auto result=TryText(text,limit)) return *result;
        throw Failure("native string invalid or exceeds limit (length="+std::to_string(stringLength(text))+", limit="+std::to_string(limit)+")");
    }
