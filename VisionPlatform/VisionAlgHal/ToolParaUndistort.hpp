/**
 * @file ToolParaUndistort.hpp
 * @brief 畸变校正运行参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARAUNDISTORT_H
#define VISION_TOOLPARAUNDISTORT_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

/** @brief 畸变校正落盘键。字符串只写在这里 */
struct UndistortKey {
    static constexpr const char* halconCam = "halconCam"; ///< Halcon 相机参数原文，首项为模型名
};

/**
 * @class ToolParaUndistort
 * @brief UndistortTool 运行参数
 */
class ToolParaUndistort : public ToolParaBase {
public:
    /** @brief 参数类型名，toolParaAs 按它识别 */
    static const char* typeId() { return "ToolParaUndistort"; }
    /** @brief 返回 typeId() */
    const char* typeName() const override { return typeId(); }

    /** @brief 示教参数，没有则为空 */
    struct Teach {
    };
    /** @brief 运行参数 */
    struct Run {
        std::string halconCam; ///< Halcon 相机参数文本，空格分隔
        Run() : halconCam() {}
    };

    Teach teach; ///< 示教参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.halconCam = m.value(UndistortKey::halconCam).toString();
        judge = src.judge;
    }
};

}

#endif
