#pragma once
#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace GakuMotion::tempo
{
    struct Result
    {
        double bpm{}, offset{}, confidence{};
        std::string source, error;
        bool Ready() const { return std::isfinite(bpm) && bpm >= 30 && bpm <= 300 && std::isfinite(offset) && std::fabs(offset)<=86400; }
        double Beat(double audioSeconds) const
        { return Ready() && std::isfinite(audioSeconds) ? (std::max)(0.0, (audioSeconds-offset)*bpm/60) : 0; }
    };
    inline Result Estimate(const std::vector<double>& energy, double rate = 100)
    {
        Result result; result.source="音频估算";
        if (!std::isfinite(rate) || rate<10 || rate>1000) { result.error="节拍采样率无效"; return result; }
        if (energy.size()<static_cast<size_t>(rate*8)) { result.error="音频太短，待填写 BPM"; return result; }
        std::vector<double> onset(energy.size()); double average{}, power{};
        for (size_t i=0;i<energy.size();++i)
        {
            if (!std::isfinite(energy[i]) || energy[i]<0) { result.error="音频数据异常"; return result; }
            average=average*.95+energy[i]*.05;
            onset[i]=(std::max)(0.0,energy[i]-average);power+=onset[i]*onset[i];
        }
        if (power<1e-12) { result.error="未检测到可靠节拍，待填写 BPM"; return result; }
        const double threshold=std::sqrt(power/onset.size())*.5;size_t peaks{};
        for (size_t i=1;i+1<onset.size();++i)
            if (onset[i]>threshold && onset[i]>=onset[i-1] && onset[i]>onset[i+1]) ++peaks;
        if (peaks<6 || peaks>energy.size()/rate*12) { result.error="未检测到可靠节拍，待填写 BPM"; return result; }
        const int low=static_cast<int>(rate*60/180),high=static_cast<int>(rate*60/65);
        auto score=[&](int lag) {
            double sum{},a{},b{};
            for (size_t i=lag;i<onset.size();++i) { sum+=onset[i]*onset[i-lag];a+=onset[i]*onset[i];b+=onset[i-lag]*onset[i-lag]; }
            return a*b>1e-24 ? sum/std::sqrt(a*b) : 0;
        };
        int best=low; double strongest{};
        for (int lag=low;lag<=high;++lag) if (const double value=score(lag);value>strongest) { strongest=value;best=lag; }
        if (strongest<.18) { result.error="节拍置信度不足，待填写 BPM"; return result; }
        double lag=best;
        if (best>low && best<high)
        {
            const double a=score(best-1),b=score(best),c=score(best+1),denom=a-2*b+c;
            if (std::fabs(denom)>1e-9) lag+=std::clamp(.5*(a-c)/denom,-.5,.5);
        }
        result.bpm=60*rate/lag; result.confidence=strongest;
        const int bins=(std::max)(1,static_cast<int>(std::round(lag)));
        std::vector<double> phases(bins);
        for (size_t i=0;i<onset.size();++i) phases[static_cast<size_t>(std::llround(std::fmod(static_cast<double>(i),lag)))%bins]+=onset[i];
        result.offset=std::distance(phases.begin(),std::max_element(phases.begin(),phases.end()))/rate;
        for (size_t i=0;i<onset.size();++i)
            if (onset[i]>threshold && std::fabs(std::remainder(static_cast<double>(i)/rate-result.offset,60/result.bpm))<1.5/rate)
            { result.offset=static_cast<double>(i)/rate;break; }
        return result;
    }
    // Integral of the ORIGINAL template's native BPM curve. Mapping source
    // beats through the inverse preserves tempo changes and cue positions.
    class BeatMap
    {
        std::vector<double> times,beats;
    public:
        bool Build(double duration,const std::function<double(double)>& bpm)
        {
            times.clear();beats.clear();
            if (!std::isfinite(duration)||duration<=0||duration>1800||!bpm) return false;
            const size_t count=static_cast<size_t>(std::ceil(duration*8));
            double previous=bpm(0);
            if (!std::isfinite(previous)||previous<30||previous>300) return false;
            times.push_back(0);beats.push_back(0);
            for (size_t i=1;i<=count;++i)
            {
                const double t=duration*i/count,value=bpm(t);
                if (!std::isfinite(value)||value<30||value>300) { times.clear();beats.clear();return false; }
                beats.push_back(beats.back()+(t-times.back())*(previous+value)/120);
                times.push_back(t); previous=value;
            }
            return Ready();
        }
        bool Ready() const { return beats.size()>1 && std::isfinite(beats.back()) && beats.back()>0; }
        double Seconds(double beat,bool loop=true) const
        {
            if (!Ready()||!std::isfinite(beat)||beat<=0) return 0;
            beat=loop ? std::fmod(beat,beats.back()) : (std::min)(beat,beats.back());
            auto upper=std::upper_bound(beats.begin(),beats.end(),beat);
            if (upper==beats.end()) return times.back();
            const size_t i=static_cast<size_t>(upper-beats.begin());
            return times[i-1]+(times[i]-times[i-1])*(beat-beats[i-1])/(beats[i]-beats[i-1]);
        }
    };
}
