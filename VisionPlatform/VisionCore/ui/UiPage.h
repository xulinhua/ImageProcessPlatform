/**
 * @file UiPage.h
 * @brief 自定义运行界面页与控件描述
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_UIPAGE_H
#define VISION_UIPAGE_H
#include "VisionCoreExport.h"

#include <string>
#include <vector>

namespace Vision {

/** @brief 自定义 UI 控件种类（存盘为整型） */
enum UiWidgetKind {
    UiWImageView = 0, ///< 图像窗
    UiWLabel = 1,     ///< 静态文字
    UiWValue = 2,     ///< 绑定数值/文本
    UiWOkNg = 3,      ///< OK/NG 指示
    UiWRunOnce = 4,   ///< 单次运行按钮
    UiWLoop = 5,      ///< 循环运行按钮
    UiWButton = 6     ///< 通用按钮（靠 action）
};

/**
 * @class UiWidget
 * @brief 画布上一个控件：几何、文案、数据路径、动作
 *
 * dataPath 例：global:Count、image:toolId、okng:stationId
 * action 例：runOnce、loop、stop、reset
 */
struct VISIONCORE_API UiWidget {
    std::string id;     ///< 控件 id
    UiWidgetKind kind;  ///< 种类
    int x;              ///< 左
    int y;              ///< 上
    int w;              ///< 宽
    int h;              ///< 高
    std::string text;   ///< 显示文案 / 按钮标题
    std::string dataPath; ///< 数据绑定路径
    std::string action; ///< 按钮动作
    /** @brief 标签、位置 0，尺寸 120×32 */
    UiWidget()
        : kind(UiWLabel)
        , x(0)
        , y(0)
        , w(120)
        , h(32)
    {}
};

/**
 * @class UiPage
 * @brief 一页自定义运行界面
 */
struct VISIONCORE_API UiPage {
    std::string id;              ///< 页 id
    std::string name;            ///< 显示名
    int width;                   ///< 画布宽
    int height;                  ///< 画布高
    bool enabled;                ///< 运行态是否启用
    std::vector<UiWidget> widgets; ///< 控件列表
    /** @brief 默认 800×600、启用 */
    UiPage()
        : width(800)
        , height(600)
        , enabled(true)
    {}
};

}

#endif
