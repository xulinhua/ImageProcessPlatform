/**
 * @file ToolSpec.h
 * @brief 调试动作、输出项、公差项和显示项
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOL_SPEC_H
#define VISION_TOOL_SPEC_H

#include <string>
#include <vector>

namespace Vision {

/** @brief 调试按钮放在哪一页 */
enum class ActionPlace {
    Run,   ///< 基础参数页
    Teach, ///< 示教页
    Both   ///< 两页都放
};

/**
 * @class ToolAction
 * @brief 调试页上的一个动作按钮
 */
struct ToolAction {
    std::string id;    ///< 动作 id，传给 action
    std::string title; ///< 按钮文字
    ActionPlace place; ///< 按钮所在页
    bool live;         ///< 参数变化时是否自动执行
    ToolAction() : place(ActionPlace::Run), live(false) {}
};

/** @brief 输出项是数值还是文本 */
enum class OutputKind {
    Number = 0, ///< 数值
    Text        ///< 文本
};

/** @class OutputField @brief 一项对外输出 */
struct OutputField {
    std::string key;   ///< 测值键
    std::string title; ///< 界面名称
    std::string unit;  ///< 单位，可空
    OutputKind kind;   ///< 数值或文本
    OutputField() : kind(OutputKind::Number) {}
};

/** @class OutputSpec @brief 工具对外输出表。cfg_output 返回这个 */
struct OutputSpec {
    std::vector<OutputField> fields; ///< 输出项
};

/** @class TolField @brief 一项公差。默认上下限，界面可改 */
struct TolField {
    std::string key;    ///< 项键，与本拍测值的键相同
    std::string title;  ///< 界面名称
    std::string unit;   ///< 单位，可空
    std::string parent; ///< 父项键。子项挂在这一项下面
    double defMin;      ///< 默认下限
    double defMax;      ///< 默认上限
    bool integer;       ///< 整数项。个数用整数上下限，面积和偏移用小数
    TolField() : defMin(0), defMax(0), integer(false) {}
};

/** @class TolSpec @brief 工具公差表。cfg_tol 返回这个 */
struct TolSpec {
    std::vector<TolField> items; ///< 公差项
};

/** @class GraphicSlot @brief 一项可开关的结果图形 */
struct GraphicSlot {
    std::string key;   ///< 图形键，如 teachRoi、resultRoi、followRoi
    std::string title; ///< 界面名称
    bool defEnable;    ///< 新建时是否显示
    bool region;       ///< 区域。界面可选填充或边缘，以及透明度
    GraphicSlot() : defEnable(true), region(false) {}
};

/** @brief 结果文字放在哪里 */
enum class TextAnchor {
    ImageCorner = 0, ///< 图像位置，坐标由显示配置给出
    ResultPoint,     ///< 结果位置
    BesideGraphic    ///< 图形旁边
};

/** @class TextSlot @brief 一项可开关的结果文字 */
struct TextSlot {
    std::string key;      ///< 文字键，如 pose、verdict
    std::string title;    ///< 界面名称
    TextAnchor defAnchor; ///< 默认位置
    bool defEnable;       ///< 新建时是否显示
    TextSlot() : defAnchor(TextAnchor::ResultPoint), defEnable(true) {}
};

/** @class DisplaySpec @brief 工具显示表。cfg_display 返回这个 */
struct DisplaySpec {
    std::vector<GraphicSlot> graphics; ///< 图形。搜索区、示教区、跟随区、结果区域等
    std::vector<TextSlot> texts;       ///< 文字。位姿、判定等
};

}

#endif
