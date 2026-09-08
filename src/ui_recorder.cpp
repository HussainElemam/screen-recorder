#include "ui_recorder.hpp"
#include "clipboard_helper.hpp"
#include "snapshot_helper.hpp"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <unistd.h>

// ==========================================
// RecorderWindow Implementation
// ==========================================

RecorderWindow::RecorderWindow(std::shared_ptr<DisplayManager> displayMgr,
                               std::shared_ptr<AudioManager> audioMgr,
                               std::shared_ptr<SettingsManager> settingsMgr,
                               std::shared_ptr<ScreencastService> screencastSvc)
    : m_displayMgr(displayMgr),
      m_audioMgr(audioMgr),
      m_settingsMgr(settingsMgr),
      m_screencastSvc(screencastSvc)
{
    applyCustomStyles();
    buildUI();
    setupSignals();
    populateDevices();
}

RecorderWindow::~RecorderWindow()
{
    if (m_window)
    {
        gtk_widget_destroy(m_window);
        m_window = nullptr;
    }
}

void RecorderWindow::applyCustomStyles()
{
    GtkCssProvider *provider = gtk_css_provider_new();
    const char *css = R"(
        window.main-window {
            background-color: #242424;
            color: #ffffff;
        }
        .header-title {
            font-size: 18px;
            font-weight: bold;
            color: #ffffff;
        }
        .header-subtitle {
            font-size: 12px;
            color: #aaaaaa;
        }
        .section-frame {
            background-color: #2c2c2e;
            border-radius: 10px;
            padding: 14px;
            margin-bottom: 8px;
            border: 1px solid #3a3a3c;
        }
        .section-title {
            font-size: 13px;
            font-weight: bold;
            color: #78aeed;
            margin-bottom: 6px;
        }
        button.record-button {
            font-weight: bold;
            font-size: 15px;
            border-radius: 8px;
            padding: 10px 24px;
        }
    )";

    gtk_css_provider_load_from_data(provider, css, -1, nullptr);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(provider);
}

