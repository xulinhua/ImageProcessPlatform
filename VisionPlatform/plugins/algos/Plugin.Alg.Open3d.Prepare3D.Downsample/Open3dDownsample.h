#ifndef VISION_OPEN3DDOWNSAMPLE_H
#define VISION_OPEN3DDOWNSAMPLE_H
#include "AlgToolBase.h"
namespace Vision {
class Open3dDownsample : public AlgToolBase {
public:
    std::string toolName() const { return "prepare3d.downsample"; }
    AlgLibKind algLib() const { return AlgLibOpen3d; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new Open3dDownsample(); }
};
}
#endif
