/**
 * @file ToolParaTmpl.hpp
 * @brief 模板定位的示教参数和运行参数
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#ifndef VISION_TOOLPARATMPL_H
#define VISION_TOOLPARATMPL_H

#include "ToolParaBase.h"
#include <HsCommData/HsConst.h>
#include <HsCommData/HsError.h>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace Vision {

/**
 * @class TmplKey
 * @brief 模板工具落盘键
 */
struct TmplKey {
    static constexpr const char* method = "method"; ///< 建模方式：ncc 或 shape；空按 ncc，其他值记 HS_ERR_TMPL_METHOD
    static constexpr const char* angleRange = "angleRange"; ///< 角度范围，度，缺省 15；建模和定位都搜 ±angleRange
    static constexpr const char* angleStep = "angleStep"; ///< 角度步长，度：auto 或一个数，0 按 auto
    static constexpr const char* numLevels = "numLevels"; ///< 建模金字塔层数：auto 或整数，0 按 auto
    static constexpr const char* metric = "metric"; ///< 极性：use_polarity、ignore_global_polarity、ignore_local_polarity、ignore_color_polarity；NCC 只认前两种，其余按 use_polarity
    static constexpr const char* optimization = "optimization"; ///< 形状模板点压缩：auto、none、point_reduction_low/medium/high，缺省 auto；只用于 shape
    static constexpr const char* minContrast = "minContrast"; ///< 形状模板最小对比度，灰度：auto 或一个数，0 按 auto；只用于 shape
    static constexpr const char* rebuildModel = "rebuildModel"; ///< 界面上的“下次运行强制重建模型”，Job 跑完一次后清回 false；HalTmpl 当前不读
    static constexpr const char* contrast = "contrast"; ///< 形状模板对比度：auto、auto_contrast、auto_contrast_hyst 或一个数，缺省 30，0 按 auto；用于 shape 建模和轮廓预览
    static constexpr const char* modelFile = "modelFile"; ///< 模型文件名，空按方法取 model.ncm 或 model.shm；HalTmpl 当前固定读写 hal 子目录下的缺省文件名，不读此值
    static constexpr const char* minScore = "minScore"; ///< 最低匹配分数，0~1，缺省 0.5
    static constexpr const char* numMatches = "numMatches"; ///< 最多匹配个数，缺省 1；小于 1 按 1
    static constexpr const char* maxOverlap = "maxOverlap"; ///< 两个匹配允许的最大重叠比例，0~1，缺省 0.5；越界截到 0~1
    static constexpr const char* subPixel = "subPixel"; ///< 亚像素：none、interpolation、least_squares、least_squares_high，缺省 least_squares；NCC 只区分 none 和其他
    static constexpr const char* findLevels = "findLevels"; ///< 定位用的金字塔层数；0 用模型全部层数
    static constexpr const char* greediness = "greediness"; ///< 形状匹配贪婪度，0~1，缺省 0.9，越界截断；只用于 shape
    static constexpr const char* search = "search"; ///< ROI 端口名：搜索区。端口名为空或 roi 的也算搜索区
    static constexpr const char* follow = "follow"; ///< ROI 端口名：跟随区，按匹配位姿变换后从 followRoi 输出
    static constexpr const char* teach = "teach"; ///< ROI 端口名：示教区，从中裁出模板图
    static constexpr const char* score = "score"; ///< 输出值键：首个匹配的分数，0~1
    static constexpr const char* count = "count"; ///< 输出值键：匹配个数
    static constexpr const char* offset = "offset"; ///< 输出/判定键：相对参考位姿的平移距离，像素；输出取首个匹配，判定逐个匹配
    static constexpr const char* offsetX = "offsetX"; ///< 输出/判定键：相对参考位姿的 X 偏移，像素
    static constexpr const char* offsetY = "offsetY"; ///< 输出/判定键：相对参考位姿的 Y 偏移，像素
    static constexpr const char* offsetAngle = "offsetAngle"; ///< 输出/判定键：相对参考位姿的角度偏移，弧度
    static constexpr const char* teachRoi = "teachRoi"; ///< 输出图形端口：示教区
    static constexpr const char* resultRoi = "resultRoi"; ///< 输出图形端口：示教区按每个匹配位姿变换后的区域；轮廓预览时为模型轮廓
    static constexpr const char* followRoi = "followRoi"; ///< 输出图形端口：跟随区按每个匹配位姿变换后的区域
    static constexpr const char* pose = "pose"; ///< 文字槽键：匹配位姿，锚在结果点
    static constexpr const char* verdict = "verdict"; ///< 文字槽键：OK/NG 结论，锚在图像角
    static constexpr const char* teachModel = "teach.model"; ///< 动作 id：用示教区建模并试匹配
    static constexpr const char* inspContour = "insp.contour"; ///< 动作 id：按 contrast 预览形状模型轮廓
};

/** @brief 模板建模方式 */
enum class TmplMethod {
    Ncc,  ///< 归一化互相关，模型文件 model.ncm
    Shape ///< 形状匹配，模型文件 model.shm
};

