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
    // Ensure dark theme is preferred for window decorations and Adwaita widgets
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);

    GtkCssProvider *provider = gtk_css_provider_new();
    const char *css = R"(
        * {
            font-family: -apple-system, BlinkMacSystemFont, "Cantarell", "Ubuntu", "Segoe UI", "DejaVu Sans", sans-serif;
        }

        /* Modern Clean Adwaita-Dark Aesthetic */
        window.main-window {
            background-color: #1e1e22;
            color: #f2f2f7;
        }

        .header-title {
            font-size: 16px;
            font-weight: 700;
            color: #ffffff;
            letter-spacing: -0.2px;
        }

        .header-dot {
            color: #e04038;
            font-size: 14px;
        }

        .header-subtitle {
            font-size: 11px;
            color: #8e8e93;
            font-weight: 500;
        }

        .card-frame {
            background-color: #26262a;
            border-radius: 12px;
            padding: 12px 14px;
            border: 1px solid rgba(255, 255, 255, 0.07);
        }

        .card-label {
            font-size: 11px;
            font-weight: 700;
            color: #8e8e93;
            letter-spacing: 0.6px;
            margin-bottom: 6px;
        }

        /* Modern Segmented Control */
        .segmented-group {
            background-color: #18181a;
            border-radius: 9px;
            padding: 3px;
            border: 1px solid rgba(255, 255, 255, 0.05);
        }

        .segmented-group button {
            background-image: none;
            background-color: transparent;
            border: none;
            border-radius: 7px;
            color: #a1a1aa;
            font-size: 13px;
            font-weight: 500;
            padding: 7px 12px;
            box-shadow: none;
            outline: none;
            transition: all 150ms ease;
        }

        .segmented-group button label {
            color: #a1a1aa;
            font-size: 13px;
            font-weight: 500;
        }

        .segmented-group button:hover {
            background-image: none;
            background-color: rgba(255, 255, 255, 0.06);
            color: #ffffff;
        }

        .segmented-group button:hover label {
            color: #ffffff;
        }

        .segmented-group button:checked {
            background-image: none;
            background-color: #3584e4;
            color: #ffffff;
            font-weight: 600;
            box-shadow: 0 1px 4px rgba(0, 0, 0, 0.35);
        }

        .segmented-group button:checked label {
            color: #ffffff;
            font-weight: 600;
        }

        /* Options Row and Switches */
        .option-row {
            padding: 3px 0;
        }

        .option-label {
            font-size: 13px;
            color: #d1d1d6;
            font-weight: 500;
        }

        /* Dropdowns & File Chooser */
        combobox button,
        filechooserbutton button {
            background-image: none;
            background-color: #18181a;
            border: 1px solid rgba(255, 255, 255, 0.10);
            border-radius: 8px;
            color: #ffffff;
            font-size: 13px;
            padding: 6px 10px;
            box-shadow: none;
        }

        combobox button:hover,
        filechooserbutton button:hover {
            background-image: none;
            background-color: #222226;
            border-color: rgba(255, 255, 255, 0.16);
        }

        combobox cellview,
        combobox label,
        filechooserbutton label {
            color: #ffffff;
            font-size: 13px;
        }

        /* Action Buttons */
        button.button-record {
            background-image: none;
            background-color: #e04038;
            color: #ffffff;
            font-weight: 700;
            font-size: 13px;
            border-radius: 10px;
            padding: 9px 20px;
            border: none;
            box-shadow: 0 2px 8px rgba(224, 64, 56, 0.35);
            transition: all 150ms ease;
        }

        button.button-record label {
            color: #ffffff;
            font-weight: 700;
            font-size: 13px;
        }

        button.button-record:hover {
            background-image: none;
            background-color: #ea4e47;
            box-shadow: 0 3px 12px rgba(224, 64, 56, 0.5);
        }

        button.button-record:active {
            background-image: none;
            background-color: #c8322b;
        }

        button.button-secondary {
            background-image: none;
            background-color: rgba(255, 255, 255, 0.08);
            color: #d1d1d6;
            font-weight: 500;
            font-size: 13px;
            border-radius: 10px;
            padding: 9px 18px;
            border: none;
            transition: all 150ms ease;
        }

        button.button-secondary label {
            color: #d1d1d6;
            font-weight: 500;
            font-size: 13px;
        }

        button.button-secondary:hover {
            background-image: none;
            background-color: rgba(255, 255, 255, 0.14);
            color: #ffffff;
        }

        button.button-secondary:hover label {
            color: #ffffff;
        }
    )";

    gtk_css_provider_load_from_data(provider, css, -1, nullptr);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(provider);
}

