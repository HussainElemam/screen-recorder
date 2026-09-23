#include "screencast_service.hpp"
#include "audio_manager.hpp"
#include "display_manager.hpp"
#include <gtk/gtk.h>
#include <iostream>
#include <chrono>

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    std::cout << "=== Starting 10-second Screencast Test ===" << std::endl;

    RecordingConfig config;
    config.captureMode = CaptureMode::FullScreen;
    config.outputDir = "/tmp";
    config.copyToClipboard = false;
    config.audioMode = AudioMode::System;

    DisplayManager displayMgr;
    const MonitorInfo *selectedMonitor = nullptr;
    for (const auto &monitor : displayMgr.getMonitors())
    {
        if (monitor.isPrimary)
        {
            selectedMonitor = &monitor;
            break;
        }
        if (!selectedMonitor)
        {
            selectedMonitor = &monitor;
        }
    }
    if (!selectedMonitor)
    {
        std::cerr << "No monitor available for recording test" << std::endl;
        return 1;
    }
    config.monitorConnector = selectedMonitor->connector;
    config.monitorWidth = selectedMonitor->width;
    config.monitorHeight = selectedMonitor->height;
    std::cout << "Capturing primary monitor " << config.monitorConnector << " ("
              << config.monitorWidth << "x" << config.monitorHeight << ")" << std::endl;

    AudioManager audioMgr;
    std::string audioPipeline = audioMgr.buildAudioPipeline(config.audioMode, "", "");

    ScreencastService service;
    bool started = service.startRecording(
        config,
        audioPipeline,
        [](const std::string &status)
        {
            std::cout << "Status update: " << status << std::endl;
        },
        [](const std::string &path, bool success)
        {
            std::cout << "Finished callback! File: " << path << " Success: " << (success ? "YES" : "NO") << std::endl;
            gtk_main_quit();
        });

    if (!started)
    {
        std::cerr << "Failed to start recording!" << std::endl;
        return 1;
    }

    // Run long enough to expose encoder backpressure and frame duplication.
    g_timeout_add_seconds(10, +[](gpointer userData) -> gboolean
                          {
        std::cout << "Stopping recording..." << std::endl;
        auto* svc = static_cast<ScreencastService*>(userData);
        svc->stopRecording();
        return G_SOURCE_REMOVE; }, &service);

    gtk_main();

    std::string out = service.getCurrentOutputFile();
    std::cout << "Recorded output file: " << out << std::endl;

    return 0;
}
