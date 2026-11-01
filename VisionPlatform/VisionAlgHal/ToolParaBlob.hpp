/**
 * @file ToolParaBlob.hpp
 * @brief Blob 参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARABLOB_H
#define VISION_TOOLPARABLOB_H

#include "ToolParaBase.h"
#include <string>
#include <vector>

namespace Vision {

/**
 * @class BlobKey
 * @brief 参数键。步骤字段写在 step_序号_ 后面
 * @note 阈值类键既是步骤字段，也是旧方案的顶层键；openR 到 maxCirc 只在旧方案顶层出现
 */
struct BlobKey {
    static constexpr const char* threshMode = "threshMode"; ///< 阈值模式：absolute、relative、dynamic，其他值按 absolute
    static constexpr const char* absMin = "absMin"; ///< 绝对阈值下限，灰度 0~255，缺省 80；找暗目标时取 [0, absMin]
    static constexpr const char* absMax = "absMax"; ///< 绝对阈值上限，灰度 0~255，缺省 255；找暗目标时不用
    static constexpr const char* relOffset = "relOffset"; ///< 相对阈值偏移，灰度，缺省 20：亮目标取 >= 区域均值+偏移，暗目标取 <= 均值-偏移
    static constexpr const char* dynOffset = "dynOffset"; ///< 动态阈值偏移，灰度，缺省 15：比均值背景亮（或暗）超过它的像素为前景
    static constexpr const char* dynKernel = "dynKernel"; ///< 动态阈值背景均值滤波边长，像素，缺省 31；小于 3 按 3，偶数加 1
    static constexpr const char* dark = "dark"; ///< true 找暗目标，false 找亮目标，缺省 false
    static constexpr const char* openR = "openR"; ///< 旧方案开运算圆半径，像素，缺省 1；大于 0.4 才拼开运算步骤
    static constexpr const char* closeR = "closeR"; ///< 旧方案闭运算圆半径，像素，缺省 0；大于 0.4 才拼闭运算步骤
    static constexpr const char* minArea = "minArea"; ///< 旧方案面积筛选下限，像素，缺省 50
    static constexpr const char* maxArea = "maxArea"; ///< 旧方案面积筛选上限，像素；0 不限
    static constexpr const char* minCirc = "minCirc"; ///< 旧方案圆度筛选下限，0~1，缺省 0
    static constexpr const char* maxCirc = "maxCirc"; ///< 旧方案圆度筛选上限，0~1，缺省 1；与 minCirc 都是缺省时不拼圆度步骤
    static constexpr const char* okMinCount = "okMinCount"; ///< 判定：Blob 个数下限；0 不判
    static constexpr const char* okMaxCount = "okMaxCount"; ///< 判定：Blob 个数上限；0 不判
    static constexpr const char* okMinArea = "okMinArea"; ///< 判定：Blob 总面积下限，像素；0 不判
    static constexpr const char* okMaxArea = "okMaxArea"; ///< 判定：Blob 总面积上限，像素；0 不判
    static constexpr const char* absolute = "absolute"; ///< threshMode 取值：固定灰度区间
    static constexpr const char* relative = "relative"; ///< threshMode 取值：以搜索区灰度均值加减偏移为阈值
    static constexpr const char* dynamic = "dynamic"; ///< threshMode 取值：与均值滤波背景逐像素比较

    static constexpr const char* stepCount = "stepCount"; ///< 步骤数；0 或缺失时按旧方案顶层键拼步骤
    static constexpr const char* type = "type"; ///< 步骤类型：threshold、morph、select，其他值按 threshold
    static constexpr const char* morphOp = "morphOp"; ///< 形态学算子：open、close、erode、dilate，其他值按 open
    static constexpr const char* radius = "radius"; ///< 形态学圆形结构元半径，像素，缺省 1；Halcon 实现中 <= 0.4 跳过该步
    static constexpr const char* feature = "feature"; ///< 筛选特征：area、circularity，其他值按 area
    static constexpr const char* selMin = "min"; ///< 筛选下限，缺省 50；单位随特征：面积为像素，圆度为 0~1
    static constexpr const char* selMax = "max"; ///< 筛选上限；0 或负数不限
    static constexpr const char* roiIndex = "roiIndex"; ///< 步骤作用的 ROI 下标（roi 端口区域按 index 排序后的位置）；-1 或越界作用于整个检测区

    static constexpr const char* stepThresh = "threshold"; ///< type 取值：阈值分割。各阈值步骤结果先取并集再做连通
    static constexpr const char* stepMorph  = "morph"; ///< type 取值：形态学，按步骤顺序作用于连通域
    static constexpr const char* stepSelect = "select"; ///< type 取值：特征筛选，按步骤顺序作用于连通域

    static constexpr const char* morphOpen   = "open"; ///< morphOp 取值：开运算
    static constexpr const char* morphClose  = "close"; ///< morphOp 取值：闭运算
    static constexpr const char* morphErode  = "erode"; ///< morphOp 取值：腐蚀
    static constexpr const char* morphDilate = "dilate"; ///< morphOp 取值：膨胀

