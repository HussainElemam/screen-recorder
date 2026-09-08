#pragma once

#include "common.hpp"
#include <vector>
#include <memory>

class DisplayManager
{
public:
    DisplayManager();
    ~DisplayManager();

    bool refreshMonitors();
    const std::vector<MonitorInfo> &getMonitors() const { return m_monitors; }
    const MonitorInfo *getMonitorByConnector(const std::string &connector) const;
    const MonitorInfo *getMonitorByIndex(int index) const;

private:
    std::vector<MonitorInfo> m_monitors;
    bool queryFromMutter();
    bool queryFromGDK();
};
