/**
 * @file ToolParaBarcode.hpp
 * @brief 一维码识别的运行参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARABARCODE_H
#define VISION_TOOLPARABARCODE_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class BarcodeKey
 * @brief 一维码工具落盘键。字符串只写在这里
 */
struct BarcodeKey {
    static constexpr const char* codeType = "codeType"; ///< 码制，原样传给 Halcon 读码（如 EAN-13、Code 128）；空按 auto
    static constexpr const char* polarity = "polarity"; ///< 极性：any、dark_on_light、light_on_dark，其他值按 any
    static constexpr const char* timeout = "timeout"; ///< 读码超时，毫秒，缺省 2000；负数按 0
    static constexpr const char* num = "num"; ///< 最多识别个数，1~99，缺省 1，越界截断；读够即停
    static constexpr const char* autoType = "auto"; ///< codeType 取值：自动识别码制
    static constexpr const char* any = "any"; ///< polarity 取值：两种极性都找
    static constexpr const char* darkOnLight = "dark_on_light"; ///< polarity 取值：浅底深码
    static constexpr const char* lightOnDark = "light_on_dark"; ///< polarity 取值：深底浅码
    static constexpr const char* roi = "roi"; ///< ROI 槽名：搜索区
    static constexpr const char* code = "code"; ///< 输出值键：识别出的文本，多个用 "; " 连接
    static constexpr const char* count = "count"; ///< 输出值键：识别个数
    static constexpr const char* resultRoi = "resultRoi"; ///< 输出图形端口：所有码区的并集
};

/**
 * @class ToolParaBarcode
 * @brief 一维码识别的运行参数
 */
class ToolParaBarcode : public ToolParaBase {
public:
    /** @brief 参数类型名，toolParaAs 按它识别 */
    static const char* typeId() { return "ToolParaBarcode"; }
    /** @brief 返回 typeId() */
    const char* typeName() const override { return typeId(); }

    /** @brief 示教参数。一维码不建模型 */
    struct Teach {
    };

    /** @brief 运行参数 */
    struct Run {
        std::string codeType; ///< 码制。auto 为自动
        std::string polarity; ///< 极性：any、dark_on_light、light_on_dark
        int timeout;          ///< 超时，毫秒。0 表示不限
        int num;              ///< 最多识别个数
        /** @brief 缺省：自动码制，任意极性，超时 2000 ms，识别 1 个 */
        Run()
            : codeType(BarcodeKey::autoType)
            , polarity(BarcodeKey::any)
            , timeout(2000)
            , num(1)
        {}
    };

    Teach teach; ///< 示教参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写。非法值回到缺省 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.codeType = m.value(BarcodeKey::codeType).toString();
        if (run.codeType.empty()) {
            run.codeType = BarcodeKey::autoType;
        }
        run.polarity = m.value(BarcodeKey::polarity).toString();
        if (run.polarity != BarcodeKey::darkOnLight && run.polarity != BarcodeKey::lightOnDark) {
            run.polarity = BarcodeKey::any;
        }
        run.timeout = m.value(BarcodeKey::timeout).toInt(2000);
        if (run.timeout < 0) {
            run.timeout = 0;
        }
        run.num = m.value(BarcodeKey::num).toInt(1);
        if (run.num < 1) {
            run.num = 1;
        }
        if (run.num > 99) {
            run.num = 99;
        }
        judge = src.judge;
    }
};

}

#endif
