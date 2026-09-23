#include "audio_manager.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace
{
void require(bool condition, const std::string &message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

size_t countOccurrences(const std::string &text, const std::string &needle)
{
    size_t count = 0;
    size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos)
    {
        ++count;
        position += needle.size();
    }
    return count;
}
} // namespace

int main()
{
    AudioManager manager;

    require(manager.buildAudioPipeline(AudioMode::None, "mic", "system").empty(),
            "muted mode must not create an audio pipeline");

    const std::string system = manager.buildAudioPipeline(AudioMode::System, "mic", "system");
    require(system.find("device=\"system\"") != std::string::npos,
            "system mode must capture the selected monitor");
    require(system.find("device=\"mic\"") == std::string::npos,
            "system mode must not capture the microphone");
    require(system.find("audio/x-raw,format=S16LE,rate=48000,channels=2") != std::string::npos,
            "system audio must be normalized to stereo 48 kHz");
    require(system.find("fdkaacenc bitrate=192000 afterburner=true") != std::string::npos,
            "system audio must use the configured high-quality AAC encoding");

    const std::string mic = manager.buildAudioPipeline(AudioMode::Mic, "mic", "system");
    require(mic.find("device=\"mic\"") != std::string::npos,
            "microphone mode must capture the selected microphone");
    require(mic.find("device=\"system\"") == std::string::npos,
            "microphone mode must not capture the system monitor");

    const std::string both = manager.buildAudioPipeline(AudioMode::Both, "mic", "system");
    require(both.find("audiomixer name=mix") != std::string::npos,
            "combined mode must mix both sources");
    require(both.find("device=\"system\"") != std::string::npos &&
                both.find("device=\"mic\"") != std::string::npos,
            "combined mode must capture both selected devices");
    require(countOccurrences(both, "fdkaacenc") == 1,
            "combined mode must encode the mixed stream exactly once");
    require(countOccurrences(both, "rate=48000,channels=2") == 3,
            "both inputs and the mixed output must use identical audio caps");

    std::cout << "Audio pipeline tests passed\n";
    return 0;
}
