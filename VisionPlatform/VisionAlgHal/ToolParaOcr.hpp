/**
 * @file ToolParaOcr.hpp
 * @brief 字符识别的运行参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARAOCR_H
#define VISION_TOOLPARAOCR_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/**
 * @class OcrKey
 * @brief 字符识别落盘键。字符串只写在这里
 */
struct OcrKey {
    static constexpr const char* font = "font"; ///< 分类器文件名，在 Halcon ocr 目录下查找；空按 defaultFont，加载失败报 HS_ERR_OCR_NO_MODEL
    static constexpr const char* polarity = "polarity"; ///< 极性：dark_on_light、light_on_dark、both，其他值按 dark_on_light
    static constexpr const char* minCharHeight = "minCharHeight"; ///< 最小字高，像素；0 自动
    static constexpr const char* maxCharHeight = "maxCharHeight"; ///< 最大字高，像素；0 自动
    static constexpr const char* expected = "expected"; ///< 期望文本；非空时与识别结果整串比较，不等判 NG
    static constexpr const char* defaultFont = "Universal_0-9A-Z+_NoRej.occ"; ///< 缺省分类器：数字、大写字母和特殊字符，无拒识类
    static constexpr const char* darkOnLight = "dark_on_light"; ///< polarity 取值：浅底深字
    static constexpr const char* lightOnDark = "light_on_dark"; ///< polarity 取值：深底浅字
    static constexpr const char* both = "both"; ///< polarity 取值：两种都找
    static constexpr const char* roi = "roi"; ///< ROI 槽名：搜索区
    static constexpr const char* text = "text"; ///< 输出值键：所有字符按读出顺序拼成的文本
    static constexpr const char* count = "count"; ///< 输出值键：字符个数
    static constexpr const char* resultRoi = "resultRoi"; ///< 输出图形端口：所有字符区的并集
};

/**
 * @class ToolParaOcr
 * @brief 字符识别的运行参数
 */
class ToolParaOcr : public ToolParaBase {
public:
    /** @brief 参数类型名，toolParaAs 按它识别 */
    static const char* typeId() { return "ToolParaOcr"; }
    /** @brief 返回 typeId() */
    const char* typeName() const override { return typeId(); }

    /** @brief 示教参数。用 Halcon 自带字体，不训练 */
    struct Teach {
    };

    /** @brief 运行参数 */
    struct Run {
        std::string font;     ///< 分类器文件名，Halcon ocr 目录下查找
        std::string polarity; ///< 极性：dark_on_light、light_on_dark、both
        int minCharHeight;    ///< 最小字高，像素。0 表示自动
        int maxCharHeight;    ///< 最大字高，像素。0 表示自动
        std::string expected; ///< 期望文本。空则不比对
        /** @brief 缺省：通用分类器，浅底深字，字高自动 */
        Run()
            : font(OcrKey::defaultFont)
            , polarity(OcrKey::darkOnLight)
            , minCharHeight(0)
            , maxCharHeight(0)
        {}
    };

    Teach teach; ///< 示教参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写。非法值回到缺省 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.font = m.value(OcrKey::font).toString();
        if (run.font.empty()) {
            run.font = OcrKey::defaultFont;
        }
        run.polarity = m.value(OcrKey::polarity).toString();
        if (run.polarity != OcrKey::lightOnDark && run.polarity != OcrKey::both) {
            run.polarity = OcrKey::darkOnLight;
        }
        run.minCharHeight = m.value(OcrKey::minCharHeight).toInt(0);
        run.maxCharHeight = m.value(OcrKey::maxCharHeight).toInt(0);
        run.expected = m.value(OcrKey::expected).toString();
        judge = src.judge;
    }
};

}

#endif
