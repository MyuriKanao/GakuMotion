// Self-authored clicks/metadata only; no user audio, motions or models.
#include "GkLibraryTempo.hpp"
#include "GkTrackTiming.hpp"
#include <cstdio>
#include <thread>
using namespace GakuMotion;
namespace
{
    int passed{},failed{};
    void Check(bool ok,const char* message) {if(ok)++passed;else{++failed;std::printf("FAIL: %s\n",message);}}
    std::vector<double> Clicks(double bpm,double offset=0)
    {
        std::vector<double> energy(3000);
        for(double t=offset;t<30;t+=60/bpm)energy[static_cast<size_t>(std::round(t*100))%energy.size()]=1;
        return energy;
    }
    void Wav(const std::filesystem::path& path,double bpm)
    {
        constexpr uint32_t rate=8000,frames=rate*20,bytes=frames*2;std::ofstream out(path,std::ios::binary);
        auto write=[&](auto x){out.write(reinterpret_cast<char*>(&x),sizeof(x));};
        out.write("RIFF",4);write(bytes+36);out.write("WAVEfmt ",8);write(uint32_t{16});write(uint16_t{1});write(uint16_t{1});
        write(rate);write(rate*2);write(uint16_t{2});write(uint16_t{16});out.write("data",4);write(bytes);
        for(uint32_t i=0;i<frames;++i)
        {const double p=std::fmod(i/static_cast<double>(rate),60/bpm);int16_t sample=p<.025?static_cast<int16_t>(16000*std::sin(i*.8)):0;write(sample);}
    }
    void Wait(tempo::Library& library,const std::string& dir,uint64_t generation)
    {for(int i=0;i<2000;++i){library.Update(dir,generation);if(!library.analyzing)return;std::this_thread::sleep_for(std::chrono::milliseconds(1));}}
}
int main()
{
    for(double bpm:{75.,90.,120.,150.,175.})
    {
        auto result=tempo::Estimate(Clicks(bpm));
        Check(result.Ready()&&std::fabs(result.bpm-bpm)<1.0,"estimated click tempo within one BPM");
    }
    Check(!tempo::Estimate(std::vector<double>(3000)).Ready(),"silence never invents a default 120 BPM");
    Check(!tempo::Estimate(std::vector<double>(3000,1)).Ready(),"constant energy is not mistaken for a beat");
    Check(!tempo::Estimate(std::vector<double>(30)).Ready(),"too-short audio reports unknown tempo");
    Check(!tempo::Estimate(Clicks(120),NAN).Ready() && !tempo::Estimate(Clicks(120),0).Ready(),"invalid analysis rate is rejected before integer conversion");
    auto invalid=Clicks(120);invalid[8]=NAN;Check(!tempo::Estimate(invalid).Ready(),"invalid energy rejected");
    const auto delayed=tempo::Estimate(Clicks(120,1.3));Check(delayed.Ready()&&std::fabs(delayed.offset-1.3)<.03,"leading silence preserves first-beat origin");
    tempo::BeatMap map;
    Check(map.Build(60,[](double){return 120.;}),"constant native template tempo builds integral");
    tempo::Result source{90,1,1,"test",""};
    Check(std::fabs(map.Seconds(source.Beat(5))-3)<1e-6,"90 BPM source at 5s maps to 120 BPM template at 3s, not 5s");
    const double paused=map.Seconds(source.Beat(5));
    Check(paused==map.Seconds(source.Beat(5)),"pause resamples exactly the same native time");
    Check(map.Seconds(source.Beat(3))<paused,"backward seek remaps absolute source beats");
    Check(std::fabs(map.Seconds(126)-3)<1e-6,"long external track loops the template by beat count");
    Check(map.Build(20,[](double t){return t<=10?60.:120.;})&&std::fabs(map.Seconds(15,false)-12.5)<.1,"native tempo changes are integrated before inversion");
    Check(!map.Build(2,[](double){return NAN;})&&!map.Ready(),"bad native BPM map is never sampled");
    Check(!map.Build(0,[](double){return 120.;})&&!map.Ready(),"zero-duration template never divides by an empty beat span");
    const auto dir=std::filesystem::temp_directory_path()/std::filesystem::path("gk-tempo-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(dir);const auto utf8=MotionPathToUtf8(dir);
    Wav(dir/"song.wav",120);auto decoded=tempo::Read(utf8);
    Check(decoded.Ready()&&std::fabs(decoded.bpm-120)<1,"PCM WAV decoding feeds automatic BPM analysis");
    {std::fstream out(dir/"song.wav",std::ios::binary|std::ios::in|std::ios::out);out.seekp(28);uint32_t invalidRate=0;out.write(reinterpret_cast<char*>(&invalidRate),4);}
    Check(!tempo::Read(utf8).Ready(),"invalid WAV byte rate is rejected");Wav(dir/"song.wav",120);
    {std::ofstream out(dir/"info.json");out<<"{\"bpm\":144,\"beat_offset_seconds\":0.25}";}
    auto metadata=tempo::Read(utf8);Check(metadata.bpm==144&&metadata.offset==.25&&metadata.source=="曲库元数据","library BPM metadata overrides estimation");
    tempo::Library library;library.Update(utf8,1);Wait(library,utf8,1);
    Check(library.value.bpm==144&&library.Matches(utf8,1)&&!library.Matches(utf8,2),"async result carries loaded-track generation identity");
    {std::ofstream out(dir/"info.json");out<<"{\"bpm\":96}";}
    library.Update(utf8,2);Check(!library.value.Ready(),"reload clears stale song BPM before new analysis");Wait(library,utf8,2);
    Check(library.value.bpm==96,"reload observes changed metadata instead of stale cached BPM");
    library.Update("",3);Check(!library.value.Ready()&&!library.analyzing,"cleared library selection releases active beat source");
    Check(tempo::Result{120,0,1,"test",""}.Beat(TrackSourceTime(5,{.5f,1}))==4,"lighting beat clock uses audio speed and timeline offset");
    std::filesystem::remove(dir/"info.json");std::filesystem::remove(dir/"song.wav");std::filesystem::remove(dir);
    std::printf("Library tempo/retiming: %d passed, %d failed\n",passed,failed);return failed?1:0;
}
