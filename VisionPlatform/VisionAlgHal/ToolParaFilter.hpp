/**
 * @file ToolParaFilter.hpp
 * @brief FilterTool 运行参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARAFILTER_H
#define VISION_TOOLPARAFILTER_H

#include "ToolParaBase.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class FilterKey
 * @brief 滤波工具落盘键。字符串只写在这里
 * @note 步骤字段写在 step_序号_ 后面；旧方案写在 pl_序号_ 后面
 */
struct FilterKey {
    static constexpr const char* stepCount = "stepCount"; ///< 步骤数；<= 0 或缺失时改读旧方案 pl_n
    static constexpr const char* op = "op"; ///< 步骤算子名，取值见 op* 常量；也接受旧方案数字 0~8，未知值按 EquHisto
    static constexpr const char* w = "w"; ///< 掩膜宽，像素，缺省 7；Median 作圆形半径，Gauss 取 3/5/7/9/11 中不超过它的最大值，形态学与 h 取大者补成奇数方形
    static constexpr const char* h = "h"; ///< 掩膜高，像素，缺省 7；Emphasize、Illuminate、Mean 和形态学用
    static constexpr const char* c0 = "c0"; ///< 系数 1，缺省 1：Emphasize/Illuminate 的增强因子，Scale 的乘数，Bilateral 的灰度域标准差
    static constexpr const char* c1 = "c1"; ///< 系数 2，缺省 3：Scale 的加数，Bilateral 的空间域标准差
    static constexpr const char* scaleMax = "f"; ///< Scale 专用：true 时把灰度拉伸到满量程，忽略 c0、c1
    static constexpr const char* shape = "shape"; ///< 灰度形态学结构元形状：rectangle、octagon、rhombus，其他按 rectangle
    static constexpr const char* roiIndex = "roiIndex"; ///< 步骤作用的 ROI 下标（roi 端口区域按 index 排序后的位置）；-1 作用于整图，越界或与图像无交集时跳过该步

    static constexpr const char* opEquHisto   = "EquHisto"; ///< op 取值：直方图均衡（旧值 0）
    static constexpr const char* opEmphasize  = "Emphasize"; ///< op 取值：锐化增强，用 w、h、c0（旧值 1）
    static constexpr const char* opIlluminate = "Illuminate"; ///< op 取值：光照均衡，用 w、h、c0（旧值 2）
    static constexpr const char* opScale      = "Scale"; ///< op 取值：灰度线性变换 g*c0+c1，或 f 为真时拉伸到满量程（旧值 3）
    static constexpr const char* opInvert     = "Invert"; ///< op 取值：反相（旧值 4）
    static constexpr const char* opMean       = "Mean"; ///< op 取值：均值滤波，w×h（旧值 5）
    static constexpr const char* opMedian     = "Median"; ///< op 取值：中值滤波，圆形半径 w，边界镜像（旧值 6）
    static constexpr const char* opGauss      = "Gauss"; ///< op 取值：高斯滤波，尺寸由 w 决定（旧值 7）
    static constexpr const char* opBilateral  = "Bilateral"; ///< op 取值：双边滤波，c0、c1 都须大于 0（旧值 8）
    static constexpr const char* opGrayOpen   = "GrayOpen"; ///< op 取值：灰度开运算，用 w、h、shape
    static constexpr const char* opGrayClose  = "GrayClose"; ///< op 取值：灰度闭运算，用 w、h、shape
    static constexpr const char* opGrayErode  = "GrayErode"; ///< op 取值：灰度腐蚀，用 w、h、shape
    static constexpr const char* opGrayDilate = "GrayDilate"; ///< op 取值：灰度膨胀，用 w、h、shape
    static constexpr const char* opTopHat     = "TopHat"; ///< op 取值：顶帽，原图减灰度开运算
    static constexpr const char* opBottomHat  = "BottomHat"; ///< op 取值：底帽，灰度闭运算减原图