void RecorderWindow::buildUI()
{
    m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(m_window), "Screen Recorder");
    gtk_window_set_default_size(GTK_WINDOW(m_window), 460, 480);
    gtk_window_set_position(GTK_WINDOW(m_window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(m_window), 16);

    GtkStyleContext *winCtx = gtk_widget_get_style_context(m_window);
    gtk_style_context_add_class(winCtx, "main-window");

    GtkWidget *mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_container_add(GTK_CONTAINER(m_window), mainBox);

    // --- Header ---
    GtkWidget *headerBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *titleLabel = gtk_label_new("Screen Recorder");
    gtk_widget_set_halign(titleLabel, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(titleLabel), "header-title");

    GtkWidget *subtitleLabel = gtk_label_new("Native Screen & Audio Recorder (Press Super+Shift+R to stop)");
    gtk_widget_set_halign(subtitleLabel, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitleLabel), "header-subtitle");

    gtk_box_pack_start(GTK_BOX(headerBox), titleLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(headerBox), subtitleLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), headerBox, FALSE, FALSE, 0);

    // --- Capture Area Section ---
    GtkWidget *areaBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(areaBox), "section-frame");

    GtkWidget *areaTitle = gtk_label_new("CAPTURE MODE");
    gtk_widget_set_halign(areaTitle, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(areaTitle), "section-title");
    gtk_box_pack_start(GTK_BOX(areaBox), areaTitle, FALSE, FALSE, 0);

    GtkWidget *modeBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    m_radioFullScreen = gtk_radio_button_new_with_label(nullptr, "Full Screen");
    m_radioRegion = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(m_radioFullScreen), "Select a Region");
    gtk_box_pack_start(GTK_BOX(modeBox), m_radioFullScreen, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(modeBox), m_radioRegion, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(areaBox), modeBox, FALSE, FALSE, 4);

    // Screen selection (dropdown)
    m_monitorRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *monLabel = gtk_label_new("Screen:");
    gtk_widget_set_halign(monLabel, GTK_ALIGN_START);
    m_comboMonitor = gtk_combo_box_text_new();
    gtk_widget_set_hexpand(m_comboMonitor, TRUE);
    gtk_box_pack_start(GTK_BOX(m_monitorRow), monLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(m_monitorRow), m_comboMonitor, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(areaBox), m_monitorRow, FALSE, FALSE, 2);

    gtk_box_pack_start(GTK_BOX(mainBox), areaBox, FALSE, FALSE, 0);

    // --- Audio Source Section ---
    GtkWidget *audioBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(audioBox), "section-frame");

    GtkWidget *audioTitle = gtk_label_new("AUDIO SOURCE");
    gtk_widget_set_halign(audioTitle, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(audioTitle), "section-title");
    gtk_box_pack_start(GTK_BOX(audioBox), audioTitle, FALSE, FALSE, 0);

    GtkWidget *audioGrid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(audioGrid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(audioGrid), 8);

    m_radioAudioNone = gtk_radio_button_new_with_label(nullptr, "None (Mute)");
    m_radioAudioSystem = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(m_radioAudioNone), "System Audio");
    m_radioAudioMic = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(m_radioAudioNone), "Microphone");
    m_radioAudioBoth = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(m_radioAudioNone), "Both (System + Mic)");

    gtk_grid_attach(GTK_GRID(audioGrid), m_radioAudioNone, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(audioGrid), m_radioAudioSystem, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(audioGrid), m_radioAudioMic, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(audioGrid), m_radioAudioBoth, 1, 1, 1, 1);
    gtk_box_pack_start(GTK_BOX(audioBox), audioGrid, FALSE, FALSE, 4);

    // Microphone selection row
    m_micRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *micLabel = gtk_label_new("Microphone:");
    gtk_widget_set_halign(micLabel, GTK_ALIGN_START);
    m_comboMic = gtk_combo_box_text_new();
    gtk_widget_set_hexpand(m_comboMic, TRUE);
    gtk_box_pack_start(GTK_BOX(m_micRow), micLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(m_micRow), m_comboMic, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(audioBox), m_micRow, FALSE, FALSE, 2);

    gtk_box_pack_start(GTK_BOX(mainBox), audioBox, FALSE, FALSE, 0);

    // --- Output & Options Section ---
    GtkWidget *optBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(optBox), "section-frame");

    GtkWidget *optTitle = gtk_label_new("OUTPUT & DESTINATION");
    gtk_widget_set_halign(optTitle, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(optTitle), "section-title");
    gtk_box_pack_start(GTK_BOX(optBox), optTitle, FALSE, FALSE, 0);

    GtkWidget *dirRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *dirLabel = gtk_label_new("Save to:");
    m_fileChooserBtn = gtk_file_chooser_button_new("Select Output Folder", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_widget_set_hexpand(m_fileChooserBtn, TRUE);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(m_fileChooserBtn), m_settingsMgr->getOutputDirectory().c_str());

    gtk_box_pack_start(GTK_BOX(dirRow), dirLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dirRow), m_fileChooserBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(optBox), dirRow, FALSE, FALSE, 2);

    m_checkClipboard = gtk_check_button_new_with_label("Copy recorded video to clipboard (for WhatsApp, chatbots, etc.)");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_checkClipboard), m_settingsMgr->getCopyToClipboard());
    gtk_box_pack_start(GTK_BOX(optBox), m_checkClipboard, FALSE, FALSE, 2);

    m_checkCountdown = gtk_check_button_new_with_label("3-second countdown before recording starts");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_checkCountdown), m_settingsMgr->getEnableCountdown());
    gtk_box_pack_start(GTK_BOX(optBox), m_checkCountdown, FALSE, FALSE, 2);

    gtk_box_pack_start(GTK_BOX(mainBox), optBox, FALSE, FALSE, 0);

    // --- Action Buttons ---
    GtkWidget *actionBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_set_halign(actionBox, GTK_ALIGN_END);

    GtkWidget *btnCancel = gtk_button_new_with_label("Cancel");
    g_signal_connect_swapped(btnCancel, "clicked", G_CALLBACK(gtk_main_quit), nullptr);

    m_btnRecord = gtk_button_new_with_label("● Start Recording");
    GtkStyleContext *recCtx = gtk_widget_get_style_context(m_btnRecord);
    gtk_style_context_add_class(recCtx, "suggested-action");
    gtk_style_context_add_class(recCtx, "record-button");

    gtk_box_pack_start(GTK_BOX(actionBox), btnCancel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(actionBox), m_btnRecord, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), actionBox, FALSE, FALSE, 4);

    g_signal_connect(m_window, "destroy", G_CALLBACK(gtk_main_quit), nullptr);
}

