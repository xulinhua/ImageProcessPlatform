/**
 * @file ToolParaRstMerge.hpp
 * @brief 结果合并运行参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARARSTMERGE_H
#define VISION_TOOLPARARSTMERGE_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/** @brief 结果合并工具落盘键。字符串只写在这里 */
struct RstMergeKey {
    static constexpr const char* minArea = "minArea";
    static constexpr const char* maxCount = "maxCount";
    static constexpr const char* count = "count";
    static constexpr const char* area = "area";
    static constexpr const char* resultRoi = "resultRoi";
    static constexpr const char* verdict = "verdict";
};

/**
 * @class ToolParaRstMerge
 * @brief RstMergeTool 运行参数
 */
class ToolParaRstMerge : public ToolParaBase {
public:
    /** @brief 参数类型名，toolParaAs 按它识别 */
    static const char* typeId() { return "ToolParaRstMerge"; }
    /** @brief 返回 typeId() */
    const char* typeName() const override { return typeId(); }

    /** @brief 示教参数，没有则为空 */
    struct Teach {
    };
    /** @brief 运行参数 */
    struct Run {
        double minArea; ///< 合并后总面积下限，像素；不足判 NG
        int maxCount;   ///< 合并后几何个数上限；超过判 NG，0 不限
        Run()
            : minArea(0.0)
            , maxCount(0)
        {}
    };

    Teach teach; ///< 示教参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.minArea = m.value(RstMergeKey::minArea).toDouble(0.0);
        run.maxCount = m.value(RstMergeKey::maxCount).toInt(0);
        judge = src.judge;
    }
};

}

#endif
