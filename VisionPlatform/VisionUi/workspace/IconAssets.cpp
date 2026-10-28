/**
 * @file IconAssets.cpp
 * @brief IconAssets实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "IconAssets.h"
#include "AppPaths.h"
#include <QFile>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>

namespace Vision {

namespace {

// 解析 exe/icons 下绝对路径
QString iconAbsPath(const QString& relativePath)
{
    return QString::fromUtf8(AppPaths::exeDir().c_str())
           + QLatin1String("/icons/")
           + relativePath;
}

} // namespace

// 优先 SVG（QSvgRenderer），其次同名 PNG
QIcon loadVisionIcon(const QString& relativePath, int px)
{
    if (relativePath.isEmpty() || px <= 0) {
        return QIcon();
    }
    const QString abs = iconAbsPath(relativePath);
    if (abs.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive) && QFile::exists(abs)) {
        QSvgRenderer renderer(abs);
        if (renderer.isValid()) {
            QPixmap pm(px, px);
            pm.fill(Qt::transparent);
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing, true);
            p.setRenderHint(QPainter::SmoothPixmapTransform, true);
            // 略留边距，避免描边贴边发糊
            const qreal pad = px * 0.08;
            renderer.render(&p, QRectF(pad, pad, px - 2 * pad, px - 2 * pad));
            p.end();
            return QIcon(pm);
        }
    }

    QString pngPath = abs;
    if (pngPath.endsWith(QLatin1String(".svg"), Qt::CaseInsensitive)) {
        pngPath.chop(4);
        pngPath += QLatin1String(".png");
    }
    QPixmap png(pngPath);
    if (png.isNull() && QFile::exists(abs)) {
        png = QPixmap(abs);
    }
    if (!png.isNull()) {
        return QIcon(png.scaled(px, px, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    return QIcon();
}

// 浅色圆角底 + 居中图标
QIcon badgeVisionIcon(const QIcon& icon, const QColor& bg, int px)
{
    if (icon.isNull() || px <= 0) {
        return icon;
    }
    QPixmap pm(px, px);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(QRectF(0.5, 0.5, px - 1.0, px - 1.0), 5.0, 5.0);
    const int inner = qMax(10, px - 8);
    const QRect ir((px - inner) / 2, (px - inner) / 2, inner, inner);
    icon.paint(&p, ir);
    p.end();
    return QIcon(pm);
}

}
