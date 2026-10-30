#ifndef VISION_PCLNDT_H
#define VISION_PCLNDT_H
#include "AlgToolBase.h"
namespace Vision {
class PclNdt : public AlgToolBase {
public:
    std::string toolName() const { return "locate3d.ndt"; }
    AlgLibKind algLib() const { return AlgLibPcl; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new PclNdt(); }
};
}
#endif