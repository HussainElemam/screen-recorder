#include "snapshot_helper.hpp"
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <iostream>
#include <chrono>
#include <unistd.h>
#include <sys/wait.h>
#include <atomic>

cairo_surface_t *SnapshotHelper::captureMonitor(const std::string &connector)
{
    std::string targetConn = connector.empty() ? "DP-1" : connector;

    GError *error = nullptr;
    GDBusConnection *bus = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!bus)
    {
        std::cerr << "SnapshotHelper: Failed to connect to D-Bus: "
                  << (error ? error->message : "unknown") << std::endl;
        if (error)
            g_error_free(error);
        return nullptr;
    }

    // 1. CreateSession on org.gnome.Mutter.ScreenCast
    GVariantBuilder b;
    g_variant_builder_init(&b, G_VARIANT_TYPE_VARDICT);

    GVariant *reply = g_dbus_connection_call_sync(
        bus,
        "org.gnome.Mutter.ScreenCast",
        "/org/gnome/Mutter/ScreenCast",
        "org.gnome.Mutter.ScreenCast",
        "CreateSession",
        g_variant_new("(a{sv})", &b),
        G_VARIANT_TYPE("(o)"),
        G_DBUS_CALL_FLAGS_NONE,
        2000,
        nullptr,
        &error);

    if (!reply)
    {
        std::cerr << "SnapshotHelper: CreateSession failed: "
                  << (error ? error->message : "unknown") << std::endl;
        if (error)
            g_error_free(error);
        g_object_unref(bus);
        return nullptr;
    }

    const gchar *sessPath = nullptr;
    g_variant_get(reply, "(&o)", &sessPath);
    std::string sessionPath = sessPath ? sessPath : "";
    g_variant_unref(reply);

    if (sessionPath.empty())
    {
        g_object_unref(bus);
        return nullptr;
    }

    // 2. RecordMonitor with cursor-mode=0 (no baked cursor)
    GVariantBuilder streamOpts;
    g_variant_builder_init(&streamOpts, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_add(&streamOpts, "{sv}", "cursor-mode", g_variant_new_uint32(0));

    GVariant *streamReply = g_dbus_connection_call_sync(
        bus,
        "org.gnome.Mutter.ScreenCast",
        sessionPath.c_str(),
        "org.gnome.Mutter.ScreenCast.Session",
        "RecordMonitor",
        g_variant_new("(sa{sv})", targetConn.c_str(), &streamOpts),
        G_VARIANT_TYPE("(o)"),
        G_DBUS_CALL_FLAGS_NONE,
        2000,
        nullptr,
        &error);

    if (!streamReply)
    {
        std::cerr << "SnapshotHelper: RecordMonitor failed for " << targetConn << ": "
                  << (error ? error->message : "unknown") << std::endl;
        if (error)
            g_error_free(error);
        g_object_unref(bus);
        return nullptr;
    }

    const gchar *sPath = nullptr;
    g_variant_get(streamReply, "(&o)", &sPath);
    std::string streamPath = sPath ? sPath : "";
    g_variant_unref(streamReply);

    // 3. Subscribe to PipeWireStreamAdded
    guint32 nodeId = 0;
    guint subId = g_dbus_connection_signal_subscribe(
        bus,
        "org.gnome.Mutter.ScreenCast",
        "org.gnome.Mutter.ScreenCast.Stream",
        "PipeWireStreamAdded",
        streamPath.c_str(),
        nullptr,
        G_DBUS_SIGNAL_FLAGS_NONE,
        [](GDBusConnection *, const gchar *, const gchar *, const gchar *, const gchar *, GVariant *parameters, gpointer user_data)
        {
            auto *nid = static_cast<guint32 *>(user_data);
            g_variant_get(parameters, "(u)", nid);
        },
        &nodeId,
        nullptr);

    // 4. Start the session
    GVariant *startReply = g_dbus_connection_call_sync(
        bus,
        "org.gnome.Mutter.ScreenCast",
        sessionPath.c_str(),
        "org.gnome.Mutter.ScreenCast.Session",
        "Start",
        nullptr,
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        2000,
        nullptr,
        &error);

    if (startReply)
    {
        g_variant_unref(startReply);
    }
    else
    {
        std::cerr << "SnapshotHelper: Session Start failed: "
                  << (error ? error->message : "unknown") << std::endl;
        if (error)
            g_error_free(error);
        g_dbus_connection_signal_unsubscribe(bus, subId);
        g_object_unref(bus);
        return nullptr;
    }

    // Wait for PipeWireStreamAdded signal
    auto waitStart = std::chrono::steady_clock::now();
    while (nodeId == 0 && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - waitStart).count() < 2500)
    {
        while (gtk_events_pending())
        {
            gtk_main_iteration();
        }
        g_main_context_iteration(nullptr, FALSE);
        usleep(5000);
    }
    g_dbus_connection_signal_unsubscribe(bus, subId);

    if (nodeId == 0)
    {
        std::cerr << "SnapshotHelper: Timed out waiting for PipeWire stream node" << std::endl;
        g_dbus_connection_call_sync(bus, "org.gnome.Mutter.ScreenCast", sessionPath.c_str(),
                                    "org.gnome.Mutter.ScreenCast.Session", "Stop",
                                    nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, 500, nullptr, nullptr);
        g_object_unref(bus);
        return nullptr;
    }

    // 5. Create a micro-damage transparent window to force Mutter to composite/dispatch a frame immediately
    GtkWidget *pulseWin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_decorated(GTK_WINDOW(pulseWin), FALSE);
    gtk_window_set_default_size(GTK_WINDOW(pulseWin), 1, 1);
    GdkScreen *screen = gtk_widget_get_screen(pulseWin);
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
    if (visual)
    {
        gtk_widget_set_visual(pulseWin, visual);
    }
    gtk_widget_set_app_paintable(pulseWin, TRUE);
    g_signal_connect(pulseWin, "draw", G_CALLBACK(+[](GtkWidget *, cairo_t *cr, gpointer) -> gboolean
                                                  {
                                                      cairo_set_source_rgba(cr, 0, 0, 0, 0);
                                                      cairo_paint(cr);
                                                      return FALSE;
                                                  }),
                     nullptr);
    gtk_widget_show_all(pulseWin);

    // 6. Spawn GStreamer pipeline to capture 1 frame
    static std::atomic<uint64_t> s_snapCounter{0};
    std::string tempPath = std::string(g_get_tmp_dir()) + "/sr_snap_" +
                           std::to_string(getpid()) + "_" +
                           std::to_string(++s_snapCounter) + ".png";

    std::string gstCmd = "gst-launch-1.0 -q pipewiresrc path=" + std::to_string(nodeId) +
                         " num-buffers=1 ! videoconvert ! pngenc ! filesink location=\"" + tempPath + "\"";

    gint argcGst = 0;
    gchar **argvGst = nullptr;
    g_shell_parse_argv(gstCmd.c_str(), &argcGst, &argvGst, nullptr);

    GPid gstPid;
    gboolean spawned = g_spawn_async(nullptr, argvGst, nullptr,
                                     static_cast<GSpawnFlags>(G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH),
                                     nullptr, nullptr, &gstPid, nullptr);
    g_strfreev(argvGst);

    if (!spawned)
    {
        std::cerr << "SnapshotHelper: Failed to spawn gst-launch" << std::endl;
        gtk_widget_destroy(pulseWin);
        g_dbus_connection_call_sync(bus, "org.gnome.Mutter.ScreenCast", sessionPath.c_str(),
                                    "org.gnome.Mutter.ScreenCast.Session", "Stop",
                                    nullptr, nullptr, G_DBUS_CALL_FLAGS_NONE, 500, nullptr, nullptr);
        g_object_unref(bus);
        return nullptr;
    }

    // Process GTK events and wait for gst-launch to finish (typically ~50-90ms)
    auto gstStart = std::chrono::steady_clock::now();
    bool finished = false;
    while (!finished && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - gstStart).count() < 3000)
    {
        while (gtk_events_pending())
        {
            gtk_main_iteration();
        }

        int status = 0;
        pid_t res = waitpid(gstPid, &status, WNOHANG);
        if (res == gstPid)
        {
            finished = true;
            break;
        }
        usleep(5000);
    }

    g_spawn_close_pid(gstPid);
    gtk_widget_destroy(pulseWin);
    while (gtk_events_pending())
    {
        gtk_main_iteration();
    }

    // 7. Stop Mutter ScreenCast session
    g_dbus_connection_call_sync(
        bus,
        "org.gnome.Mutter.ScreenCast",
        sessionPath.c_str(),
        "org.gnome.Mutter.ScreenCast.Session",
        "Stop",
        nullptr, nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        1000, nullptr, nullptr);
    g_object_unref(bus);

    if (!finished)
    {
        std::cerr << "SnapshotHelper: gst-launch capture timed out" << std::endl;
        unlink(tempPath.c_str());
        return nullptr;
    }

    // 8. Load image into Cairo surface
    cairo_surface_t *surface = cairo_image_surface_create_from_png(tempPath.c_str());
    unlink(tempPath.c_str());

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
    {
        std::cerr << "SnapshotHelper: Failed to create Cairo surface from snapshot: "
                  << cairo_status_to_string(cairo_surface_status(surface)) << std::endl;
        cairo_surface_destroy(surface);
        return nullptr;
    }

    return surface;
}
