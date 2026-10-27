# -*- coding: utf-8 -*-
"""Generate tool/algo plugin projects and patch the solution."""
import os
import re

ROOT = r"d:\Git\VisionPlatformTest\VisionPlatform"

VCX_HEAD = r'''<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|Win32"><Configuration>Debug</Configuration><Platform>Win32</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Release|Win32"><Configuration>Release</Configuration><Platform>Win32</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>{%s}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>%s</RootNamespace>
    <WindowsTargetPlatformVersion>8.1</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
'''

def configs(kind, extra=""):
    lines = []
    for cfg, plat, dbg, opt in (
        ("Debug", "Win32", "true", ""),
        ("Release", "Win32", "false", "<WholeProgramOptimization>true</WholeProgramOptimization>"),
        ("Debug", "x64", "true", "<UseOfMfc>false</UseOfMfc>"),
        ("Release", "x64", "false", "<WholeProgramOptimization>true</WholeProgramOptimization>"),
    ):
        lines.append(
            '  <PropertyGroup Condition="\'$(Configuration)|$(Platform)\'==\'%s|%s\'" Label="Configuration">'
            '<ConfigurationType>%s</ConfigurationType><UseDebugLibraries>%s</UseDebugLibraries>'
            '<PlatformToolset>v140</PlatformToolset><CharacterSet>Unicode</CharacterSet>%s</PropertyGroup>'
            % (cfg, plat, kind, dbg, opt)
        )
    return "\n".join(lines)

def write(path, text):
    folder = os.path.dirname(path)
    if folder and not os.path.isdir(folder):
        os.makedirs(folder)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text.replace("\r\n", "\n"))

def plugin_vcx(guid, name, target, out_rel, sources, headers, refs, halcon, exports, extra_inc, extra_lib):
    inc = extra_inc or ""
    lib = extra_lib or ""
    props = '    <Import Project="..\\..\\vision.props" />\n'
    if halcon:
        props += '    <Import Project="..\\..\\halcon.props" />\n'
    ref_xml = ""
    for rpath, rguid in refs:
        ref_xml += '    <ProjectReference Include="%s"><Project>{%s}</Project></ProjectReference>\n' % (rpath, rguid)
    src_xml = "".join('    <ClCompile Include="%s" />\n' % s for s in sources)
    hdr_xml = "".join('    <ClInclude Include="%s" />\n' % h for h in headers)
    pre = ""
    if exports:
        pre = "<PreprocessorDefinitions>%s;%%(PreprocessorDefinitions)</PreprocessorDefinitions>" % exports
    text = VCX_HEAD % (guid, name) + configs("DynamicLibrary") + r'''
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings" />
  <ImportGroup Label="PropertySheets">
    <Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props" Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')" Label="LocalAppDataPlatform" />
''' + props + '''  </ImportGroup>
  <PropertyGroup>
    <TargetName>%s</TargetName>
    <OutDir>$(SolutionDir)bin\\$(Platform)\\$(Configuration)\\%s</OutDir>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>%s%%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      %s
    </ClCompile>
    <Link>
      <AdditionalDependencies>%s%%(AdditionalDependencies)</AdditionalDependencies>
      <AdditionalLibraryDirectories>$(SolutionDir)bin\\$(Platform)\\$(Configuration)\\;%%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
    </Link>
  </ItemDefinitionGroup>
  <ItemGroup>
%s  </ItemGroup>
  <ItemGroup>
%s  </ItemGroup>
  <ItemGroup>
%s  </ItemGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
''' % (target, out_rel, inc, pre, lib, src_xml, hdr_xml, ref_xml)
    return text

TOOL_BASE = "B4E91A10-7C22-4F18-9A01-A0B1C2D3E440"
ALGOR_BASE = "B4E91A10-7C22-4F18-9A01-A0B1C2D3E441"
HALCON_COMMON = "B4E91A10-7C22-4F18-9A01-A0B1C2D3E442"
COMMON = "B4E91A10-7C22-4F18-9A01-A0B1C2D3E402"
LANG = "B4E91A10-7C22-4F18-9A01-A0B1C2D3E40C"
GRAB = "B4E91A10-7C22-4F18-9A01-A0B1C2D3E40A"

def image_cpp(tool_name, type_name, title, desc, pass_image, caps_code, field_code):
    return r'''#include "PluginEntry.h"
#include "ImageAlgoTool.h"
#include <HsCommData/Graphic.h>
#include <vector>

namespace {

std::vector<Vision::ParamField> fields()
{
    std::vector<Vision::ParamField> f;
%s
    return f;
}

Vision::ToolCaps caps()
{
    Vision::ToolCaps c;
%s
    return c;
}

}

VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion()
{
    return 1;
}

VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (!host) {
        return;
    }
    host->addTool(new Vision::ImageAlgoTool("%s", Vision::ToolType::%s,
        "%s", "%s", fields(), caps(), %s));
}
''' % (field_code, caps_code, tool_name, type_name, title, desc, "true" if pass_image else "false")

