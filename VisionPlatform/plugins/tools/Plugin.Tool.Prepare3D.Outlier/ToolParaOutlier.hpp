#ifndef VISION_TOOLPARAOUTLIER_H
#define VISION_TOOLPARAOUTLIER_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct OutlierKey {
    static constexpr const char* meanK = "meanK";
    static constexpr const char* stddev = "stddev";
};
class ToolParaOutlier : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaOutlier"; }
    const char* typeName() const override { return typeId(); }
    struct Run {
        int meanK;
        double stddev;
        Run() : meanK(50), stddev(1.0) {}
    };
    Run run;
    int loadCode() const { return loadCode_; }
    void load(const ToolParam& src) override {
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
        run.meanK = runMap.value(OutlierKey::meanK).toInt(50);
        run.stddev = runMap.value(OutlierKey::stddev).toDouble(1.0);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
