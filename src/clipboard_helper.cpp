#include "clipboard_helper.hpp"
#include <gtk/gtk.h>
#include <glib.h>
#include <iostream>
#include <cstdlib>

bool ClipboardHelper::copyVideoToClipboard(const std::string &filePath)
{
    if (filePath.empty())
        return false;

    char *uri = g_filename_to_uri(filePath.c_str(), nullptr, nullptr);
    if (!uri)
        return false;

    std::string uriStr = uri;
    g_free(uri);

    // Use wl-copy for persistent Wayland clipboard offering text/uri-list, text/plain, etc.
    // This allows pasting directly into WhatsApp Web, Chatbots, Slack, Discord, Nautilus, etc.
    std::string wlCmd = "printf '" + uriStr + "\\r\\n' | wl-copy -t text/uri-list 2>/dev/null &";
    system(wlCmd.c_str());

    return true;
}

void ClipboardHelper::showNotification(const std::string &title,
                                       const std::string &message,
                                       const std::string &filePath)
{
    if (!filePath.empty())
    {
        // Spawn a detached background process to listen for user clicking "View Recording" or "Show in Files"
        const char *argv[] = {
            "sh",
            "-c",
            "ACTION=$(notify-send -a \"Screen Recorder\" -i video-x-generic "
            "-A default=\"default\" "
            "-A open=\"View Recording\" "
            "-A folder=\"Show in Files\" "
            "\"$1\" \"$2\"); "
            "case \"$ACTION\" in "
            "  open|default) "
            "    xdg-open \"$3\" 2>/dev/null & ;; "
            "  folder) "
            "    dbus-send --session --dest=org.freedesktop.FileManager1 --type=method_call "
            "      /org/freedesktop/FileManager1 org.freedesktop.FileManager1.ShowItems "
            "      array:string:\"file://$3\" string:\"\" 2>/dev/null || xdg-open \"$(dirname \"$3\")\" 2>/dev/null & ;; "
            "esac",
            "screen-recorder-notifier",
            title.c_str(),
            message.c_str(),
            filePath.c_str(),
            nullptr};

        GError *error = nullptr;
        g_spawn_async(nullptr,
                      const_cast<gchar **>(argv),
                      nullptr,
                      static_cast<GSpawnFlags>(G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD),
                      nullptr, nullptr, nullptr, &error);
        if (error)
        {
            std::cerr << "ClipboardHelper: Failed to spawn notification handler: "
                      << error->message << std::endl;
            g_error_free(error);
        }
    }
    else
    {
        const char *argv[] = {
            "notify-send",
            "-a", "Screen Recorder",
            "-i", "video-x-generic",
            title.c_str(),
            message.c_str(),
            nullptr};

        g_spawn_async(nullptr,
                      const_cast<gchar **>(argv),
                      nullptr,
                      static_cast<GSpawnFlags>(G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD),
                      nullptr, nullptr, nullptr, nullptr);
    }
}
