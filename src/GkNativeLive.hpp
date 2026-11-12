#pragma once
#include "GkInjectionPolicy.hpp"
#include "GkTempo.hpp"
#include "GkNativeLiveComposition.hpp"
#include "GkNativeLiveMediaPolicy.hpp"
#include <algorithm>
#include <cmath>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <utility>
#include <atomic>

namespace GakuMotion::native_live
{
    inline constexpr size_t kMaxBundleNameLength = 4096, kMaxTitleBytes = 512;
    inline constexpr const char* kStageOriginRoot = "GakuMotion.LiveOrigin";
    struct Song
    {
        std::string key, title, stage, timeline;
        int grade{};
        std::string variant;
        bool operator==(const Song&) const = default;
    };
    inline bool ResourceName(std::string_view name, std::string_view prefix)
    {
        return name.starts_with(prefix) && name.size() > prefix.size() && name.size() <= 160 &&
            std::all_of(name.begin(), name.end(), [](unsigned char c) {
                return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '_' || c == '-'; });
    }
    inline bool SongKey(std::string_view key)
    {
        const auto separator = key.rfind('@');
        return separator != std::string_view::npos && separator + 2 == key.size() &&
            key.back() >= '0' && key.back() <= '9' && ResourceName(key.substr(0, separator), "music-");
    }
    // Candidates are accepted only when present in the client's own catalog.
    // Never manufacture a stage/timeline key from a title (including Agitato).
    inline std::optional<std::string> BundleName(std::string_view raw, std::string_view prefix,
        const std::set<std::string>& available)
    {
        std::set<std::string> matches;
        for (const auto& name : {std::string(raw), std::string(prefix) + std::string(raw)})
            if (ResourceName(name, prefix) && available.contains(name)) matches.insert(name);
        return matches.size() == 1 ? std::optional(*matches.begin()) : std::nullopt;
    }
    // Copies a managed reference array's element range. Parentheses are
    // required: braces select vector's initializer_list constructor and yield
    // the two range pointers themselves as the only elements.
    inline std::vector<void*> ArrayElements(void* const* first, size_t count)
    { return std::vector<void*>(first, first + count); }
    // Scene name Unity assigns to a bundle scene path: last path segment
    // without an optional ".unity" suffix. This client's Live bundles declare
    // bare names (the container key is "tln_live_all-012"), while standard
    // builds use "Assets/.../Name.unity"; both forms are accepted.
    inline std::optional<std::string> SceneNameFromPath(std::string_view path)
    {
        if (path.empty() || path.size() > 1024 || path.find('\0') != std::string_view::npos) return std::nullopt;
        const auto slash = path.find_last_of("/\\");
        auto name = slash == std::string_view::npos ? path : path.substr(slash + 1);
        constexpr std::string_view suffix = ".unity";
        if (name.size() >= suffix.size() && std::equal(suffix.begin(), suffix.end(), name.end() - suffix.size(),
            [](char a, char b) { return a == (b >= 'A' && b <= 'Z' ? b - 'A' + 'a' : b); }))
            name.remove_suffix(suffix.size());
        if (name.empty()) return std::nullopt;
        return std::string(name);
    }
    // Tempo map for BPM-driven equipment clips. Older Lives keep the
    // CampusBpmTimelineTrack beside the equipment tracks; Live packages keep
    // it only in the master timeline (the one with director-control tracks).
    // Returns the index of the other timeline whose single unmuted BPM track
    // is cloned, or -1 when the equipment timeline has its own or no unique
    // source exists (a clone that needs a tempo map then fails closed).
    struct TempoCandidate { size_t bpmTracks{}; bool master{}; };
    inline int TempoSource(size_t ownBpmTracks, const std::vector<TempoCandidate>& others)
    {
        if (ownBpmTracks) return -1;
        int master = -1, single = -1, masters = 0, singles = 0;
        for (int i = 0; i < static_cast<int>(others.size()); ++i)
        {
            if (!others[static_cast<size_t>(i)].bpmTracks) continue;
            if (others[static_cast<size_t>(i)].master) { ++masters; master = i; } else { ++singles; single = i; }
        }
        if (masters) return masters == 1 && others[static_cast<size_t>(master)].bpmTracks == 1 ? master : -1;
        return singles == 1 && others[static_cast<size_t>(single)].bpmTracks == 1 ? single : -1;
    }
    inline double SampleTime(double time, double offset, double duration)
    {
        if (!std::isfinite(time)) time = 0;
        if (!std::isfinite(offset)) offset = 0;
        if (!std::isfinite(duration) || duration <= 0) return 0;
        return std::clamp(time + offset, 0.0, duration);
    }
    class Catalog
    {
        mutable std::mutex mutex;
        std::vector<Song> songs;
    public:
        bool Publish(std::vector<Song> input)
        {
            if (input.empty() || input.size() > 4096) return false;
            std::set<std::string> keys;
            for (const auto& song : input)
                if (!SongKey(song.key) || song.title.size() > 512 || song.variant.size() > 64 || song.grade < 0 || song.grade > 9 || song.key.back() != '0' + song.grade ||
                    !ResourceName(song.stage, "env_3d_") || !ResourceName(song.timeline, "tln_live_") ||
                    !keys.insert(song.key).second) return false;
            std::sort(input.begin(), input.end(), [](const Song& a, const Song& b) { return a.key < b.key; });
            std::lock_guard lock(mutex);
            songs = std::move(input);
            return true;
        }
        std::vector<Song> Snapshot() const { std::lock_guard lock(mutex); return songs; }
        std::optional<Song> Find(std::string_view key) const
        {
            std::lock_guard lock(mutex);
            for (const auto& song : songs) if (song.key == key) return song;
            return {};
        }
        std::optional<Song> ForStage(std::string_view stage) const
        {
            std::lock_guard lock(mutex);
            for (const auto& song:songs) if (song.stage==stage && song.variant=="TrueEnd") return song;
            for (const auto& song : songs) if (song.stage==stage) return song;
            return {};
        }
        bool HasStage(std::string_view stage) const
        {
            std::lock_guard lock(mutex);
            for (const auto& song : songs) if (song.stage == stage) return true;
            return false;
        }
    };
    inline Catalog catalog;
    inline std::atomic<bool> refreshRequested{false}, retryRequested{false};

