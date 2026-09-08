# Screen Recorder for Fedora Linux (Native C++ / Wayland)

A modern, native, and lightweight Linux screen and audio recorder built in C++17 with GTK3, GStreamer, and PipeWire, designed specifically for Fedora GNOME on Wayland.

---

## ✨ Features

- **Global Shortcut & Toggle**: Start recording with a single keyboard shortcut (`Super+Shift+R`), and press it again anytime to stop recording.
- **Full Screen or Region Capture**:
  - **Full Screen**: Automatically enumerates all connected monitors (external displays, built-in display) with resolution and primary display badges.
  - **Region Selection**: Takes an instant freeze-frame snapshot of your monitor and provides a precision crosshair selection overlay with real-time dimension badges (`W × H px`) and resize handles.
- **Flexible Audio Routing**:
  - **None (Mute)**: Pure video recording.
  - **System Audio**: Records desktop and application sounds (default audio sink monitor).
  - **Microphone**: Automatically lists and connects to all input devices (built-in mics, Bluetooth headsets, USB mics).
  - **Both**: High-fidelity real-time audio mixing combining system output and microphone input into a unified AAC stream.
- **Animated Transparent Countdown**:
  - Optional 3-second hardware-synced animated countdown with a sweeping radial progress arc, spring-pop vector digits, and an expanding pulse ripple.
  - Clean frosted translucent backdrop ensuring high contrast over light or dark screens.
  - Press <kbd>Esc</kbd> anytime during the countdown to cancel.
- **Universal Clipboard Integration**:
  - Automatically copies the recorded MP4 file to your clipboard as a `text/uri-list` and standard file reference.
  - Allows immediate <kbd>Ctrl</kbd> + <kbd>V</kbd> pasting into **WhatsApp Web**, **Slack**, **Discord**, **ChatGPT**, **Claude**, **Telegram**, and **Nautilus**.
- **Interactive Stop Notification**:
  - Start recording notification is silenced so no popup is captured into your video.
  - Stop recording notification includes interactive action buttons:
    - **View Recording**: Opens the video immediately in your default video player.
    - **Show in Files**: Reveals and highlights the video file directly in the Nautilus file manager.
- **Modern Dark UI**:
  - Polished GTK3 floating card interface styled for Adwaita Dark.
  - Segmented controls, clean toggle switches, and folder picker.
- **Zero-Bloat Native C++**:
  - Compiles to a small (~180 KB) standalone native binary with zero Python or Electron overhead.

---

## 📦 System Requirements & Dependencies

This application is built for **Fedora Linux** running **GNOME Wayland**.

Install the required development libraries and runtime tools via `dnf`:

```bash
sudo dnf install -y \
  gcc-c++ \
  cmake \
  make \
  pkg-config \
  gtk3-devel \
  glib2-devel \
  gstreamer1-devel \
  gstreamer1-plugins-base-devel \
  gstreamer1-plugins-good \
  gstreamer1-plugins-bad-free \
  pipewire-devel \
  wireplumber \
  wl-clipboard \
  xdg-utils
```

### Why these packages?
- `gtk3-devel`, `glib2-devel`: Provides the GTK3 GUI, Cairo vector rendering, and GIO D-Bus bindings.
- `gstreamer1-plugins-good`, `gstreamer1-plugins-bad-free`: Provides PipeWire video capture (`pipewiresrc`), H.264 video encoding, and MP4 muxing.
- `wl-clipboard`: Provides `wl-copy` used to copy video files directly to the Wayland clipboard for instant pasting.
- `xdg-utils`: Provides `xdg-open` to launch the recorded video in your default media player.

---

## 🛠️ Build & Installation

### 1. Clone the repository
```bash
git clone https://github.com/YOUR_USERNAME/screen_recorder_2.git
cd screen_recorder_2
```

### 2. Build the project
```bash
make
```
*(This creates an optimized Release build in the `build/` directory using CMake.)*

