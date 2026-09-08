#include "screencast_service.hpp"
#include "clipboard_helper.hpp"
#include <gtk/gtk.h>
#include <glib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>

ScreencastService::ScreencastService()
{
}

ScreencastService::~ScreencastService()
{
    stopRecording();
    if (m_busConnection)
    {
        g_object_unref(m_busConnection);
        m_busConnection = nullptr;
    }
}

std::string ScreencastService::getPidFilePath()
{
    const char *runtimeDir = g_get_user_runtime_dir();
    if (runtimeDir && access(runtimeDir, W_OK) == 0)
    {
        return std::string(runtimeDir) + "/screen-recorder.pid";
    }
    const char *cacheDir = g_get_user_cache_dir();
    std::string fallbackDir = std::string(cacheDir) + "/screen-recorder";
    g_mkdir_with_parents(fallbackDir.c_str(), 0755);
    return fallbackDir + "/screen-recorder.pid";
}

bool ScreencastService::isRecordingActive()
{
    pid_t pid = getActiveRecordingPid();
    return pid > 0;
}

pid_t ScreencastService::getActiveRecordingPid()
{
    std::string pidFile = getPidFilePath();
    std::ifstream file(pidFile);
    if (!file.is_open())
    {
        return -1;
    }

    pid_t pid = -1;
    file >> pid;
    if (pid <= 0)
    {
        removePidFile();
        return -1;
    }

    // Check if process is actually running
    if (kill(pid, 0) == 0)
    {
        return pid;
    }

    // Process is dead, clean up stale PID file
    removePidFile();
    return -1;
}

bool ScreencastService::stopActiveRecording()
{
    pid_t pid = getActiveRecordingPid();
    if (pid > 0)
    {
        std::cout << "Stopping active recording process PID: " << pid << std::endl;
        kill(pid, SIGINT);
        removePidFile();
        return true;
    }
    return false;
}

void ScreencastService::writePidFile()
{
    std::string pidFile = getPidFilePath();
    std::ofstream file(pidFile);
    if (file.is_open())
    {
        file << getpid() << "\n";
    }
}

void ScreencastService::removePidFile()
{
    std::string pidFile = getPidFilePath();
    unlink(pidFile.c_str());
}