    struct Request
    {
        Song song;
        injection::OwnerSnapshot owner;
        int stageScene{};
        bool environment{true}, monitor{true}, audience{true};
        bool Same(const Request& other) const
        { return song == other.song && stageScene == other.stageScene && owner.SameOwner(other.owner) &&
            environment == other.environment && monitor == other.monitor && audience == other.audience; }
        bool Valid() const { return stageScene && owner.AllowedContext() == injection::Context::Photography; }
    };
    enum class Operation { Pending, Ready, Failed };
    enum class Phase { Idle, Loading, Ready, Releasing, Fault };
    struct Api
    {
        bool (*begin)(const Request&){};
        Operation (*poll)(){};
        bool (*sample)(double){};
        bool (*release)(){};
    };
    class Session
    {
        std::optional<Request> current;
        std::optional<Request> failed;
    public:
        Api api;
        Phase phase{Phase::Idle};
        bool Held() const { return current.has_value(); }
        void Retry() { failed.reset(); }
        bool Release()
        {
            if (!current) { phase = Phase::Idle; return true; }
            phase = Phase::Releasing;
            if (!api.release || !api.release()) return false;
            current.reset(); phase = Phase::Idle;
            return true;
        }
        bool Update(const std::optional<Request>& desired, double time)
        {
            if (!desired || !desired->Valid()) return Release();
            if (phase == Phase::Releasing || (current && !current->Same(*desired)))
            { Release(); return false; }
            if (failed && failed->Same(*desired)) { phase = Phase::Fault; return false; }
            if (!current)
            {
                current = desired; phase = Phase::Loading;
                if (!api.begin || !api.begin(*desired))
                { failed = current; Release(); phase = Held() ? Phase::Releasing : Phase::Fault; return false; }
            }
            if (phase == Phase::Loading)
            {
                const auto status = api.poll ? api.poll() : Operation::Failed;
                if (status == Operation::Pending) return true;
                if (status == Operation::Failed)
                { failed = current; Release(); phase = Held() ? Phase::Releasing : Phase::Fault; return false; }
                phase = Phase::Ready;
            }
            if (!std::isfinite(time) || time < 0) time = 0;
            if (api.sample && api.sample(time)) return true;
            failed = current; Release(); phase = Held() ? Phase::Releasing : Phase::Fault;
            return false;
        }
    };
}
