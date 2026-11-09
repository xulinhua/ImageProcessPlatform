/**
 * @file VisionLicense.h
 * @brief 给 VisionPlatform 的许可接口。静态库，只有复查，没有申请和签发
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_LICENSE_H
#define VISION_LICENSE_H

#ifdef _MSC_VER
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#endif

#include <string>

namespace Vision {

/** @brief 许可校验结果 */
enum LicenseReason {
    LicenseOk = 0,          ///< 许可有效
    LicenseMissing,         ///< 还没有许可证文件
    LicenseBadSign,         ///< 注册码签名不对，或前缀不是注册码
    LicenseMachine,         ///< 硬件摘要对不上，或解不开本机文件
    LicenseProduct,         ///< 不是本产品
    LicenseExpired,         ///< 到期日已过
    LicenseClock,           ///< 系统时间早于见过的最晚时间
    LicenseRecordsCleared,  ///< 三处时间记录都没有，许可证还在
    LicenseDamaged,         ///< 文件头或长度不对，或文件内签名、正文校验不过
    LicenseSaveFailed,      ///< 许可证目录写不进去
    LicenseNotAttached,     ///< 硬件狗这一期没有接入
    LicenseSerial           ///< 签发序号不比已落地的更新
};

/** @brief 已落地许可证的摘要。文件解不开时编号为空、序号为 0 */
struct LicenseView {
    std::string licenseId;  ///< 签发编号。还没有可读的许可证时为空
    std::string until;      ///< 到期日 YYYY-MM-DD。永久注册或还没有许可证时为空
    bool perpetual;         ///< 永久注册。还没有许可证时为 false
    int serial;             ///< 签发序号。还没有许可证时为 0
    /** @brief 默认没有许可证 */
    LicenseView() : perpetual(false), serial(0) {}
};

/**
 * @class License
 * @brief 本机许可证复查。注册界面由 Vision.LicenseRequest.exe 负责
 */
class License {
public:
    /** @brief 尚未读过许可证文件 */
    License();
    /** @brief 释放内部实现 */
    ~License();

    /** @brief 校验许可证文件并刷新三处时间记录 */
    LicenseReason open();
    /** @brief 与 open 相同：重新校验文件并刷新时间记录 */
    LicenseReason poll();
    /** @brief 给界面的一句原因 */
    std::string reasonText(LicenseReason reason) const;
    /** @brief 最近一次 open 或 poll 解开的许可证摘要。失败时也可能带编号和到期日 */
    LicenseView view() const;

private:
    License(const License&);
    License& operator=(const License&);
    class Impl;
    Impl* m;
};

}

#endif
