#ifndef VISION_TOOLPARAICP_H
#define VISION_TOOLPARAICP_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct IcpKey {
    static constexpr const char* maxCorr = "maxCorr";
    static constexpr const char* maxIter = "maxIter";
    static constexpr const char* minFitness = "minFitness";
};
class ToolParaIcp : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaIcp"; }
    const char* typeName() const override { return typeId(); }
    struct Run {
        double maxCorr;
        int maxIter;
        double minFitness; ///< fitness 上限（ICP score / RMSE）
        Run() : maxCorr(5.0), maxIter(50), minFitness(1.0e9) {}
    };
    Run run;
    int loadCode() const { return loadCode_; }
    void load(const ToolParam& src) override {
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
        run.maxCorr = runMap.value(IcpKey::maxCorr).toDouble(5.0);
        run.maxIter = runMap.value(IcpKey::maxIter).toInt(50);
        run.minFitness = runMap.value(IcpKey::minFitness).toDouble(1.0e9);
        if (run.minFitness < 0.0) run.minFitness = 0.0;
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif