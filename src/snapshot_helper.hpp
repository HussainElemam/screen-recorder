#pragma once

#include <string>
#include <cairo/cairo.h>

class SnapshotHelper
{
public:
    /**
     * Captures a single frame of the given monitor connector (e.g. "DP-1", "eDP-1")
     * via Mutter ScreenCast and GStreamer PipeWire source.
     * 
     * Returns a cairo_surface_t* containing the full monitor snapshot.
     * The caller is responsible for destroying it via cairo_surface_destroy().
     * Returns nullptr if capture fails.
     */
    static cairo_surface_t *captureMonitor(const std::string &connector);
};
