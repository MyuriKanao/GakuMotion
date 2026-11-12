#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <map>
#include <limits>
#include <vector>
#include "GkNativeLive.hpp"

namespace GakuMotion::stage_light
{
    enum class Mode { Native, Wash, Breath, Cycle, SongTimeline, AutoBeat };
    using Rgb = std::array<float, 3>;
    inline constexpr int kMaxCustomColors = 4;
    struct Settings
    {
        Mode mode{Mode::Native};
        int palette{0};
        float strength{1.0f}, bpm{120.0f}, speed{1.0f};
        std::string songKey;
        float nativeOffset{0.0f};
        bool environment{true}, monitor{true}, audience{true};
        // Used only when palette == kCustomPalette; 2..4 colors.
        int customCount{3};
        std::array<Rgb, kMaxCustomColors> custom{{{1, 0.35f, 0.7f}, {0.3f, 0.6f, 1}, {1, 0.95f, 0.85f}, {0.6f, 0.3f, 1}}};
        bool operator==(const Settings&) const = default;
    };

    // Preset palettes. Values are this plugin's own choices, not game data.
    struct Palette { const char* name; int count; std::array<Rgb, 5> colors; };
    inline constexpr std::array<Palette, 8> palettes{{
        {"霓虹（青 / 紫 / 粉）", 3, {{{0.08f, 0.8f, 1}, {0.65f, 0.15f, 1}, {1, 0.12f, 0.55f}}}},
        {"暖色（橙 / 红 / 金）", 3, {{{1, 0.45f, 0.12f}, {1, 0.12f, 0.3f}, {1, 0.8f, 0.35f}}}},
        {"冷色（蓝 / 青绿 / 淡蓝）", 3, {{{0.3f, 0.65f, 1}, {0.25f, 1, 0.8f}, {0.7f, 0.8f, 1}}}},
        {"舞台白（白 / 暖白 / 浅金）", 3, {{{1, 1, 1}, {1, 0.92f, 0.8f}, {1, 0.85f, 0.55f}}}},
        {"粉彩（樱粉 / 薄荷 / 薰衣草 / 柠檬）", 4, {{{1, 0.7f, 0.82f}, {0.65f, 1, 0.85f}, {0.78f, 0.7f, 1}, {1, 0.96f, 0.6f}}}},
        {"日落（橙 / 玫红 / 紫）", 3, {{{1, 0.5f, 0.15f}, {1, 0.22f, 0.45f}, {0.5f, 0.2f, 0.9f}}}},
        {"全彩（红 / 黄 / 绿 / 蓝 / 紫）", 5, {{{1, 0.15f, 0.15f}, {1, 0.85f, 0.15f}, {0.2f, 1, 0.35f}, {0.2f, 0.45f, 1}, {0.75f, 0.25f, 1}}}},
        {"粉白（粉 / 白）", 2, {{{1, 0.35f, 0.65f}, {1, 1, 1}}}}
    }};
    inline constexpr int kCustomPalette = static_cast<int>(palettes.size());

    inline Settings Sanitize(Settings value)
    {
        if (value.mode < Mode::Native || value.mode > Mode::AutoBeat) value.mode = Mode::Native;
        value.palette = std::clamp(value.palette, 0, kCustomPalette);
        auto limit = [](float v, float low, float high, float fallback)
        { return std::isfinite(v) ? std::clamp(v, low, high) : fallback; };
        value.strength = limit(value.strength, 0.0f, 3.0f, 1.0f);
        value.bpm = limit(value.bpm, 30.0f, 240.0f, 120.0f);
        value.speed = limit(value.speed, 0.1f, 3.0f, 1.0f);
        value.nativeOffset = limit(value.nativeOffset, -120.0f, 120.0f, 0.0f);
        value.customCount = std::clamp(value.customCount, 2, kMaxCustomColors);
        for (auto& color : value.custom)
            for (auto& channel : color) channel = limit(channel, 0.0f, 1.0f, 1.0f);
        if (!value.songKey.empty() && !native_live::SongKey(value.songKey)) value.songKey.clear();
        return value;
    }

    using Color = std::array<float, 4>;
    struct Cue { Color color; float intensity; };
    inline Cue Sample(Settings settings, double seconds, double phase)
    {
        settings = Sanitize(settings);
        std::array<Rgb, 5> colors{};
        int count{};
        if (settings.palette == kCustomPalette)
        {
            count = settings.customCount;
            for (int i = 0; i < count; ++i) colors[i] = settings.custom[i];
        }
        else { colors = palettes[settings.palette].colors; count = palettes[settings.palette].count; }
        if (!std::isfinite(seconds) || seconds < 0) seconds = 0;
        if (!std::isfinite(phase)) phase = 0;
        phase -= std::floor(phase);
        // Eight beats per color cycle; four per breathing cycle. No strobe.
        const double beatRate = (settings.bpm / 60.0) * settings.speed;
        const double beats = std::fmod(seconds, 8.0 / beatRate) * beatRate;
        double position = phase;
        if (settings.mode == Mode::Cycle) position += std::fmod(beats / 8.0, 1.0);
        position = (position - std::floor(position)) * count;
        const int first = std::clamp(static_cast<int>(position), 0, count - 1);
        const float blend = static_cast<float>(position - first);
        Cue cue{{colors[first][0], colors[first][1], colors[first][2], 1}, settings.strength};
        for (int i = 0; i < 3; ++i)
            cue.color[i] += (colors[(first + 1) % count][i] - cue.color[i]) * blend;
        if (settings.mode == Mode::Breath || settings.mode == Mode::Cycle)
            cue.intensity *= static_cast<float>(0.7 + 0.3 * std::cos(std::fmod(beats / 4.0 + phase, 1.0) * 6.283185307179586));
        return cue;
    }

