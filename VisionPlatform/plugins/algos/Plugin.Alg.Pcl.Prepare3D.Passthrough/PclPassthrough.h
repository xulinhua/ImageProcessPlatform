#ifndef VISION_PCLPASSTHROUGH_H
#define VISION_PCLPASSTHROUGH_H
#include "AlgToolBase.h"
namespace Vision {
class PclPassthrough : public AlgToolBase {
public:
    std::string toolName() const { return "prepare3d.passthrough"; }
    AlgLibKind algLib() const { return AlgLibPcl; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new PclPassthrough(); }
};
}
#endif