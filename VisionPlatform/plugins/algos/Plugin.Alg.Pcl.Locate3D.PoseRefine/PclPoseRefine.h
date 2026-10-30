#ifndef VISION_PCLPOSEREFINE_H
#define VISION_PCLPOSEREFINE_H
#include "AlgToolBase.h"
namespace Vision {
class PclPoseRefine : public AlgToolBase {
public:
    std::string toolName() const { return "locate3d.pose_refine"; }
    AlgLibKind algLib() const { return AlgLibPcl; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new PclPoseRefine(); }
};
}
#endif