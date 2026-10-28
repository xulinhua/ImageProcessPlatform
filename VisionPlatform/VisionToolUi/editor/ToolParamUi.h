/**
 * @file ToolParamUi.h
 * @brief ToolParamUi头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARAMUI_H
#define VISION_TOOLPARAMUI_H
#include "VisionToolUiExport.h"

#include <QString>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QSpinBox;
class QVBoxLayout;
class QWidget;

namespace Vision {

/** @brief 参数页根样式（objectName=toolDebugParam） */
VISION_TOOLUI_API void initToolParamEditor(QWidget* editor);
/** @brief 为页内 Spin/Combo/Check/Label 补 objectName 与高度 */
VISION_TOOLUI_API void applyParamControlStyle(QWidget* root);
/** @brief 锁定参数页高度为内容高度（bind/show 后调用） */
VISION_TOOLUI_API void finalizeParamEditorLayout(QWidget* editor);

/** @brief 参数标签 */
VISION_TOOLUI_API QLabel* mkParamLabel(QWidget* parent, const QString& text);
/** @brief 整型步进框 */
VISION_TOOLUI_API QSpinBox* mkParamSpin(QWidget* parent, int lo, int hi);
/** @brief 浮点步进框 */
VISION_TOOLUI_API QDoubleSpinBox* mkParamDSpin(QWidget* parent, double lo, double hi, int dec, double step = 1.0);
/** @brief 下拉框 */
VISION_TOOLUI_API QComboBox* mkParamCombo(QWidget* parent);
/** @brief 勾选框 */
VISION_TOOLUI_API QCheckBox* mkParamCheck(QWidget* parent, const QString& text);

/**
 * @brief 单列参数：标签在上、控件在下
 * @param parent 宿主
 * @param label 标签文字
 * @param control 已创建的控件
 */
VISION_TOOLUI_API QWidget* mkParamCell(QWidget* parent, const QString& label, QWidget* control);

/**
 * @brief 一行两列参数（各占半宽）
 */
VISION_TOOLUI_API QWidget* mkParamRow2(QWidget* parent,
                     const QString& label1, QWidget* control1,
                     const QString& label2, QWidget* control2,
                     int hSpacing = 12);

/** @brief 自上而下排列的参数栈 */
VISION_TOOLUI_API QVBoxLayout* mkParamStack(QWidget* host, int spacing = 10);
/** @brief 向参数栈追加一行（顶对齐） */
VISION_TOOLUI_API void addParamStackItem(QVBoxLayout* lay, QWidget* block);

}

#endif