    static constexpr const char* featArea = "area"; ///< feature 取值：面积，像素
    static constexpr const char* featCirc = "circularity"; ///< feature 取值：圆度，0~1

    static constexpr const char* roi = "roi"; ///< 输入区域端口名。搜索区和步骤 ROI 都从这个端口取，空端口名也算
};

/**
 * @class ToolParaBlob
 * @brief Blob 参数
 * @note Cv 实现只取第一个阈值步骤、最后的开/闭运算半径、第一个面积筛选和最后的圆度筛选，不支持 roiIndex、腐蚀和膨胀
 */
class ToolParaBlob : public ToolParaBase {
public:
    /** @brief 参数类型名，toolParaAs 按它识别 */
    static const char* typeId() { return "ToolParaBlob"; }
    /** @brief 返回 typeId() */
    const char* typeName() const override { return typeId(); }
    /** @brief 示教参数。Blob 不需要示教 */
    struct Teach {
    };

    /** @brief 一步。类型是阈值、形态学或筛选 */
    struct Step {
        std::string type;       ///< 步骤类型：BlobKey::stepThresh、stepMorph、stepSelect
        std::string threshMode; ///< 阈值模式：BlobKey::absolute、relative、dynamic
        int absMin;             ///< 绝对阈值下限，灰度；找暗目标时取 [0, absMin]
        int absMax;             ///< 绝对阈值上限，灰度；找暗目标时不用
        double relOffset;       ///< 相对阈值偏移，灰度，相对区域均值
        double dynOffset;       ///< 动态阈值偏移，灰度，相对均值背景
        int dynKernel;          ///< 动态阈值背景均值滤波边长，像素
        bool dark;              ///< true 找暗目标
        std::string morphOp;    ///< 形态学算子：BlobKey::morphOpen、morphClose、morphErode、morphDilate
        double radius;          ///< 形态学圆形结构元半径，像素
        std::string feature;    ///< 筛选特征：BlobKey::featArea、featCirc
        double minVal;          ///< 筛选下限，单位随 feature
        double maxVal;          ///< 筛选上限；<= 0 不限
        int roiIndex;           ///< 作用的 ROI 下标；-1 作用于整个检测区

        /** @brief 缺省：绝对阈值 80~255 找亮目标 */
        Step()
            : type(BlobKey::stepThresh)
            , threshMode(BlobKey::absolute)
            , absMin(80)
            , absMax(255)
            , relOffset(20.0)
            , dynOffset(15.0)
            , dynKernel(31)
            , dark(false)
            , morphOp(BlobKey::morphOpen)
            , radius(1.0)
            , feature(BlobKey::featArea)
            , minVal(50.0)
            , maxVal(0.0)
            , roiIndex(-1)
        {}
    };

    /** @brief 运行参数：有序步骤和结果判定窗口 */
    struct Run {
        int stepCount;           ///< 步骤数，load 后等于 steps.size()
        std::vector<Step> steps; ///< 按顺序执行的步骤
        int okMinCount;          ///< Blob 个数下限；0 不判
        int okMaxCount;          ///< Blob 个数上限；0 不判
        double okMinArea;        ///< Blob 总面积下限，像素；0 不判
        double okMaxArea;        ///< Blob 总面积上限，像素；0 不判

        /** @brief 缺省一个阈值步骤，不判定 */
        Run()
            : stepCount(1)
            , okMinCount(0)
            , okMaxCount(0)
            , okMinArea(0.0)
            , okMaxArea(0.0)
        {
            steps.push_back(Step());
        }
    };

    Teach teach; ///< 示教参数，为空
    Run run;     ///< 运行参数

    /************************************************** 读取 **************************************************/

    /** @brief 从 ToolParam 读步骤。没有步骤时按旧字段拼 */
    void load(const ToolParam& src)
    {
        const ParamMap& m = src.run;
        run.steps.clear();
        int n = m.value(BlobKey::stepCount).toInt(0);
        if (n < 0) {
            n = 0;
        }
        for (int i = 0; i < n; ++i) {
            run.steps.push_back(readStep(m, i));
        }
        if (run.steps.empty()) {
            readLegacy(m, &run.steps);
        }
        run.stepCount = static_cast<int>(run.steps.size());
        run.okMinCount = m.value(BlobKey::okMinCount).toInt(0);
        run.okMaxCount = m.value(BlobKey::okMaxCount).toInt(0);
        run.okMinArea = m.value(BlobKey::okMinArea).toDouble(0.0);
        run.okMaxArea = m.value(BlobKey::okMaxArea).toDouble(0.0);
        judge = src.judge;
    }

    /************************************************** 归一化 **************************************************/

    /** @brief 步骤类型归一化，未知值按 threshold */
    static std::string normalizeType(const std::string& raw)
    {
        if (raw == BlobKey::stepMorph)  return BlobKey::stepMorph;
        if (raw == BlobKey::stepSelect) return BlobKey::stepSelect;
        return BlobKey::stepThresh;
    }

