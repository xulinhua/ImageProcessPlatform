/**
 * @file UiIcons.cpp
 * @brief UiIcons实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "UiIcons.h"
#include "IconAssets.h"

namespace Vision {

namespace {

const char* kUiFiles[UiIconCount] = {
    "ui/new.svg",
    "ui/open.svg",
    "ui/save.svg",
    "ui/stations.svg",
    "ui/devices.svg",
    "ui/comm.svg",
    "ui/run.svg",
    "ui/start.svg",
    "ui/stop.svg",
    "ui/reset.svg",
    "ui/design.svg",
    "ui/runtime.svg",
    "ui/recheck.svg",
    "ui/new-station.svg",
    "ui/new-job.svg"
};

QIcon g_uiCache[UiIconCount];
bool g_uiOk[UiIconCount] = { false };

} // namespace

// 加载并缓存主界面图标
QIcon uiIcon(int key, int px)
{
    if (key < 0 || key >= UiIconCount) {
        key = UiIconNew;
    }
    if (g_uiOk[key] && !g_uiCache[key].isNull()) {
        return g_uiCache[key];
    }
    QIcon ic = loadVisionIcon(QLatin1String(kUiFiles[key]), px);
    g_uiCache[key] = ic;
    g_uiOk[key] = true;
    return g_uiCache[key];
}

// 丢掉主界面图标缓存，下次从磁盘重载
void clearUiIconCache()
{
    for (int i = 0; i < UiIconCount; ++i) {
        g_uiCache[i] = QIcon();
        g_uiOk[i] = false;
    }
}

}