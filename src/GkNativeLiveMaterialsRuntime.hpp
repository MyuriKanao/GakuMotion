#pragma once
namespace NativeLiveRuntime
{
    // Property blocks are copied into private managed containers. Shared
    // Material assets are neither cloned nor modified by this adapter.
    struct MaterialSnapshot
    {
        struct Entry { ComponentId id; std::vector<r::Root> blocks; };
        std::vector<Entry> rows;
        void *rendererClass{}, *blockClass{};
        const void *constructor{}, *materials{}, *get{}, *getSlot{}, *set{}, *setSlot{};
        int scene{};
        void Resolve()
        {
            rendererClass = r::Unity("Renderer"); blockClass = r::Unity("MaterialPropertyBlock");
            void* materialClass = r::Unity("Material");
            constructor = r::Find(blockClass, ".ctor", false, {0x01}, {});
            materials = r::Find(rendererClass, "get_sharedMaterials", false, {0x1d, r::arrayClass(materialClass, 1)}, {});
            get = r::Find(rendererClass, "GetPropertyBlock", false, {0x01}, {{0x12, blockClass}});
            getSlot = r::Find(rendererClass, "GetPropertyBlock", false, {0x01}, {{0x12, blockClass}, {0x08}});
            set = r::Find(rendererClass, "SetPropertyBlock", false, {0x01}, {{0x12, blockClass}});
            setSlot = r::Find(rendererClass, "SetPropertyBlock", false, {0x01}, {{0x12, blockClass}, {0x08}});
            r::Require(constructor && materials && get && getSlot && set && setSlot, "native material property-block snapshot unavailable");
        }
        void Capture(int handle)
        {
            Resolve(); scene = handle; r::Arena roots;
            for (void* value : Components(rendererClass, scene, roots))
            {
                Entry entry{Identity(value, roots)};
                const int count = static_cast<int>(r::Items(roots.Keep(r::Call(materials, value)), 64).size());
                for (int slot = -1; slot < count; ++slot)
                {
                    r::Root block(r::objectNew(blockClass)); r::Call(constructor, block.Get());
                    void* args[]{block.Get(), &slot}; r::Call(slot < 0 ? get : getSlot, value, args);
                    entry.blocks.push_back(std::move(block));
                }
                rows.push_back(std::move(entry));
            }
        }
        bool Restore()
        {
            if (rows.empty()) return true;
            r::Arena roots; std::map<int, std::pair<ComponentId, void*>> fresh;
            for (void* value : Components(rendererClass, scene, roots))
            { const auto id = Identity(value, roots); fresh.emplace(id.component, std::pair{id, value}); }
            for (auto it = rows.begin(); it != rows.end();)
            {
                const auto found = fresh.find(it->id.component);
                try
                {
                    if (found != fresh.end() && found->second.first == it->id)
                        for (int slot = -1; slot + 1 < static_cast<int>(it->blocks.size()); ++slot)
                        {
                            void* args[]{it->blocks[slot + 1].Get(), &slot};
                            r::Call(slot < 0 ? set : setSlot, found->second.second, args);
                        }
                    it = rows.erase(it);
                }
                catch (...) { ++it; }
            }
            return rows.empty();
        }
    };
}
