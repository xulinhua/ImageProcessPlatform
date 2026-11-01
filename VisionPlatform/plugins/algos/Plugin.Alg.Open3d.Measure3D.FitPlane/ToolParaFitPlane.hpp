#ifndef VISION_TOOLPARAFITPLANE_H
#define VISION_TOOLPARAFITPLANE_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct FitPlaneKey {
    static constexpr const char* distance = "distance";
};
class ToolParaFitPlane : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaFitPlane"; }
    const char* typeName() const override { return typeId(); }
    struct Run {
        double distance;
        Run() : distance(0.5) {}
    };
    Run run;
    int loadCode() const { return loadCode_; }
    void load(const ToolParam& src) override {
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
        run.distance = runMap.value(FitPlaneKey::distance).toDouble(0.5);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
