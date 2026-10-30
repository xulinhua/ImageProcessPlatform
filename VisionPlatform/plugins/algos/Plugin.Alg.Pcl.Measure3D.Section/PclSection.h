#ifndef VISION_PCLSECTION_H
#define VISION_PCLSECTION_H
#include "AlgToolBase.h"
namespace Vision {
class PclSection : public AlgToolBase {
public:
    std::string toolName() const { return "measure3d.section"; }
    AlgLibKind algLib() const { return AlgLibPcl; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new PclSection(); }
};
}
#endif