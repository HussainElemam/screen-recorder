#include "countdown_overlay.hpp"
#include <gdk/gdkkeysyms.h>
#include <cmath>
#include <unistd.h>
#include <iostream>

CountdownOverlay::CountdownOverlay(int seconds,
                                   FinishCallback onFinished,
                                   CancelCallback onCanceled)
    : m_secondsRemaining(seconds > 0 ? seconds : 3),
      m_onFinished(onFinished),
      m_onCanceled(onCanceled)
{
}

CountdownOverlay::~CountdownOverlay()
{
    if (m_timerSourceId > 0)
    {
        g_source_remove(m_timerSourceId);
        m_timerSourceId = 0;
    }
    if (m_window)
    {
        gtk_widget_destroy(m_window);
        m_window = nullptr;
    }
}

void CountdownOverlay::start()
{
    m_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(m_window), "Recording Countdown");
    gtk_window_set_decorated(GTK_WINDOW(m_window), FALSE);
    gtk_window_set_skip_taskbar_hint(GTK_WINDOW(m_window), TRUE);
    gtk_window_set_skip_pager_hint(GTK_WINDOW(m_window), TRUE);
    gtk_window_set_keep_above(GTK_WINDOW(m_window), TRUE);
    gtk_widget_set_app_paintable(m_window, TRUE);
    gtk_window_set_default_size(GTK_WINDOW(m_window), 200, 200);
    gtk_window_set_position(GTK_WINDOW(m_window), GTK_WIN_POS_CENTER);

    GdkScreen *screen = gtk_widget_get_screen(m_window);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual)
    {
        gtk_widget_set_visual(m_window, visual);
    }

    GtkCssProvider *cssProvider = gtk_css_provider_new();
    const char *css = R"(
        window.countdown-window {
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

    gtk_style_context_add_class(gtk_widget_get_style_context(m_window), "countdown-window");

    gtk_widget_add_events(m_window, GDK_KEY_PRESS_MASK);

    g_signal_connect(m_window, "draw", G_CALLBACK(onDraw), this);
    g_signal_connect(m_window, "key-press-event", G_CALLBACK(onKeyPress), this);

    gtk_widget_show_all(m_window);
    gtk_window_present(GTK_WINDOW(m_window));

    m_timerSourceId = g_timeout_add(1000, onTimerTick, this);
}

void CountdownOverlay::cancel()
{
    if (m_timerSourceId > 0)
    {
        g_source_remove(m_timerSourceId);
        m_timerSourceId = 0;
    }

    auto cb = m_onCanceled;
    if (m_window)
    {
        gtk_widget_destroy(m_window);
        m_window = nullptr;
    }

    while (gtk_events_pending())
    {
        gtk_main_iteration();
    }

    if (cb)
    {
        cb();
    }
}

gboolean CountdownOverlay::onTimerTick(gpointer userData)
{
    auto *self = static_cast<CountdownOverlay *>(userData);
    self->m_secondsRemaining--;

    if (self->m_secondsRemaining > 0)
    {
        gtk_widget_queue_draw(self->m_window);
        return G_SOURCE_CONTINUE;
    }

    // Countdown reached 0: finish and clean up before starting recording
    self->m_timerSourceId = 0;
    auto cb = self->m_onFinished;

    if (self->m_window)
    {
        gtk_widget_destroy(self->m_window);
        self->m_window = nullptr;
    }

    // Flush all GTK and compositor events so the overlay is completely removed from the screen
    while (gtk_events_pending())
    {
        gtk_main_iteration();
    }
    gdk_display_sync(gdk_display_get_default());
    usleep(50000); // 50ms pause for Mutter to paint the screen without the countdown window

    if (cb)
    {
        cb();
    }

    return G_SOURCE_REMOVE;
}

gboolean CountdownOverlay::onDraw(GtkWidget *widget, cairo_t *cr, gpointer userData)
{
    auto *self = static_cast<CountdownOverlay *>(userData);
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    // Clear background to fully transparent
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    double cx = w / 2.0;
    double cy = (h / 2.0) - 12.0;
    double r = 58.0;

    // Outer subtle glow shadow
    cairo_arc(cr, cx, cy, r + 4.0, 0, 2 * G_PI);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.35);
    cairo_fill(cr);

    // Dark translucent circle badge
    cairo_arc(cr, cx, cy, r, 0, 2 * G_PI);
    cairo_set_source_rgba(cr, 0.08, 0.10, 0.15, 0.92);
    cairo_fill_preserve(cr);

    // Vibrant cyan/blue accent ring
    cairo_set_source_rgba(cr, 0.20, 0.60, 1.0, 0.95);
    cairo_set_line_width(cr, 3.5);
    cairo_stroke(cr);

    // Large countdown number
    std::string numStr = std::to_string(self->m_secondsRemaining);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 56.0);

    cairo_text_extents_t ext;
    cairo_text_extents(cr, numStr.c_str(), &ext);

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_move_to(cr, cx - ext.width / 2.0 - ext.x_bearing,
                      cy - ext.height / 2.0 - ext.y_bearing);
    cairo_show_text(cr, numStr.c_str());

    // Small "Esc to cancel" badge below
    std::string hint = "Esc to cancel";
    cairo_set_font_size(cr, 11.0);
    cairo_text_extents(cr, hint.c_str(), &ext);

    double pillW = ext.width + 16.0;
    double pillH = ext.height + 8.0;
    double pillX = cx - pillW / 2.0;
    double pillY = cy + r + 8.0;

    cairo_set_source_rgba(cr, 0.10, 0.12, 0.16, 0.88);
    cairo_rectangle(cr, pillX, pillY, pillW, pillH);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 0.75, 0.80, 0.90, 0.85);
    cairo_move_to(cr, cx - ext.width / 2.0 - ext.x_bearing,
                      pillY + pillH - 5.0);
    cairo_show_text(cr, hint.c_str());

    return FALSE;
}

gboolean CountdownOverlay::onKeyPress(GtkWidget * /*widget*/, GdkEventKey *event, gpointer userData)
{
    auto *self = static_cast<CountdownOverlay *>(userData);
    if (event->keyval == GDK_KEY_Escape)
    {
        self->cancel();
        return TRUE;
    }
    return FALSE;
}
