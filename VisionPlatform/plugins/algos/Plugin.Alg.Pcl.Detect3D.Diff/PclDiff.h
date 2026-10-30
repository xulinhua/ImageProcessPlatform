#ifndef VISION_PCLDIFF_H
#define VISION_PCLDIFF_H
#include "AlgToolBase.h"
namespace Vision {
class PclDiff : public AlgToolBase {
public:
    std::string toolName() const { return "detect3d.diff"; }
    AlgLibKind algLib() const { return AlgLibPcl; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new PclDiff(); }
};
}
#endif