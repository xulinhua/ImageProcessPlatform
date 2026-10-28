/**
 * @file AppMode.h
 * @brief AppMode头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_APP_MODE_H
#define VISION_APP_MODE_H

namespace Vision {

/** @brief 应用模式：设计态 / 运行态 / 复检态 */
enum AppMode {
    ModeDesign = 0, ///< 设计态
    ModeRuntime,    ///< 运行态
    ModeRecheck     ///< 复检态（离线批跑）
};

}

#endif
