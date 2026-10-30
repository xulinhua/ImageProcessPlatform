#ifndef VISION_OPEN3DTEMPLATE_H
#define VISION_OPEN3DTEMPLATE_H
#include "AlgToolBase.h"
namespace Vision {
class Open3dTemplate : public AlgToolBase {
public:
    std::string toolName() const { return "locate3d.template"; }
    AlgLibKind algLib() const { return AlgLibOpen3d; }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new Open3dTemplate(); }
};
}
#endif