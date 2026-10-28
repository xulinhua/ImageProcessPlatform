/**
 * @file ToolParaTmpl.hpp
 * @brief 模板定位的示教参数和运行参数
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

/** @brief 模板工具落盘键 */
struct TmplKey {
    static constexpr const char* method = "method";
    static constexpr const char* angleRange = "angleRange";
    static constexpr const char* angleStep = "angleStep";
    static constexpr const char* numLevels = "numLevels";
    static constexpr const char* metric = "metric";
    static constexpr const char* optimization = "optimization";
    static constexpr const char* minContrast = "minContrast";
    static constexpr const char* rebuildModel = "rebuildModel";
    static constexpr const char* contrast = "contrast";
    static constexpr const char* modelFile = "modelFile";
    static constexpr const char* minScore = "minScore";
    static constexpr const char* numMatches = "numMatches";
    static constexpr const char* maxOverlap = "maxOverlap";
    static constexpr const char* subPixel = "subPixel";
    static constexpr const char* findLevels = "findLevels";
    static constexpr const char* greediness = "greediness";
    static constexpr const char* search = "search";
    static constexpr const char* follow = "follow";
    static constexpr const char* teach = "teach";
    static constexpr const char* score = "score";
    static constexpr const char* count = "count";
    static constexpr const char* offset = "offset";
    static constexpr const char* offsetX = "offsetX";
    static constexpr const char* offsetY = "offsetY";
    static constexpr const char* offsetAngle = "offsetAngle";
    static constexpr const char* teachRoi = "teachRoi";
    static constexpr const char* resultRoi = "resultRoi";
    static constexpr const char* followRoi = "followRoi";
    static constexpr const char* pose = "pose";
    static constexpr const char* verdict = "verdict";
    static constexpr const char* teachModel = "teach.model";
    static constexpr const char* inspContour = "insp.contour";
};

enum class TmplMethod {
    Ncc,
    Shape
};

inline const char* tmplMethodName(TmplMethod method)
{
    return method == TmplMethod::Shape ? "shape" : "ncc";
}

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

inline const char* tmplModelFile(TmplMethod method)
{
    return method == TmplMethod::Shape ? "model.shm" : "model.ncm";
}

/** @brief 词、一个数，或下限和上限 */
struct NumOrString {
    enum Form { Word, Number, Pair };
    Form form;
    std::string word;
    double low;
    double high;
    NumOrString() : form(Word), low(0), high(0) {}
    static NumOrString from_word(const char* text)
    {
        NumOrString one;
        one.form = Word;
        one.word = text != nullptr ? text : "";
        return one;
    }
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
    static const char* typeId() { return "ToolParaTmpl"; }
    const char* typeName() const override { return typeId(); }

    struct Teach {
        TmplMethod method;
        double angleRange;
        double angleRad;
        NumOrString angleStep;
        NumOrString numLevels;
        std::string metric;
        std::string optimization;
        NumOrString contrast;
        NumOrString minContrast;
        std::string modelFile;
        bool rebuildModel;
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
    struct Run {
        double minScore;
        int numMatches;
        double maxOverlap;
        std::string subPixel;
        int findLevels;
        double greediness;
        Run()
            : minScore(0.5)
            , numMatches(1)
            , maxOverlap(0.5)
            , subPixel("least_squares")
            , findLevels(0)
            , greediness(0.9)
        {}
    };

    Teach teach;
    Run run;

    int loadCode() const { return loadCode_; }

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

    int loadCode_ = HS_OK;
};

}

#endif
