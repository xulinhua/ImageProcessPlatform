#ifndef VISION_PCLDEPTHTOCLOUD_H
#define VISION_PCLDEPTHTOCLOUD_H
#include "AlgToolBase.h"
namespace Vision {
class PclDepthToCloud : public AlgToolBase {
public:
    std::string toolName() const { return "prepare3d.depth_to_cloud"; }
    AlgLibKind algLib() const { return AlgLibPcl; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new PclDepthToCloud(); }
};
}
#endif