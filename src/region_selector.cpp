#include "region_selector.hpp"
#include <gdk/gdkkeysyms.h>
#include <algorithm>
#include <cmath>
#include <iostream>

RegionSelector::RegionSelector(const MonitorInfo &monitor,
                               cairo_surface_t *bgSurface,
                               Callback onSelected,
                               CancelCallback onCanceled)
    : m_monitor(monitor),
      m_bgSurface(bgSurface),
      m_onSelected(onSelected),
      m_onCanceled(onCanceled)
{
}

RegionSelector::~RegionSelector()
{
    if (m_window)
    {
        gtk_widget_destroy(m_window);
        m_window = nullptr;
    }
    if (m_bgSurface)
    {
        cairo_surface_destroy(m_bgSurface);
        m_bgSurface = nullptr;
    }
}

void RegionSelector::show()
{
    m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(m_window), "Select Region");
    gtk_window_set_decorated(GTK_WINDOW(m_window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(m_window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(m_window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(m_window), TRUE);
    gtk_widget_set_app_paintable(m_window, TRUE);

    GdkScreen *screen = gtk_widget_get_screen(m_window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual)
    {
        gtk_widget_set_visual(m_window, visual);
    }

    // CSS styling
    GtkCssProvider *cssProvider = gtk_css_provider_new();
    const char *css = R"(
        window.region-selector {
            background-color: transparent !important;
            background: none !important;
            border: none !important;
            box-shadow: none !important;
        }
    )";
    gtk_css_provider_load_from_data(cssProvider, css, -1, nullptr);
    gtk_style_context_add_provider_for_screen(
        screen,
        GTK_STYLE_PROVIDER(cssProvider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(cssProvider);

    gtk_style_context_add_class(gtk_widget_get_style_context(m_window), "region-selector");

    gtk_widget_add_events(m_window,
                          GDK_BUTTON_PRESS_MASK |
                              GDK_BUTTON_RELEASE_MASK |
                              GDK_POINTER_MOTION_MASK |
                              GDK_KEY_PRESS_MASK);

    g_signal_connect(m_window, "draw", G_CALLBACK(onDraw), this);
    g_signal_connect(m_window, "button-press-event", G_CALLBACK(onButtonPress), this);
    g_signal_connect(m_window, "button-release-event", G_CALLBACK(onButtonRelease), this);
    g_signal_connect(m_window, "motion-notify-event", G_CALLBACK(onMotionNotify), this);
    g_signal_connect(m_window, "key-press-event", G_CALLBACK(onKeyPress), this);

    gtk_window_fullscreen_on_monitor(GTK_WINDOW(m_window), screen, m_monitor.index);
    gtk_widget_show_all(m_window);

    GdkWindow *gdkWin = gtk_widget_get_window(m_window);
    if (gdkWin)
    {
        GdkDisplay *display = gdk_display_get_default();
        GdkCursor *cursor = gdk_cursor_new_from_name(display, "crosshair");
        gdk_window_set_cursor(gdkWin, cursor);
        if (cursor)
            g_object_unref(cursor);
    }
}

void RegionSelector::getSelectionRect(int &rx, int &ry, int &rw, int &rh) const
{
    double minX = std::min(m_startX, m_currX);
    double minY = std::min(m_startY, m_currY);
    double maxX = std::max(m_startX, m_currX);
    double maxY = std::max(m_startY, m_currY);

    rx = static_cast<int>(std::round(minX));
    ry = static_cast<int>(std::round(minY));
    rw = static_cast<int>(std::round(maxX - minX));
    rh = static_cast<int>(std::round(maxY - minY));
}

gboolean RegionSelector::onDraw(GtkWidget *widget, cairo_t *cr, gpointer userData)
{
    auto *self = static_cast<RegionSelector *>(userData);
    int winW = gtk_widget_get_allocated_width(widget);
    int winH = gtk_widget_get_allocated_height(widget);

    // 1. Paint the monitor snapshot background
    if (self->m_bgSurface)
    {
        int imgW = cairo_image_surface_get_width(self->m_bgSurface);
        int imgH = cairo_image_surface_get_height(self->m_bgSurface);

        cairo_save(cr);
        if (imgW > 0 && imgH > 0 && (winW != imgW || winH != imgH))
        {
            cairo_scale(cr, static_cast<double>(winW) / imgW, static_cast<double>(winH) / imgH);
        }
        cairo_set_source_surface(cr, self->m_bgSurface, 0, 0);
        cairo_paint(cr);
        cairo_restore(cr);
    }
    else
    {
        cairo_set_source_rgba(cr, 0.08, 0.08, 0.10, 0.85);
        cairo_paint(cr);
    }

    if (self->m_isDragging)
    {
        int rx, ry, rw, rh;
        self->getSelectionRect(rx, ry, rw, rh);

        // 2. Dim everything outside the selection box using Even-Odd rule
        cairo_save(cr);
        cairo_set_fill_rule(cr, CAIRO_FILL_RULE_EVEN_ODD);
        cairo_rectangle(cr, 0, 0, winW, winH);
        if (rw > 0 && rh > 0)
        {
            cairo_rectangle(cr, rx, ry, rw, rh);
        }
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.40);
        cairo_fill(cr);
        cairo_restore(cr);

        if (rw > 0 && rh > 0)
        {
            // Subtle highlight inside the cutout
            cairo_set_source_rgba(cr, 0.20, 0.60, 1.0, 0.04);
            cairo_rectangle(cr, rx, ry, rw, rh);
            cairo_fill(cr);

            // Double border for strong visibility against both dark & light content
            cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.7);
            cairo_set_line_width(cr, 3.0);
            cairo_rectangle(cr, rx, ry, rw, rh);
            cairo_stroke(cr);

            cairo_set_source_rgba(cr, 0.20, 0.60, 1.0, 1.0);
            cairo_set_line_width(cr, 1.5);
            cairo_rectangle(cr, rx, ry, rw, rh);
            cairo_stroke(cr);

            // Corner grab handles
            double cornerLen = std::min(16.0, std::min(rw / 4.0, rh / 4.0));
            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
            cairo_set_line_width(cr, 3.0);

            // Top-left
            cairo_move_to(cr, rx, ry + cornerLen);
            cairo_line_to(cr, rx, ry);
            cairo_line_to(cr, rx + cornerLen, ry);

            // Top-right
            cairo_move_to(cr, rx + rw - cornerLen, ry);
            cairo_line_to(cr, rx + rw, ry);
            cairo_line_to(cr, rx + rw, ry + cornerLen);

            // Bottom-right
            cairo_move_to(cr, rx + rw, ry + rh - cornerLen);
            cairo_line_to(cr, rx + rw, ry + rh);
            cairo_line_to(cr, rx + rw - cornerLen, ry + rh);

            // Bottom-left
            cairo_move_to(cr, rx + cornerLen, ry + rh);
            cairo_line_to(cr, rx, ry + rh);
            cairo_line_to(cr, rx, ry + rh - cornerLen);
            cairo_stroke(cr);

            // Dimension badge
            std::string dimText = std::to_string(rw) + " × " + std::to_string(rh) + " px";
            cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
            cairo_set_font_size(cr, 13.0);

            cairo_text_extents_t extents;
            cairo_text_extents(cr, dimText.c_str(), &extents);

            double pillW = extents.width + 20.0;
            double pillH = extents.height + 12.0;
            double pillX = rx + (rw - pillW) / 2.0;
            double pillY = (ry > pillH + 12.0) ? (ry - pillH - 8.0) : (ry + rh + 8.0);

            // Pill container
            double rad = 6.0;
            cairo_new_sub_path(cr);
            cairo_arc(cr, pillX + pillW - rad, pillY + rad, rad, -G_PI / 2, 0);
            cairo_arc(cr, pillX + pillW - rad, pillY + pillH - rad, rad, 0, G_PI / 2);
            cairo_arc(cr, pillX + rad, pillY + pillH - rad, rad, G_PI / 2, G_PI);
            cairo_arc(cr, pillX + rad, pillY + rad, rad, G_PI, 3 * G_PI / 2);
            cairo_close_path(cr);

            cairo_set_source_rgba(cr, 0.10, 0.12, 0.16, 0.95);
            cairo_fill_preserve(cr);

            cairo_set_source_rgba(cr, 0.25, 0.65, 1.0, 0.9);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);

            cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
            cairo_move_to(cr, pillX + 10.0, pillY + pillH - 7.0);
            cairo_show_text(cr, dimText.c_str());
        }
    }
    else
    {
        // When not dragging yet, shade entire screen subtly
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.30);
        cairo_paint(cr);

        // Friendly floating guidance pill at top center
        std::string hint = "Click and drag to select recording area • Esc to cancel";
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 14.0);

        cairo_text_extents_t extents;
        cairo_text_extents(cr, hint.c_str(), &extents);

        double pillW = extents.width + 36.0;
        double pillH = extents.height + 18.0;
        double pillX = (winW - pillW) / 2.0;
        double pillY = 32.0;

        double rad = 8.0;
        cairo_new_sub_path(cr);
        cairo_arc(cr, pillX + pillW - rad, pillY + rad, rad, -G_PI / 2, 0);
        cairo_arc(cr, pillX + pillW - rad, pillY + pillH - rad, rad, 0, G_PI / 2);
        cairo_arc(cr, pillX + rad, pillY + pillH - rad, rad, G_PI / 2, G_PI);
        cairo_arc(cr, pillX + rad, pillY + rad, rad, G_PI, 3 * G_PI / 2);
        cairo_close_path(cr);

        cairo_set_source_rgba(cr, 0.10, 0.12, 0.16, 0.94);
        cairo_fill_preserve(cr);

        cairo_set_source_rgba(cr, 0.25, 0.65, 1.0, 0.9);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);

        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
        cairo_move_to(cr, pillX + 18.0, pillY + pillH - 10.0);
        cairo_show_text(cr, hint.c_str());
    }

    return FALSE;
}

