#pragma once
#include <array>
#include <string_view>

namespace GakuMotion::stage_load
{
    struct Stage
    {
        const char* key;
        const char* name;
        bool concertLighting;
    };

    // Fixed scene literals verified in the local v31 client metadata. These
    // are resource names, not evidence of installation or visual acceptance.
    inline constexpr std::array<Stage, 16> stages{{
        {"env_3d_live_all001-00-noon", "通用 Live 舞台", true},
        {"env_3d_prod_hifstage-00-00-noon", "初星舞台", true},
        {"env_3d_live_courtyard-00-00-noon", "中庭舞台（日中）", false},
        {"env_3d_live_courtyard-00-00-evening", "中庭舞台（傍晚）", false},
        {"env_3d_live_courtyard-00-01-noon", "中庭舞台（日中·变体）", false},
        {"env_3d_prod_audition-00-00-noon", "试镜舞台 01", true},
        {"env_3d_prod_audition-01-00-noon", "试镜舞台 02", true},
        {"env_3d_prod_audition-02-00-noon", "试镜舞台 03", true},
        {"env_3d_prod_audition-03-00-noon", "试镜舞台 04", true},
        {"env_3d_lesson_danceroom-00-00-noon", "舞蹈练习室", false},
        {"env_3d_lesson_visualroom-00-00-noon", "形象练习室", false},
        {"env_3d_lesson_vocalroom-00-00-noon", "声乐练习室", false},
        {"env_3d_home_classroom-00-00-morning", "教室（清晨）", false},
        {"env_3d_home_classroom-00-00-noon", "教室（日中）", false},
        {"env_3d_home_classroom-00-00-evening", "教室（傍晚）", false},
        {"env_3d_home_classroom-00-00-night", "教室（夜间）", false}
    }};

    inline const Stage* FindStage(std::string_view key) noexcept
    {
        for (const auto& stage : stages) if (key == stage.key) return &stage;
        return nullptr;
    }
}