void RecorderWindow::setupSignals()
{
    g_signal_connect_swapped(m_radioAudioNone, "toggled", G_CALLBACK(+[](RecorderWindow *w)
                                                                     {
        gboolean needMic = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->m_radioAudioMic)) ||
                           gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->m_radioAudioBoth));
        gtk_widget_set_sensitive(w->m_micRow, needMic); }),
                             this);
    g_signal_connect_swapped(m_radioAudioSystem, "toggled", G_CALLBACK(+[](RecorderWindow *w)
                                                                       {
        gboolean needMic = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->m_radioAudioMic)) ||
                           gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->m_radioAudioBoth));
        gtk_widget_set_sensitive(w->m_micRow, needMic); }),
                             this);
    g_signal_connect_swapped(m_radioAudioMic, "toggled", G_CALLBACK(+[](RecorderWindow *w)
                                                                    {
        gboolean needMic = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->m_radioAudioMic)) ||
                           gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->m_radioAudioBoth));
        gtk_widget_set_sensitive(w->m_micRow, needMic); }),
                             this);
    g_signal_connect_swapped(m_radioAudioBoth, "toggled", G_CALLBACK(+[](RecorderWindow *w)
                                                                     {
        gboolean needMic = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->m_radioAudioMic)) ||
                           gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->m_radioAudioBoth));
        gtk_widget_set_sensitive(w->m_micRow, needMic); }),
                             this);

    // Record button clicked
    g_signal_connect_swapped(m_btnRecord, "clicked", G_CALLBACK(+[](RecorderWindow *w)
                                                                { w->onRecordClicked(); }),
                             this);

    gboolean needMic = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_radioAudioMic)) ||
                       gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_radioAudioBoth));
    gtk_widget_set_sensitive(m_micRow, needMic);
}

void RecorderWindow::populateDevices()
{
    // Populate Monitors
    const auto &monitors = m_displayMgr->getMonitors();
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(m_comboMonitor));

    for (size_t i = 0; i < monitors.size(); ++i)
    {
        const auto &mon = monitors[i];
        std::string label = mon.displayName;
        if (!mon.connector.empty())
        {
            label += " (" + mon.connector + " - " + std::to_string(mon.width) + "x" + std::to_string(mon.height) + ")";
        }
        if (mon.isPrimary)
        {
            label += " [Primary]";
        }
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_comboMonitor), std::to_string(i).c_str(), label.c_str());
    }

    if (!monitors.empty())
    {
        gtk_combo_box_set_active(GTK_COMBO_BOX(m_comboMonitor), 0);
    }

    // Populate Microphones
    const auto &mics = m_audioMgr->getMicrophones();
    gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(m_comboMic));

    std::string savedMic = m_settingsMgr->getLastMicrophone();
    int activeMicIndex = 0;

    for (size_t i = 0; i < mics.size(); ++i)
    {
        const auto &mic = mics[i];
        std::string desc = mic.description.empty() ? mic.name : mic.description;
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_comboMic), mic.name.c_str(), desc.c_str());
        if (!savedMic.empty() && mic.name == savedMic)
        {
            activeMicIndex = static_cast<int>(i);
        }
    }

    if (!mics.empty())
    {
        gtk_combo_box_set_active(GTK_COMBO_BOX(m_comboMic), activeMicIndex);
    }
    else
    {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(m_comboMic), "default", "Default Microphone");
        gtk_combo_box_set_active(GTK_COMBO_BOX(m_comboMic), 0);
    }

    // Restore saved audio mode
    AudioMode savedMode = m_settingsMgr->getAudioMode();
    switch (savedMode)
    {
    case AudioMode::None:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_radioAudioNone), TRUE);
        break;
    case AudioMode::System:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_radioAudioSystem), TRUE);
        break;
    case AudioMode::Mic:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_radioAudioMic), TRUE);
        break;
    case AudioMode::Both:
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(m_radioAudioBoth), TRUE);
        break;
    }
}

void RecorderWindow::show()
{
    gtk_widget_show_all(m_window);
}

