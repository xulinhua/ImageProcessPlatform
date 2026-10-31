/**
 * @file ToolEditorRegistry.cpp
 * @brief ToolEditorRegistry实现
 * @author Hoson
 *
 * @copyright Copyright (c) 2026 Hoson. All rights reserved.
 */

#include "ToolEditorRegistry.h"
#include "ToolEditors.h"

namespace Vision {

// 进程内唯一登记表
ToolEditorRegistry& ToolEditorRegistry::instance()
{
    static ToolEditorRegistry g;
    return g;
}

// 登记工具名到参数页工厂。通用页不按名字登记
void ToolEditorRegistry::add(const std::string& toolName, ToolEditorFactory factory)
{
    if (factory) {
        m_map[toolName] = factory;
    }
}

// 有专用页用专用页，其余按工具壳参数说明生成
ToolEditorBase* ToolEditorRegistry::create(const std::string& toolName, QWidget* parent)
{
    std::map<std::string, ToolEditorFactory>::const_iterator it = m_map.find(toolName);
    if (it != m_map.end() && it->second) {
        return it->second(parent);
    }
    return createGenericEditor(parent);
}

// 登记 Cam / Locate / 预处理等专用参数页；main 在算法插件加载后调用
void registerToolEditors()
{
    ToolEditorRegistry& r = ToolEditorRegistry::instance();
    r.add("camera.acquire", createCamDeviceEditor);
    r.add("camera.file", createCameraFileEditor);
    r.add("camera.device", createCamDeviceEditor);
    r.add("acquire3d.load_cloud", createAcquire3DEditor);

    r.add("preprocess.enhance", createEnhanceEditor);
    r.add("preprocess.smooth", createSmoothEditor);
    r.add("preprocess.filter", createFilterStepEditor);
    r.add("detect.blob", createBlobStepEditor);

    r.add("locate.template", createTmplEditor);
    r.add("locate.find_line", createFindLineEditor);
    r.add("locate.find_circle", createFindCircleEditor);
    r.add("locate.caliper", createCaliperEditor);
    r.add("detect.texture", createTextureEditor);

    r.add("result.merge", createMergeEditor);
    r.add("logic.judge", createJudgeEditor);
    r.add("comm.plc", createPlcEditor);
}

}