    static constexpr const char* shapeRect    = "rectangle"; ///< shape 取值：矩形
    static constexpr const char* shapeOctagon = "octagon"; ///< shape 取值：八边形
    static constexpr const char* shapeRhombus = "rhombus"; ///< shape 取值：菱形

    static constexpr const char* legacyStepCount = "pl_n"; ///< 旧方案步骤数，只读；旧步骤只有 op、w、h、c0、c1、f
    static constexpr const char* roi = "roi"; ///< 输入区域端口名，步骤 ROI 从这里取；rois 里端口名为空的也算
    static constexpr const char* outWidth = "filter.width"; ///< 输出值键：结果图宽，像素
    static constexpr const char* outHeight = "filter.height"; ///< 输出值键：结果图高，像素
    static constexpr const char* outCostMs = "filter.costMs"; ///< 输出值键：耗时 ms；HalFilter 当前不写
    static constexpr const char* previewFilter = "preview.filter"; ///< 调试动作：滤波预览
};

/**
 * @class ToolParaFilter
 * @brief FilterTool 运行参数
 */
class ToolParaFilter : public ToolParaBase {
public:
    /** @brief 参数类型名，toolParaAs 按它识别 */
    static const char* typeId() { return "ToolParaFilter"; }
    /** @brief 返回 typeId() */
    const char* typeName() const override { return typeId(); }
    /** @brief 示教参数。滤波不需要示教 */
    struct Teach {
    };

    /** @brief 一步预处理的运行参数 */
    struct Step {
        std::string op;    ///< 算子名，见 FilterKey::op*
        int maskW;         ///< 掩膜宽，像素
        int maskH;         ///< 掩膜高，像素
        double coeff;      ///< 系数 1，含义随算子，见 FilterKey::c0
        double coeff2;     ///< 系数 2，含义随算子，见 FilterKey::c1
        bool scaleMax;     ///< Scale 时拉伸到满量程
        std::string shape; ///< 灰度形态学结构元形状
        int roiIndex;      ///< 作用的 ROI 下标；-1 作用于整图

        /** @brief 缺省：直方图均衡，掩膜 7×7 */
        Step()
            : op(FilterKey::opEquHisto)
            , maskW(7)
            , maskH(7)
            , coeff(1.0)
            , coeff2(3.0)
            , scaleMax(false)
            , shape(FilterKey::shapeRect)
            , roiIndex(-1)
        {
        }
    };

    /** @brief 运行参数：步数加有序步骤列表 */
    struct Run {
        int stepCount;           ///< 步骤数，load 后等于 steps.size()
        std::vector<Step> steps; ///< 按顺序执行的步骤，至少一步

        /** @brief 缺省一步直方图均衡 */
        Run() : stepCount(1)
        {
            steps.push_back(Step());
        }
    };

    Teach teach; ///< 示教参数，为空
    Run run;     ///< 运行参数

    /************************************************** 读取 **************************************************/

    /** @brief 从 ToolParam 填写。新 step 键优先，旧 pl_* 只读 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.steps.clear();
        int n = m.value(FilterKey::stepCount).toInt(0);
        if (n > 0) {
            for (int i = 0; i < n; ++i) {
                run.steps.push_back(readStep(m, i));
            }
        } else if (m.contains(FilterKey::legacyStepCount)) {
            n = m.value(FilterKey::legacyStepCount).toInt(0);
            for (int i = 0; i < n; ++i) {
                run.steps.push_back(readLegacyStep(m, i));
            }
        }
        if (run.steps.empty()) {
            run.steps.push_back(Step());
        }
        run.stepCount = static_cast<int>(run.steps.size());
        judge = src.judge;
    }

    /************************************************** 归一化 **************************************************/

