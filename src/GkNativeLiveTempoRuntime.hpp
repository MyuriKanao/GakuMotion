#pragma once
namespace NativeLiveRuntime
{
    inline void BuildTemplateBeats(NativeGraph& graph,GakuMotion::tempo::BeatMap& map)
    {
        r::Set(directorTime,graph.director.Get(),0.0);r::Call(directorEvaluate,graph.director.Get());
        r::Arena roots;void* info=nullptr;
        for (auto& asset:graph.clone.ownedAssets)
        {
            if (!r::Is(asset.Get(),equipmentTrackClass)) continue;
            void* candidate=roots.Keep(r::Call(getBpmInfo,asset.Get()));
            if (!candidate) continue;
            r::Require(!info || info==candidate,"template has conflicting BPM maps");info=candidate;
        }
        r::Require(info!=nullptr,"template has no native BPM map; select universal beat lighting");
        const bool ready=map.Build(graph.duration,[&](double time) {
            float at=static_cast<float>(time);void* args[]{&at};return static_cast<double>(r::Read<float>(evaluateBpm,info,args));
        });
        r::Require(ready,"template BPM curve is invalid or exceeds bounds");
    }
}
