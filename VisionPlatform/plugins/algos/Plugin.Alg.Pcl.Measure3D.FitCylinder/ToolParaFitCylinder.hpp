#ifndef VISION_TOOLPARAFITCYLINDER_H
#define VISION_TOOLPARAFITCYLINDER_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct FitCylinderKey {
    static constexpr const char* distance = "distance";
};
class ToolParaFitCylinder : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaFitCylinder"; }
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
        run.distance = runMap.value(FitCylinderKey::distance).toDouble(0.5);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
