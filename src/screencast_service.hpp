#pragma once

#include "common.hpp"
#include <gio/gio.h>
#include <string>
#include <functional>
#include <memory>
#include <sys/types.h>

class ScreencastService
{
public:
    using StatusCallback = std::function<void(const std::string &status)>;
    using FinishedCallback = std::function<void(const std::string &filePath, bool success)>;

    ScreencastService();
    ~ScreencastService();

    // Check if recording is actively running (via PID file)
    static bool isRecordingActive();
    static pid_t getActiveRecordingPid();
    static bool stopActiveRecording();

    // Start recording with the provided configuration
    bool startRecording(const RecordingConfig &config,
                        const std::string &audioPipelineFragment,
                        StatusCallback onStatus,
                        FinishedCallback onFinished);

    // Stop current recording session
    void stopRecording();

    bool isRunning() const { return m_isRecording; }
    std::string getCurrentOutputFile() const { return m_outputFilePath; }

private:
    RecordingConfig m_config;
    std::string m_audioPipelineFragment;
    StatusCallback m_onStatus;
    FinishedCallback m_onFinished;

    bool m_isRecording = false;
    std::string m_outputFilePath;

    GDBusConnection *m_busConnection = nullptr;
    std::string m_sessionPath;
    std::string m_streamPath;
    guint m_signalSubId = 0;

    pid_t m_pipelinePid = -1;

    static std::string getPidFilePath();
    void writePidFile();
    static void removePidFile();

    void onPipeWireStreamAdded(guint32 nodeId);
    void launchPipeline(guint32 nodeId);

    static void gdbusSignalHandler(GDBusConnection *connection,
                                   const gchar *sender_name,
                                   const gchar *object_path,
                                   const gchar *interface_name,
                                   const gchar *signal_name,
                                   GVariant *parameters,
                                   gpointer user_data);
};
