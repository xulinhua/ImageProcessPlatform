/**
 * @file ToolParaQr.hpp
 * @brief 二维码识别的运行参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARAQR_H
#define VISION_TOOLPARAQR_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class QrKey
 * @brief 二维码工具落盘键。字符串只写在这里
 */
struct QrKey {
    static constexpr const char* polarity = "polarity";        ///< 极性：any / dark_on_light / light_on_dark，其他值按 any
    static constexpr const char* timeout = "timeout";          ///< 识别超时（ms），缺省 2000，0 不限，负数按 0
    static constexpr const char* num = "num";                  ///< 最多识别个数，1~99，缺省 1
    static constexpr const char* moduleMin = "moduleMin";      ///< 最小模块尺寸（像素），0 不设，负数按 0
    static constexpr const char* any = "any";                  ///< polarity 取值：不限
    static constexpr const char* darkOnLight = "dark_on_light"; ///< polarity 取值：浅底深码
    static constexpr const char* lightOnDark = "light_on_dark"; ///< polarity 取值：深底浅码
    static constexpr const char* roi = "roi";                  ///< 搜索区 ROI 槽名
    static constexpr const char* code = "code";                ///< 输出值键：全部码文本，用“; ”连接
    static constexpr const char* count = "count";              ///< 输出值键：识别到的个数
    static constexpr const char* resultRoi = "resultRoi";      ///< 输出图形端口：全部码区合并后的区域
};

/**
 * @class ToolParaQr
 * @brief 二维码识别的运行参数
 */
class ToolParaQr : public ToolParaBase {
public:
    /** @brief 参数类型名，toolParaAs 按它识别 */
    static const char* typeId() { return "ToolParaQr"; }
    /** @brief 返回 typeId() */
    const char* typeName() const override { return typeId(); }

    /** @brief 示教参数。二维码不建模型 */
    struct Teach {
    };

    /** @brief 运行参数 */
    struct Run {
        std::string polarity; ///< 极性：any、dark_on_light、light_on_dark
        int timeout;          ///< 超时，毫秒。0 表示不限
        int num;              ///< 最多识别个数，1~99
        int moduleMin;        ///< 最小模块尺寸，像素。0 表示不限
        /** @brief 缺省：任意极性、超时 2000ms、识别 1 个、不限模块尺寸 */
        Run()
            : polarity(QrKey::any)
            , timeout(2000)
            , num(1)
            , moduleMin(0)
        {}
    };

    Teach teach; ///< 示教参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写。非法值回到缺省 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.polarity = m.value(QrKey::polarity).toString();
        if (run.polarity != QrKey::darkOnLight && run.polarity != QrKey::lightOnDark) {
            run.polarity = QrKey::any;
        }
        run.timeout = m.value(QrKey::timeout).toInt(2000);
        if (run.timeout < 0) {
            run.timeout = 0;
        }
        run.num = m.value(QrKey::num).toInt(1);
        if (run.num < 1) {
            run.num = 1;
        }
        if (run.num > 99) {
            run.num = 99;
        }
        run.moduleMin = m.value(QrKey::moduleMin).toInt(0);
        if (run.moduleMin < 0) {
            run.moduleMin = 0;
        }
        judge = src.judge;
    }
};

}

#endif
