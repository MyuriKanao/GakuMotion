#pragma once
namespace NativeLiveRuntime
{
    inline std::map<std::string,std::string> resolvedBundles;
    inline std::optional<std::string> LookupBundle(void* database,const std::string& key)
    {
        r::Arena roots;void* text=r::String(key,roots);bool deleted=false;void* argument=&deleted;
        if (nullableLookupClass)
        {
            r::Require(g_safeBones.unbox && StageLightingRuntime::fieldSet,"native lookup value API unavailable");
            void* box=roots.Keep(r::objectNew(nullableLookupClass));
            r::Require(box!=nullptr,"native lookup value allocation failed");
            bool hasValue=true;
            StageLightingRuntime::fieldSet(box,r::Field(nullableLookupClass,"hasValue",{0x02}),&hasValue);
            StageLightingRuntime::fieldSet(box,r::Field(nullableLookupClass,"value",{0x02}),&deleted);
            argument=g_safeBones.unbox(box);r::Require(argument!=nullptr,"native lookup argument unavailable");
        }
        void* args[]{text,argument};void* item=roots.Keep(r::Call(lookupBundle,database,args));
        if (!item) return {};
        r::Require(r::Is(item,itemClass),"native bundle item type mismatch");
        void* name=r::FieldObject(item,itemNameField,roots);
        auto actual=r::TryText(name,GakuMotion::native_live::kMaxBundleNameLength);
        if (!actual || actual->empty() || actual->find('\0')!=std::string::npos) return {};
        return actual;
    }
    inline std::optional<std::string> ResolveBundle(void* database,const std::string& raw,const char* prefix,
        std::map<std::string,std::string>& mapping)
    {
        const auto canonical=raw.starts_with(prefix)?raw:std::string(prefix)+raw;
        if (!GakuMotion::native_live::ResourceName(canonical,prefix)) return {};
        auto actual=LookupBundle(database,canonical);
        if (!actual) return {};
        mapping[canonical]=*actual;return canonical;
    }
    inline std::string BundleKey(const std::string& canonical)
    { const auto found=resolvedBundles.find(canonical);return found==resolvedBundles.end()?canonical:found->second; }
}
