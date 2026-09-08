#pragma once

#include "common.hpp"
#include "display_manager.hpp"
#include "audio_manager.hpp"
#include "settings_manager.hpp"
#include "screencast_service.hpp"
#include "region_selector.hpp"
#include "countdown_overlay.hpp"
#include <gtk/gtk.h>
#include <memory>
#include <string>

class RecorderWindow
{
public:
    RecorderWindow(std::shared_ptr<DisplayManager> displayMgr,
                   std::shared_ptr<AudioManager> audioMgr,
                   std::shared_ptr<SettingsManager> settingsMgr,
                   std::shared_ptr<ScreencastService> screencastSvc);
    ~RecorderWindow();

    void show();
    void stopRecording();
    void cancelRecording();

private:
    std::shared_ptr<DisplayManager> m_displayMgr;
    std::shared_ptr<AudioManager> m_audioMgr;
    std::shared_ptr<SettingsManager> m_settingsMgr;
    std::shared_ptr<ScreencastService> m_screencastSvc;

    std::unique_ptr<RegionSelector> m_regionSelector;
    std::unique_ptr<CountdownOverlay> m_countdownOverlay;

    GtkWidget *m_window = nullptr;
    GtkWidget *m_radioFullScreen = nullptr;
    GtkWidget *m_radioRegion = nullptr;
    GtkWidget *m_comboMonitor = nullptr;
    GtkWidget *m_monitorRow = nullptr;

    GtkWidget *m_radioAudioNone = nullptr;
    GtkWidget *m_radioAudioSystem = nullptr;
    GtkWidget *m_radioAudioMic = nullptr;
    GtkWidget *m_radioAudioBoth = nullptr;
    GtkWidget *m_comboMic = nullptr;
    GtkWidget *m_micRow = nullptr;

    GtkWidget *m_fileChooserBtn = nullptr;
    GtkWidget *m_checkClipboard = nullptr;
    GtkWidget *m_checkCountdown = nullptr;
    GtkWidget *m_btnRecord = nullptr;

    void buildUI();
    void setupSignals();
    void populateDevices();
    void onRecordClicked();
    void initiateRecording(const RecordingConfig &config);
    void startRecordingWithConfig(const RecordingConfig &config);

    static void applyCustomStyles();
};
