#ifndef VISION_PCLFITSPHERE_H
#define VISION_PCLFITSPHERE_H
#include "AlgToolBase.h"
namespace Vision {
class PclFitSphere : public AlgToolBase {
public:
    std::string toolName() const { return "measure3d.fit_sphere"; }
    AlgLibKind algLib() const { return AlgLibPcl; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new PclFitSphere(); }
};
}
#endif
