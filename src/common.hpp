#pragma once

#include <string>
#include <vector>
#include <cstdint>

enum class CaptureMode
{
    FullScreen,
    Region
};

enum class AudioMode
{
    None,
    System,
    Mic,
    Both
};

struct MonitorInfo
{
    int index = 0;
    std::string connector;   // e.g. "DP-1", "eDP-1"
    std::string displayName; // e.g. "Samsung Electric Company 24\""
    std::string model;
    std::string manufacturer;
    int x = 0;
    int y = 0;
    int width = 1920;
    int height = 1080;
    double scale = 1.0;
    bool isPrimary = false;
};

struct AudioDevice
{
    int index = 0;
    std::string name;        // Pulse/PipeWire device name (e.g. alsa_input...)
    std::string description; // Friendly name (e.g. "Digital Microphone")
    bool isMonitor = false;  // True if it's a monitor of a sink (system audio)
};

struct RecordingConfig
{
    CaptureMode captureMode = CaptureMode::FullScreen;
    std::string monitorConnector; // e.g. "DP-1"
    int monitorIndex = 0;

    // Region coordinates (in Mutter global desktop coordinates)
    int regionX = 0;
    int regionY = 0;
    int regionWidth = 0;
    int regionHeight = 0;

    AudioMode audioMode = AudioMode::None;
    std::string micDeviceName;         // Selected mic device name
    std::string systemAudioDeviceName; // System audio sink monitor name

    std::string outputDir;
    std::string outputFilePath;
    bool copyToClipboard = true;
    bool enableCountdown = true;
};
