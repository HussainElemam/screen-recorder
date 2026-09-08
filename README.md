# Screen Recorder for Fedora Linux (Native C++ / Wayland)

A native, lightweight Linux screen and audio recorder built in modern C++17 with GTK3, GIO, and PipeWire, designed specifically for Fedora GNOME on Wayland.

## Features

- **Global Shortcut & Toggle**: Start recording with a single shortcut press (`Super+Shift+R`), and stop recording by pressing the shortcut again or clicking "Stop" on the floating HUD.
- **Full Screen or Region**:
  - **Full Screen**: Automatically detects all connected monitors (e.g. external displays, built-in display) and lets you choose which screen to record.
  - **Region Selection**: Clean semi-transparent overlay with a crosshair cursor allowing you to drag-select an area with live dimensions.
- **Flexible Audio Recording**:
  - **None (Mute)**: Video only.
  - **System Audio**: Records desktop and application audio (default sink monitor).
  - **Microphone**: Dynamically enumerates all connected input devices (built-in mics, Bluetooth headsets, USB mics).
  - **Both**: Real-time audio mixing combining system output and microphone input into a high-quality AAC stream.
- **Universal Clipboard Integration**:
  - Automatically copies the recorded video to the clipboard as `text/uri-list` and standard file formats.
  - Allows instant `Ctrl+V` pasting directly into **WhatsApp Web**, **ChatGPT**, **Claude**, **Slack**, **Discord**, and **Nautilus**.
- **MP4 (H.264 + AAC)**: Hardware-friendly MP4 container with fast-start enabled for immediate web streaming and playback.
- **Native & Zero-Bloat**: Written in C++17 with zero Python dependencies, compiling down to a ~170KB native binary.

---

## Keyboard Shortcut

Your shortcut is configured in GNOME Settings:
- **Shortcut**: `<Super><Shift>R`
- **Command**: `/home/hussain/.local/bin/screen-recorder --toggle`

Press `Super+Shift+R` to open the recording menu or stop an ongoing recording.

---

## CLI Usage

```bash
# Toggle recording (opens menu if idle, stops if recording)
screen-recorder --toggle

# Open menu directly
screen-recorder

# Stop an active recording
screen-recorder --stop

# Check status
screen-recorder --status
```

---

## Build & Install

```bash
# Build
make

# Install to ~/.local/bin
make install
```

