#ifndef VISION_TOOLPARADEPTHTOCLOUD_H
#define VISION_TOOLPARADEPTHTOCLOUD_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct DepthToCloudKey {
    static constexpr const char* fx = "fx";
    static constexpr const char* fy = "fy";
    static constexpr const char* cx = "cx";
    static constexpr const char* cy = "cy";
    static constexpr const char* scale = "scale";
};
class ToolParaDepthToCloud : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaDepthToCloud"; }
    const char* typeName() const override { return typeId(); }
    struct Run {
        double fx;
        double fy;
        double cx;
        double cy;
        double scale;
        Run() : fx(1000), fy(1000), cx(320), cy(240), scale(1.0) {}
    };
    Run run;
    int loadCode() const { return loadCode_; }
    void load(const ToolParam& src) override {
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
        run.fx = runMap.value(DepthToCloudKey::fx).toDouble(1000);
        run.fy = runMap.value(DepthToCloudKey::fy).toDouble(1000);
        run.cx = runMap.value(DepthToCloudKey::cx).toDouble(320);
        run.cy = runMap.value(DepthToCloudKey::cy).toDouble(240);
        run.scale = runMap.value(DepthToCloudKey::scale).toDouble(1.0);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
