#ifndef VISION_TOOLPARADIFF_H
#define VISION_TOOLPARADIFF_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct DiffKey {
    static constexpr const char* threshold = "threshold";
};
class ToolParaDiff : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaDiff"; }
    const char* typeName() const override { return typeId(); }
    struct Run {
        double threshold;
        Run() : threshold(1.0) {}
    };
    Run run;
    int loadCode() const { return loadCode_; }
    void load(const ToolParam& src) override {
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
        run.threshold = runMap.value(DiffKey::threshold).toDouble(1.0);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
