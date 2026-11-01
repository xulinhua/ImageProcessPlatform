#ifndef VISION_TOOLPARAREGIONGROW_H
#define VISION_TOOLPARAREGIONGROW_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct RegionGrowKey {
    static constexpr const char* smoothness = "smoothness";
    static constexpr const char* minSize = "minSize";
};
class ToolParaRegionGrow : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaRegionGrow"; }
    const char* typeName() const override { return typeId(); }
    struct Run {
        double smoothness;
        int minSize;
        Run() : smoothness(3.0), minSize(50) {}
    };
    Run run;
    int loadCode() const { return loadCode_; }
    void load(const ToolParam& src) override {
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
        run.smoothness = runMap.value(RegionGrowKey::smoothness).toDouble(3.0);
        run.minSize = runMap.value(RegionGrowKey::minSize).toInt(50);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
