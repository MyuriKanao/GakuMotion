#pragma once
#include "GkTempoWav.hpp"
#include <future>
#include <chrono>
#include <map>

namespace GakuMotion::tempo
{
    class Library
    {
        struct Work { std::string key;Result result; };
        std::future<Work> pending;
        std::map<std::string,Result> cache;
        std::string wanted;
    public:
        Result value;
        bool analyzing{};
        void Update(const std::string& directory,uint64_t generation=0)
        {
            const auto identity=directory.empty()?std::string{}:directory+"#"+std::to_string(generation);
            if (wanted!=identity) { wanted=identity;value={}; }
            if (pending.valid() && pending.wait_for(std::chrono::seconds(0))==std::future_status::ready)
            {
                auto done=pending.get();cache[done.key]=std::move(done.result);
                if (cache.size()>16)
                    for (auto it=cache.begin();it!=cache.end();++it) if (it->first!=wanted) {cache.erase(it);break;}
            }
            if (wanted.empty()) { analyzing=false;return; }
            if (auto found=cache.find(wanted);found!=cache.end()) { value=found->second;analyzing=false;return; }
            analyzing=true;
            if (!pending.valid())
            {
                const auto key=wanted;
                pending=std::async(std::launch::async,[key,directory] {
                    try { return Work{key,Read(directory)}; }
                    catch (...) { Result failed;failed.error="BPM 分析失败";return Work{key,failed}; }
                });
            }
        }
        bool Matches(const std::string& directory,uint64_t generation) const
        { return wanted==directory+"#"+std::to_string(generation); }
        void Wait() { if (pending.valid()) pending.wait(); }
    };
}
