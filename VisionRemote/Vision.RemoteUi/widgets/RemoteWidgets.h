/**
 * @file RemoteWidgets.h
 * @brief 两端共用的小控件：开关、状态点、标签、分段按钮、分隔线、头像
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_REMOTE_WIDGETS_H
#define VISION_REMOTE_WIDGETS_H

#include <QAbstractButton>
#include <QFrame>
#include <QLabel>
#include <QWidget>
#include <functional>
#include <vector>

class QButtonGroup;
class QHBoxLayout;
class QPushButton;

namespace Vision {

/**
 * @class SwitchButton
 * @brief 34×18 的开关。可勾选按钮，选中时滑块在右、轨道为主色
 */
class SwitchButton : public QAbstractButton {
public:
    /** @brief 可勾选、手形光标、固定 34×18 */
    explicit SwitchButton(QWidget* parent = 0);
    /** @brief 固定 34×18 */
    QSize sizeHint() const override;

protected:
    /** @brief 画圆角轨道和白色滑块，禁用时轨道变浅 */
    void paintEvent(QPaintEvent* event) override;
};

/**
 * @class StatusDot
 * @brief 8 像素圆点。green 在线，gray 离线，orange 告警，red 失败
 */
class StatusDot : public QWidget {
public:
    /** @brief 固定 8×8，初始色调为 tone */
    explicit StatusDot(const char* tone = "green", QWidget* parent = 0);
    /** @brief 换色调：gray、orange、red、blue，其他值按 green；颜色不变时不重绘 */
    void setTone(const char* tone);
    /** @brief 固定 8×8 */
    QSize sizeHint() const override;

protected:
    /** @brief 用当前颜色画满整个控件的实心圆 */
    void paintEvent(QPaintEvent* event) override;

private:
    QColor m_color;     ///< 当前填充色，由 setTone 决定
};

/**
 * @class TagLabel
 * @brief 圆角小标签，tone 取 gray、blue、green、red、orange、purple
 * @note 颜色由样式表的 QLabel[tone=...] 规则决定
 */
class TagLabel : public QLabel {
public:
    /** @brief 设好 tone 属性，尺寸随文字固定 */
    TagLabel(const QString& text, const char* tone, QWidget* parent = 0);
    /** @brief 换色调并重新套样式；与当前相同时什么都不做 */
    void setTone(const char* tone);
};

/**
 * @class SegmentBar
 * @brief 一组互斥按钮，首尾圆角
 * @note 按钮编号即添加顺序，从 0 起
 */
class SegmentBar : public QWidget {
public:
    /** @brief 空的一组，按钮之间无间距 */
    explicit SegmentBar(QWidget* parent = 0);

    /************************************************** 按钮 **************************************************/
    /**
     * @brief 末尾加一个可勾选按钮；text 为空时按纯图标按钮显示
     * @return 新按钮的编号
     */
    int addButton(const QString& text, const char* iconName = 0);
    /** @brief 删除全部按钮 */
    void clear();
    /** @brief 按钮个数 */
    int count() const;

    /************************************************** 选中 **************************************************/
    /** @brief 当前选中的编号，没有选中为 -1 */
    int current() const;
    /** @brief 选中 index；越界时取消选中。不触发 setOnChanged 的回调 */
    void setCurrent(int index);

    /************************************************** 状态 **************************************************/
    /** @brief 启用或禁用某个按钮并设提示；越界忽略 */
    void setButtonEnabled(int index, bool enabled, const QString& tip = QString());
    /** @brief 用户点击按钮时回调，参数为编号 */
    void setOnChanged(const std::function<void(int)>& changed);

private:
    /** @brief 按位置给按钮设 seg 属性（first、mid、last），只有一个时清空 */
    void restyle();

    QHBoxLayout* m_layout;                  ///< 横向排列按钮，子对象
    QButtonGroup* m_group;                  ///< 互斥分组，id 即按钮编号
    std::vector<QPushButton*> m_buttons;    ///< 按编号排列的按钮
    std::function<void(int)> m_changed;     ///< 点击回调，可空
};

/**
 * @class ToolSeparator
 * @brief 工具条里的竖线，1×22，颜色由样式表 QFrame#sep 决定
 */
class ToolSeparator : public QFrame {
public:
    /** @brief 设 objectName 为 sep，固定 1×22 */
    explicit ToolSeparator(QWidget* parent = 0);
};

/**
 * @class AvatarLabel
 * @brief 圆形头像，显示名字第一个字
 */
class AvatarLabel : public QLabel {
public:
    /**
     * @brief 按 index 从 5 种底色里轮流取色
     * @param index 颜色序号，负数按 0
     * @param side 直径（像素）
     */
    AvatarLabel(const QString& name, int index, int side, QWidget* parent = 0);
};

}

#endif