    /** @brief 阈值模式归一化，未知值按 absolute */
    static std::string normalizeThreshMode(const std::string& raw)
    {
        if (raw == BlobKey::relative) return BlobKey::relative;
        if (raw == BlobKey::dynamic)  return BlobKey::dynamic;
        return BlobKey::absolute;
    }

    /** @brief 形态学算子归一化，未知值按 open */
    static std::string normalizeMorphOp(const std::string& raw)
    {
        if (raw == BlobKey::morphClose)  return BlobKey::morphClose;
        if (raw == BlobKey::morphErode)  return BlobKey::morphErode;
        if (raw == BlobKey::morphDilate) return BlobKey::morphDilate;
        return BlobKey::morphOpen;
    }

    /** @brief 筛选特征归一化，未知值按 area */
    static std::string normalizeFeature(const std::string& raw)
    {
        if (raw == BlobKey::featCirc) return BlobKey::featCirc;
        return BlobKey::featArea;
    }

private:
    /************************************************** 读步骤 **************************************************/

    /** @brief 拼步骤键 step_<index>_<suffix> */
    static std::string stepKey(int index, const char* suffix)
    {
        return std::string("step_") + std::to_string(static_cast<long long>(index)) + "_" + suffix;
    }

    /** @brief 读第 index 步，缺失字段取 Step 缺省值 */
    static Step readStep(const ParamMap& m, int index)
    {
        Step st;
        const std::string p = stepKey(index, "");
        st.type = normalizeType(m.value(p + BlobKey::type).toString());
        st.threshMode = normalizeThreshMode(m.value(p + BlobKey::threshMode).toString());
        st.absMin = m.value(p + BlobKey::absMin).toInt(80);
        st.absMax = m.value(p + BlobKey::absMax).toInt(255);
        st.relOffset = m.value(p + BlobKey::relOffset).toDouble(20.0);
        st.dynOffset = m.value(p + BlobKey::dynOffset).toDouble(15.0);
        st.dynKernel = m.value(p + BlobKey::dynKernel).toInt(31);
        st.dark = m.value(p + BlobKey::dark).toBool(false);
        st.morphOp = normalizeMorphOp(m.value(p + BlobKey::morphOp).toString());
        st.radius = m.value(p + BlobKey::radius).toDouble(1.0);
        st.feature = normalizeFeature(m.value(p + BlobKey::feature).toString());
        st.minVal = m.value(p + BlobKey::selMin).toDouble(50.0);
        st.maxVal = m.value(p + BlobKey::selMax).toDouble(0.0);
        st.roiIndex = m.value(p + BlobKey::roiIndex).toInt(-1);
        return st;
    }

    /** @brief 按旧方案顶层键依次拼阈值、开运算、闭运算、面积筛选、圆度筛选步骤 */
    static void readLegacy(const ParamMap& m, std::vector<Step>* out)
    {
        Step th;
        th.type = BlobKey::stepThresh;
        th.threshMode = normalizeThreshMode(m.value(BlobKey::threshMode).toString());
        th.absMin = m.value(BlobKey::absMin).toInt(80);
        th.absMax = m.value(BlobKey::absMax).toInt(255);
        th.relOffset = m.value(BlobKey::relOffset).toDouble(20.0);
        th.dynOffset = m.value(BlobKey::dynOffset).toDouble(15.0);
        th.dynKernel = m.value(BlobKey::dynKernel).toInt(31);
        th.dark = m.value(BlobKey::dark).toBool(false);
        out->push_back(th);

        const double openR = m.value(BlobKey::openR).toDouble(1.0);
        if (openR > 0.4) {
            Step st;
            st.type = BlobKey::stepMorph;
            st.morphOp = BlobKey::morphOpen;
            st.radius = openR;
            out->push_back(st);
        }
        const double closeR = m.value(BlobKey::closeR).toDouble(0.0);
        if (closeR > 0.4) {
            Step st;
            st.type = BlobKey::stepMorph;
            st.morphOp = BlobKey::morphClose;
            st.radius = closeR;
            out->push_back(st);
        }

        Step sel;
        sel.type = BlobKey::stepSelect;
        sel.feature = BlobKey::featArea;
        sel.minVal = m.value(BlobKey::minArea).toDouble(50.0);
        sel.maxVal = m.value(BlobKey::maxArea).toDouble(0.0);
        out->push_back(sel);

        const double minCirc = m.value(BlobKey::minCirc).toDouble(0.0);
        const double maxCirc = m.value(BlobKey::maxCirc).toDouble(1.0);
        if (minCirc > 0.0 || maxCirc < 0.999) {
            Step circ;
            circ.type = BlobKey::stepSelect;
            circ.feature = BlobKey::featCirc;
            circ.minVal = minCirc;
            circ.maxVal = maxCirc;
            out->push_back(circ);
        }
    }
};

}

#endif
