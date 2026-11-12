// Production catalog reader and bounded string helper; synthetic managed data.
#include "GkNativeLive.hpp"
#include <cstdio>
#include <memory>
#include <cstring>
#include <map>
namespace GakuMotion::host
{
    template<class... Args> void Info(const char*, Args...) {}
    template<class... Args> void Warn(const char*, Args...) {}
}
namespace NativeLiveRuntime
{
    struct ManagedString { void* klass; void* monitor{}; int32_t length{}; char16_t chars[8192]{}; };
    using Il2CppString = ManagedString;
    struct Object { void* klass; std::vector<void*> items; int index{}; };
    struct Row { std::string id, stage, timeline, title; int grade; };
    enum Op { Database, Bundles, Master, Lives, Count, Item, SongId, Grade, StageId, TimelineId, Music, Title, Dependencies, DownloadSize, SetArray, Last };
    inline int methods[Last]{}, types[6]{};
    inline const void *getDatabase=&methods[Database], *allBundles=&methods[Bundles], *getMaster=&methods[Master], *getLives=&methods[Lives],
        *listCount=&methods[Count], *listItem=&methods[Item], *getSongKey=&methods[SongId], *getGrade=&methods[Grade],
        *getStageKey=&methods[StageId], *getTimelineKey=&methods[TimelineId], *getMusic=&methods[Music], *getTitle=&methods[Title],
        *dependencies=&methods[Dependencies], *downloadSize=&methods[DownloadSize], *setArrayValue=&methods[SetArray];
    inline void *dataClass=&types[0], *liveMasterClass=&types[1], *listClass=&types[2], *liveClass=&types[3], *musicClass=&types[4],
        *stringClass=&types[5], *gradeClass=&types[3];
    inline bool metadataReady{true}, mainThread{true}, failCatalog{};
    inline std::string error, catalogNotice;
    inline int sortOrder{};
    inline struct { bool Valid() const { return true; } } g_sceneOwner;
    inline Object fixtureDatabase{dataClass}, fixtureMaster{liveMasterClass}, lives{listClass}, bundles{nullptr}, deps{nullptr};
    inline std::vector<Row> fixtureRows;
    inline std::vector<std::unique_ptr<Object>> objects;
    inline std::vector<std::unique_ptr<ManagedString>> strings;
    inline int64_t bytesToDownload{};
    inline size_t checkedNames{};
    inline bool ProtectionOnUnityThread() { return mainThread; }
    inline struct { void* (*objectClass)(void*) = [](void* value) { return *static_cast<void**>(value); }; } g_faceRuntime;
    inline void* MakeText(const std::string& text)
    {
        auto value=std::make_unique<ManagedString>(); value->klass=stringClass; value->length=static_cast<int32_t>(text.size());
        for (size_t i=0; i<text.size() && i<8192; ++i) value->chars[i]=static_cast<unsigned char>(text[i]);
        auto result=value.get(); strings.push_back(std::move(value)); return result;
    }
    inline std::string Il2CppStringToUtf8(void* text)
    {
        auto value=static_cast<ManagedString*>(text); std::string result;
        for (int32_t i=0; i<value->length; ++i)
            if (value->chars[i] < 128) result += static_cast<char>(value->chars[i]); else result += "\xe3\x81\x82";
        return result;
    }
    namespace reflection
    {
        struct Failure:std::runtime_error { using std::runtime_error::runtime_error; };
        inline void Require(bool ok,const char* message) { if (!ok) throw Failure(message); }
        inline void* managedStringClass=stringClass;
#include "GkNativeLiveTextRuntime.hpp"
        struct Arena { void* Keep(void* value) { return value; } };
        inline bool Is(void* value,void* klass) { return value && *static_cast<void**>(value)==klass; }
        inline std::vector<void*> Items(void* value,size_t limit=16384)
        { Require(value!=nullptr,"missing array"); const auto& items=static_cast<Object*>(value)->items; Require(items.size()<=limit,"array bound"); return items; }
        inline int Enum(void*,const char* name)
        {
            const char* names[]{"Unknown","TrueEnd","A","B","C","D","E"};
            for (int i=0;i<7;++i) if (!std::strcmp(names[i],name)) return i;
            throw Failure("unexpected enum");
        }
        inline void* String(const std::string& value,Arena&) { return MakeText(value); }
        inline void* Call(const void* method,void* target=nullptr,void** args=nullptr)
        {
            if (method==getDatabase) return &fixtureDatabase;
            if (method==allBundles) { Require(!failCatalog,"injected catalog failure"); return &bundles; }
            if (method==getMaster) return &fixtureMaster;
            if (method==getLives) return &lives;
            if (method==listItem) return lives.items.at(*static_cast<int*>(args[0]));
            if (method==dependencies) return &deps;
            if (method==setArrayValue)
            { static_cast<Object*>(target)->items.at(*static_cast<int*>(args[1]))=args[0]; return nullptr; }
            auto value=static_cast<Object*>(target);
            if (method==getMusic) { auto music=std::make_unique<Object>(Object{musicClass,{},value->index}); auto result=music.get();objects.push_back(std::move(music));return result; }
            const auto& row=fixtureRows.at(value->index);
            if (method==getSongKey) return MakeText(row.id);
            if (method==getStageKey) return MakeText(row.stage);
            if (method==getTimelineKey) return MakeText(row.timeline);
            if (method==getTitle) return MakeText(row.title);
            throw Failure("unexpected invocation");
        }
        template<class T> T Read(const void* method,void* target=nullptr,void** args=nullptr)
        {
            if (method==listCount) return static_cast<T>(lives.items.size());
            if (method==getGrade) return static_cast<T>(fixtureRows.at(static_cast<Object*>(target)->index).grade);
            if (method==downloadSize) { checkedNames=static_cast<Object*>(args[0])->items.size(); return static_cast<T>(bytesToDownload); }
            throw Failure("unexpected value invocation");
        }
    }
    namespace r=reflection;
    inline void* NewArray(void*,uintptr_t count)
    { auto value=std::make_unique<Object>(Object{nullptr,std::vector<void*>(count)});auto result=value.get();objects.push_back(std::move(value));return result; }
    inline auto arrayNew=&NewArray;
}
#include "GkNativeLiveCatalogRuntime.hpp"
namespace
{
    int passed{},failed{};
    void Check(bool ok,const char* message)
    { if (ok) ++passed; else { ++failed; std::printf("FAIL: %s\n",message); } }
    template<class F> bool Throws(F run) { try { run();return false; } catch (const std::exception&) { return true; } }
}
int main()
{
    using namespace NativeLiveRuntime;
    Check(r::TryText(MakeText("short"),5)=="short","exact string length limit accepted");
    Check(!r::TryText(MakeText("sixsix"),5),"over-limit optional text skipped without throwing");
    Check(!r::TryText(nullptr),"nullable text does not abort catalog");
    Object wrong{dataClass}; Check(!r::TryText(&wrong),"non-string item rejected before string layout access");
    auto negative=static_cast<ManagedString*>(MakeText("x"));negative->length=-1;
    Check(!r::TryText(negative),"negative length rejected");
    Check(Throws([&]{r::Text(MakeText("toolong"),3);}),"required strings retain strict bounds");
    const std::string longDependency(400,'d');
    const std::string nulName("bad\0suffix",10);
    bundles.items={MakeText("env_3d_fixture"),MakeText("tln_live_fixture"),MakeText(longDependency),nullptr,&wrong,MakeText(std::string(4097,'x')),MakeText(nulName)};
    fixtureRows={{"music-catalog-a","fixture","fixture","Synthetic Agitato",1},
          {"music-catalog-b","fixture","fixture",std::string(600,'t'),2},
          {"music-catalog-c","fixture","fixture",std::string(200,static_cast<char>(0x81)),3},
          {"music-catalog-bad","fixture",std::string(600,'x'),"bad",4},
          {"music-catalog-missing","missing","missing","missing",5}};
    for (int i=0;i<static_cast<int>(fixtureRows.size());++i)
    { auto row=std::make_unique<Object>(Object{liveClass,{},i});lives.items.push_back(row.get());objects.push_back(std::move(row)); }
    ReadCatalog();
    auto snapshot=GakuMotion::native_live::catalog.Snapshot();
    Check(snapshot.size()==3,"one long/invalid global name and incomplete row never hide all valid songs");
    Check(bundleNames.contains(longDependency),"legitimate bundle name longer than old 256 limit retained");
    Check(!bundleNames.contains(nulName),"embedded NUL names never reach the native C-string allocator");
    auto song=GakuMotion::native_live::catalog.Find("music-catalog-a@1");
    Check(song && song->title=="Synthetic Agitato" && song->variant=="TrueEnd" && song->stage=="env_3d_fixture","real reader publishes valid title/version/stage pair");
    Check(GakuMotion::native_live::catalog.Find("music-catalog-b@2")->title=="music-catalog-b","overlong display title falls back to ID, not full-catalog failure");
    Check(GakuMotion::native_live::catalog.Find("music-catalog-c@3")->title=="music-catalog-c","UTF-8 byte bound applied after UTF-16 decoding");
    Check(!GakuMotion::native_live::catalog.Find("music-catalog-bad@4"),"overlong resource field skips only its own row");
    deps.items={MakeText(longDependency)};RequireCached("tln_live_fixture");
    Check(checkedNames==2,"local cache check accepts the same long dependency retained by reader");
    bytesToDownload=1;Check(Throws([]{RequireCached("tln_live_fixture");}),"uncached dependency still fails closed");bytesToDownload=0;
    failCatalog=true;GakuMotion::native_live::refreshRequested.store(true);catalogAttempted=true;PollCatalog();
    Check(GakuMotion::native_live::catalog.Snapshot()==snapshot,"failed refresh preserves prior valid catalog");
    Check(catalogNotice.find("资源目录")!=std::string::npos && catalogNotice.find("injected")!=std::string::npos,"refresh error identifies failing reader phase");
    failCatalog=false;GakuMotion::native_live::refreshRequested.store(true);PollCatalog();
    Check(catalogNotice.find("本地原生 Live 曲目：3")!=std::string::npos,"explicit refresh recovers catalog and ready notice");
    std::printf("Native Live catalog adapter: %d passed, %d failed\n",passed,failed);return failed?1:0;
}
