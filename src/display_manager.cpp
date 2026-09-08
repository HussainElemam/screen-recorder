#include "display_manager.hpp"
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <iostream>
#include <map>

DisplayManager::DisplayManager()
{
    refreshMonitors();
}

DisplayManager::~DisplayManager()
{
}

bool DisplayManager::refreshMonitors()
{
    m_monitors.clear();
    if (queryFromMutter() && !m_monitors.empty())
    {
        return true;
    }
    return queryFromGDK();
}

const MonitorInfo *DisplayManager::getMonitorByConnector(const std::string &connector) const
{
    for (const auto &mon : m_monitors)
    {
        if (mon.connector == connector)
        {
            return &mon;
        }
    }
    return nullptr;
}

const MonitorInfo *DisplayManager::getMonitorByIndex(int index) const
{
    if (index >= 0 && index < static_cast<int>(m_monitors.size()))
    {
        return &m_monitors[index];
    }
    return nullptr;
}

bool DisplayManager::queryFromMutter()
{
    GError *error = nullptr;
    GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
    if (!connection)
    {
        if (error)
        {
            std::cerr << "DisplayManager: Failed to connect to session bus: " << error->message << std::endl;
            g_error_free(error);
        }
        return false;
    }

    GVariant *reply = g_dbus_connection_call_sync(
        connection,
        "org.gnome.Mutter.DisplayConfig",
        "/org/gnome/Mutter/DisplayConfig",
        "org.gnome.Mutter.DisplayConfig",
        "GetCurrentState",
        nullptr,
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        2000,
        nullptr,
        &error);

    if (!reply)
    {
        if (error)
        {
            std::cerr << "DisplayManager: Mutter GetCurrentState call failed: " << error->message << std::endl;
            g_error_free(error);
        }
        g_object_unref(connection);
        return false;
    }

    struct RawMonitor
    {
        std::string connector;
        std::string vendor;
        std::string product;
        std::string serial;
        std::string displayName;
    };
    std::map<std::string, RawMonitor> monitorMap;

    // reply is (u a(...) a(...) a{sv})
    // Child 1: monitors array
    GVariant *monitorsArray = g_variant_get_child_value(reply, 1);
    if (monitorsArray)
    {
        gsize nMonitors = g_variant_n_children(monitorsArray);
        for (gsize i = 0; i < nMonitors; ++i)
        {
            GVariant *monTuple = g_variant_get_child_value(monitorsArray, i);
            if (!monTuple)
                continue;

            GVariant *specTuple = g_variant_get_child_value(monTuple, 0);
            if (specTuple)
            {
                const char *connector = nullptr;
                const char *vendor = nullptr;
                const char *product = nullptr;
                const char *serial = nullptr;
                g_variant_get(specTuple, "(&s&s&s&s)", &connector, &vendor, &product, &serial);

                RawMonitor raw;
                raw.connector = connector ? connector : "";
                raw.vendor = vendor ? vendor : "";
                raw.product = product ? product : "";
                raw.serial = serial ? serial : "";
                raw.displayName = raw.connector;

                GVariant *propsDict = g_variant_get_child_value(monTuple, 2);
                if (propsDict)
                {
                    GVariant *dnVal = g_variant_lookup_value(propsDict, "display-name", G_VARIANT_TYPE_STRING);
                    if (dnVal)
                    {
                        raw.displayName = g_variant_get_string(dnVal, nullptr);
                        g_variant_unref(dnVal);
                    }
                    g_variant_unref(propsDict);
                }

                if (!raw.connector.empty())
                {
                    monitorMap[raw.connector] = raw;
                }
                g_variant_unref(specTuple);
            }
            g_variant_unref(monTuple);
        }
        g_variant_unref(monitorsArray);
    }

    // Child 2: logical_monitors array
    GVariant *logicalMonitorsArray = g_variant_get_child_value(reply, 2);
    int monIndex = 0;
    if (logicalMonitorsArray)
    {
        gsize nLogicals = g_variant_n_children(logicalMonitorsArray);
        for (gsize i = 0; i < nLogicals; ++i)
        {
            GVariant *logTuple = g_variant_get_child_value(logicalMonitorsArray, i);
            if (!logTuple)
                continue;

            gint32 x = 0, y = 0;
            gdouble scale = 1.0;
            gboolean isPrimary = FALSE;

            GVariant *xVal = g_variant_get_child_value(logTuple, 0);
            GVariant *yVal = g_variant_get_child_value(logTuple, 1);
            GVariant *scaleVal = g_variant_get_child_value(logTuple, 2);
            GVariant *primVal = g_variant_get_child_value(logTuple, 4);

            if (xVal)
            {
                x = g_variant_get_int32(xVal);
                g_variant_unref(xVal);
            }
            if (yVal)
            {
                y = g_variant_get_int32(yVal);
                g_variant_unref(yVal);
            }
            if (scaleVal)
            {
                scale = g_variant_get_double(scaleVal);
                g_variant_unref(scaleVal);
            }
            if (primVal)
            {
                isPrimary = g_variant_get_boolean(primVal);
                g_variant_unref(primVal);
            }

            GVariant *specsArray = g_variant_get_child_value(logTuple, 5);
            if (specsArray)
            {
                gsize nSpecs = g_variant_n_children(specsArray);
                for (gsize s = 0; s < nSpecs; ++s)
                {
                    GVariant *sTuple = g_variant_get_child_value(specsArray, s);
                    if (!sTuple)
                        continue;

                    const char *connector = nullptr;
                    const char *vendor = nullptr;
                    const char *product = nullptr;
                    const char *serial = nullptr;
                    g_variant_get(sTuple, "(&s&s&s&s)", &connector, &vendor, &product, &serial);

                    if (connector)
                    {
                        std::string connStr = connector;
                        MonitorInfo info;
                        info.index = monIndex++;
                        info.connector = connStr;
                        info.x = x;
                        info.y = y;
                        info.scale = scale;
                        info.isPrimary = isPrimary;

                        if (monitorMap.find(connStr) != monitorMap.end())
                        {
                            info.displayName = monitorMap[connStr].displayName;
                            info.manufacturer = monitorMap[connStr].vendor;
                            info.model = monitorMap[connStr].product;
                        }
                        else
                        {
                            info.displayName = connStr;
                        }

                        m_monitors.push_back(info);
                    }
                    g_variant_unref(sTuple);
                }
                g_variant_unref(specsArray);
            }
            g_variant_unref(logTuple);
        }
        g_variant_unref(logicalMonitorsArray);
    }

    g_variant_unref(reply);
    g_object_unref(connection);

    // Cross-reference with GDK to obtain width and height
    GdkDisplay *display = gdk_display_get_default();
    if (display)
    {
        int nGdk = gdk_display_get_n_monitors(display);
        for (size_t i = 0; i < m_monitors.size() && static_cast<int>(i) < nGdk; ++i)
        {
            GdkMonitor *gdkMon = gdk_display_get_monitor(display, static_cast<int>(i));
            if (gdkMon)
            {
                GdkRectangle geom;
                gdk_monitor_get_geometry(gdkMon, &geom);
                m_monitors[i].width = geom.width;
                m_monitors[i].height = geom.height;
            }
        }
    }

    return !m_monitors.empty();
}

bool DisplayManager::queryFromGDK()
{
    GdkDisplay *display = gdk_display_get_default();
    if (!display)
        return false;

    int nMonitors = gdk_display_get_n_monitors(display);
    for (int i = 0; i < nMonitors; ++i)
    {
        GdkMonitor *mon = gdk_display_get_monitor(display, i);
        if (!mon)
            continue;

        GdkRectangle geom;
        gdk_monitor_get_geometry(mon, &geom);

        MonitorInfo info;
        info.index = i;
        const char *model = gdk_monitor_get_model(mon);
        const char *manuf = gdk_monitor_get_manufacturer(mon);
        info.model = model ? model : "";
        info.manufacturer = manuf ? manuf : "";
        info.connector = "screen-" + std::to_string(i);
        info.displayName = (!info.model.empty()) ? (info.manufacturer + " " + info.model) : ("Display " + std::to_string(i + 1));
        info.x = geom.x;
        info.y = geom.y;
        info.width = geom.width;
        info.height = geom.height;
        info.scale = gdk_monitor_get_scale_factor(mon);
        info.isPrimary = gdk_monitor_is_primary(mon);

        m_monitors.push_back(info);
    }
    return !m_monitors.empty();
}
