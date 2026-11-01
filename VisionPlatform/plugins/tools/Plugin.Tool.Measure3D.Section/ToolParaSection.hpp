#ifndef VISION_TOOLPARASECTION_H
#define VISION_TOOLPARASECTION_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct SectionKey {
    static constexpr const char* band = "band";
};
class ToolParaSection : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaSection"; }
    const char* typeName() const override { return typeId(); }
    struct Run {
        double band;
        Run() : band(1.0) {}
    };
    Run run;
    int loadCode() const { return loadCode_; }
    void load(const ToolParam& src) override {
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
        run.band = runMap.value(SectionKey::band).toDouble(1.0);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
