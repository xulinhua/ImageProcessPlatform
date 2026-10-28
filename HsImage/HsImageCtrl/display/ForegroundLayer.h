/**
 * @file ForegroundLayer.h
 * @brief ForegroundLayer头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef HSIMAGE_FOREGROUNDLAYER_H
#define HSIMAGE_FOREGROUNDLAYER_H
#include <HsCtrl/HsImageWindowTypes.h>
#include <QColor>
#include <QPointF>
#include <QString>
#include <QVector>
namespace HsCtrl {
struct DrawInfoEntry {
    QString text;
    QPointF pos;
    int fontPx;
    QColor color;
};
/** @class ForegroundLayer @brief 前景：drawInfo、信息条与 HUD开关 */
class ForegroundLayer {
public:
    ForegroundLayer();
    bool& hudVisible();
    bool hudVisible() const;
    QString& infoText();
    const QString& infoText() const;
    QVector<DrawInfoEntry>& imageInfos();
    QVector<DrawInfoEntry>& windowInfos();
    void clearInfos();
    void clearInfos(DrawInfoSpace space);
private:
    bool m_hud;
    QString m_info;
    QVector<DrawInfoEntry> m_imageInfos;
    QVector<DrawInfoEntry> m_windowInfos;
};
}
#endif