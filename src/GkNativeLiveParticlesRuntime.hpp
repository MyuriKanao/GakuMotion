#pragma once
namespace NativeLiveRuntime
{
    struct ParticleSnapshot
    {
        struct Entry { ComponentId id; r::Root particles; int count; float time; uint32_t seed; bool autoSeed, playing, paused; };
        std::vector<Entry> rows;
        void* klass{};
        const void *getCount{}, *getTime{}, *setTime{}, *getSeed{}, *setSeed{}, *getAuto{}, *setAuto{},
            *getPlaying{}, *getPaused{}, *getParticles{}, *setParticles{}, *play{}, *pause{}, *stop{};
        int scene{}, clearMode{};
        void* particleClass{};
        void Resolve()
        {
            klass = r::Class("UnityEngine.ParticleSystemModule.dll", "UnityEngine", "ParticleSystem");
            particleClass = r::Nested(klass, "Particle"); r::Require(particleClass != nullptr, "native particle snapshot type unavailable");
            void* array = r::arrayClass(particleClass, 1);
            auto property = [&](const char* name, int code, const void*& get, const void*& set) {
                get = r::Find(klass, (std::string("get_") + name).c_str(), false, {code}, {});
                set = r::Find(klass, (std::string("set_") + name).c_str(), false, {0x01}, {{code}});
                r::Require(get && set, "native particle state properties unavailable");
            };
            property("time", 0x0c, getTime, setTime); property("randomSeed", 0x09, getSeed, setSeed);
            property("useAutoRandomSeed", 0x02, getAuto, setAuto);
            getCount = r::Find(klass, "get_particleCount", false, {0x08}, {});
            getPlaying = r::Find(klass, "get_isPlaying", false, {0x02}, {}); getPaused = r::Find(klass, "get_isPaused", false, {0x02}, {});
            getParticles = r::Find(klass, "GetParticles", false, {0x08}, {{0x1d, array}});
            setParticles = r::Find(klass, "SetParticles", false, {0x01}, {{0x1d, array}, {0x08}});
            play = r::Find(klass, "Play", false, {0x01}, {{0x02}}); pause = r::Find(klass, "Pause", false, {0x01}, {{0x02}});
            void* behavior = r::Class("UnityEngine.ParticleSystemModule.dll", "UnityEngine", "ParticleSystemStopBehavior");
            clearMode = r::Enum(behavior, "StopEmittingAndClear");
            stop = r::Find(klass, "Stop", false, {0x01}, {{0x02}, {0x11, behavior}});
            r::Require(getCount && getPlaying && getPaused && getParticles && setParticles && play && pause && stop, "native particle copy/restore methods unavailable");
        }
        void Capture(int handle)
        {
            Resolve(); scene = handle; r::Arena roots;
            for (void* value : Components(klass, scene, roots))
            {
                const int count = r::Read<int>(getCount, value);
                r::Require(count >= 0 && count <= 32768, "native particle snapshot exceeds bound");
                r::Root particles(arrayNew(particleClass, count));
                void* args[]{particles.Get()};
                r::Require(r::Read<int>(getParticles, value, args) == count, "native particle count changed during snapshot");
                rows.push_back({Identity(value, roots), std::move(particles), count, r::Read<float>(getTime, value),
                    r::Read<uint32_t>(getSeed, value), r::Read<bool>(getAuto, value), r::Read<bool>(getPlaying, value), r::Read<bool>(getPaused, value)});
            }
        }
        bool Restore()
        {
            if (rows.empty()) return true;
            r::Arena roots; std::map<int, std::pair<ComponentId, void*>> fresh;
            for (void* value : Components(klass, scene, roots))
            { const auto id = Identity(value, roots); fresh.emplace(id.component, std::pair{id, value}); }
            for (auto it = rows.begin(); it != rows.end();)
            {
                const auto found = fresh.find(it->id.component);
                try
                {
                    if (found != fresh.end() && found->second.first == it->id)
                    {
                        void* value = found->second.second; bool children = false;
                        void* stopArgs[]{&children, &clearMode}; r::Call(stop, value, stopArgs);
                        r::Set(setAuto, value, false); r::Set(setSeed, value, it->seed);
                        void* childArgs[]{&children};
                        if (it->playing || it->paused) r::Call(play, value, childArgs);
                        if (it->paused) r::Call(pause, value, childArgs);
                        r::Set(setTime, value, it->time); void* args[]{it->particles.Get(), &it->count}; r::Call(setParticles, value, args);
                        r::Set(setAuto, value, it->autoSeed);
                    }
                    it = rows.erase(it);
                }
                catch (...) { ++it; }
            }
            return rows.empty();
        }
    };
}