    /** @brief 兼容旧方案的数字算子名 */
    static std::string normalizeOp(const std::string& raw)
    {
        if (raw == "0" || raw == FilterKey::opEquHisto)   return FilterKey::opEquHisto;
        if (raw == "1" || raw == FilterKey::opEmphasize)  return FilterKey::opEmphasize;
        if (raw == "2" || raw == FilterKey::opIlluminate) return FilterKey::opIlluminate;
        if (raw == "3" || raw == FilterKey::opScale)      return FilterKey::opScale;
        if (raw == "4" || raw == FilterKey::opInvert)     return FilterKey::opInvert;
        if (raw == "5" || raw == FilterKey::opMean)       return FilterKey::opMean;
        if (raw == "6" || raw == FilterKey::opMedian)     return FilterKey::opMedian;
        if (raw == "7" || raw == FilterKey::opGauss)      return FilterKey::opGauss;
        if (raw == "8" || raw == FilterKey::opBilateral)  return FilterKey::opBilateral;
        if (raw == FilterKey::opGrayOpen)   return FilterKey::opGrayOpen;
        if (raw == FilterKey::opGrayClose)  return FilterKey::opGrayClose;
        if (raw == FilterKey::opGrayErode)  return FilterKey::opGrayErode;
        if (raw == FilterKey::opGrayDilate) return FilterKey::opGrayDilate;
        if (raw == FilterKey::opTopHat)     return FilterKey::opTopHat;
        if (raw == FilterKey::opBottomHat)  return FilterKey::opBottomHat;
        return FilterKey::opEquHisto;
    }

    /** @brief 形态学掩膜形状归一化 */
    static std::string normalizeShape(const std::string& raw)
    {
        if (raw == FilterKey::shapeOctagon) return FilterKey::shapeOctagon;
        if (raw == FilterKey::shapeRhombus) return FilterKey::shapeRhombus;
        return FilterKey::shapeRect;
    }

private:
    /************************************************** 读步骤 **************************************************/

    /** @brief 拼步骤键 step_<index>_<suffix> */
    static std::string stepKey(int index, const char* suffix)
    {
        return std::string("step_") + std::to_string(static_cast<long long>(index)) + "_" + suffix;
    }

    /** @brief 拼旧方案步骤键 pl_<index>_<suffix> */
    static std::string legacyStepKey(int index, const char* suffix)
    {
        return std::string("pl_") + std::to_string(static_cast<long long>(index)) + "_" + suffix;
    }

    /** @brief 读第 index 步，缺失字段取 Step 缺省值 */
    static Step readStep(const ParamMap& m, int index)
    {
        Step st;
        const std::string p = stepKey(index, "");
        st.op = normalizeOp(m.value(p + FilterKey::op).toString());
        st.maskW = m.value(p + FilterKey::w).toInt(7);
        st.maskH = m.value(p + FilterKey::h).toInt(7);
        st.coeff = m.value(p + FilterKey::c0).toDouble(1.0);
        st.coeff2 = m.value(p + FilterKey::c1).toDouble(3.0);
        st.scaleMax = m.value(p + FilterKey::scaleMax).toBool(false);
        st.shape = normalizeShape(m.value(p + FilterKey::shape).toString());
        st.roiIndex = m.value(p + FilterKey::roiIndex).toInt(-1);
        return st;
    }

    /** @brief 读旧方案第 index 步；没有 shape、roiIndex，取缺省 */
    static Step readLegacyStep(const ParamMap& m, int index)
    {
        Step st;
        const std::string p = legacyStepKey(index, "");
        st.op = normalizeOp(m.value(p + FilterKey::op).toString());
        st.maskW = m.value(p + FilterKey::w).toInt(7);
        st.maskH = m.value(p + FilterKey::h).toInt(7);
        st.coeff = m.value(p + FilterKey::c0).toDouble(1.0);
        st.coeff2 = m.value(p + FilterKey::c1).toDouble(3.0);
        st.scaleMax = m.value(p + FilterKey::scaleMax).toBool(false);
        return st;
    }
};

}

#endif
