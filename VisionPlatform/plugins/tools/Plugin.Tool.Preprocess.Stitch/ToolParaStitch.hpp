/**
 * @file ToolParaStitch.hpp
 * @brief 图像拼接的运行参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARASTITCH_H
#define VISION_TOOLPARASTITCH_H

#include "ToolParaBase.h"

namespace Vision {

/**
 * @class StitchKey
 * @brief 图像拼接落盘键。字符串只写在这里
 */
struct StitchKey {
    static constexpr const char* columns = "columns"; ///< 参数与输出值键：每行张数，缺省 2；小于 1 或大于张数时排成一行
    static constexpr const char* count = "count";     ///< 输出值键：参与拼接的图像张数，至少 2
};

/**
 * @class ToolParaStitch
 * @brief 图像拼接的运行参数
 */
class ToolParaStitch : public ToolParaBase {
public:
    /** @brief 参数类型名，toolParaAs 按它识别 */
    static const char* typeId() { return "ToolParaStitch"; }
    /** @brief 返回 typeId() */
    const char* typeName() const override { return typeId(); }

    /** @brief 示教参数。拼接不建模型 */
    struct Teach {
    };

    /** @brief 运行参数 */
    struct Run {
        int columns; ///< 每行张数。小于 1 或大于张数时全部排成一行
        /** @brief 缺省每行 2 张 */
        Run() : columns(2) {}
    };

    Teach teach; ///< 示教参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        run.columns = src.run.value(StitchKey::columns).toInt(2);
        judge = src.judge;
    }
};

}

#endif
