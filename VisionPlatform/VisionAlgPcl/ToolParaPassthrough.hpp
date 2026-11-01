#ifndef VISION_TOOLPARAPASSTHROUGH_H
#define VISION_TOOLPARAPASSTHROUGH_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct PassthroughKey {
    static constexpr const char* axis = "axis";
    static constexpr const char* min = "min";
    static constexpr const char* max = "max";
};
class ToolParaPassthrough : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaPassthrough"; }
    const char* typeName() const override { return typeId(); }
    struct Run {
        std::string axis;
        double min;
        double max;
        Run() : axis("z"), min(-1e9), max(1e9) {}
    };
    Run run;
    int loadCode() const { return loadCode_; }
    void load(const ToolParam& src) override {
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
        run.axis = runMap.value(PassthroughKey::axis).toString();
        if (run.axis.empty()) run.axis = "z";
        run.min = runMap.value(PassthroughKey::min).toDouble(-1e9);
        run.max = runMap.value(PassthroughKey::max).toDouble(1e9);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