IMAGES = [
    ("Tool.Preprocess.Enhance", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E450", "preprocess.enhance", "Preprocess",
     "图像增强", "可叠加直方图均衡、强调、照度、缩放、反色", True, "",
     '''    f.push_back(Vision::paramInt("pl_n", "步骤数", 1, true, 0, true, 8));
    f.push_back(Vision::paramInt("pl_0_op", "算子", 0, true, 0, true, 4));
    f.push_back(Vision::paramInt("pl_0_w", "掩膜宽", 7, true, 1, true, 255));
    f.push_back(Vision::paramInt("pl_0_h", "掩膜高", 7, true, 1, true, 255));
    f.push_back(Vision::paramDouble("pl_0_c0", "系数", 1.0, false, 0, false, 0));
    f.push_back(Vision::paramDouble("pl_0_c1", "系数2", 0.0, false, 0, false, 0));
    f.push_back(Vision::paramInt("pl_0_f", "缩放到最大", 0, true, 0, true, 1));
'''),
    ("Tool.Preprocess.Smooth", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E451", "preprocess.smooth", "Preprocess",
     "图像平滑", "可叠加均值、中值、高斯、双边滤波", True, "",
     '''    f.push_back(Vision::paramInt("pl_n", "步骤数", 1, true, 0, true, 8));
    f.push_back(Vision::paramInt("pl_0_op", "算子", 1, true, 0, true, 3));
    f.push_back(Vision::paramInt("pl_0_w", "掩膜宽", 3, true, 1, true, 255));
    f.push_back(Vision::paramInt("pl_0_h", "掩膜高", 3, true, 1, true, 255));
    f.push_back(Vision::paramDouble("pl_0_c0", "灰度域", 20.0, false, 0, false, 0));
    f.push_back(Vision::paramDouble("pl_0_c1", "空间域", 3.0, false, 0, false, 0));
'''),
    ("Tool.Locate.Template", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E452", "locate.template", "Locate",
     "模板定位", "NCC/形状模板定位；模型存方案旁 .vpdata，输出位姿", False, "",
     '''    f.push_back(Vision::paramString("method", "匹配方式", "ncc", Vision::paramChoices("ncc", "shape")));
    f.push_back(Vision::paramDouble("minScore", "最低分", 0.5, true, 0.0, true, 1.0));
    f.push_back(Vision::paramDouble("angleRange", "角度范围", 15.0, true, 0.0, true, 180.0));
    f.push_back(Vision::paramInt("numMatches", "匹配个数", 1, true, 1, true, 50));
    f.push_back(Vision::paramPath("modelFile", "模型文件", "model.ncm"));
    f.push_back(Vision::paramBool("rebuildModel", "强制重建", false));
'''),
    ("Tool.Locate.FindCircle", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E453", "locate.find_circle", "Locate",
     "找圆", "在 ROI 内找圆，输出圆心、半径与叠加图形", False,
     "    c.preferredKind = static_cast<int>(HsCommData::ObjCircle);\n",
     '''    f.push_back(Vision::paramInt("threshold", "阈值", 80, true, 0, true, 255));
    f.push_back(Vision::paramInt("minR", "最小半径", 8, true, 1, false, 0));
    f.push_back(Vision::paramInt("maxR", "最大半径", 400, true, 1, false, 0));
    f.push_back(Vision::paramBool("dark", "暗目标", true));
    f.push_back(Vision::paramDouble("sigma", "平滑", 1.0, true, 0.4, true, 20.0));
    f.push_back(Vision::paramInt("low", "低阈值", 20, true, 0, true, 255));
    f.push_back(Vision::paramInt("high", "高阈值", 40, true, 0, true, 255));
'''),
    ("Tool.Locate.FindLine", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E454", "locate.find_line", "Locate",
     "找线", "多 ROI，每框可设查找条数、极性与选择策略", False,
     "    c.usesMultiRoi = true;\n    c.preferredKind = static_cast<int>(HsCommData::ObjLine);\n",
     '''    f.push_back(Vision::paramString("polarity", "极性", "all", Vision::paramChoices("all", "positive", "negative")));
    f.push_back(Vision::paramString("select", "选择", "best", Vision::paramChoices("best", "first", "last")));
    f.push_back(Vision::paramDouble("minLength", "最短长度", 20.0, true, 0.0, false, 0));
    f.push_back(Vision::paramDouble("sigma", "平滑", 1.0, true, 0.4, true, 20.0));
    f.push_back(Vision::paramInt("low", "低阈值", 20, true, 0, true, 255));
    f.push_back(Vision::paramInt("high", "高阈值", 40, true, 0, true, 255));
    f.push_back(Vision::paramInt("searchCount", "查找条数", 1, true, 1, true, 50));
'''),
    ("Tool.Locate.Caliper", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E455", "locate.caliper", "Locate",
     "卡尺", "测宽或找边，支持多卡尺 ROI", False,
     "    c.usesMultiRoi = true;\n    c.preferredKind = static_cast<int>(HsCommData::ObjLine);\n",
     '''    f.push_back(Vision::paramString("mode", "模式", "width", Vision::paramChoices("width", "edge")));
    f.push_back(Vision::paramDouble("sigma", "平滑", 1.0, true, 0.4, true, 20.0));
    f.push_back(Vision::paramDouble("threshold", "阈值", 20.0, true, 0.0, false, 0));
    f.push_back(Vision::paramString("transition", "极性", "all", Vision::paramChoices("all", "positive", "negative")));
    f.push_back(Vision::paramString("select", "选择", "all", Vision::paramChoices("all", "first", "last")));
    f.push_back(Vision::paramInt("searchCount", "查找条数", 1, true, 1, true, 50));
'''),
    ("Tool.Detect.Blob", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E456", "detect.blob", "Detect",
     "Blob", "阈值分割；筛选后可按数量/总面积做 OK/NG 判定", False, "",
     '''    f.push_back(Vision::paramString("threshMode", "阈值方式", "absolute", Vision::paramChoices("absolute", "relative", "dynamic")));
    f.push_back(Vision::paramInt("absMin", "绝对下限", 80, true, 0, true, 255));
    f.push_back(Vision::paramInt("absMax", "绝对上限", 255, true, 0, true, 255));
    f.push_back(Vision::paramDouble("relOffset", "相对偏移", 20.0, false, 0, false, 0));
    f.push_back(Vision::paramDouble("dynOffset", "动态偏移", 15.0, false, 0, false, 0));
    f.push_back(Vision::paramInt("dynKernel", "动态核", 31, true, 3, true, 255));
    f.push_back(Vision::paramBool("dark", "暗目标", false));
    f.push_back(Vision::paramDouble("openR", "开运算", 1.0, true, 0.0, false, 0));
    f.push_back(Vision::paramDouble("closeR", "闭运算", 0.0, true, 0.0, false, 0));
    f.push_back(Vision::paramInt("minArea", "最小面积", 50, true, 0, false, 0));
    f.push_back(Vision::paramInt("maxArea", "最大面积", 0, true, 0, false, 0));
    f.push_back(Vision::paramDouble("minCirc", "最小圆度", 0.0, true, 0.0, true, 1.0));
    f.push_back(Vision::paramDouble("maxCirc", "最大圆度", 1.0, true, 0.0, true, 1.0));
    f.push_back(Vision::paramInt("okMinCount", "数量下限", 0, true, 0, false, 0));
    f.push_back(Vision::paramInt("okMaxCount", "数量上限", 0, true, 0, false, 0));
    f.push_back(Vision::paramDouble("okMinArea", "面积下限", 0.0, true, 0.0, false, 0));
    f.push_back(Vision::paramDouble("okMaxArea", "面积上限", 0.0, true, 0.0, false, 0));
'''),
    ("Tool.Detect.Texture", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E457", "detect.texture", "Detect",
     "纹理缺陷", "规则纹理下破损脏污：大核背景差加形态学", False, "",
     '''    f.push_back(Vision::paramString("bgMethod", "背景", "mean", Vision::paramChoices("mean", "median", "gauss")));
    f.push_back(Vision::paramInt("kernelW", "核宽", 31, true, 3, true, 255));
    f.push_back(Vision::paramInt("kernelH", "核高", 31, true, 3, true, 255));
    f.push_back(Vision::paramDouble("residThresh", "残差阈值", 18.0, true, 0.0, false, 0));
    f.push_back(Vision::paramDouble("openR", "开运算", 1.5, true, 0.0, false, 0));
    f.push_back(Vision::paramInt("minArea", "最小面积", 30, true, 0, false, 0));
    f.push_back(Vision::paramInt("maxArea", "最大面积", 0, true, 0, false, 0));
'''),
]

