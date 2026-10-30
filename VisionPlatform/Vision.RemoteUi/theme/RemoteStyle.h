/**
 * @file RemoteStyle.h
 * @brief 复选框、单选框的画法。样式表画不出勾，交给这里
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_STYLE_H
#define VISION_REMOTE_STYLE_H

#include <QProxyStyle>

namespace Vision {

/**
 * @class RemoteStyle
 * @brief 包一层 Fusion：14 像素复选框与单选框、去掉焦点虚线框，其余交给基础风格
 * @note 由 RemoteTheme::apply 创建并交给 QApplication 持有
 */
class RemoteStyle : public QProxyStyle {
public:
    /** @brief base 为被代理的基础风格，所有权转给本对象 */
    explicit RemoteStyle(QStyle* base);

    /** @brief 自画复选框（含列表项勾选框）和单选框，不画焦点框，其余交给基础风格 */
    void drawPrimitive(PrimitiveElement element, const QStyleOption* option, QPainter* painter,
        const QWidget* widget = 0) const override;
    /** @brief 指示框边长 14 像素、与文字间距 6 像素，其余交给基础风格 */
    int pixelMetric(PixelMetric metric, const QStyleOption* option = 0, const QWidget* widget = 0) const override;
    /** @brief 列表选中时整行（含图标区）高亮，其余交给基础风格 */
    int styleHint(StyleHint hint, const QStyleOption* option = 0, const QWidget* widget = 0,
        QStyleHintReturn* returnData = 0) const override;

private:
    /** @brief 画圆角方框：选中为主色底白勾，半选为白横线，禁用为灰 */
    void drawCheck(const QStyleOption* option, QPainter* painter) const;
    /** @brief 画圆框：选中为主色边框加 6 像素实心点，禁用为灰 */
    void drawRadio(const QStyleOption* option, QPainter* painter) const;
};

}

#endif