void RecorderWindow::buildUI()
{
    m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(m_window), "Screen Recorder");
    gtk_window_set_default_size(GTK_WINDOW(m_window), 410, -1);
    gtk_window_set_resizable(GTK_WINDOW(m_window), FALSE);
    gtk_window_set_position(GTK_WINDOW(m_window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width(GTK_CONTAINER(m_window), 18);

    GtkStyleContext *winCtx = gtk_widget_get_style_context(m_window);
    gtk_style_context_add_class(winCtx, "main-window");

    GtkWidget *mainBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 14);
    gtk_container_add(GTK_CONTAINER(m_window), mainBox);

    // --- Modern Header ---
    GtkWidget *headerBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *headerLeft = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    GtkWidget *dotLabel = gtk_label_new("●");
    gtk_style_context_add_class(gtk_widget_get_style_context(dotLabel), "header-dot");
    GtkWidget *titleLabel = gtk_label_new("Screen Recorder");
    gtk_style_context_add_class(gtk_widget_get_style_context(titleLabel), "header-title");
    gtk_box_pack_start(GTK_BOX(headerLeft), dotLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(headerLeft), titleLabel, FALSE, FALSE, 0);

    GtkWidget *subtitleLabel = gtk_label_new("Stop: Super+Shift+R");
    gtk_widget_set_halign(subtitleLabel, GTK_ALIGN_END);
    gtk_style_context_add_class(gtk_widget_get_style_context(subtitleLabel), "header-subtitle");

    gtk_box_pack_start(GTK_BOX(headerBox), headerLeft, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(headerBox), subtitleLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(mainBox), headerBox, FALSE, FALSE, 0);

    // --- Section 1: Capture Mode (Segmented Control) ---
    GtkWidget *areaBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(areaBox), "card-frame");

    GtkWidget *areaTitle = gtk_label_new("CAPTURE MODE");
    gtk_widget_set_halign(areaTitle, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(areaTitle), "card-label");
    gtk_box_pack_start(GTK_BOX(areaBox), areaTitle, FALSE, FALSE, 0);

    GtkWidget *modeGroup = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(modeGroup), "segmented-group");
    gtk_style_context_add_class(gtk_widget_get_style_context(modeGroup), "linked");

    m_radioFullScreen = gtk_radio_button_new_with_label(nullptr, "Full Screen");
    m_radioRegion = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(m_radioFullScreen), "Area / Region");
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(m_radioFullScreen), FALSE);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(m_radioRegion), FALSE);
    gtk_widget_set_hexpand(m_radioFullScreen, TRUE);
    gtk_widget_set_hexpand(m_radioRegion, TRUE);

    gtk_box_pack_start(GTK_BOX(modeGroup), m_radioFullScreen, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(modeGroup), m_radioRegion, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(areaBox), modeGroup, FALSE, FALSE, 2);

    // Screen selection row
    m_monitorRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *monLabel = gtk_label_new("Screen:");
    gtk_widget_set_halign(monLabel, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(monLabel), "option-label");

    m_comboMonitor = gtk_combo_box_text_new();
    gtk_widget_set_hexpand(m_comboMonitor, TRUE);
    gtk_box_pack_start(GTK_BOX(m_monitorRow), monLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(m_monitorRow), m_comboMonitor, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(areaBox), m_monitorRow, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(mainBox), areaBox, FALSE, FALSE, 0);

    // --- Section 2: Audio Source (Segmented Control) ---
    GtkWidget *audioBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(audioBox), "card-frame");

    GtkWidget *audioTitle = gtk_label_new("AUDIO SOURCE");
    gtk_widget_set_halign(audioTitle, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(audioTitle), "card-label");
    gtk_box_pack_start(GTK_BOX(audioBox), audioTitle, FALSE, FALSE, 0);

    GtkWidget *audioGroup = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_style_context_add_class(gtk_widget_get_style_context(audioGroup), "segmented-group");
    gtk_style_context_add_class(gtk_widget_get_style_context(audioGroup), "linked");

    m_radioAudioNone = gtk_radio_button_new_with_label(nullptr, "None");
    m_radioAudioSystem = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(m_radioAudioNone), "System Audio");
    m_radioAudioMic = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(m_radioAudioNone), "Microphone");
    m_radioAudioBoth = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(m_radioAudioNone), "Both");

    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(m_radioAudioNone), FALSE);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(m_radioAudioSystem), FALSE);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(m_radioAudioMic), FALSE);
    gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(m_radioAudioBoth), FALSE);

    gtk_widget_set_hexpand(m_radioAudioNone, TRUE);
    gtk_widget_set_hexpand(m_radioAudioSystem, TRUE);
    gtk_widget_set_hexpand(m_radioAudioMic, TRUE);
    gtk_widget_set_hexpand(m_radioAudioBoth, TRUE);

    gtk_box_pack_start(GTK_BOX(audioGroup), m_radioAudioNone, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(audioGroup), m_radioAudioSystem, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(audioGroup), m_radioAudioMic, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(audioGroup), m_radioAudioBoth, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(audioBox), audioGroup, FALSE, FALSE, 2);

    // Microphone selection row
    m_micRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *micLabel = gtk_label_new("Microphone:");
    gtk_widget_set_halign(micLabel, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(micLabel), "option-label");

    m_comboMic = gtk_combo_box_text_new();
    gtk_widget_set_hexpand(m_comboMic, TRUE);
    gtk_box_pack_start(GTK_BOX(m_micRow), micLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(m_micRow), m_comboMic, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(audioBox), m_micRow, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(mainBox), audioBox, FALSE, FALSE, 0);

    // --- Section 3: Options Card ---
    GtkWidget *optBox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_style_context_add_class(gtk_widget_get_style_context(optBox), "card-frame");

    GtkWidget *optTitle = gtk_label_new("SETTINGS & DESTINATION");
    gtk_widget_set_halign(optTitle, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(optTitle), "card-label");
    gtk_box_pack_start(GTK_BOX(optBox), optTitle, FALSE, FALSE, 0);

    // Save to row
    GtkWidget *dirRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(dirRow), "option-row");
    GtkWidget *dirLabel = gtk_label_new("Save to:");
    gtk_widget_set_halign(dirLabel, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(dirLabel), "option-label");

    m_fileChooserBtn = gtk_file_chooser_button_new("Select Output Folder", GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_widget_set_hexpand(m_fileChooserBtn, TRUE);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(m_fileChooserBtn), m_settingsMgr->getOutputDirectory().c_str());

    gtk_box_pack_start(GTK_BOX(dirRow), dirLabel, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(dirRow), m_fileChooserBtn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(optBox), dirRow, FALSE, FALSE, 2);

    // 3-second Countdown Switch Row
    GtkWidget *countRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(countRow), "option-row");
    GtkWidget *countLabel = gtk_label_new("3-second animated countdown");
    gtk_widget_set_halign(countLabel, GTK_ALIGN_START);
    gtk_widget_set_hexpand(countLabel, TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(countLabel), "option-label");

    m_switchCountdown = gtk_switch_new();
    gtk_widget_set_valign(m_switchCountdown, GTK_ALIGN_CENTER);
    gtk_switch_set_active(GTK_SWITCH(m_switchCountdown), m_settingsMgr->getEnableCountdown());

    gtk_box_pack_start(GTK_BOX(countRow), countLabel, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(countRow), m_switchCountdown, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(optBox), countRow, FALSE, FALSE, 2);

    // Copy to Clipboard Switch Row
    GtkWidget *clipRow = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(clipRow), "option-row");
    GtkWidget *clipLabel = gtk_label_new("Auto-copy video to clipboard");
    gtk_widget_set_halign(clipLabel, GTK_ALIGN_START);
    gtk_widget_set_hexpand(clipLabel, TRUE);
    gtk_style_context_add_class(gtk_widget_get_style_context(clipLabel), "option-label");

    m_switchClipboard = gtk_switch_new();
    gtk_widget_set_valign(m_switchClipboard, GTK_ALIGN_CENTER);
    gtk_switch_set_active(GTK_SWITCH(m_switchClipboard), m_settingsMgr->getCopyToClipboard());

    gtk_box_pack_start(GTK_BOX(clipRow), clipLabel, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(clipRow), m_switchClipboard, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(optBox), clipRow, FALSE, FALSE, 2);

    gtk_box_pack_start(GTK_BOX(mainBox), optBox, FALSE, FALSE, 0);

    // --- Action Buttons ---
    GtkWidget *actionBox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(actionBox, GTK_ALIGN_END);

    GtkWidget *btnCancel = gtk_button_new_with_label("Cancel");
    gtk_style_context_add_class(gtk_widget_get_style_context(btnCancel), "button-secondary");
    g_signal_connect_swapped(btnCancel, "clicked", G_CALLBACK(gtk_main_quit), nullptr);

    m_btnRecord = gtk_button_new_with_label("Start Recording");
    gtk_style_context_add_class(gtk_widget_get_style_context(m_btnRecord), "button-record");

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
                                                                         gtk_widget_set_sensitive(w->m_micRow, needMic);
                                                                     }),
                             this);

    g_signal_connect_swapped(m_radioAudioSystem, "toggled", G_CALLBACK(+[](RecorderWindow *w)
                                                                       {
                                                                           gboolean needMic = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->m_radioAudioMic)) ||
                                                                                              gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->m_radioAudioBoth));
                                                                           gtk_widget_set_sensitive(w->m_micRow, needMic);
                                                                       }),
                             this);

    g_signal_connect_swapped(m_radioAudioMic, "toggled", G_CALLBACK(+[](RecorderWindow *w)
                                                                    {
                                                                        gboolean needMic = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->m_radioAudioMic)) ||
                                                                                           gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->m_radioAudioBoth));
                                                                        gtk_widget_set_sensitive(w->m_micRow, needMic);
                                                                    }),
                             this);

    g_signal_connect_swapped(m_radioAudioBoth, "toggled", G_CALLBACK(+[](RecorderWindow *w)
                                                                     {
                                                                         gboolean needMic = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->m_radioAudioMic)) ||
                                                                                            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(w->m_radioAudioBoth));
                                                                         gtk_widget_set_sensitive(w->m_micRow, needMic);
                                                                     }),
                             this);

    // Record button clicked
    g_signal_connect_swapped(m_btnRecord, "clicked", G_CALLBACK(+[](RecorderWindow *w)
                                                                {
                                                                    w->onRecordClicked();
                                                                }),
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

    if (m_switchClipboard)
    {
        gtk_switch_set_active(GTK_SWITCH(m_switchClipboard), m_settingsMgr->getCopyToClipboard());
    }
    if (m_switchCountdown)
    {
        gtk_switch_set_active(GTK_SWITCH(m_switchCountdown), m_settingsMgr->getEnableCountdown());
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
    config.copyToClipboard = gtk_switch_get_active(GTK_SWITCH(m_switchClipboard));
    m_settingsMgr->setCopyToClipboard(config.copyToClipboard);

    // Countdown Option
    config.enableCountdown = gtk_switch_get_active(GTK_SWITCH(m_switchCountdown));
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
            else
            {
                std::cerr << "Recording failed or canceled." << std::endl;
            }
            gtk_main_quit();
        });

    if (!started)
    {
        std::cerr << "Failed to start recording!" << std::endl;
        gtk_widget_show_all(m_window);
    }
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
