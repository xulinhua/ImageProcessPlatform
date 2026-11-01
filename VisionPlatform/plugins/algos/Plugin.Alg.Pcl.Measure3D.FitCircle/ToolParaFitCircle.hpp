#ifndef VISION_TOOLPARAFITCIRCLE_H
#define VISION_TOOLPARAFITCIRCLE_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct FitCircleKey {
    static constexpr const char* distance = "distance";
};
class ToolParaFitCircle : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaFitCircle"; }
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
        run.distance = runMap.value(FitCircleKey::distance).toDouble(0.5);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