### 3. Install
```bash
make install
```
*(This installs the compiled executable to `~/.local/bin/screen-recorder`.)*

> **Note:** Ensure `~/.local/bin` is in your `$PATH` (this is default on Fedora). You can verify with:
> ```bash
> which screen-recorder
> ```

---

## ⌨️ Configuring the Global Keyboard Shortcut

To start and stop recording at any time using a global shortcut:

### Method 1: Via GNOME Settings (Recommended)
1. Open **Settings** on your desktop.
2. Navigate to **Keyboard** → **View and Customize Shortcuts** → **Custom Shortcuts**.
3. Click the **+** (Add) button.
4. Fill in the fields:
   - **Name**: `Screen Recorder`
   - **Command**: `screen-recorder --toggle`
   - **Shortcut**: Press your desired keys (e.g. <kbd>Super</kbd> + <kbd>Shift</kbd> + <kbd>R</kbd>).
5. Click **Add**.

### Method 2: Via Terminal (`gsettings`)
You can also bind the shortcut directly from the command line:
```bash
# Add custom keybinding to GNOME
KEY_PATH="/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/custom0/"
gsettings set org.gnome.settings-daemon.plugins.media-keys custom-keybindings "['$KEY_PATH']"
gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$KEY_PATH name "Screen Recorder"
gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$KEY_PATH command "screen-recorder --toggle"
gsettings set org.gnome.settings-daemon.plugins.media-keys.custom-keybinding:$KEY_PATH binding "<Super><Shift>R"
```

---

## 🚀 Usage

### Using the Keyboard Shortcut
1. Press <kbd>Super</kbd> + <kbd>Shift</kbd> + <kbd>R</kbd>.
2. The modern recording dialog will appear:
   - Select **Full Screen** (and pick a monitor if multi-monitor) or **Area / Region**.
   - Choose your **Audio Source** (None, System Audio, Microphone, or Both).
   - Set destination folder and toggle the 3-second countdown or clipboard auto-copy.
3. Click **Start Recording** (or drag an area if region mode was chosen).
4. An animated countdown plays, and recording starts.
5. When finished, press <kbd>Super</kbd> + <kbd>Shift</kbd> + <kbd>R</kbd> again to stop.
6. A desktop notification appears with options to **View Recording** or **Show in Files**, and the video is already on your clipboard ready to be pasted!

### Command Line Interface (CLI)

```bash
# Toggle recording: opens dialog if idle, stops recording if active
screen-recorder --toggle

# Open the dialog directly
screen-recorder

# Stop an active recording session and finalize the MP4 file
screen-recorder --stop

# Check whether recording is currently running
screen-recorder --status

# Display help and options
screen-recorder --help
```

---

## ⚙️ Configuration & Settings

Your preferences are automatically saved between sessions in:
`~/.config/screen-recorder/config.ini`

Saved preferences include:
- Last used output directory (defaults to `~/Videos/Recordings`)
- Last selected microphone device
- Last selected audio recording mode
- Countdown toggle state
- Clipboard auto-copy toggle state

---

## 🏗️ Architecture Overview

- **Window Management & UI**: Built with GTK3 and Cairo for high-DPI scaling and custom styling.
- **Wayland Screencasting**: Interfaces directly with GNOME Mutter's D-Bus Screencast service (`org.gnome.Mutter.ScreenCast`).
- **Media Pipeline**: Utilizes GStreamer with PipeWire integration (`pipewiresrc`) and hardware-accelerated H.264/AAC muxing for high-quality, fast-start MP4 files.
- **Region Freezing**: Captures an instant micro-damage snapshot through Mutter D-Bus and PipeWire (~90ms) before opening the selection overlay, solving Wayland opaque fullscreen window limitations.
- **Clipboard Management**: Communicates via `wl-clipboard` (`wl-copy`) with proper MIME type mapping (`text/uri-list`).

---

## 📄 License

This project is licensed under the MIT License.
