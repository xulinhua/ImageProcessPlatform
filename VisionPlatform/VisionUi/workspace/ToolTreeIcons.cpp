/**
 * @file ToolTreeIcons.cpp
 * @brief ToolTreeIcons实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolTreeIcons.h"
#include "IconAssets.h"
#include <QColor>
#include <QHash>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QPolygon>

namespace Vision {

namespace {

const char* kIconSvgNames[ToolTreeIconCount] = {
    "tools/camera.svg",
    "tools/preprocess.svg",
    "tools/locate.svg",
    "tools/detect.svg",
    "tools/logic.svg",
    "tools/comm.svg",
    "tools/result.svg",
    "tools/parallel.svg",
    "tools/if.svg",
    "tools/branch.svg"
};

const QColor kIconColors[ToolTreeIconCount] = {
    QColor(0x1E, 0x88, 0xE5),
    QColor(0x43, 0xA0, 0x47),
    QColor(0x8E, 0x24, 0xAA),
    QColor(0xE5, 0x39, 0x35),
    QColor(0x39, 0x49, 0xAB),
    QColor(0x00, 0x89, 0x7B),
    QColor(0xFB, 0x8C, 0x00),
    QColor(0x15, 0x65, 0xC0),
    QColor(0x6A, 0x1B, 0x9A),
    QColor(0x54, 0x6E, 0x7A)
};

QIcon g_cache[ToolTreeIconCount];
bool g_cacheOk[ToolTreeIconCount] = { false };
QHash<QString, QIcon> g_nameCache;

// 磁盘缺失时：细线风格占位（接近 Lucide 线宽）
QIcon makeFallbackIcon(int key)
{
    if (key < 0 || key >= ToolTreeIconCount) {
        key = ToolTreeIconCamera;
    }
    const QColor c = kIconColors[key];
    QPixmap pm(20, 20);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(c, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    switch (key) {
    case ToolTreeIconCamera:
        p.drawRoundedRect(QRectF(3, 6, 14, 10), 2, 2);
        p.drawEllipse(QPointF(10, 11), 2.5, 2.5);
        p.drawLine(QPointF(7, 6), QPointF(8.5, 4));
        p.drawLine(QPointF(8.5, 4), QPointF(11.5, 4));
        p.drawLine(QPointF(11.5, 4), QPointF(13, 6));
        break;
    case ToolTreeIconLocate:
        p.drawEllipse(QPointF(10, 10), 5, 5);
        p.drawLine(QPointF(10, 3), QPointF(10, 7));
        p.drawLine(QPointF(10, 13), QPointF(10, 17));
        p.drawLine(QPointF(3, 10), QPointF(7, 10));
        p.drawLine(QPointF(13, 10), QPointF(17, 10));
        break;
    case ToolTreeIconDetect:
        p.drawEllipse(QPointF(9, 9), 4.5, 4.5);
        p.drawLine(QPointF(12.5, 12.5), QPointF(16.5, 16.5));
        break;
    case ToolTreeIconParallel:
        p.drawLine(QPointF(6, 4), QPointF(6, 16));
        p.drawLine(QPointF(10, 4), QPointF(10, 16));
        p.drawLine(QPointF(14, 4), QPointF(14, 16));
        break;
    case ToolTreeIconIf:
        p.drawLine(QPointF(10, 3), QPointF(10, 10));
        p.drawLine(QPointF(10, 10), QPointF(5, 16));
        p.drawLine(QPointF(10, 10), QPointF(15, 16));
        break;
    case ToolTreeIconBranch:
        p.drawLine(QPointF(7, 4), QPointF(7, 16));
        p.drawLine(QPointF(7, 10), QPointF(14, 6));
        p.drawEllipse(QPointF(7, 4), 1.6, 1.6);
        p.drawEllipse(QPointF(14, 6), 1.6, 1.6);
        p.drawEllipse(QPointF(7, 16), 1.6, 1.6);
        break;
    default:
        p.drawRoundedRect(QRectF(4, 4, 12, 12), 2.5, 2.5);
        break;
    }
    p.end();
    return QIcon(pm);
}

} // namespace

// ToolType → 图标键
int toolTreeIconKeyForType(ToolType type)
{
    switch (type) {
    case ToolType::Acquire: return ToolTreeIconCamera;
    case ToolType::Preprocess: return ToolTreeIconPreprocess;
    case ToolType::Locate: return ToolTreeIconLocate;
    case ToolType::Detect: return ToolTreeIconDetect;
    case ToolType::Logic: return ToolTreeIconLogic;
    case ToolType::Comm: return ToolTreeIconComm;
    case ToolType::RstProcess: return ToolTreeIconResult;
    case ToolType::Calib: return ToolTreeIconResult;
    }
    return ToolTreeIconCamera;
}

// 优先 icons/tools/*.svg，缺失时细线占位
QIcon toolTreeIcon(int key)
{
    if (key < 0 || key >= ToolTreeIconCount) {
        key = ToolTreeIconCamera;
    }
    if (g_cacheOk[key] && !g_cache[key].isNull()) {
        return g_cache[key];
    }
    QIcon ic = loadVisionIcon(QLatin1String(kIconSvgNames[key]), 20);
    if (ic.isNull()) {
        ic = makeFallbackIcon(key);
    }
    g_cache[key] = ic;
    g_cacheOk[key] = true;
    return g_cache[key];
}

// 优先 tools/<toolName>.svg，缺失时回退类型图标
QIcon toolTreeIconForName(const QString& toolName, int typeKeyFallback)
{
    if (toolName.isEmpty()) {
        return toolTreeIcon(typeKeyFallback);
    }
    const QHash<QString, QIcon>::const_iterator it = g_nameCache.constFind(toolName);
    if (it != g_nameCache.constEnd()) {
        return it.value();
    }
    const QIcon named = loadVisionIcon(QLatin1String("tools/") + toolName + QLatin1String(".svg"), 20);
    if (!named.isNull()) {
        g_nameCache.insert(toolName, named);
        return named;
    }
    const QIcon fallback = toolTreeIcon(typeKeyFallback);
    g_nameCache.insert(toolName, fallback);
    return fallback;
}

// 丢掉工具树图标缓存，下次从磁盘重载
void clearToolTreeIconCache()
{
    for (int i = 0; i < ToolTreeIconCount; ++i) {
        g_cache[i] = QIcon();
        g_cacheOk[i] = false;
    }
    g_nameCache.clear();
}

}