/** @brief 建模方式的落盘名：ncc 或 shape */
inline const char* tmplMethodName(TmplMethod method)
{
    return method == TmplMethod::Shape ? "shape" : "ncc";
}

/**
 * @brief 落盘名转建模方式
 * @param text 落盘名，空串按 ncc
 * @param out 输出方式，不可空
 * @return 识别成功返回 true；未知名或 out 为空返回 false
 */
inline bool tmplMethodFrom(const std::string& text, TmplMethod* out)
{
    if (out == nullptr) {
        return false;
    }
    if (text.empty() || text == tmplMethodName(TmplMethod::Ncc)) {
        *out = TmplMethod::Ncc;
        return true;
    }
    if (text == tmplMethodName(TmplMethod::Shape)) {
        *out = TmplMethod::Shape;
        return true;
    }
    return false;
}

/** @brief 建模方式对应的缺省模型文件名：model.shm 或 model.ncm */
inline const char* tmplModelFile(TmplMethod method)
{
    return method == TmplMethod::Shape ? "model.shm" : "model.ncm";
}

/**
 * @class NumOrString
 * @brief 词、一个数，或下限和上限
 */
struct NumOrString {
    enum Form { Word, Number, Pair }; ///< 取值形式：词（如 auto）、一个数、下限和上限
    Form form;        ///< 当前形式
    std::string word; ///< Word 时的词
    double low;       ///< Number 时的数；Pair 时的下限
    double high;      ///< Pair 时的上限
    /** @brief 缺省为空词 */
    NumOrString() : form(Word), low(0), high(0) {}
    /** @brief 构造词形式，空指针按空串 */
    static NumOrString from_word(const char* text)
    {
        NumOrString one;
        one.form = Word;
        one.word = text != nullptr ? text : "";
        return one;
    }
    /** @brief 构造一个数的形式 */
    static NumOrString from_number(double n)
    {
        NumOrString one;
        one.form = Number;
        one.low = n;
        return one;
    }
};

/**
 * @class ToolParaTmpl
 * @brief 模板定位参数。示教键优先读 learn，没有再读 run
 */
class ToolParaTmpl : public ToolParaBase {
public:
    /** @brief 参数类型名，toolParaAs 按它识别 */
    static const char* typeId() { return "ToolParaTmpl"; }
    /** @brief 返回 typeId() */
    const char* typeName() const override { return typeId(); }

    /** @brief 示教参数：建模用，learn 里有就读 learn，否则读 run */
    struct Teach {
        TmplMethod method;         ///< 建模方式
        double angleRange;         ///< 角度范围，度，搜 ±angleRange
        double angleRad;           ///< angleRange 换算的弧度，load 时算
        NumOrString angleStep;     ///< 角度步长，度；Word 为 auto
        NumOrString numLevels;     ///< 建模金字塔层数；Word 为 auto
        std::string metric;        ///< 极性，见 TmplKey::metric
        std::string optimization;  ///< 形状模板点压缩方式
        NumOrString contrast;      ///< 形状模板对比度
        NumOrString minContrast;   ///< 形状模板最小对比度
        std::string modelFile;     ///< 模型文件名；HalTmpl 当前不读
        bool rebuildModel;         ///< 强制重建标志；HalTmpl 当前不读
        /** @brief 缺省：NCC，±15 度，步长和层数自动，对比度 30 */
        Teach()
            : method(TmplMethod::Ncc)
            , angleRange(15.0)
            , angleRad(15.0 * HsCommData::kPi / 180.0)
            , angleStep(NumOrString::from_word("auto"))
            , numLevels(NumOrString::from_word("auto"))
            , metric("use_polarity")
            , optimization("auto")
            , contrast(NumOrString::from_number(30))
            , minContrast(NumOrString::from_word("auto"))
            , modelFile(tmplModelFile(TmplMethod::Ncc))
            , rebuildModel(false)
        {}
    };
    /** @brief 运行参数：定位用，只从 run 读 */
    struct Run {
        double minScore;      ///< 最低匹配分数，0~1
        int numMatches;       ///< 最多匹配个数
        double maxOverlap;    ///< 匹配间最大重叠比例，0~1
        std::string subPixel; ///< 亚像素方式
        int findLevels;       ///< 定位金字塔层数；0 用模型全部层数
        double greediness;    ///< 形状匹配贪婪度，0~1
        /** @brief 缺省：分数 0.5，找 1 个，最小二乘亚像素，贪婪度 0.9 */
        Run()
            : minScore(0.5)
            , numMatches(1)
            , maxOverlap(0.5)
            , subPixel("least_squares")
            , findLevels(0)
            , greediness(0.9)
        {}
    };

    Teach teach; ///< 示教参数
    Run run;     ///< 运行参数

    /** @brief 最近一次 load 的结果：HS_OK，或首个非法字段的错误码 */
    int loadCode() const { return loadCode_; }

