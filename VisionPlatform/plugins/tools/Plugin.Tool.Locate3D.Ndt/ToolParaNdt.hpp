#ifndef VISION_TOOLPARANDT_H
#define VISION_TOOLPARANDT_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct NdtKey {
    static constexpr const char* resolution = "resolution";
    static constexpr const char* maxIter = "maxIter";
    static constexpr const char* maxCorr = "maxCorr";
    static constexpr const char* minFitness = "minFitness";
};
class ToolParaNdt : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaNdt"; }
    const char* typeName() const override { return typeId(); }
    struct Run {
        double resolution;
        int maxIter;
        double maxCorr;
        double minFitness;
        Run() : resolution(1.0), maxIter(50), maxCorr(5.0), minFitness(1.0e9) {}
    };
    Run run;
    int loadCode() const { return loadCode_; }
    void load(const ToolParam& src) override {
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
        run.resolution = runMap.value(NdtKey::resolution).toDouble(1.0);
        run.maxIter = runMap.value(NdtKey::maxIter).toInt(50);
        run.maxCorr = runMap.value(NdtKey::maxCorr).toDouble(5.0);
        run.minFitness = runMap.value(NdtKey::minFitness).toDouble(1.0e9);
        if (run.minFitness < 0.0) run.minFitness = 0.0;
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif