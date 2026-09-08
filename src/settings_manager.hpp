#pragma once

#include "common.hpp"
#include <string>

class SettingsManager
{
public:
    SettingsManager();
    ~SettingsManager();

    void load();
    void save();

    std::string getOutputDirectory() const { return m_outputDir; }
    void setOutputDirectory(const std::string &dir) { m_outputDir = dir; }

    AudioMode getAudioMode() const { return m_audioMode; }
    void setAudioMode(AudioMode mode) { m_audioMode = mode; }

    std::string getLastMicrophone() const { return m_lastMic; }
    void setLastMicrophone(const std::string &mic) { m_lastMic = mic; }

    bool getCopyToClipboard() const { return m_copyToClipboard; }
    void setCopyToClipboard(bool copy) { m_copyToClipboard = copy; }

    bool getEnableCountdown() const { return m_enableCountdown; }
    void setEnableCountdown(bool enable) { m_enableCountdown = enable; }

private:
    std::string m_configPath;
    std::string m_outputDir;
    AudioMode m_audioMode = AudioMode::None;
    std::string m_lastMic;
    bool m_copyToClipboard = true;
    bool m_enableCountdown = true;

    void ensureConfigDir();
};