    /** @brief 从 ToolParam 填写。非法值回缺省，错误码记入 loadCode() */
    void load(const ToolParam& src) override
    {
        loadCode_ = HS_OK;
        const ParamMap& learnMap = src.learn;
        const ParamMap& runMap = src.run;
        const ParamMap& methodMap = learnMap.contains(TmplKey::method) ? learnMap : runMap;
        if (!tmplMethodFrom(methodMap.value(TmplKey::method).toString(), &teach.method)) {
            teach.method = TmplMethod::Ncc;
            loadCode_ = HS_ERR_TMPL_METHOD;
        }
        const ParamMap& angleMap = learnMap.contains(TmplKey::angleRange) ? learnMap : runMap;
        teach.angleRange = angleMap.value(TmplKey::angleRange).toDouble(15.0);
        teach.angleRad = teach.angleRange * HsCommData::kPi / 180.0;
        takeNum(learnMap, runMap, TmplKey::angleStep, true, &teach.angleStep);
        takeNum(learnMap, runMap, TmplKey::numLevels, false, &teach.numLevels);
        const ParamMap& metricMap = learnMap.contains(TmplKey::metric) ? learnMap : runMap;
        teach.metric = metricMap.value(TmplKey::metric).toString();
        if (teach.metric.empty()) {
            teach.metric = "use_polarity";
        }
        const ParamMap& optMap = learnMap.contains(TmplKey::optimization) ? learnMap : runMap;
        teach.optimization = optMap.value(TmplKey::optimization).toString();
        if (teach.optimization.empty()) {
            teach.optimization = "auto";
        }
        takeNum(learnMap, runMap, TmplKey::contrast, false, &teach.contrast);
        takeNum(learnMap, runMap, TmplKey::minContrast, false, &teach.minContrast);
        teach.modelFile = learnMap.contains(TmplKey::modelFile)
            ? learnMap.value(TmplKey::modelFile).toString()
            : runMap.value(TmplKey::modelFile).toString();
        if (teach.modelFile.empty()) {
            teach.modelFile = tmplModelFile(teach.method);
        }
        const ParamMap& rebuildMap = learnMap.contains(TmplKey::rebuildModel) ? learnMap : runMap;
        teach.rebuildModel = rebuildMap.value(TmplKey::rebuildModel).toBool(false);
        run.minScore = runMap.value(TmplKey::minScore).toDouble(0.5);
        run.numMatches = runMap.value(TmplKey::numMatches).toInt(1);
        run.maxOverlap = runMap.value(TmplKey::maxOverlap).toDouble(0.5);
        run.subPixel = runMap.value(TmplKey::subPixel).toString();
        if (run.subPixel.empty()) {
            run.subPixel = "least_squares";
        }
        run.findLevels = runMap.value(TmplKey::findLevels).toInt(0);
        run.greediness = runMap.value(TmplKey::greediness).toDouble(0.9);
        judge = src.judge;
    }

private:
    /**
     * @brief 读一个“词或数”字段，learn 有该键就读 learn，否则读 run
     * @param key 参数键
     * @param allowPair 是否接受“下限,上限”写法
     * @param slot 输出；键缺失或值非法时保持原值
     * @note 数值 0 按 auto；非法值把 loadCode_ 记为 HS_ERR_TMPL_CONTRAST（contrast）或 HS_ERR_TMPL_VALUE
     */
    void takeNum(const ParamMap& learnMap, const ParamMap& runMap, const char* key, bool allowPair, NumOrString* slot)
    {
        if (slot == nullptr) {
            return;
        }
        const ParamMap& map = learnMap.contains(key) ? learnMap : runMap;
        const ParamValue raw = map.value(key);
        if (raw.isNull()) {
            return;
        }
        const std::string text = raw.toString();
        if (text == "auto" || text == "auto_contrast" || text == "auto_contrast_hyst") {
            *slot = NumOrString::from_word(text.c_str());
            return;
        }
        if (allowPair) {
            const size_t comma = text.find(',');
            if (comma != std::string::npos) {
                char* endA = nullptr;
                char* endB = nullptr;
                const double a = std::strtod(text.c_str(), &endA);
                const double b = std::strtod(text.c_str() + comma + 1, &endB);
                if (endA != text.c_str() && endB != text.c_str() + comma + 1) {
                    NumOrString pair;
                    pair.form = NumOrString::Pair;
                    pair.low = a;
                    pair.high = b;
                    *slot = pair;
                    return;
                }
            }
        }
        if (raw.kind() == ParamValue::KindInt || raw.kind() == ParamValue::KindDouble) {
            const double n = raw.toDouble(0);
            if (n == 0.0) {
                *slot = NumOrString::from_word("auto");
            } else {
                *slot = NumOrString::from_number(n);
            }
            return;
        }
        char* end = nullptr;
        const double n = std::strtod(text.c_str(), &end);
        if (!text.empty() && end != nullptr && *end == '\0') {
            *slot = n == 0.0 ? NumOrString::from_word("auto") : NumOrString::from_number(n);
            return;
        }
        if (loadCode_ == HS_OK) {
            loadCode_ = key == TmplKey::contrast ? HS_ERR_TMPL_CONTRAST : HS_ERR_TMPL_VALUE;
        }
    }

    int loadCode_ = HS_OK; ///< load 结果码，见 loadCode()
};

}

#endif