bool ScreencastService::startRecording(const RecordingConfig &config,
                                       const std::string &audioPipelineFragment,
                                       StatusCallback onStatus,
                                       FinishedCallback onFinished)
{
    m_config = config;
    m_audioPipelineFragment = audioPipelineFragment;
    m_onStatus = onStatus;
    m_onFinished = onFinished;

    GError *error = nullptr;
    m_busConnection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!m_busConnection)
    {
        std::cerr << "Failed to connect to D-Bus: " << (error ? error->message : "unknown") << std::endl;
        if (error)
            g_error_free(error);
        return false;
    }

    // 1. CreateSession on org.gnome.Mutter.ScreenCast
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE_VARDICT);

    GVariant *reply = g_dbus_connection_call_sync(
        m_busConnection,
        "org.gnome.Mutter.ScreenCast",
        "/org/gnome/Mutter/ScreenCast",
        "org.gnome.Mutter.ScreenCast",
        "CreateSession",
        g_variant_new("(a{sv})", &b),
        G_VARIANT_TYPE("(o)"),
        G_DBUS_CALL_FLAGS_NONE,
        2000,
        nullptr,
        &error);

    if (!reply)
    {
        std::cerr << "Mutter CreateSession failed: " << (error ? error->message : "unknown") << std::endl;
        if (error)
            g_error_free(error);
        return false;
    }

    const gchar *sessPath = nullptr;
    g_variant_get(reply, "(&o)", &sessPath);
    m_sessionPath = sessPath ? sessPath : "";
    g_variant_unref(reply);

    if (m_sessionPath.empty())
    {
        return false;
    }

    // 2. Setup stream options
    GVariantBuilder streamOpts;
    g_variant_builder_init(&streamOpts, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&streamOpts, "{sv}", "cursor-mode", g_variant_new_uint32(1)); // Embedded cursor

    GVariant *streamReply = nullptr;

    if (m_config.captureMode == CaptureMode::FullScreen)
    {
        std::string connector = m_config.monitorConnector;
        if (connector.empty())
            connector = "DP-1";

        streamReply = g_dbus_connection_call_sync(
            m_busConnection,
            "org.gnome.Mutter.ScreenCast",
            m_sessionPath.c_str(),
            "org.gnome.Mutter.ScreenCast.Session",
            "RecordMonitor",
            g_variant_new("(sa{sv})", connector.c_str(), &streamOpts),
            G_VARIANT_TYPE("(o)"),
            G_DBUS_CALL_FLAGS_NONE,
            2000,
            nullptr,
            &error);
    }
    else
    {
        // Region recording
        streamReply = g_dbus_connection_call_sync(
            m_busConnection,
            "org.gnome.Mutter.ScreenCast",
            m_sessionPath.c_str(),
            "org.gnome.Mutter.ScreenCast.Session",
            "RecordArea",
            g_variant_new("(iiiia{sv})", m_config.regionX, m_config.regionY,
                          m_config.regionWidth, m_config.regionHeight, &streamOpts),
            G_VARIANT_TYPE("(o)"),
            G_DBUS_CALL_FLAGS_NONE,
            2000,
            nullptr,
            &error);
    }

    if (!streamReply)
    {
        std::cerr << "Mutter RecordMonitor/Area failed: " << (error ? error->message : "unknown") << std::endl;
        if (error)
            g_error_free(error);
        return false;
    }

    const gchar *sPath = nullptr;
    g_variant_get(streamReply, "(&o)", &sPath);
    m_streamPath = sPath ? sPath : "";
    g_variant_unref(streamReply);

    if (m_streamPath.empty())
    {
        return false;
    }

    // 3. Subscribe to PipeWireStreamAdded signal on streamPath
    m_signalSubId = g_dbus_connection_signal_subscribe(
        m_busConnection,
        "org.gnome.Mutter.ScreenCast",
        "org.gnome.Mutter.ScreenCast.Stream",
        "PipeWireStreamAdded",
        m_streamPath.c_str(),
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        gdbusSignalHandler,
        this,
        nullptr);

    // 4. Start the session
    GVariant *startReply = g_dbus_connection_call_sync(
        m_busConnection,
        "org.gnome.Mutter.ScreenCast",
        m_sessionPath.c_str(),
        "org.gnome.Mutter.ScreenCast.Session",
        "Start",
        nullptr,
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        2000,
        nullptr,
        &error);

    if (!startReply)
    {
        std::cerr << "Mutter Session Start failed: " << (error ? error->message : "unknown") << std::endl;
        if (error)
            g_error_free(error);
        return false;
    }
    g_variant_unref(startReply);

    m_isRecording = true;
    writePidFile();

    if (m_onStatus)
    {
        m_onStatus("Initializing capture stream...");
    }

    return true;
}

void ScreencastService::gdbusSignalHandler(GDBusConnection * /*connection*/,
                                           const gchar * /*sender_name*/,
                                           const gchar * /*object_path*/,
                                           const gchar * /*interface_name*/,
                                           const gchar *signal_name,
                                           GVariant *parameters,
                                           gpointer user_data)
{
    if (g_strcmp0(signal_name, "PipeWireStreamAdded") == 0)
    {
        guint32 nodeId = 0;
        g_variant_get(parameters, "(u)", &nodeId);
        auto *self = static_cast<ScreencastService *>(user_data);
        self->onPipeWireStreamAdded(nodeId);
    }
}

void ScreencastService::onPipeWireStreamAdded(guint32 nodeId)
{
    std::cout << "ScreenCast: Received PipeWire Node ID: " << nodeId << std::endl;
    launchPipeline(nodeId);
}

