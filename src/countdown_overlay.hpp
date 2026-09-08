#pragma once

#include <gtk/gtk.h>
#include <cairo/cairo.h>
#include <functional>

class CountdownOverlay
{
public:
    using FinishCallback = std::function<void()>;
    using CancelCallback = std::function<void()>;

    CountdownOverlay(int seconds,
                     FinishCallback onFinished,
                     CancelCallback onCanceled);
    ~CountdownOverlay();

    void start();
    void cancel();

private:
    int m_secondsRemaining = 3;
    FinishCallback m_onFinished;
    CancelCallback m_onCanceled;

    GtkWidget *m_window = nullptr;
    guint m_timerSourceId = 0;

    static gboolean onDraw(GtkWidget *widget, cairo_t *cr, gpointer userData);
    static gboolean onKeyPress(GtkWidget *widget, GdkEventKey *event, gpointer userData);
    static gboolean onTimerTick(gpointer userData);
};
