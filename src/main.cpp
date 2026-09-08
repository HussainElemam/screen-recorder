#include "display_manager.hpp"
#include "audio_manager.hpp"
#include "settings_manager.hpp"
#include "screencast_service.hpp"
#include "ui_recorder.hpp"
#include <gtk/gtk.h>
#include <glib-unix.h>
#include <iostream>
#include <memory>
#include <string>

static gboolean onSignalReceived(gpointer userData)
{
    std::cout << "\nReceived termination signal, finalizing recording..." << std::endl;
    auto *recorderWin = static_cast<RecorderWindow *>(userData);
    if (recorderWin)
    {
        recorderWin->stopRecording();
    }
    else
    {
        gtk_main_quit();
    }
    return G_SOURCE_REMOVE;
}

int main(int argc, char *argv[])
{
    // Check CLI arguments
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        if (arg == "--help" || arg == "-h")
        {
            std::cout << "Screen Recorder for Fedora Linux (Native C++ / Wayland)\n\n"
                      << "Usage:\n"
                      << "  screen-recorder [options]\n\n"
                      << "Options:\n"
                      << "  --toggle, -t    Toggle recording: starts if idle, stops if recording\n"
                      << "  --stop, -s      Stop active recording session and save file\n"
                      << "  --status        Check if recording is currently active\n"
                      << "  --help, -h      Show this help message\n";
            return 0;
        }

        if (arg == "--status")
        {
            if (ScreencastService::isRecordingActive())
            {
                std::cout << "Recording is ACTIVE (PID: "
                          << ScreencastService::getActiveRecordingPid() << ")\n";
                return 0;
            }
            else
            {
                std::cout << "Recording is IDLE\n";
                return 1;
            }
        }

        if (arg == "--stop" || arg == "-s")
        {
            if (ScreencastService::isRecordingActive())
            {
                std::cout << "Stopping active recording...\n";
                ScreencastService::stopActiveRecording();
                return 0;
            }
            else
            {
                std::cout << "No active recording found.\n";
                return 0;
            }
        }

        if (arg == "--toggle" || arg == "-t")
        {
            if (ScreencastService::isRecordingActive())
            {
                std::cout << "Toggle: active recording detected, stopping...\n";
                ScreencastService::stopActiveRecording();
                return 0;
            }
            // If not active, fall through to launch GUI
            break;
        }
    }

    // Initialize GTK
    gtk_init(&argc, &argv);

    auto displayMgr = std::make_shared<DisplayManager>();
    auto audioMgr = std::make_shared<AudioManager>();
    auto settingsMgr = std::make_shared<SettingsManager>();
    auto screencastSvc = std::make_shared<ScreencastService>();

    RecorderWindow recorderWin(displayMgr, audioMgr, settingsMgr, screencastSvc);

    // Register UNIX signals for clean termination on SIGINT/SIGTERM/SIGUSR1
    g_unix_signal_add(SIGINT, onSignalReceived, &recorderWin);
    g_unix_signal_add(SIGTERM, onSignalReceived, &recorderWin);
    g_unix_signal_add(SIGUSR1, onSignalReceived, &recorderWin);

    recorderWin.show();

    gtk_main();

    return 0;
}
