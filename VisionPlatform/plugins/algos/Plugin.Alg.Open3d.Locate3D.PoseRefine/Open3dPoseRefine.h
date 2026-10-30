#ifndef VISION_OPEN3DPOSEREFINE_H
#define VISION_OPEN3DPOSEREFINE_H
#include "AlgToolBase.h"
namespace Vision {
class Open3dPoseRefine : public AlgToolBase {
public:
    std::string toolName() const { return "locate3d.pose_refine"; }
    AlgLibKind algLib() const { return AlgLibOpen3d; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new Open3dPoseRefine(); }
};
}
#endif