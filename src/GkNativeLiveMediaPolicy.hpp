#pragma once
#include <set>
#include <string_view>
namespace GakuMotion::native_live
{
    // A source-load receipt, not a process-wide ban on movie components.
    inline bool IsOwnedMediaScene(int scene, std::string_view name, std::string_view expected,
        const std::set<int>& preexisting)
    { return scene != 0 && !expected.empty() && name == expected && !preexisting.contains(scene); }
}
