#include "audio_manager.hpp"
#include <iostream>
#include <sstream>
#include <array>
#include <memory>
#include <algorithm>

namespace
{
constexpr int kAudioSampleRate = 48000;
constexpr int kAudioChannels = 2;
constexpr int kAudioBitrate = 192000;

std::string rawAudioCaps()
{
    return "audio/x-raw,format=S16LE,rate=" + std::to_string(kAudioSampleRate) +
           ",channels=" + std::to_string(kAudioChannels);
}

std::string pulseSource(const std::string &device)
{
    return "pulsesrc device=\"" + device + "\" do-timestamp=true ! "
           "queue ! audioconvert ! audioresample ! " +
           rawAudioCaps();
}

std::string aacEncoder()
{
    return "fdkaacenc bitrate=" + std::to_string(kAudioBitrate) +
           " afterburner=true ! aacparse ! queue ! mux.audio_0";
}
} // namespace

AudioManager::AudioManager()
{
    refreshDevices();
}

AudioManager::~AudioManager()
{
}

std::string AudioManager::runCommand(const std::string &cmd) const
{
    std::array<char, 256> buffer;
    std::string result;
    FILE *pipe = popen(cmd.c_str(), "r");
    if (!pipe)
    {
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
    {
        result += buffer.data();
    }
    pclose(pipe);
    // Trim trailing whitespace/newlines
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' '))
    {
        result.pop_back();
    }
    return result;
}

bool AudioManager::refreshDevices()
{
    m_microphones.clear();
    m_systemMonitors.clear();

    m_defaultSink = runCommand("pactl get-default-sink 2>/dev/null");
    m_defaultSource = runCommand("pactl get-default-source 2>/dev/null");

    std::string output = runCommand("pactl list sources 2>/dev/null");
    if (output.empty())
    {
        return false;
    }

    std::istringstream stream(output);
    std::string line;

    AudioDevice currentDevice;
    bool hasDevice = false;

    auto finalizeDevice = [this, &currentDevice, &hasDevice]()
    {
        if (!hasDevice || currentDevice.name.empty())
            return;

        bool isMon = false;
        if (currentDevice.name.rfind(".monitor") != std::string::npos ||
            currentDevice.description.find("Monitor of") != std::string::npos)
        {
            isMon = true;
        }

        currentDevice.isMonitor = isMon;
        if (isMon)
        {
            m_systemMonitors.push_back(currentDevice);
        }
        else
        {
            m_microphones.push_back(currentDevice);
        }
        currentDevice = AudioDevice();
        hasDevice = false;
    };

    while (std::getline(stream, line))
    {
        // Trim leading spaces/tabs
        size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace == std::string::npos)
            continue;
        std::string trimmed = line.substr(firstNonSpace);

        if (trimmed.rfind("Source #", 0) == 0)
        {
            finalizeDevice();
            hasDevice = true;
            try
            {
                currentDevice.index = std::stoi(trimmed.substr(8));
            }
            catch (...)
            {
                currentDevice.index = 0;
            }
        }
        else if (trimmed.rfind("Name: ", 0) == 0)
        {
            currentDevice.name = trimmed.substr(6);
        }
        else if (trimmed.rfind("Description: ", 0) == 0)
        {
            currentDevice.description = trimmed.substr(13);
        }
    }
    finalizeDevice();

    return !m_microphones.empty() || !m_systemMonitors.empty();
}

std::string AudioManager::getDefaultSinkMonitor() const
{
    // The output device can change while the recorder window is open (Bluetooth
    // headphones are a common example), so resolve it again when recording starts.
    std::string currentSink = runCommand("pactl get-default-sink 2>/dev/null");
    if (currentSink.empty())
    {
        currentSink = m_defaultSink;
    }
    if (!currentSink.empty())
    {
        return currentSink + ".monitor";
    }
    return "@DEFAULT_SINK@.monitor";
}

std::string AudioManager::getDefaultSource() const
{
    std::string currentSource = runCommand("pactl get-default-source 2>/dev/null");
    if (currentSource.empty())
    {
        currentSource = m_defaultSource;
    }
    if (!currentSource.empty())
    {
        return currentSource;
    }
    return "@DEFAULT_SOURCE@";
}

std::string AudioManager::buildAudioPipeline(AudioMode mode,
                                             const std::string &selectedMic,
                                             const std::string &selectedSystemAudio) const
{
    std::string sysDev = selectedSystemAudio.empty() ? getDefaultSinkMonitor() : selectedSystemAudio;
    std::string micDev = selectedMic.empty() ? getDefaultSource() : selectedMic;

    switch (mode)
    {
    case AudioMode::None:
        return "";

    case AudioMode::System:
        return pulseSource(sysDev) + " ! " + aacEncoder();

    case AudioMode::Mic:
        return pulseSource(micDev) + " ! " + aacEncoder();

    case AudioMode::Both:
        // Normalize both inputs before mixing. Without explicit caps, negotiation
        // produced 44.1 kHz mono at 56 kb/s even when the system monitor was a
        // 48 kHz stereo source.
        return "audiomixer name=mix ! audioconvert ! audioresample ! " + rawAudioCaps() +
               " ! " + aacEncoder() + " " +
               pulseSource(sysDev) + " ! mix. " +
               pulseSource(micDev) + " ! mix.";
    }

    return "";
}
