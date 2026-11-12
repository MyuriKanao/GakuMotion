#pragma once
#include "GkNativeLiveLookupRuntime.hpp"
namespace NativeLiveRuntime
{
    inline bool catalogAttempted{};
    inline std::set<std::string> bundleNames;
    inline std::string catalogPhase;

    inline void ReadCatalog()
    {
        r::Require(ProtectionOnUnityThread() && metadataReady, "native metadata not ready");
        catalogPhase = "资源目录";
        r::Arena roots;
        void* database = roots.Keep(r::Call(getDatabase));
        r::Require(r::Is(database, dataClass), "Octo catalog database type unavailable");
        std::set<std::string> available;
        size_t skippedNames{}, skippedRows{}, fallbackTitles{};
        size_t validIds{},stageMatches{},timelineMatches{};
        std::map<std::string,std::string> mapping;
        try
        {
            void* array=roots.Keep(r::Call(allBundles,database));
            for (void* text:r::Items(array,200000))
            {
                auto name=r::TryText(text,GakuMotion::native_live::kMaxBundleNameLength);
                if (name && !name->empty() && name->size()<=GakuMotion::native_live::kMaxBundleNameLength && name->find('\0')==std::string::npos)
                    available.insert(std::move(*name));
                else ++skippedNames;
            }
        }
        catch (const std::exception& problem)
        { GakuMotion::host::Warn("[GakuMotion][NativeLive] global enumeration skipped; using native per-item lookup: %s",problem.what()); }
        catalogPhase = "歌曲表";
        void* master = roots.Keep(r::Call(getMaster));
        r::Require(r::Is(master, liveMasterClass), "native live master not initialized");
        void* args[]{&sortOrder};
        void* rows = roots.Keep(r::Call(getLives, master, args));
        r::Require(r::Is(rows, listClass), "native live table unavailable");
        const int count = r::Read<int>(listCount, rows);
        r::Require(count > 0 && count <= 16384, "native live row count unavailable");
        std::map<std::string, GakuMotion::native_live::Song> pairs;
        std::set<std::string> ambiguous;
        std::map<int, std::string> variants;
        for (const char* name : {"Unknown", "TrueEnd", "A", "B", "C", "D", "E"}) variants.emplace(r::Enum(gradeClass, name), name);
        for (int i = 0; i < count; ++i)
        {
            catalogPhase = "歌曲记录 " + std::to_string(i + 1) + "/" + std::to_string(count);
            // Row-scoped roots keep even large catalogs bounded.
            r::Arena rowRoots;
            void* indexArgs[]{&i};
            void* row = rowRoots.Keep(r::Call(listItem, rows, indexArgs));
            r::Require(r::Is(row, liveClass), "native live row type changed");
            auto text = [&](const void* method, void* target) {
                return r::TryText(rowRoots.Keep(r::Call(method, target)), 512).value_or(std::string{});
            };
            const auto id = text(getSongKey, row);
            const int grade = r::Read<int>(getGrade, row);
            const auto stage = ResolveBundle(database,text(getStageKey, row),"env_3d_",mapping);
            const auto timeline = ResolveBundle(database,text(getTimelineKey, row),"tln_live_",mapping);
            const auto key = id + "@" + std::to_string(grade);
            if (GakuMotion::native_live::SongKey(key)) ++validIds;
            if (stage) ++stageMatches;
            if (timeline) ++timelineMatches;
            if (!stage || !timeline || !GakuMotion::native_live::SongKey(key)) { ++skippedRows; continue; }
            void* music = rowRoots.Keep(r::Call(getMusic, row));
            auto title = r::Is(music, musicClass) ? text(getTitle, music) : std::string{};
            if (title.empty() || title.size() > GakuMotion::native_live::kMaxTitleBytes) { title = id; ++fallbackTitles; }
            GakuMotion::native_live::Song song{key, title, *stage, *timeline, grade};
            if (variants.contains(grade)) song.variant = variants.at(grade);
            if (pairs.contains(key) && pairs.at(key) != song) ambiguous.insert(key);
            else pairs.emplace(key, std::move(song));
        }
        std::vector<GakuMotion::native_live::Song> songs;
        for (auto& [key, song] : pairs) if (!ambiguous.contains(key)) songs.push_back(std::move(song));
        catalogPhase = "资源配对";
        GakuMotion::host::Info("[GakuMotion][NativeLive] pairing rows=%d names=%zu valid_ids=%zu stages=%zu timelines=%zu pairs=%zu ambiguous=%zu skipped_names=%zu",
            count,available.size(),validIds,stageMatches,timelineMatches,songs.size(),ambiguous.size(),skippedNames);
        if (!GakuMotion::native_live::catalog.Publish(std::move(songs)))
            throw r::Failure("没有有效模板：记录 "+std::to_string(count)+" / 有效ID "+std::to_string(validIds)+
                " / 舞台 "+std::to_string(stageMatches)+" / 时间轴 "+std::to_string(timelineMatches)+" / 冲突 "+std::to_string(ambiguous.size()));
        bundleNames = std::move(available);
        resolvedBundles=std::move(mapping);
        const auto songCount = GakuMotion::native_live::catalog.Snapshot().size();
        catalogNotice = "可用原生灯光模板：" + std::to_string(songCount);
        GakuMotion::host::Info("[GakuMotion][NativeLive] catalog ready songs=%zu bundles=%zu skipped_names=%zu skipped_rows=%zu fallback_titles=%zu",
            songCount, bundleNames.size(), skippedNames, skippedRows, fallbackTitles);
    }
    inline void PollCatalog()
    {
        if (!ProtectionOnUnityThread() || !metadataReady) return;
        if (!g_sceneOwner.Valid() && !GakuMotion::native_live::refreshRequested.load()) return;
        if (catalogAttempted && !GakuMotion::native_live::refreshRequested.exchange(false)) return;
        catalogAttempted = true;
        try { ReadCatalog(); }
        catch (const std::exception& problem)
        {
            catalogNotice = "曲目目录待刷新（" + catalogPhase + "）：" + problem.what();
            GakuMotion::host::Warn("[GakuMotion][NativeLive] catalog failed phase=%s reason=%s", catalogPhase.c_str(), problem.what());
        }
    }
    inline void RequireCached(const std::string& bundle)
    {
        r::Arena roots;
        void* database=roots.Keep(r::Call(getDatabase));
        const auto key=BundleKey(bundle);
        r::Require(LookupBundle(database,key).has_value(),"native resource is not in the local catalog");
        void* text = r::String(key, roots); void* args[]{text};
        void* array = roots.Keep(r::Call(dependencies, nullptr, args));
        std::set<std::string> names{key};
        for (void* item : r::Items(array)) names.insert(r::Text(item, GakuMotion::native_live::kMaxBundleNameLength));
        void* typedArray = roots.Keep(arrayNew(stringClass, names.size()));
        r::Require(typedArray != nullptr, "local cache query array unavailable");
        int index = 0;
        for (const auto& name : names)
        {
            r::Require(LookupBundle(database,name).has_value(), "native dependency absent from catalog");
            void* value = r::String(name, roots); void* valueArgs[]{value, &index};
            r::Call(setArrayValue, typedArray, valueArgs); ++index;
        }
        void* sizeArgs[]{typedArray};
        r::Require(r::Read<int64_t>(downloadSize, nullptr, sizeArgs) == 0,
            "song or dependency is not cached locally; native lighting has not loaded it");
    }
}
