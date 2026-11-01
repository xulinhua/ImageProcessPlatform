#ifndef VISION_TOOLPARANORMALS_H
#define VISION_TOOLPARANORMALS_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct NormalsKey {
    static constexpr const char* radius = "radius";
};
class ToolParaNormals : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaNormals"; }
    const char* typeName() const override { return typeId(); }
    struct Run {
        double radius;
        Run() : radius(5.0) {}
    };
    Run run;
    int loadCode() const { return loadCode_; }
    void load(const ToolParam& src) override {
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
        run.radius = runMap.value(NormalsKey::radius).toDouble(5.0);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
