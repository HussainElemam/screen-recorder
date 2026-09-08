#include "display_manager.hpp"
#include "audio_manager.hpp"
#include "settings_manager.hpp"
#include <gtk/gtk.h>
#include <iostream>

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    std::cout << "=== Testing DisplayManager ===" << std::endl;
    DisplayManager dm;
    const auto &monitors = dm.getMonitors();
    std::cout << "Found " << monitors.size() << " monitors:" << std::endl;
    for (const auto &mon : monitors)
    {
        std::cout << "  - [" << mon.index << "] " << mon.displayName
                  << " (Connector: " << mon.connector << ", "
                  << mon.width << "x" << mon.height << ", Primary: "
                  << (mon.isPrimary ? "Yes" : "No") << ")" << std::endl;
    }

    std::cout << "\n=== Testing AudioManager ===" << std::endl;
    AudioManager am;
    std::cout << "Default sink monitor: " << am.getDefaultSinkMonitor() << std::endl;
    std::cout << "Default source: " << am.getDefaultSource() << std::endl;

    const auto &mics = am.getMicrophones();
    std::cout << "Found " << mics.size() << " microphones:" << std::endl;
    for (const auto &mic : mics)
    {
        std::cout << "  - [" << mic.index << "] " << mic.description << " (" << mic.name << ")" << std::endl;
    }

    const auto &sinks = am.getSystemMonitors();
    std::cout << "Found " << sinks.size() << " system monitors:" << std::endl;
    for (const auto &s : sinks)
    {
        std::cout << "  - [" << s.index << "] " << s.description << " (" << s.name << ")" << std::endl;
    }

    std::cout << "\n=== Pipeline Generations ===" << std::endl;
    std::cout << "None: " << am.buildAudioPipeline(AudioMode::None, "", "") << std::endl;
    std::cout << "System: " << am.buildAudioPipeline(AudioMode::System, "", "") << std::endl;
    std::cout << "Mic: " << am.buildAudioPipeline(AudioMode::Mic, "", "") << std::endl;
    std::cout << "Both: " << am.buildAudioPipeline(AudioMode::Both, "", "") << std::endl;

    std::cout << "\n=== Testing SettingsManager ===" << std::endl;
    SettingsManager sm;
    std::cout << "Output dir: " << sm.getOutputDirectory() << std::endl;
    std::cout << "Copy to clipboard: " << (sm.getCopyToClipboard() ? "true" : "false") << std::endl;

    return 0;
}
