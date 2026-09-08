#pragma once

#include "common.hpp"
#include <vector>
#include <string>

class AudioManager
{
public:
    AudioManager();
    ~AudioManager();

    bool refreshDevices();

    const std::vector<AudioDevice> &getMicrophones() const { return m_microphones; }
    const std::vector<AudioDevice> &getSystemMonitors() const { return m_systemMonitors; }

    std::string getDefaultSinkMonitor() const;
    std::string getDefaultSource() const;

    // Generates the GStreamer audio pipeline string fragment
    std::string buildAudioPipeline(AudioMode mode,
                                   const std::string &selectedMic,
                                   const std::string &selectedSystemAudio) const;

private:
    std::vector<AudioDevice> m_microphones;
    std::vector<AudioDevice> m_systemMonitors;
    std::string m_defaultSink;
    std::string m_defaultSource;

    std::string runCommand(const std::string &cmd) const;
};
