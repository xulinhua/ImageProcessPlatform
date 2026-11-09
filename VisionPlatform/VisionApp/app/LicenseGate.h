/**
 * @file LicenseGate.h
 * @brief 主程序的许可入口。启动时挡住，运行中倒计时
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_APP_LICENSE_GATE_H
#define VISION_APP_LICENSE_GATE_H

class QObject;
class QApplication;

namespace Vision {

class MainWindow;
class License;
enum LicenseReason;

/** @brief 启动失败时打开 Vision.LicenseRequest。注册完成并通过复查后返回 true */
bool promptLicense(License* license, LicenseReason reason);

/** @brief 主窗起来之后每 30 秒复查。失效则倒计时，到点停工站，不退出进程 */
void watchLicense(QApplication* app, MainWindow* window);

}

#endif
