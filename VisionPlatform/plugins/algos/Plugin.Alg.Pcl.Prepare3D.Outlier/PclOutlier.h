#ifndef VISION_PCLOUTLIER_H
#define VISION_PCLOUTLIER_H
#include "AlgToolBase.h"
namespace Vision {
class PclOutlier : public AlgToolBase {
public:
    std::string toolName() const { return "prepare3d.outlier"; }
    AlgLibKind algLib() const { return AlgLibPcl; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new PclOutlier(); }
};
}
#endif