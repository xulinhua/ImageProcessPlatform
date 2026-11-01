#ifndef VISION_TOOLPARADOWNSAMPLE_H
#define VISION_TOOLPARADOWNSAMPLE_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct DownsampleKey {
    static constexpr const char* leaf = "leaf";
};
class ToolParaDownsample : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaDownsample"; }
    const char* typeName() const override { return typeId(); }
    struct Run {
        double leaf;
        Run() : leaf(1.0) {}
    };
    Run run;
    int loadCode() const { return loadCode_; }
    void load(const ToolParam& src) override {
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
        run.leaf = runMap.value(DownsampleKey::leaf).toDouble(1.0);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
