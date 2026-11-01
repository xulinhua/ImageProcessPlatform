#ifndef VISION_TOOLPARAFITSPHERE_H
#define VISION_TOOLPARAFITSPHERE_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct FitSphereKey {
    static constexpr const char* distance = "distance";
};
class ToolParaFitSphere : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaFitSphere"; }
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
        run.distance = runMap.value(FitSphereKey::distance).toDouble(0.5);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
