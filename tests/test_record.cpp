#include "screencast_service.hpp"
#include "audio_manager.hpp"
#include <gtk/gtk.h>
#include <iostream>
#include <chrono>

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    std::cout << "=== Starting 3-second Screencast Test ===" << std::endl;

    RecordingConfig config;
    config.captureMode = CaptureMode::FullScreen;
    config.monitorConnector = "DP-1";
    config.outputDir = "/tmp";
    config.copyToClipboard = true;
    config.audioMode = AudioMode::System;

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

    // Schedule stop after 3 seconds
    g_timeout_add_seconds(3, +[](gpointer userData) -> gboolean
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
