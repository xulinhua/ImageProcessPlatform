#ifndef VISION_TOOLPARACLOUDTODEPTH_H
#define VISION_TOOLPARACLOUDTODEPTH_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct CloudToDepthKey {
    static constexpr const char* fx = "fx";
    static constexpr const char* fy = "fy";
    static constexpr const char* cx = "cx";
    static constexpr const char* cy = "cy";
    static constexpr const char* width = "width";
    static constexpr const char* height = "height";
};
class ToolParaCloudToDepth : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaCloudToDepth"; }
    const char* typeName() const override { return typeId(); }
    struct Run {
        double fx;
        double fy;
        double cx;
        double cy;
        int width;
        int height;
        Run() : fx(1000), fy(1000), cx(320), cy(240), width(640), height(480) {}
    };
    Run run;
    int loadCode() const { return loadCode_; }
    void load(const ToolParam& src) override {
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
        run.fx = runMap.value(CloudToDepthKey::fx).toDouble(1000);
        run.fy = runMap.value(CloudToDepthKey::fy).toDouble(1000);
        run.cx = runMap.value(CloudToDepthKey::cx).toDouble(320);
        run.cy = runMap.value(CloudToDepthKey::cy).toDouble(240);
        run.width = runMap.value(CloudToDepthKey::width).toInt(640);
        run.height = runMap.value(CloudToDepthKey::height).toInt(480);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
