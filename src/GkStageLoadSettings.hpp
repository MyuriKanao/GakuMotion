#pragma once

#include "GkStageLoadPolicy.hpp"
#include "GkStageLighting.hpp"
#include "GkJsonLite.hpp"
#include "GkMotionData.hpp"
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace GakuMotion::stage_load
{
    // Persist only desired native-load selection. Receipts and live scene
    // identities belong to this process and must never survive a restart.
    struct Settings
    {
        std::string key;
        stage_light::Settings lighting;

        bool Load(const std::string& path, std::string& error)
        {
            jsonlite::Value root;
            if (!jsonlite::ParseFile(path, root, error)) return false;
            const auto* version = root.Find("version");
            const auto* target = root.Find("load_replacement_key");
            if (!root.IsObject() || !version || !version->IsNumber() || version->numberValue != 1 ||
                !target || !target->IsString() || (!target->stringValue.empty() && !IsSavedTarget(target->stringValue)))
            {
                error = "Invalid photography stage settings (version/load_replacement_key)";
                return false;
            }
            stage_light::Settings loadedLighting;
            if (const auto* saved = root.Find("live_lighting"))
            {
                const char* names[]{"mode", "palette", "strength", "bpm", "speed"};
                for (const char* name : names)
                {
                    const auto* value = saved->Find(name);
                    if (!saved->IsObject() || !value || !value->IsNumber() || !std::isfinite(value->numberValue))
                    { error = "Invalid live lighting settings"; return false; }
                }
                const double mode = saved->Find("mode")->numberValue, palette = saved->Find("palette")->numberValue;
                if (mode < 0 || mode > 5 || std::floor(mode) != mode || palette < 0 || palette > 2 || std::floor(palette) != palette)
                { error = "Invalid live lighting mode/palette"; return false; }
                loadedLighting = {static_cast<stage_light::Mode>(mode), static_cast<int>(palette),
                    saved->Find("strength")->AsFloat(1), saved->Find("bpm")->AsFloat(120), saved->Find("speed")->AsFloat(1)};
                if (const auto* song = saved->Find("song_key"))
                {
                    if (!song->IsString() || (!song->stringValue.empty() && !native_live::SongKey(song->stringValue)))
                    { error = "Invalid native live song key"; return false; }
                    loadedLighting.songKey = song->stringValue;
                }
                if (const auto* offset = saved->Find("native_offset"))
                {
                    if (!offset->IsNumber() || !std::isfinite(offset->numberValue))
                    { error = "Invalid native live timeline offset"; return false; }
                    loadedLighting.nativeOffset = offset->AsFloat(0);
                }
                const std::pair<const char*, bool*> layers[]{
                    {"environment", &loadedLighting.environment}, {"monitor", &loadedLighting.monitor},
                    {"audience", &loadedLighting.audience}};
                for (const auto& [name, enabled] : layers)
                    if (const auto* value = saved->Find(name))
                    {
                        if (!value->IsBool()) { error = "Invalid native live layer setting"; return false; }
                        *enabled = value->boolValue;
                    }
                if (stage_light::Sanitize(loadedLighting) != loadedLighting)
                { error = "Live lighting settings out of range"; return false; }
            }
            key = target->stringValue;
            lighting = loadedLighting;
            error.clear();
            return true;
        }

        bool Save(const std::string& path, std::string& error) const
        {
            error.clear();
            if (path.empty() || (!key.empty() && !IsSavedTarget(key)) || stage_light::Sanitize(lighting) != lighting)
            { error = "Invalid photography stage settings"; return false; }
            const std::filesystem::path destination(MotionPathFromUtf8(path));
            std::error_code ec;
            if (!destination.parent_path().empty()) std::filesystem::create_directories(destination.parent_path(), ec);
            if (ec) { error = ec.message(); return false; }
            const std::string bytes = "{\n  \"version\": 1,\n  \"load_replacement_key\": \"" + key +
                "\",\n  \"live_lighting\": {\"mode\": " + std::to_string(static_cast<int>(lighting.mode)) +
                ", \"palette\": " + std::to_string(lighting.palette) + ", \"strength\": " + std::to_string(lighting.strength) +
                ", \"bpm\": " + std::to_string(lighting.bpm) + ", \"speed\": " + std::to_string(lighting.speed) +
                ", \"song_key\": \"" + lighting.songKey + "\", \"native_offset\": " + std::to_string(lighting.nativeOffset) +
                ", \"environment\": " + (lighting.environment ? "true" : "false") +
                ", \"monitor\": " + (lighting.monitor ? "true" : "false") +
                ", \"audience\": " + (lighting.audience ? "true" : "false") + "}\n}\n";
            static std::atomic<uint64_t> sequence{0};
            std::filesystem::path temp = destination;
            temp += "." + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "." +
                std::to_string(++sequence) + ".tmp";
            auto cleanup = [&] { std::error_code ignored; std::filesystem::remove(temp, ignored); };
#ifdef _WIN32
            HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (file == INVALID_HANDLE_VALUE)
            { error = std::error_code(GetLastError(), std::system_category()).message(); return false; }
            DWORD written = 0;
            bool ok = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size();
            if (ok) ok = FlushFileBuffers(file) != 0;
            if (!ok) error = std::error_code(GetLastError(), std::system_category()).message();
            if (!CloseHandle(file) && ok)
            { error = std::error_code(GetLastError(), std::system_category()).message(); ok = false; }
            if (!ok) { cleanup(); return false; }
#else
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            out << bytes;
            out.close();
            if (!out) { error = "Cannot write photography stage settings"; cleanup(); return false; }
#endif
            // Keep the previous bytes recoverable. Never remove the current
            // destination first: a locked/failed replacement leaves it intact.
            const bool exists = std::filesystem::exists(destination, ec);
            if (ec) { error = ec.message(); cleanup(); return false; }
            if (exists)
            {
                auto backup = destination;
                backup += ".bak";
                std::filesystem::copy_file(destination, backup, std::filesystem::copy_options::overwrite_existing, ec);
                if (ec) { error = ec.message(); cleanup(); return false; }
            }
#ifdef _WIN32
            if (!MoveFileExW(temp.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            { error = std::error_code(GetLastError(), std::system_category()).message(); cleanup(); return false; }
#else
            std::filesystem::rename(temp, destination, ec);
            if (ec) { error = ec.message(); cleanup(); return false; }
#endif
            return true;
        }
    };
}
