#pragma once
// Pure planning rules for the disposable native Live source scene. No Unity
// calls: the runtime adapter supplies hierarchy, component and binding facts.
// Root layout evidence: static audit of tln_live_all-001/012/019 (2026-11-12).
#include <map>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace GakuMotion::native_live::composition
{
    enum Feature : unsigned { Rig = 1, Environment = 2, Monitor = 4, Audience = 8, All = 15 };
    // Light rigs, screens and audiences are explicit layers. A root carrying one
    // of them is shown only when every such layer it carries is enabled.
    inline constexpr unsigned Dominant = Rig | Monitor | Audience;
    inline constexpr size_t kMaxNodes = 16384;

    struct Node
    {
        int id{}, parent{};
        unsigned features{};
        // forbidden: actor, audio, virtual camera, CRI movie, Live controller,
        // overlay canvas or a camera without a render texture. camera: render-
        // texture camera or world/camera canvas, admitted only in a screen root.
        // effect: particle system whose ControlTrack is not cloned; allowed
        // only inside a light rig, otherwise it would emit unsynchronized.
        bool forbidden{}, camera{}, receiver{}, effect{};
    };
    struct RootPlan { int id{}; unsigned features{}; bool active{}; };
    struct Result
    {
        std::vector<RootPlan> roots;
        int excluded{};   // Feature roots kept hidden because they mix forbidden content.
        bool receiver{};  // The source receiver root is planned active.
    };

    // Authored crowd roots may hold only skinned meshes (Mob in all-019); the
    // names keep them under the audience switch rather than the environment.
    inline unsigned NameHint(std::string_view root)
    { return root == "Crowd" || root == "Mob" || root == "AudiencePenlight" ? Audience : 0u; }

    inline int RootOf(int id, const std::map<int, Node>& nodes)
    {
        for (size_t depth = 0; depth <= kMaxNodes; ++depth)
        {
            const auto found = nodes.find(id);
            if (found == nodes.end()) throw std::runtime_error("composition hierarchy identity absent");
            if (!found->second.parent) return id;
            id = found->second.parent;
        }
        throw std::runtime_error("composition hierarchy cycle or depth limit");
    }

    // sourceReceiver=false: the paired stage owns the receiver, so source roots
    // holding another receiver stay hidden instead of duplicating the rig.
    inline Result Plan(const std::vector<Node>& values, unsigned mask,
        const std::map<int, bool>& originalRoots = {}, bool sourceReceiver = true)
    {
        if (mask & ~All) throw std::runtime_error("composition feature mask invalid");
        if (values.size() > kMaxNodes) throw std::runtime_error("composition hierarchy exceeds bound");
        std::map<int, Node> nodes, aggregates;
        for (const auto& node : values)
            if (!node.id || node.id == node.parent || (node.features & ~All) || !nodes.emplace(node.id, node).second)
                throw std::runtime_error("composition hierarchy identity invalid or duplicated");
        for (const auto& [id, node] : nodes)
        {
            auto& root = aggregates[RootOf(id, nodes)];
            root.features |= node.features;
            root.forbidden |= node.forbidden;
            root.camera |= node.camera;
            root.receiver |= node.receiver;
            root.effect |= node.effect;
        }
        Result result;
        for (const auto& [id, root] : aggregates)
        {
            if (!root.features || (root.receiver && !sourceReceiver)) continue;
            const unsigned dominant = root.features & Dominant;
            const unsigned admitted = root.features & mask;
            if ((dominant & mask) != dominant || !admitted) continue;
            if (root.forbidden || (root.camera && !(admitted & Monitor)) || (root.effect && !(admitted & Rig)))
            {
                // Never enable actors/audio/cameras/overlays as a side effect.
                if (root.receiver) throw std::runtime_error("source receiver root mixes actor/audio/camera content");
                ++result.excluded; continue;
            }
            const auto original = originalRoots.find(id);
            const bool active = original == originalRoots.end() || original->second;
            result.roots.push_back({id, admitted, active});
            result.receiver = result.receiver || (root.receiver && active);
        }
        return result;
    }

    enum class Track { Unsupported, Light, Volume, VolumeProfile, Crowd, Penlight, MobPenlight, Animation, Activation, Video, TargetImage };
    // Admitted: inside an active planned root. Hidden: owned source object that
    // stays inactive. Foreign: outside the owned source scene or not a scene object.
    enum class Binding { None, Admitted, Hidden, Foreign };
    struct TrackFacts
    {
        Track kind{};
        bool clips{};            // Non-empty and every clip has the expected playable asset type.
        bool clipMode{true};     // AnimationTrack without an infinite/recorded clip.
        Binding binding{};
        unsigned rootFeatures{}; // Admitted features of the bound object's root.
        bool nested{};           // Has a non-group parent track.
        unsigned parent{};       // Decision of that parent track.
    };
    inline unsigned Primary(unsigned features)
    {
        return features & Rig ? Rig : features & Monitor ? Monitor : features & Audience ? Audience :
            features & Environment ? Environment : 0u;
    }
    inline unsigned Required(Track kind)
    {
        switch (kind)
        {
        case Track::Volume: case Track::VolumeProfile: return Environment;
        case Track::Crowd: case Track::Penlight: case Track::MobPenlight: return Audience;
        case Track::Video: case Track::TargetImage: return Monitor;
        default: return 0;
        }
    }
    // Returns the layer a cloned auxiliary track belongs to, or 0 to exclude it.
    // The source director stays the host, so exposed references remain valid.
    inline unsigned Decide(const TrackFacts& track, unsigned mask)
    {
        if (mask & ~All) throw std::runtime_error("composition feature mask invalid");
        if (track.kind == Track::Unsupported || !track.clips || (track.kind == Track::Animation && !track.clipMode)) return 0;
        // A child of an excluded track cannot be attached without its parent.
        if (track.nested && !(track.parent & mask)) return 0;
        if (track.binding == Binding::Hidden || track.binding == Binding::Foreign) return 0;
        const unsigned required = Required(track.kind);
        if (required && !(mask & required)) return 0;
        if (track.binding == Binding::None)
        {
            // Volume clips address their Volume through exposed references;
            // animation override layers inherit the admitted parent track.
            if (track.kind == Track::Volume) return Environment;
            return track.kind == Track::Animation && track.nested ? Primary(track.parent & mask) : 0u;
        }
        const unsigned admitted = track.rootFeatures & mask;
        if (required) return (admitted & required) ? required : 0u;
        return Primary(admitted);
    }
}
