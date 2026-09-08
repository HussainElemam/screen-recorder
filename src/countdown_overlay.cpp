#include "countdown_overlay.hpp"
#include <gdk/gdkkeysyms.h>
#include <cmath>
#include <unistd.h>
#include <iostream>

CountdownOverlay::CountdownOverlay(int seconds,
                                   FinishCallback onFinished,
                                   CancelCallback onCanceled)
    : m_totalSeconds(seconds > 0 ? seconds : 3),
      m_onFinished(onFinished),
      m_onCanceled(onCanceled)
{
}

CountdownOverlay::~CountdownOverlay()
{
    if (m_window && m_tickId > 0)
    {
        gtk_widget_remove_tick_callback(m_window, m_tickId);
        m_tickId = 0;
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
    gtk_window_set_default_size(GTK_WINDOW(m_window), 220, 220);
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
            background-color: transparent;
            background: none;
            border: none;
            box-shadow: none;
        }
    )";
    gtk_css_provider_load_from_data(cssProvider, css, -1, nullptr);
    gtk_style_context_add_provider_for_screen(
        screen,
        GTK_STYLE_PROVIDER(cssProvider),
        GTK_STYLE_PROVIDER_PRIORITY_USER);
    g_object_unref(cssProvider);

    gtk_style_context_add_class(gtk_widget_get_style_context(m_window), "countdown-window");

    gtk_widget_add_events(m_window, GDK_KEY_PRESS_MASK);

    g_signal_connect(m_window, "draw", G_CALLBACK(onDraw), this);
    g_signal_connect(m_window, "key-press-event", G_CALLBACK(onKeyPress), this);

    gtk_widget_show_all(m_window);
    gtk_window_present(GTK_WINDOW(m_window));

    m_startTimeUs = g_get_monotonic_time();
    m_tickId = gtk_widget_add_tick_callback(m_window, onTick, this, nullptr);
}

void CountdownOverlay::cancel()
{
    if (m_window && m_tickId > 0)
    {
        gtk_widget_remove_tick_callback(m_window, m_tickId);
        m_tickId = 0;
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

gboolean CountdownOverlay::onTick(GtkWidget *widget, GdkFrameClock * /*frame_clock*/, gpointer userData)
{
    auto *self = static_cast<CountdownOverlay *>(userData);
    gint64 now = g_get_monotonic_time();
    double elapsed = (now - self->m_startTimeUs) / 1000000.0;

    if (elapsed >= self->m_totalSeconds)
    {
        // Countdown completed! Clean up overlay window and trigger recording
        self->m_tickId = 0;
        auto cb = self->m_onFinished;

        if (self->m_window)
        {
            gtk_widget_destroy(self->m_window);
            self->m_window = nullptr;
        }

        while (gtk_events_pending())
        {
            gtk_main_iteration();
        }
        gdk_display_sync(gdk_display_get_default());
        usleep(40000); // 40ms compositor repaint flush

        if (cb)
        {
            cb();
        }

        return G_SOURCE_REMOVE;
    }

    gtk_widget_queue_draw(widget);
    return G_SOURCE_CONTINUE;
}

gboolean CountdownOverlay::onDraw(GtkWidget *widget, cairo_t *cr, gpointer userData)
{
    auto *self = static_cast<CountdownOverlay *>(userData);
    int w = gtk_widget_get_allocated_width(widget);
    int h = gtk_widget_get_allocated_height(widget);

    // 1. Fully transparent background (no solid background)
    cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    gint64 now = g_get_monotonic_time();
    double elapsed = (now - self->m_startTimeUs) / 1000000.0;
    if (elapsed < 0.0)
        elapsed = 0.0;
    if (elapsed >= self->m_totalSeconds)
        return FALSE;

    int currentNumber = self->m_totalSeconds - static_cast<int>(elapsed);
    if (currentNumber < 1)
        currentNumber = 1;

    double secProgress = elapsed - static_cast<int>(elapsed); // 0.0 to 1.0

    double cx = w / 2.0;
    double cy = (h / 2.0) - 10.0;
    double baseR = 52.0;

    // 2. Soft Translucent Frosted Glass Disc (modern, not solid, ensures high contrast on all screen backgrounds)
    cairo_arc(cr, cx, cy, baseR, 0, 2 * G_PI);
    cairo_set_source_rgba(cr, 0.08, 0.08, 0.10, 0.55);
    cairo_fill(cr);

    // 3. Animated Expanding Pulse Ripple (fades as it expands)
    double rippleR = baseR + 32.0 * secProgress;
    double rippleAlpha = (1.0 - secProgress) * 0.40;
    cairo_arc(cr, cx, cy, rippleR, 0, 2 * G_PI);
    cairo_set_source_rgba(cr, 0.25, 0.65, 1.0, rippleAlpha);
    cairo_set_line_width(cr, 2.5 * (1.0 - secProgress));
    cairo_stroke(cr);

    // 4. Subtle Track Ring
    cairo_arc(cr, cx, cy, baseR, 0, 2 * G_PI);
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.15);
    cairo_set_line_width(cr, 3.0);
    cairo_stroke(cr);

    // 5. Sweeping Radial Accent Progress Ring
    double sweepAngle = 2.0 * G_PI * (1.0 - secProgress);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_arc(cr, cx, cy, baseR, -G_PI / 2.0, -G_PI / 2.0 + sweepAngle);
    cairo_set_source_rgba(cr, 0.20, 0.65, 1.0, 0.95);
    cairo_set_line_width(cr, 3.5);
    cairo_stroke(cr);

    // 6. Number with Spring-Pop Animation and Crisp White Font (zero shadow)
    double popScale = 1.0 + 0.32 * std::exp(-5.5 * secProgress);

    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_scale(cr, popScale, popScale);

    std::string numStr = std::to_string(currentNumber);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 56.0);

    cairo_text_extents_t ext;
    cairo_text_extents(cr, numStr.c_str(), &ext);
    double tx = -ext.width / 2.0 - ext.x_bearing;
    double ty = -ext.height / 2.0 - ext.y_bearing;

    // Razor-sharp clean white digit with NO dirty/blurry multi-offset shadow
    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 1.0);
    cairo_move_to(cr, tx, ty);
    cairo_show_text(cr, numStr.c_str());
    cairo_restore(cr);

    // 7. Subtle "Esc to cancel" hint badge below
    std::string hint = "Esc to cancel";
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.0);
    cairo_text_extents(cr, hint.c_str(), &ext);

    double pillW = ext.width + 16.0;
    double pillH = ext.height + 8.0;
    double pillX = cx - pillW / 2.0;
    double pillY = cy + baseR + 14.0;

    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.65);
    double rad = 5.0;
    cairo_new_sub_path(cr);
    cairo_arc(cr, pillX + pillW - rad, pillY + rad, rad, -G_PI / 2, 0);
    cairo_arc(cr, pillX + pillW - rad, pillY + pillH - rad, rad, 0, G_PI / 2);
    cairo_arc(cr, pillX + rad, pillY + pillH - rad, rad, G_PI / 2, G_PI);
    cairo_arc(cr, pillX + rad, pillY + rad, rad, G_PI, 3 * G_PI / 2);
    cairo_close_path(cr);
    cairo_fill(cr);

    cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.90);
    cairo_move_to(cr, cx - ext.width / 2.0 - ext.x_bearing, pillY + pillH - 4.5);
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