gboolean RegionSelector::onButtonPress(GtkWidget *widget, GdkEventButton *event, gpointer userData)
{
    auto *self = static_cast<RegionSelector *>(userData);
    if (event->button == 1) // Left click
    {
        self->m_isDragging = true;
        self->m_startX = event->x;
        self->m_startY = event->y;
        self->m_currX = event->x;
        self->m_currY = event->y;
        gtk_widget_queue_draw(widget);
        return TRUE;
    }
    return FALSE;
}

gboolean RegionSelector::onMotionNotify(GtkWidget *widget, GdkEventMotion *event, gpointer userData)
{
    auto *self = static_cast<RegionSelector *>(userData);
    if (self->m_isDragging)
    {
        self->m_currX = event->x;
        self->m_currY = event->y;
        gtk_widget_queue_draw(widget);
        return TRUE;
    }
    return FALSE;
}

gboolean RegionSelector::onButtonRelease(GtkWidget *widget, GdkEventButton *event, gpointer userData)
{
    auto *self = static_cast<RegionSelector *>(userData);
    if (event->button == 1 && self->m_isDragging)
    {
        self->m_isDragging = false;
        self->m_currX = event->x;
        self->m_currY = event->y;

        int rx, ry, rw, rh;
        self->getSelectionRect(rx, ry, rw, rh);

        if (rw >= 20 && rh >= 20)
        {
            // Translate to Mutter global desktop coordinates
            int globalX = self->m_monitor.x + rx;
            int globalY = self->m_monitor.y + ry;

            // Ensure even dimensions for H.264 video encoding
            if (rw % 2 != 0)
                rw++;
            if (rh % 2 != 0)
                rh++;

            auto cb = self->m_onSelected;
            gtk_widget_destroy(self->m_window);
            self->m_window = nullptr;

            if (cb)
            {
                cb(globalX, globalY, rw, rh);
            }
            return TRUE;
        }
        else
        {
            // Too small, reset selection
            gtk_widget_queue_draw(widget);
        }
    }
    return FALSE;
}

gboolean RegionSelector::onKeyPress(GtkWidget * /*widget*/, GdkEventKey *event, gpointer userData)
{
    auto *self = static_cast<RegionSelector *>(userData);
    if (event->keyval == GDK_KEY_Escape)
    {
        auto cancelCb = self->m_onCanceled;
        gtk_widget_destroy(self->m_window);
        self->m_window = nullptr;
        if (cancelCb)
        {
            cancelCb();
        }
        return TRUE;
    }
    return FALSE;
}
