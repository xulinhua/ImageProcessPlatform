/**
 * @file UiIcons.h
 * @brief UiIcons头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_UIICONS_H
#define VISION_UIICONS_H
#include "VisionUiExport.h"

#include <QIcon>

namespace Vision {

/** @brief 主界面/工具栏图标键（对应 icons/ui/*.svg） */
enum UiIconKey {
    UiIconNew = 0, ///< 新建方案
    UiIconOpen, ///< 打开方案
    UiIconSave, ///< 保存方案
    UiIconStations, ///< 工站管理
    UiIconDevices, ///< 设备管理
    UiIconComm, ///< 通信管理
    UiIconRun, ///< 跑本流程
    UiIconStart, ///< 开始运行
    UiIconStop, ///< 停止运行
    UiIconReset, ///< 产量清零
    UiIconDesign, ///< 设计页
    UiIconRuntime, ///< 运行页
    UiIconRecheck, ///< 复检页
    UiIconNewStation, ///< 新建工站
    UiIconNewJob, ///< 新建流程
    UiIconCount ///< 图标个数
};

/**
 * @brief 取主界面图标（优先 icons/ui/*.svg）
 * @param key UiIconKey
 * @param px 渲染尺寸，默认 20
 */
VISION_UI_API QIcon uiIcon(int key, int px = 20);

/** @brief 清空主界面图标缓存 */
VISION_UI_API void clearUiIconCache();

}

#endif