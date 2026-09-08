#include "settings_manager.hpp"
#include <glib.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <sys/stat.h>

SettingsManager::SettingsManager()
{
    const char *home = g_get_home_dir();
    std::string configDir = std::string(home) + "/.config/screen-recorder";
    m_configPath = configDir + "/config.json";

    // Default output directory: ~/Videos/Recordings
    m_outputDir = std::string(home) + "/Videos/Recordings";

    ensureConfigDir();
    load();
}

SettingsManager::~SettingsManager()
{
    save();
}

void SettingsManager::ensureConfigDir()
{
    const char *home = g_get_home_dir();
    std::string configDir = std::string(home) + "/.config/screen-recorder";
    g_mkdir_with_parents(configDir.c_str(), 0755);
    g_mkdir_with_parents(m_outputDir.c_str(), 0755);
}

void SettingsManager::load()
{
    std::ifstream file(m_configPath);
    if (!file.is_open())
    {
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        size_t colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        std::string key = line.substr(0, colon);
        std::string val = line.substr(colon + 1);

        // Strip quotes, spaces, commas
        auto clean = [](std::string &s)
        {
            size_t start = s.find_first_not_of(" \t\"'{}[],");
            if (start == std::string::npos)
            {
                s = "";
                return;
            }
            size_t end = s.find_last_not_of(" \t\"'{}[],");
            s = s.substr(start, end - start + 1);
        };
        clean(key);
        clean(val);

        if (key == "output_directory" && !val.empty())
        {
            m_outputDir = val;
        }
        else if (key == "audio_mode" && !val.empty())
        {
            try
            {
                int modeInt = std::stoi(val);
                if (modeInt >= 0 && modeInt <= 3)
                {
                    m_audioMode = static_cast<AudioMode>(modeInt);
                }
            }
            catch (...)
            {
            }
        }
        else if (key == "last_microphone")
        {
            m_lastMic = val;
        }
        else if (key == "copy_to_clipboard")
        {
            m_copyToClipboard = (val == "true" || val == "1");
        }
        else if (key == "enable_countdown")
        {
            m_enableCountdown = (val == "true" || val == "1");
        }
    }
}

void SettingsManager::save()
{
    ensureConfigDir();
    std::ofstream file(m_configPath);
    if (!file.is_open())
        return;

    file << "{\n";
    file << "  \"output_directory\": \"" << m_outputDir << "\",\n";
    file << "  \"audio_mode\": " << static_cast<int>(m_audioMode) << ",\n";
    file << "  \"last_microphone\": \"" << m_lastMic << "\",\n";
    file << "  \"copy_to_clipboard\": " << (m_copyToClipboard ? "true" : "false") << ",\n";
    file << "  \"enable_countdown\": " << (m_enableCountdown ? "true" : "false") << "\n";
    file << "}\n";
}
