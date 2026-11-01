#ifndef VISION_TOOLPARACLUSTER_H
#define VISION_TOOLPARACLUSTER_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {
struct ClusterKey {
    static constexpr const char* tolerance = "tolerance";
    static constexpr const char* minSize = "minSize";
};
class ToolParaCluster : public ToolParaBase {
public:
    static const char* typeId() { return "ToolParaCluster"; }
    const char* typeName() const override { return typeId(); }
    struct Run {
        double tolerance;
        int minSize;
        Run() : tolerance(2.0), minSize(50) {}
    };
    Run run;
    int loadCode() const { return loadCode_; }
    void load(const ToolParam& src) override {
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
        run.tolerance = runMap.value(ClusterKey::tolerance).toDouble(2.0);
        run.minSize = runMap.value(ClusterKey::minSize).toInt(50);
        judge = src.judge;
    }
private:
    int loadCode_ = HS_OK;
};
}
#endif