void ScreencastService::launchPipeline(guint32 nodeId)
{
    // Generate output file path with timestamp
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&t);

    std::ostringstream ss;
    ss << "recording-"
       << std::setfill('0') << std::setw(4) << (tm.tm_year + 1900) << "-"
       << std::setfill('0') << std::setw(2) << (tm.tm_mon + 1) << "-"
       << std::setfill('0') << std::setw(2) << tm.tm_mday << "_"
       << std::setfill('0') << std::setw(2) << tm.tm_hour << "-"
       << std::setfill('0') << std::setw(2) << tm.tm_min << "-"
       << std::setfill('0') << std::setw(2) << tm.tm_sec << ".mp4";

    std::string outDir = m_config.outputDir.empty() ? (std::string(g_get_home_dir()) + "/Videos/Recordings") : m_config.outputDir;
    g_mkdir_with_parents(outDir.c_str(), 0755);

    m_outputFilePath = outDir + "/" + ss.str();

    // Construct GStreamer command line with high-bitrate (20 Mbps), smooth 30 fps, and crisp quantization
    std::ostringstream cmd;
    cmd << "gst-launch-1.0 -e "
        << "mp4mux name=mux faststart=true ! filesink location=\"" << m_outputFilePath << "\" "
        << "pipewiresrc path=" << nodeId << " keepalive-time=1000 do-timestamp=true ! "
        << "videorate ! video/x-raw,framerate=30/1 ! "
        << "videoconvert ! "
        << "openh264enc bitrate=20000000 max-bitrate=35000000 qp-min=8 qp-max=20 complexity=high enable-frame-skip=false multi-thread=0 ! "
        << "h264parse ! queue ! mux.video_0";

    if (!m_audioPipelineFragment.empty())
    {
        cmd << " " << m_audioPipelineFragment;
    }

    std::cout << "Executing GStreamer pipeline:\n"
              << cmd.str() << std::endl;

    gint argc = 0;
    gchar **argv = nullptr;
    GError *error = nullptr;

    if (!g_shell_parse_argv(cmd.str().c_str(), &argc, &argv, &error))
    {
        std::cerr << "Failed to parse pipeline command: " << (error ? error->message : "unknown") << std::endl;
        if (error)
            g_error_free(error);
        return;
    }

    GPid childPid;
    if (!g_spawn_async(nullptr, argv, nullptr,
                       static_cast<GSpawnFlags>(G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH),
                       nullptr, nullptr, &childPid, &error))
    {
        std::cerr << "Failed to spawn gst-launch: " << (error ? error->message : "unknown") << std::endl;
        if (error)
            g_error_free(error);
        g_strfreev(argv);
        return;
    }
    g_strfreev(argv);

    m_pipelinePid = childPid;
    std::cout << "Recording pipeline launched with PID: " << m_pipelinePid << std::endl;

    if (m_onStatus)
    {
        m_onStatus("Recording in progress");
    }
}

void ScreencastService::stopRecording()
{
    if (!m_isRecording && m_pipelinePid <= 0)
    {
        return;
    }

    m_isRecording = false;

    if (m_pipelinePid > 0)
    {
        std::cout << "Sending SIGINT to pipeline process: " << m_pipelinePid << std::endl;
        kill(m_pipelinePid, SIGINT);

        // Wait up to 5 seconds for gst-launch to cleanly write MP4 moov atom
        for (int i = 0; i < 50; ++i)
        {
            int status = 0;
            pid_t res = waitpid(m_pipelinePid, &status, WNOHANG);
            if (res > 0)
            {
                std::cout << "Pipeline terminated cleanly." << std::endl;
                break;
            }
            g_usleep(100000); // 100ms
        }
        m_pipelinePid = -1;
    }

    // Stop Mutter ScreenCast session
    if (m_busConnection && !m_sessionPath.empty())
    {
        g_dbus_connection_call(
            m_busConnection,
            "org.gnome.Mutter.ScreenCast",
            m_sessionPath.c_str(),
            "org.gnome.Mutter.ScreenCast.Session",
            "Stop",
            nullptr,
            nullptr,
            G_DBUS_CALL_FLAGS_NONE,
            1000,
            nullptr,
            nullptr,
            nullptr);
        m_sessionPath.clear();
    }

    if (m_signalSubId > 0 && m_busConnection)
    {
        g_dbus_connection_signal_unsubscribe(m_busConnection, m_signalSubId);
        m_signalSubId = 0;
    }

    removePidFile();

    // Clipboard and notification handling
    if (!m_outputFilePath.empty() && access(m_outputFilePath.c_str(), F_OK) == 0)
    {
        if (m_config.copyToClipboard)
        {
            ClipboardHelper::copyVideoToClipboard(m_outputFilePath);
        }

        std::string filename = m_outputFilePath.substr(m_outputFilePath.find_last_of('/') + 1);
        std::string msg = "Saved: " + filename;
        if (m_config.copyToClipboard)
        {
            msg += "\nCopied to clipboard (ready to paste)!";
        }
        ClipboardHelper::showNotification("Recording Finished", msg, m_outputFilePath);

        if (m_onFinished)
        {
            m_onFinished(m_outputFilePath, true);
        }
    }
    else
    {
        if (m_onFinished)
        {
            m_onFinished("", false);
        }
    }
}
