#ifndef VISION_PCLICP_H
#define VISION_PCLICP_H
#include "AlgToolBase.h"
namespace Vision {
class PclIcp : public AlgToolBase {
public:
    std::string toolName() const { return "align3d.icp"; }
    AlgLibKind algLib() const { return AlgLibPcl; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new PclIcp(); }
};
}
#endif
