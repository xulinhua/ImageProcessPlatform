#ifndef VISION_HEIGHTTOOL_H
#define VISION_HEIGHTTOOL_H
#include "ToolBase.h"
namespace Vision {
/** @class HeightTool @brief 点到平面有符号高度，壳内计算，无算法库 */
class HeightTool : public ToolBase {
public:
    std::string toolName() const;
    ToolType type() const;
    std::string displayName() const;
    std::string version() const;
    std::vector<VerNote> versionNotes() const;
    std::string description() const;
    ParamMap defaultParams() const;
    std::vector<ParamField> paramFields() const;
    std::vector<PortDesc> ports() const;
    ToolOut execute(const ToolIn& in);
    ToolBase* clone() const;
};
}
#endif
