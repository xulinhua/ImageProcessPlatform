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
/**
 * @class DrawInfoEntry
 * @brief 一条叠加文字（drawInfo），在前景按设备坐标绘制
 */
struct DrawInfoEntry {
    QString text; ///< 文字内容
    QPointF pos;  ///< 锚点：图像坐标或窗口坐标，取决于所在列表
    int fontPx;   ///< 字号（屏幕像素）
    QColor color; ///< 文字颜色
};
/** @class ForegroundLayer @brief 前景：drawInfo、信息条与 HUD开关 */
class ForegroundLayer {
public:
    /** @brief 默认打开 HUD */
    ForegroundLayer();
    /** @brief HUD 开关 */
    bool& hudVisible();
    /** @brief HUD 开关（只读） */
    bool hudVisible() const;
    /** @brief 信息条文本 */
    QString& infoText();
    /** @brief 信息条文本（只读） */
    const QString& infoText() const;
    /** @brief 图像坐标系的叠加文字 */
    QVector<DrawInfoEntry>& imageInfos();
    /** @brief 窗口坐标系的叠加文字 */
    QVector<DrawInfoEntry>& windowInfos();
    /** @brief 清空两种坐标系的叠加文字 */
    void clearInfos();
    /** @brief 清空一种坐标系的叠加文字 */
    void clearInfos(DrawInfoSpace space);
private:
    bool m_hud;                           ///< HUD 开关
    QString m_info;                       ///< 信息条文本
    QVector<DrawInfoEntry> m_imageInfos;  ///< 图像坐标叠加文字
    QVector<DrawInfoEntry> m_windowInfos; ///< 窗口坐标叠加文字
};
}
#endif