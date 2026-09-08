#pragma once

#include <string>

class ClipboardHelper
{
public:
    // Copies the video file path as a file URI to the system clipboard
    static bool copyVideoToClipboard(const std::string &filePath);

    // Displays a desktop notification via notify-send
    static void showNotification(const std::string &title,
                                 const std::string &message,
                                 const std::string &filePath = "");
};