print("images", len(IMAGES))

def tool_refs(extra=None):
    refs = [
        ("..\\..\\VisionToolBase\\VisionToolBase.vcxproj", TOOL_BASE),
        ("..\\..\\VisionCommon\\VisionCommon.vcxproj", COMMON),
    ]
    if extra:
        refs.extend(extra)
    return refs

projects = []

for dll, guid, tool_name, typ, title, desc, pass_image, caps, fields in IMAGES:
    folder = os.path.join(ROOT, "plugins", "tools", dll)
    write(os.path.join(folder, "PluginEntry.cpp"),
          image_cpp(tool_name, typ, title, desc, pass_image, caps, fields))
    vcx = plugin_vcx(guid, dll.replace(".", ""), dll, "plugins\\tools\\",
                     ["PluginEntry.cpp"], [], tool_refs(), False, "",
                     "$(SolutionDir)VisionPluginAPI;",
                     "VisionToolBase.lib;VisionCommon.lib;")
    write(os.path.join(folder, dll + ".vcxproj"), vcx)
    projects.append((dll, "plugins\\tools\\%s\\%s.vcxproj" % (dll, dll), guid, "3"))

print("image plugins", len(projects))
write(os.path.join(ROOT, "tools", "_gen_marker.txt"), "images %d\n" % len(projects))