    enum class Kind { Light, Beam };
    struct Identity
    {
        Kind kind{};
        int component{}, gameObject{}, scene{};
        auto operator<=>(const Identity&) const = default;
    };
    struct Target { Identity id; void* token{}; bool active{}; };
    struct Values
    {
        Color color{};
        float intensity{}, outside{};
        bool enabled{}, colorFromLight{}, intensityFromLight{};
        int colorMode{};
        bool operator==(const Values&) const = default;
    };
    inline float ScaleIntensity(float original, float multiplier)
    {
        const double base = original > 0 ? original : 1.0;
        return static_cast<float>((std::min)(base * multiplier, static_cast<double>((std::numeric_limits<float>::max)())));
    }
    struct Api
    {
        // Tokens live only inside begin/end; originals retain primitive data only.
        bool (*begin)(std::vector<Target>&){};
        void (*end)(){};
        bool (*read)(const Target&, Values&){};
        bool (*write)(const Target&, const Values&){};
    };
    struct Access
    {
        Api& api;
        std::vector<Target> targets;
        bool ready;
        explicit Access(Api& value) : api(value), ready(api.begin && api.end && api.begin(targets)) {}
        ~Access() { if (ready) api.end(); }
    };

    class Controller
    {
        std::map<Identity, Values> originals;
        bool restorePending{};

        bool RestoreIn(const std::vector<Target>& targets, int keepScene = 0)
        {
            for (auto it = originals.begin(); it != originals.end();)
            {
                const auto found = std::find_if(targets.begin(), targets.end(),
                    [&](const Target& target) { return target.id == it->first; });
                // Successful typed enumeration proves an absent identity is gone.
                if (found == targets.end()) { it = originals.erase(it); continue; }
                if (keepScene && found->id.scene == keepScene && found->active) { ++it; continue; }
                if (api.write && api.write(*found, it->second)) it = originals.erase(it);
                else { restorePending = true; ++it; }
            }
            return !restorePending;
        }

    public:
        Api api;
        int lights{}, beams{};
        bool HasChanges() const { return !originals.empty(); }

        bool Capture(int scene)
        {
            Access access(api);
            if (!access.ready || !api.read) return false;
            for (const auto& target : access.targets)
            {
                if (target.id.scene != scene || originals.contains(target.id)) continue;
                Values value;
                if (!api.read(target, value)) return false;
                originals.emplace(target.id, value);
            }
            return true;
        }

        bool Restore()
        {
            lights = beams = 0;
            if (originals.empty()) { restorePending = false; return true; }
            Access access(api);
            if (!access.ready) { restorePending = true; return false; }
            restorePending = false;
            return RestoreIn(access.targets);
        }

        bool Update(Settings settings, double seconds, int scene)
        {
            settings = Sanitize(settings);
            lights = beams = 0;
            if (!scene || settings.mode == Mode::Native || settings.mode == Mode::SongTimeline || settings.mode == Mode::AutoBeat) return Restore();
            if (restorePending) { Restore(); return false; } // Cleanup precedes any new writes.
            Access access(api);
            if (!access.ready || !api.read || !api.write) return false;
            if (!RestoreIn(access.targets, scene)) return false;
            for (const auto& target : access.targets)
            {
                if (target.id.scene != scene || !target.active) continue;
                auto it = originals.find(target.id);
                if (it == originals.end())
                {
                    Values original;
                    if (!api.read(target, original)) { restorePending = true; break; }
                    it = originals.emplace(target.id, original).first;
                }
                Values desired = it->second;
                const double phase = static_cast<unsigned int>(target.id.gameObject) % 8 / 8.0;
                const Cue cue = Sample(settings, seconds, phase);
                desired.color = cue.color;
                desired.enabled = true;
                desired.intensity = ScaleIntensity(it->second.intensity, cue.intensity);
                if (target.id.kind == Kind::Beam)
                {
                    desired.outside = ScaleIntensity(it->second.outside, cue.intensity);
                    desired.colorFromLight = desired.intensityFromLight = false;
                    desired.colorMode = 0; // VLB.ColorMode.Flat, verified by runtime metadata.
                }
                // Record BEFORE a setter: even a failed setter can partially mutate.
                if (!api.write(target, desired)) { restorePending = true; break; }
                target.id.kind == Kind::Light ? ++lights : ++beams;
            }
            if (!restorePending) return true;
            restorePending = false;
            RestoreIn(access.targets);
            lights = beams = 0;
            return false;
        }
    };
}
