/**
 * @file ToolEditors.h
 * @brief 工具参数页工厂声明
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLEDITORS_H
#define VISION_TOOLEDITORS_H
#include "VisionToolUiExport.h"

#include "ToolEditorBase.h"
#include <string>

namespace Vision {

/** @brief 按工具壳参数说明生成参数页（示教/运行分栏） */
VISION_TOOLUI_API ToolEditorBase* createGenericEditor(QWidget* parent);

/** @brief 滤波和斑点的多步骤参数页 */
VISION_TOOLUI_API ToolEditorBase* createStepEditor(const std::string& toolName, QWidget* parent);
/** @brief preprocess.filter 步骤页（登记表工厂签名） */
VISION_TOOLUI_API ToolEditorBase* createFilterStepEditor(QWidget* parent);
/** @brief detect.blob 步骤页（登记表工厂签名） */
VISION_TOOLUI_API ToolEditorBase* createBlobStepEditor(QWidget* parent);

/** @brief 三维采集：文件 / 文件夹点云来源 */
VISION_TOOLUI_API ToolEditorBase* createAcquire3DEditor(QWidget* parent);

/** @brief 相机设备采集 */
VISION_TOOLUI_API ToolEditorBase* createCamDeviceEditor(QWidget* parent);
/** @brief 文件源采集 */
VISION_TOOLUI_API ToolEditorBase* createCameraFileEditor(QWidget* parent);
/** @brief 图像增强管线 */
VISION_TOOLUI_API ToolEditorBase* createEnhanceEditor(QWidget* parent);
/** @brief 图像平滑管线 */
VISION_TOOLUI_API ToolEditorBase* createSmoothEditor(QWidget* parent);

/** @brief 模板定位 */
VISION_TOOLUI_API ToolEditorBase* createTmplEditor(QWidget* parent);
/** @brief 找线 */
VISION_TOOLUI_API ToolEditorBase* createFindLineEditor(QWidget* parent);
/** @brief 找圆 */
VISION_TOOLUI_API ToolEditorBase* createFindCircleEditor(QWidget* parent);
/** @brief 卡尺 */
VISION_TOOLUI_API ToolEditorBase* createCaliperEditor(QWidget* parent);
/** @brief 斑点（专用页；与 Step 页二选一登记） */
VISION_TOOLUI_API ToolEditorBase* createBlobEditor(QWidget* parent);
/** @brief 纹理检测 */
VISION_TOOLUI_API ToolEditorBase* createTextureEditor(QWidget* parent);

/** @brief 结果合并 */
VISION_TOOLUI_API ToolEditorBase* createMergeEditor(QWidget* parent);
/** @brief 逻辑判断 */
VISION_TOOLUI_API ToolEditorBase* createJudgeEditor(QWidget* parent);
/** @brief PLC 通信 */
VISION_TOOLUI_API ToolEditorBase* createPlcEditor(QWidget* parent);

}

#endif
