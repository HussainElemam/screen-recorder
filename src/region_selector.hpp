#pragma once

#include "common.hpp"
#include <gtk/gtk.h>
#include <cairo/cairo.h>
#include <functional>

class RegionSelector
{
public:
    using Callback = std::function<void(int x, int y, int width, int height)>;
    using CancelCallback = std::function<void()>;

    RegionSelector(const MonitorInfo &monitor,
                   cairo_surface_t *bgSurface,
                   Callback onSelected,
                   CancelCallback onCanceled);
    ~RegionSelector();

    void show();

private:
    MonitorInfo m_monitor;
    cairo_surface_t *m_bgSurface = nullptr;
    Callback m_onSelected;
    CancelCallback m_onCanceled;

    GtkWidget *m_window = nullptr;
    bool m_isDragging = false;
    double m_startX = 0;
    double m_startY = 0;
    double m_currX = 0;
    double m_currY = 0;

    static gboolean onDraw(GtkWidget *widget, cairo_t *cr, gpointer userData);
    static gboolean onButtonPress(GtkWidget *widget, GdkEventButton *event, gpointer userData);
    static gboolean onButtonRelease(GtkWidget *widget, GdkEventButton *event, gpointer userData);
    static gboolean onMotionNotify(GtkWidget *widget, GdkEventMotion *event, gpointer userData);
    static gboolean onKeyPress(GtkWidget *widget, GdkEventKey *event, gpointer userData);

    void getSelectionRect(int &rx, int &ry, int &rw, int &rh) const;
};