void RecorderWindow::onRecordClicked()
{
    RecordingConfig config;

    // Output Directory
    gchar *chosenFolder = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(m_fileChooserBtn));
    if (chosenFolder)
    {
        config.outputDir = chosenFolder;
        g_free(chosenFolder);
    }
    else
    {
        config.outputDir = m_settingsMgr->getOutputDirectory();
    }
    m_settingsMgr->setOutputDirectory(config.outputDir);

    // Clipboard Option
    config.copyToClipboard = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_checkClipboard));
    m_settingsMgr->setCopyToClipboard(config.copyToClipboard);

    // Countdown Option
    config.enableCountdown = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_checkCountdown));
    m_settingsMgr->setEnableCountdown(config.enableCountdown);

    // Audio Mode
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_radioAudioSystem)))
    {
        config.audioMode = AudioMode::System;
    }
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_radioAudioMic)))
    {
        config.audioMode = AudioMode::Mic;
    }
    else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_radioAudioBoth)))
    {
        config.audioMode = AudioMode::Both;
    }
    else
    {
        config.audioMode = AudioMode::None;
    }
    m_settingsMgr->setAudioMode(config.audioMode);

    // Microphone Device
    const gchar *selectedMicId = gtk_combo_box_get_active_id(GTK_COMBO_BOX(m_comboMic));
    if (selectedMicId)
    {
        config.micDeviceName = selectedMicId;
        m_settingsMgr->setLastMicrophone(config.micDeviceName);
    }

    // Monitor Selection
    int monIdx = gtk_combo_box_get_active(GTK_COMBO_BOX(m_comboMonitor));
    if (monIdx < 0)
        monIdx = 0;
    config.monitorIndex = monIdx;

    const MonitorInfo *monInfo = m_displayMgr->getMonitorByIndex(monIdx);
    if (monInfo)
    {
        config.monitorConnector = monInfo->connector;
    }

    m_settingsMgr->save();

    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(m_radioFullScreen)))
    {
        config.captureMode = CaptureMode::FullScreen;
        gtk_widget_hide(m_window);
        initiateRecording(config);
    }
    else
    {
        // Region Mode: Hide main dialog, take instant monitor snapshot, show region selector
        config.captureMode = CaptureMode::Region;
        gtk_widget_hide(m_window);

        // Process pending GTK events to ensure the window is unmapped before snapshotting
        while (gtk_events_pending())
        {
            gtk_main_iteration();
        }
        gdk_display_sync(gdk_display_get_default());
        usleep(50000); // 50ms pause for Mutter compositor to repaint the vacated area

        if (!monInfo)
        {
            std::cerr << "No monitor info found for region selector!" << std::endl;
            gtk_main_quit();
            return;
        }

        // Capture instant snapshot of the target monitor
        cairo_surface_t *bgSurface = SnapshotHelper::captureMonitor(monInfo->connector);

        m_regionSelector = std::make_unique<RegionSelector>(
            *monInfo,
            bgSurface,
            [this, config](int x, int y, int w, int h) mutable
            {
                config.regionX = x;
                config.regionY = y;
                config.regionWidth = w;
                config.regionHeight = h;
                initiateRecording(config);
            },
            [this]()
            {
                // User pressed Escape: restore main window
                gtk_widget_show_all(m_window);
            });
        m_regionSelector->show();
    }
}

void RecorderWindow::initiateRecording(const RecordingConfig &config)
{
    if (config.enableCountdown)
    {
        m_countdownOverlay = std::make_unique<CountdownOverlay>(
            3,
            [this, config]()
            {
                m_countdownOverlay.reset();
                startRecordingWithConfig(config);
            },
            [this]()
            {
                m_countdownOverlay.reset();
                // User pressed Escape during countdown: restore main window
                gtk_widget_show_all(m_window);
            });
        m_countdownOverlay->start();
    }
    else
    {
        startRecordingWithConfig(config);
    }
}

void RecorderWindow::startRecordingWithConfig(const RecordingConfig &config)
{
    std::string audioFragment = m_audioMgr->buildAudioPipeline(
        config.audioMode,
        config.micDeviceName,
        "");

    bool started = m_screencastSvc->startRecording(
        config,
        audioFragment,
        [](const std::string &status)
        {
            std::cout << "Status: " << status << std::endl;
        },
        [](const std::string &filePath, bool success)
        {
            if (success)
            {
                std::cout << "Recording completed: " << filePath << std::endl;
            }
            gtk_main_quit();
        });

    if (!started)
    {
        std::cerr << "Failed to start recording session!" << std::endl;
        gtk_main_quit();
        return;
    }

    // No intrusive overlay is created.
    // The user stops the recording cleanly by pressing the keyboard shortcut (Super+Shift+R)
    // or by running screen-recorder --stop / --toggle.
}

void RecorderWindow::stopRecording()
{
    m_screencastSvc->stopRecording();
}

void RecorderWindow::cancelRecording()
{
    m_screencastSvc->stopRecording();
    gtk_main_quit();
}
