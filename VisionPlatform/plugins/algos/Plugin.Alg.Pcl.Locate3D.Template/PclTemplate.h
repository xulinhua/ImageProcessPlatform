#ifndef VISION_PCLTEMPLATE_H
#define VISION_PCLTEMPLATE_H
#include "AlgToolBase.h"
namespace Vision {
class PclTemplate : public AlgToolBase {
public:
    std::string toolName() const { return "locate3d.template"; }
    AlgLibKind algLib() const { return AlgLibPcl; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new PclTemplate(); }
};
}
#endif