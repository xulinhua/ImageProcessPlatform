#ifndef VISION_TOOLPARACROP_H
#define VISION_TOOLPARACROP_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct CropKey {
    static constexpr const char* minX = "minX";
    static constexpr const char* minY = "minY";
    static constexpr const char* minZ = "minZ";
    static constexpr const char* maxX = "maxX";
    static constexpr const char* maxY = "maxY";
    static constexpr const char* maxZ = "maxZ";
};
class ToolParaCrop : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaCrop"; }
    const char* typeName() const override { return typeId(); }
    struct Run {
        double minX;
        double minY;
        double minZ;
        double maxX;
        double maxY;
        double maxZ;
        Run() : minX(-1e9), minY(-1e9), minZ(-1e9), maxX(1e9), maxY(1e9), maxZ(1e9) {}
    };
    Run run;
    int loadCode() const { return loadCode_; }
    void load(const ToolParam& src) override {
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
        run.minX = runMap.value(CropKey::minX).toDouble(-1e9);
        run.minY = runMap.value(CropKey::minY).toDouble(-1e9);
        run.minZ = runMap.value(CropKey::minZ).toDouble(-1e9);
        run.maxX = runMap.value(CropKey::maxX).toDouble(1e9);
        run.maxY = runMap.value(CropKey::maxY).toDouble(1e9);
        run.maxZ = runMap.value(CropKey::maxZ).toDouble(1e9);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
