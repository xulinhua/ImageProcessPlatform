/**
 * @file RemoteTheme.h
 * @brief 远程控制两端共用的界面风格：配色、样式表、线条图标
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_THEME_H
#define VISION_REMOTE_THEME_H

#include <QColor>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <QVariant>

class QApplication;
class QPushButton;
class QWidget;

namespace Vision {

/**
 * @class RemoteTheme
 * @brief 颜色取自《远程控制方案》12.1。图标与 docs/remote-ui/ui.js 是同一套线条图标
 */
class RemoteTheme {
public:
    static const char* const kPrimary;  ///< 主色 #1890FF：主按钮、选中、开关打开
    static const char* const kSuccess;  ///< 成功色 #52C41A：在线状态点
    static const char* const kWarning;  ///< 警告色 #FAAD14：告警状态点
    static const char* const kDanger;   ///< 危险色 #FF4D4F：断开、删除、失败
    static const char* const kText;     ///< 正文色 #262626，图标默认颜色
    static const char* const kMuted;    ///< 次要文字色 #8C8C8C，下拉箭头颜色
    static const char* const kBorder;   ///< 边框色 #E8E8E8
    static const char* const kPage;     ///< 页面底色 #F5F7FA，调色板 Window

    /************************************************** 全局 **************************************************/
    /** @brief 设置字体、调色板、基础风格和样式表。两个 exe 的 main 里调用一次 */
    static void apply(QApplication* app);

    /************************************************** 图标 **************************************************/
    /**
     * @brief 线条图标，含 1x / 2x 的正常态和灰色禁用态
     * @param name 图标名，见 body() 里的表；未知名字得到空图
     * @param px 逻辑边长（像素）
     */
    static QIcon icon(const char* name, const QColor& color = QColor(kText), int px = 15);
    /**
     * @brief 按设备像素比渲染一张线条图标
     * @param px 逻辑边长（像素），实际像素为 px * dpr
     * @param dpr 设备像素比，通常传 devicePixelRatioF()
     */
    static QPixmap pixmap(const char* name, const QColor& color, int px, qreal dpr = 1.0);

    /************************************************** 控件 **************************************************/
    /**
     * @brief 按样式表里的属性分类建按钮
     * @param kind 空、primary、danger、dangerFill、icon
     */
    static QPushButton* button(const QString& text, const char* iconName, const char* kind, QWidget* parent);

    /** @brief 改动态属性后重新套样式 */
    static void setProp(QWidget* widget, const char* key, const QVariant& value);

    /** @brief 尚未提供的功能：灰掉，tooltip 说明何时提供（phase 文案） */
    static void markLater(QWidget* widget, const QString& phase);

    /**
     * @brief 用 GDI+ 解 JPEG，避免 Debug 下 Qt qjpegd 对部分帧 fail-fast 闪退
     * @param data JPEG 字节；size <= 0 或失败返回空图
     */
    static QImage imageFromJpeg(const unsigned char* data, int size);
    /**
     * @brief 把抓屏 BGRA 建成可显示的 QImage
     * @note 抓屏 alpha 常为 0；用 Format_RGB32 按不透明画，避免会话窗一片空、像没数据
     */
    static QImage imageFromBgra(const unsigned char* data, int size, int width, int height);

private:
    /** @brief 全局样式表；arrow 为下拉箭头图片路径，替换样式表里的 ARROW */
    static QString styleSheet(const QString& arrow);
    /** @brief 拼出 24×24 视口的完整 SVG，描边与 currentColor 都换成 color */
    static QByteArray svg(const char* name, const QColor& color);
    /** @brief 按名字查图标的 SVG 内部元素，找不到返回空串 */
    static const char* body(const char* name);
};

}

#endif
