#ifndef VISION_TOOLPARAFITLINE_H
#define VISION_TOOLPARAFITLINE_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct FitLineKey {
    static constexpr const char* distance = "distance";
};
class ToolParaFitLine : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaFitLine"; }
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
        run.distance = runMap.value(FitLineKey::distance).toDouble(0.5);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
