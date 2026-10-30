#ifndef VISION_OPEN3DFITCIRCLE_H
#define VISION_OPEN3DFITCIRCLE_H
#include "AlgToolBase.h"
namespace Vision {
class Open3dFitCircle : public AlgToolBase {
public:
    std::string toolName() const { return "measure3d.fit_circle"; }
    AlgLibKind algLib() const { return AlgLibOpen3d; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new Open3dFitCircle(); }
};
}
#endif