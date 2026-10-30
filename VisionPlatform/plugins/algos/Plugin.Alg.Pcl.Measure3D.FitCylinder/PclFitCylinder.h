#ifndef VISION_PCLFITCYLINDER_H
#define VISION_PCLFITCYLINDER_H
#include "AlgToolBase.h"
namespace Vision {
class PclFitCylinder : public AlgToolBase {
public:
    std::string toolName() const { return "measure3d.fit_cylinder"; }
    AlgLibKind algLib() const { return AlgLibPcl; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new PclFitCylinder(); }
};
}
#endif