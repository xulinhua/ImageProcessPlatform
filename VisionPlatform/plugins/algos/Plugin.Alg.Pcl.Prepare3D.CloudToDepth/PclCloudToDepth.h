#ifndef VISION_PCLCLOUDTODEPTH_H
#define VISION_PCLCLOUDTODEPTH_H
#include "AlgToolBase.h"
namespace Vision {
class PclCloudToDepth : public AlgToolBase {
public:
    std::string toolName() const { return "prepare3d.cloud_to_depth"; }
    AlgLibKind algLib() const { return AlgLibPcl; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new PclCloudToDepth(); }
};
}
#endif