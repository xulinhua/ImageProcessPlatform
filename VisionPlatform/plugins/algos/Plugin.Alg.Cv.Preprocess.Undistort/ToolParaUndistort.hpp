/**
 * @file ToolParaUndistort.hpp
 * @brief ToolParaUndistort头文件
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARAUNDISTORT_H
#define VISION_TOOLPARAUNDISTORT_H

#include "ToolParaBase.h"
#include <string>

namespace Vision {

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
    /** @brief 学习参数，没有则为空 */
    struct Learn {
    };
    /** @brief 运行参数 */
    struct Run {
    };

    Learn learn; ///< 学习参数，没有则为空
    Run run;     ///< 运行参数

    /** @brief 从 ToolParam 填写 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        judge = src.judge;
    }
};

}

#endif
