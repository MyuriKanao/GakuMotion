#pragma once
#include "GkTempo.hpp"
#include "GkMotionData.hpp"
#include "GkJsonLite.hpp"
#include <filesystem>
#include <fstream>
#include <cstring>
#include <cstdint>

namespace GakuMotion::tempo
{
    inline Result Read(const std::string& directory)
    {
        Result result;jsonlite::Value info;std::string error;
        if (jsonlite::ParseFile(directory+"/info.json",info,error))
            if (const auto* bpm=info.Find("bpm");bpm && bpm->IsNumber())
            {
                result.bpm=bpm->numberValue;result.confidence=1;result.source="曲库元数据";
                if (const auto* offset=info.Find("beat_offset_seconds");offset && offset->IsNumber()) result.offset=offset->numberValue;
                if (result.Ready()) return result;
                result={};result.error="info.json BPM/拍点无效";return result;
            }
        std::ifstream file(MotionPathFromUtf8(directory+"/song.wav"),std::ios::binary|std::ios::ate);
        if (!file) { result.error="当前曲目没有 song.wav 或 BPM 元数据";return result; }
        const auto length=file.tellg();file.seekg(0);char header[12]{};file.read(header,12);
        if (!file||std::memcmp(header,"RIFF",4)||std::memcmp(header+8,"WAVE",4)) { result.error="BPM 分析需要 PCM WAV";return result; }
        uint16_t format{},channels{},align{},bits{};uint32_t sampleRate{},byteRate{},dataSize{};std::streamoff dataOffset{};
        for (int chunk=0;chunk<4096 && file.tellg()<=length-std::streamoff{8};++chunk)
        {
            char kind[4];uint32_t size{};file.read(kind,4);file.read(reinterpret_cast<char*>(&size),4);const auto start=file.tellg();
            if (!file||size>length-start) { result.error="WAV 数据块越界";return result; }
            if (!std::memcmp(kind,"fmt ",4) && size>=16)
            {
                file.read(reinterpret_cast<char*>(&format),2);file.read(reinterpret_cast<char*>(&channels),2);
                file.read(reinterpret_cast<char*>(&sampleRate),4);file.read(reinterpret_cast<char*>(&byteRate),4);
                file.read(reinterpret_cast<char*>(&align),2);file.read(reinterpret_cast<char*>(&bits),2);
            }
            if (!std::memcmp(kind,"data",4)) { dataOffset=start;dataSize=size; }
            file.seekg(start+static_cast<std::streamoff>(static_cast<uint64_t>(size)+(size&1)));
        }
        if ((format!=1 && format!=3)||!channels||channels>8||sampleRate<8000||sampleRate>192000||
            (bits!=8 && bits!=16 && bits!=24 && bits!=32)||(format==3 && bits!=32)||align!=channels*(bits/8)||byteRate!=sampleRate*align||!dataOffset)
        { result.error="不支持的 WAV 格式，待填写 BPM";return result; }
        constexpr uint64_t maxBytes=64*1024*1024;
        const auto frames=(std::min)({static_cast<uint64_t>(dataSize/align),static_cast<uint64_t>(sampleRate)*120,maxBytes/align});
        std::vector<uint8_t> pcm(static_cast<size_t>(frames*align));file.clear();file.seekg(dataOffset);
        if (!file.read(reinterpret_cast<char*>(pcm.data()),pcm.size())) { result.error="WAV 读取失败";return result; }
        const size_t hop=(std::max)(1u,sampleRate/100);std::vector<double> envelope(static_cast<size_t>(frames/hop));
        double previous{};
        for (size_t i=0;i<frames;++i)
        {
            double mono{};
            for (unsigned channel=0;channel<channels;++channel)
            {
                const uint8_t* p=pcm.data()+i*align+channel*(bits/8);double sample{};
                if (format==3) { float v{};std::memcpy(&v,p,4);sample=std::isfinite(v)?v:0; }
                else if (bits==8) sample=(static_cast<int>(*p)-128)/128.0;
                else
                {
                    uint32_t raw{};for (unsigned b=0;b<static_cast<unsigned>(bits/8);++b) raw|=static_cast<uint32_t>(p[b])<<(8*b);
                    const int64_t signedValue=(raw&(uint32_t{1}<<(bits-1))) ? static_cast<int64_t>(raw)-(int64_t{1}<<bits) : raw;
                    sample=signedValue/std::ldexp(1.0,bits-1);
                }
                mono+=sample/channels;
            }
            if (i/hop<envelope.size()) envelope[i/hop]+=std::fabs(mono-previous)+.1*std::fabs(mono);
            previous=mono;
        }
        return Estimate(envelope,static_cast<double>(sampleRate)/hop);
    }
}
