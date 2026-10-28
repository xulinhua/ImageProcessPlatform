/**
 * @file LicenseGate.h
 * @brief 主程序的许可入口。启动时挡住，运行中倒计时
 */

#ifndef VISION_APP_LICENSE_GATE_H
#define VISION_APP_LICENSE_GATE_H

class QObject;
class QApplication;

namespace Vision {

class MainWindow;
class SoftLicense;
enum LicenseReason;

/** @brief 启动失败时给出申请码，并可粘贴注册码。通过返回 true */
bool promptLicense(SoftLicense* license, LicenseReason reason);

/** @brief 主窗起来之后每 30 秒复查。失效则倒计时，到点停工站，不退出进程 */
void watchLicense(QApplication* app, MainWindow* window);

}

#endif
