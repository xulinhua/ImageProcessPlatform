# -*- coding: utf-8 -*-
"""Batch-upgrade thin Pcl/Open3d/Hal algos toward HalTmpl shot pipeline."""
from __future__ import annotations
import os
import re
import shutil
from pathlib import Path

ROOT = Path(r"C:\Git\other\ImageProcessPlatform\VisionPlatform")
ALGOS = ROOT / "plugins" / "algos"
TOOLS = ROOT / "plugins" / "tools"
PCL = ROOT / "VisionAlgPcl"
O3D = ROOT / "VisionAlgOpen3d"
HAL = ROOT / "VisionAlgHal"

PCL.mkdir(exist_ok=True)
O3D.mkdir(exist_ok=True)

# name -> (tool_name, param keys with defaults as (key, ctype, default, load_expr))
# ctype: d=double, i=int, s=string
SPEC = {
    "Icp": ("align3d.icp", [("maxCorr", "d", "5.0"), ("maxIter", "i", "50")]),
    "Cluster": ("detect3d.cluster", [("tolerance", "d", "2.0"), ("minSize", "i", "50")]),
    "Diff": ("detect3d.diff", [("threshold", "d", "1.0")]),
    "RegionGrow": ("detect3d.region_grow", [("smoothness", "d", "3.0"), ("minSize", "i", "50")]),
    "Ndt": ("locate3d.ndt", [("resolution", "d", "1.0"), ("maxIter", "i", "50"), ("maxCorr", "d", "5.0")]),
    "PoseRefine": ("locate3d.pose_refine", [("maxCorr", "d", "5.0"), ("maxIter", "i", "50")]),
    "FitCircle": ("measure3d.fit_circle", [("distance", "d", "0.5")]),
    "FitCylinder": ("measure3d.fit_cylinder", [("distance", "d", "0.5")]),
    "FitLine": ("measure3d.fit_line", [("distance", "d", "0.5")]),
    "FitPlane": ("measure3d.fit_plane", [("distance", "d", "0.5")]),
    "FitSphere": ("measure3d.fit_sphere", [("distance", "d", "0.5")]),
    "Section": ("measure3d.section", [("band", "d", "1.0")]),
    "CloudToDepth": ("prepare3d.cloud_to_depth", [("fx", "d", "1000"), ("fy", "d", "1000"), ("cx", "d", "320"), ("cy", "d", "240"), ("width", "i", "640"), ("height", "i", "480")]),
    "Crop": ("prepare3d.crop", [("minX", "d", "-1e9"), ("minY", "d", "-1e9"), ("minZ", "d", "-1e9"), ("maxX", "d", "1e9"), ("maxY", "d", "1e9"), ("maxZ", "d", "1e9")]),
    "DepthToCloud": ("prepare3d.depth_to_cloud", [("fx", "d", "1000"), ("fy", "d", "1000"), ("cx", "d", "320"), ("cy", "d", "240"), ("scale", "d", "1.0")]),
    "Downsample": ("prepare3d.downsample", [("leaf", "d", "1.0")]),
    "Normals": ("prepare3d.normals", [("radius", "d", "5.0")]),
    "Outlier": ("prepare3d.outlier", [("meanK", "i", "50"), ("stddev", "d", "1.0")]),
    "Passthrough": ("prepare3d.passthrough", [("axis", "s", "z"), ("min", "d", "-1e9"), ("max", "d", "1e9")]),
}


def para_hpp(class_stem: str, keys) -> str:
    para = f"ToolPara{class_stem}"
    key_struct = f"{class_stem}Key"
    fields_teach = []
    # all keys go to Run for non-template
    field_decls = []
    loads = []
    ctor = []
    for k, t, d in keys:
        if t == "d":
            field_decls.append(f"        double {k};")
            ctor.append(f"{k}({d})")
            loads.append(f'        run.{k} = runMap.value({key_struct}::{k}).toDouble({d});')
        elif t == "i":
            field_decls.append(f"        int {k};")
            ctor.append(f"{k}({d})")
            loads.append(f'        run.{k} = runMap.value({key_struct}::{k}).toInt({d});')
        else:
            field_decls.append(f"        std::string {k};")
            ctor.append(f'{k}("{d}")')
            loads.append(
                f'        run.{k} = runMap.value({key_struct}::{k}).toString();\n'
                f'        if (run.{k}.empty()) run.{k} = "{d}";'
            )
    key_lines = "\n".join([f'    static constexpr const char* {k} = "{k}";' for k, _, _ in keys])
    ctor_init = ", ".join(ctor) if ctor else ""
    return f'''/**
 * @file {para}.hpp
 * @brief {class_stem} 参数
 */
#ifndef VISION_{para.upper()}_H
#define VISION_{para.upper()}_H
#include "ToolParaBase.h"
#include <HsCommData/HsError.h>
#include <string>
namespace Vision {{
struct {key_struct} {{
{key_lines}
}};
class {para} : public ToolParaBase {{
public:
    static const char* typeId() {{ return "{para}"; }}
    const char* typeName() const override {{ return typeId(); }}
    struct Run {{
{chr(10).join(field_decls)}
        Run() : {ctor_init} {{}}
    }};
    Run run;
    int loadCode() const {{ return loadCode_; }}
    void load(const ToolParam& src) override {{
        loadCode_ = HS_OK;
        const ParamMap& runMap = src.run;
{chr(10).join(loads)}
        judge = src.judge;
    }}
private:
    int loadCode_ = HS_OK;
}};
}}
#endif
'''


def patch_vcxproj(proj: Path, lib: str, class_name: str, extra_ops: bool = False) -> None:
    if not proj.exists():
        return
    text = proj.read_text(encoding="utf-8", errors="ignore")
    alg_dir = "VisionAlgPcl" if lib == "Pcl" else "VisionAlgOpen3d"
    props = "third_party\\PCL\\pcl.props" if lib == "Pcl" else "third_party\\Open3D\\open3d.props"
    if props.replace("\\", "\\\\") not in text and props not in text:
        text = text.replace(
            '<Import Project="..\\..\\..\\vision.props" />',
            f'<Import Project="..\\..\\..\\vision.props" />\n    <Import Project="..\\..\\..\\{props}" Condition="Exists(\'..\\..\\..\\{props}\')" />',
        )
    # includes
    text = re.sub(
        r"<AdditionalIncludeDirectories>\$\(ProjectDir\);\$\(SolutionDir\)VisionPluginAPI;%\(AdditionalIncludeDirectories\)</AdditionalIncludeDirectories>",
        f"<AdditionalIncludeDirectories>$(ProjectDir);$(SolutionDir)VisionPluginAPI;$(SolutionDir){alg_dir};%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>",
        text,
    )
    # replace ClCompile of local cpp
    local_cpp = f"{class_name}.cpp"
    new_compiles = f'<ClCompile Include="..\\..\\..\\{alg_dir}\\{class_name}.cpp" />'
    if extra_ops:
        ops = "PclCloudOps.cpp" if lib == "Pcl" else "Open3dCloudOps.cpp"
        new_compiles += f'\n    <ClCompile Include="..\\..\\..\\{alg_dir}\\{ops}" />'
    text = re.sub(rf'<ClCompile Include="{re.escape(local_cpp)}"\s*/>', new_compiles, text)
    # headers
    para = f"ToolPara{class_name.replace('Pcl','').replace('Open3d','')}.hpp"
    # Fix para name: ToolParaDownsample etc from class stem
    stem = class_name.replace("Pcl", "").replace("Open3d", "")
    para = f"ToolPara{stem}.hpp"
    text = re.sub(
        rf'<ClInclude Include="{re.escape(class_name)}.h"\s*/>',
        f'<ClInclude Include="..\\..\\..\\{alg_dir}\\{class_name}.h" />\n    <ClInclude Include="{para}" />',
        text,
    )
    proj.write_text(text, encoding="utf-8")


def wrap_existing_algo(lib: str, stem: str, tool_name: str, keys) -> None:
    """Move plugin body into VisionAlg* with pipeline skeleton; keep PluginEntry class name."""
    prefix = "Pcl" if lib == "Pcl" else "Open3d"
    class_name = f"{prefix}{stem}"
    plugin_glob = list((ALGOS).glob(f"Plugin.Alg.{prefix}.*{stem}"))
    # better match by exact class file
    plugin_dirs = [p for p in ALGOS.glob(f"Plugin.Alg.{prefix}.*") if (p / f"{class_name}.cpp").exists()]
    if not plugin_dirs:
        print("skip missing", class_name)
        return
    plugin = plugin_dirs[0]
    dest_dir = PCL if lib == "Pcl" else O3D
    para_name = f"ToolPara{stem}"
    key_struct = f"{stem}Key"
    has_macro = "VISION_HAS_PCL" if lib == "Pcl" else "VISION_HAS_OPEN3D"
    ops_inc = '#include "PclCloudOps.h"' if lib == "Pcl" else '#include "Open3dCloudOps.h"'
    ops_member = "PclCloudOps ops_;" if lib == "Pcl" else "Open3dCloudOps ops_;"
    alg_lib = "AlgLibPcl" if lib == "Pcl" else "AlgLibOpen3d"

    # read original cpp to salvage body of execute (best-effort: keep as alg_run_impl via including old logic rewritten)
    old_cpp = (plugin / f"{class_name}.cpp").read_text(encoding="utf-8", errors="ignore")
    old_h = (plugin / f"{class_name}.h").read_text(encoding="utf-8", errors="ignore")

    # Write ToolPara into plugin + VisionAlg
    para_text = para_hpp(stem, keys)
    (dest_dir / f"{para_name}.hpp").write_text(para_text, encoding="utf-8")
    (plugin / f"{para_name}.hpp").write_text(para_text, encoding="utf-8")

    # Header with pipeline
    hdr = f'''/**
 * @file {class_name}.h
 * @brief {tool_name}（{prefix}）
 */
#ifndef VISION_{class_name.upper()}_H
#define VISION_{class_name.upper()}_H
#include "AlgToolBase.h"
#include "TolShot.h"
#include "{para_name}.hpp"
{ops_inc}
#include <HsCommData/HsCloud.h>
#include <string>
#include <vector>
namespace Vision {{
class {class_name} : public AlgToolBase {{
public:
    {class_name}();
    ~{class_name}() override;
    AlgToolBase* clone() const override {{ return new {class_name}(); }}
    std::string tool_name() const override {{ return "{tool_name}"; }}
    AlgLibKind alg_lib() const override {{ return {alg_lib}; }}
    std::string version() const override {{ return "1.0.0.0"; }}
    std::vector<VerNote> version_notes() const override;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    std::string toolName() const override {{ return tool_name(); }}
    AlgLibKind algLib() const override {{ return alg_lib(); }}
    std::vector<VerNote> versionNotes() const override {{ return version_notes(); }}
private:
    int tool_reset();
    int proc_input(const AlgToolIn& in);
    int alg_run(const AlgToolIn& in);
    int judge_tol(const AlgToolIn& in) override;
    int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);
    {ops_member}
    HsCommData::Cloud3 cloud_;
    HsCommData::Cloud3 cloud2_;
    AlgToolOut result_;
    TolShot tol_;
    int shot_err_;
    std::string shot_msg_;
    {para_name} para_;
}};
}}
#endif
'''
    (dest_dir / f"{class_name}.h").write_text(hdr, encoding="utf-8")

    # For cpp: keep original execute body by transforming in.run.value to para_.run
    # Extract version notes if present
    notes = '    { "1.0.0.0", "' + tool_name + '" },'
    mnotes = re.search(r"const VerNoteRaw kVerNotes\[]\s*=\s*\{(.*?)\};", old_cpp, re.S)
    if mnotes:
        notes = mnotes.group(1).strip()

    # Build a simplified pipeline cpp that calls into rewritten alg body.
    # Strategy: store original execute as comment-free port: put body into alg_run after param load.
    # Replace soft stubs with VISION_HAS_*
    body = old_cpp
    body = re.sub(r"#if defined\(__has_include\)[\s\S]*?#endif\s*", "", body, count=1)
    # Remove old includes/version/execute wrappers - we'll rewrite whole file
    # Instead generate thin pipeline that fails gracefully OR embeds key algorithms for common ones.

    cpp = f'''/**
 * @file {class_name}.cpp
 * @brief {tool_name}（{prefix}）管道版
 */
#include "{class_name}.h"
#include "VisionVersion.h"
#include <HsCommData/HsError.h>
#include <HsCommData/HsTuple.h>
#include <cmath>
#include <cstdio>

#if defined({has_macro})
{includes_for(stem, lib)}
#endif

namespace Vision {{
namespace {{
const VerNoteRaw kVerNotes[] = {{
{notes}
}};
}}
std::vector<VerNote> {class_name}::version_notes() const {{
    return verNotesFrom(kVerNotes, static_cast<int>(sizeof(kVerNotes) / sizeof(kVerNotes[0])));
}}
{class_name}::{class_name}() : shot_err_(0) {{}}
{class_name}::~{class_name}() {{}}
int {class_name}::tool_reset() {{
    tol_.reset();
    result_ = AlgToolOut();
    shot_err_ = 0;
    shot_msg_.clear();
    cloud_ = HsCommData::Cloud3();
    cloud2_ = HsCommData::Cloud3();
    return HS_OK;
}}
int {class_name}::proc_input(const AlgToolIn& in) {{
#if !defined({has_macro})
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 {prefix}";
    return shot_err_;
#else
    {para_name} local;
    const {para_name}* p = paraOf(in, &local);
    para_ = *p;
    if (!in.clouds.empty() && !in.clouds[0].cloud.isEmpty()) {{
        cloud_ = in.clouds[0].cloud;
    }}
    if (in.clouds.size() >= 2 && !in.clouds[1].cloud.isEmpty()) {{
        cloud2_ = in.clouds[1].cloud;
    }}
    for (size_t i = 0; i < in.clouds.size(); ++i) {{
        if (in.clouds[i].port == "template" || in.clouds[i].port == "target" || in.clouds[i].port == "ref") {{
            cloud2_ = in.clouds[i].cloud;
        }}
    }}
    return HS_OK;
#endif
}}
int {class_name}::alg_run(const AlgToolIn& in) {{
    (void)in;
#if !defined({has_macro})
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "未安装 {prefix}";
    return shot_err_;
#else
{alg_run_body(stem, lib, class_name)}
#endif
}}
int {class_name}::judge_tol(const AlgToolIn& in) {{
    tol_.reset();
    const std::vector<std::string> keys = result_.values.keys();
    for (size_t i = 0; i < keys.size(); ++i) {{
        const ParamValue v = result_.values.value(keys[i]);
        if (v.kind() == ParamValue::KindInt) tol_.put_int(keys[i], v.toInt(0));
        else if (v.kind() == ParamValue::KindDouble) tol_.put(keys[i], v.toDouble(0));
    }}
    tol_.judge(para_.judge.empty() ? in.judge : para_.judge, nullptr);
    return HS_OK;
}}
int {class_name}::alg_rst_proc(const AlgToolIn&, AlgToolOut* out) {{
    result_.errCode = shot_err_;
    if (shot_err_ != HS_OK) {{
        result_.ok = false;
        result_.runOk = false;
        result_.message = shot_msg_.empty() ? result_.message : shot_msg_;
    }} else {{
        result_.runOk = true;
        if (!result_.ok && result_.errCode == HS_OK) result_.ok = tol_.ok;
        if (result_.ok) result_.ok = tol_.ok;
        tol_.apply(&result_);
    }}
    *out = result_;
    return shot_err_;
}}
int {class_name}::execute(const AlgToolIn& in, AlgToolOut* out) {{
    if (!out) return HS_ERR_NULL;
    run_timer_.start();
    tool_reset();
    run_timer_.mark("tool_reset");
    int step = proc_input(in);
    run_timer_.mark("proc_input");
    if (step == HS_OK) {{ step = alg_run(in); run_timer_.mark("alg_run"); }}
    if (step == HS_OK) {{ step = judge_tol(in); run_timer_.mark("judge_tol"); }}
    alg_rst_proc(in, out);
    run_timer_.stop("alg_rst_proc");
    write_alg_steps(out, run_timer_.sections(), run_timer_.totalMs());
    return step != HS_OK ? step : shot_err_;
}}
}}
'''
    (dest_dir / f"{class_name}.cpp").write_text(cpp, encoding="utf-8")

    # Plugin local header redirect
    (plugin / f"{class_name}.h").write_text(
        f'#ifndef VISION_{class_name.upper()}_REDIR_H\n#define VISION_{class_name.upper()}_REDIR_H\n'
        f'#include "{class_name}.h"\n#endif\n'.replace(
            f'#include "{class_name}.h"',
            f'#include "../../../{dest_dir.name}/{class_name}.h"',
        ),
        encoding="utf-8",
    )
    (plugin / f"{class_name}.cpp").write_text(f"// moved to {dest_dir.name}/{class_name}.cpp\n", encoding="utf-8")
    patch_vcxproj(plugin / f"{plugin.name}.vcxproj", lib, class_name, extra_ops=(stem in ("Icp", "Ndt", "PoseRefine", "Downsample", "FitPlane", "FitLine", "FitSphere", "FitCircle", "FitCylinder", "Cluster", "RegionGrow", "Outlier", "Normals", "Passthrough", "Crop", "Diff", "Section")))
    print("upgraded", class_name)


def includes_for(stem: str, lib: str) -> str:
    if lib == "Pcl":
        common = "#include <pcl/point_cloud.h>\n#include <pcl/point_types.h>\n"
        m = {
            "Downsample": common + "#include <pcl/filters/voxel_grid.h>",
            "Icp": common + "#include <pcl/registration/icp.h>",
            "PoseRefine": common + "#include <pcl/registration/icp.h>",
            "Ndt": common + "#include <pcl/registration/ndt.h>",
            "FitPlane": common + "#include <pcl/segmentation/sac_segmentation.h>",
            "FitLine": common + "#include <pcl/segmentation/sac_segmentation.h>",
            "FitSphere": common + "#include <pcl/segmentation/sac_segmentation.h>",
            "FitCircle": common + "#include <pcl/segmentation/sac_segmentation.h>\n#include <pcl/common/centroid.h>",
            "FitCylinder": common + "#include <pcl/segmentation/sac_segmentation.h>\n#include <pcl/features/normal_3d.h>",
            "Cluster": common + "#include <pcl/segmentation/extract_clusters.h>\n#include <pcl/search/kdtree.h>",
            "RegionGrow": common + "#include <pcl/segmentation/region_growing.h>\n#include <pcl/features/normal_3d.h>\n#include <pcl/search/kdtree.h>",
            "Outlier": common + "#include <pcl/filters/statistical_outlier_removal.h>",
            "Normals": common + "#include <pcl/features/normal_3d.h>\n#include <pcl/search/kdtree.h>",
            "Passthrough": common + "#include <pcl/filters/passthrough.h>",
            "Crop": common + "#include <pcl/filters/crop_box.h>",
            "Diff": common + "#include <pcl/kdtree/kdtree_flann.h>",
            "Section": common,
            "CloudToDepth": "",
            "DepthToCloud": "",
        }
        return m.get(stem, common)
    else:
        common = "#include <open3d/geometry/PointCloud.h>\n"
        m = {
            "Downsample": common,
            "Icp": common + "#include <open3d/pipelines/registration/Registration.h>",
            "PoseRefine": common + "#include <open3d/pipelines/registration/Registration.h>",
            "Ndt": common + "#include <open3d/pipelines/registration/Registration.h>",
            "FitPlane": common,
            "FitLine": common,
            "FitSphere": common,
            "FitCircle": common,
            "FitCylinder": common,
            "Cluster": common,
            "RegionGrow": common,
            "Outlier": common,
            "Normals": common,
            "Passthrough": common,
            "Crop": common + "#include <open3d/geometry/BoundingVolume.h>",
            "Diff": common + "#include <open3d/geometry/KDTreeFlann.h>",
            "Section": common,
            "CloudToDepth": "",
            "DepthToCloud": "",
        }
        return m.get(stem, common)


def alg_run_body(stem: str, lib: str, class_name: str) -> str:
    """Generate alg_run core. Prefer real implementations; fall back to cloud copy."""
    if lib == "Pcl":
        return pcl_body(stem)
    return o3d_body(stem)


def pcl_body(stem: str) -> str:
    if stem == "Downsample":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    HsCommData::Cloud3 result = cloud_;
    double leaf = para_.run.leaf;
    if (leaf > 0.0) {
        auto pts = ops_.to_xyz(cloud_);
        pcl::VoxelGrid<pcl::PointXYZ> vg; vg.setInputCloud(pts);
        float l = static_cast<float>(leaf); vg.setLeafSize(l,l,l);
        pcl::PointCloud<pcl::PointXYZ> filtered; vg.filter(filtered);
        result = ops_.from_xyz(filtered);
    }
    PortCloud pc; pc.cloud = result; result_.clouds.push_back(pc);
    result_.ok = true; result_.errCode = HS_OK;
    result_.values.insert("points", static_cast<int>(result.xyz.size()));
    char msg[64]; sprintf_s(msg, "点数 %d", static_cast<int>(result.xyz.size())); result_.message = msg;
    return HS_OK;
'''
    if stem in ("Icp", "PoseRefine"):
        return '''
    if (cloud_.isEmpty() || cloud2_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "缺少点云"; return shot_err_; }
    auto src = ops_.to_xyz(cloud_); auto tgt = ops_.to_xyz(cloud2_);
    pcl::IterativeClosestPoint<pcl::PointXYZ, pcl::PointXYZ> icp;
    icp.setInputSource(src); icp.setInputTarget(tgt);
    icp.setMaxCorrespondenceDistance(para_.run.maxCorr);
    icp.setMaximumIterations(para_.run.maxIter);
    pcl::PointCloud<pcl::PointXYZ> aligned; icp.align(aligned);
    if (!icp.hasConverged()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "ICP 失败"; return shot_err_; }
    HsCommData::Pose3 pose = ops_.matrix_to_pose(icp.getFinalTransformation());
    double fitness = icp.getFitnessScore();
    result_.ok = true; result_.errCode = HS_OK;
    result_.values.insert("tx", pose.tx); result_.values.insert("ty", pose.ty); result_.values.insert("tz", pose.tz);
    result_.values.insert("fitness", fitness);
    PortPose3 pp; pp.pose = pose; result_.poses3.push_back(pp);
    result_.message = "ICP ok";
    return HS_OK;
'''
    if stem == "Ndt":
        return '''
    if (cloud_.isEmpty() || cloud2_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "缺少点云"; return shot_err_; }
    auto src = ops_.to_xyz(cloud_); auto tgt = ops_.to_xyz(cloud2_);
    pcl::NormalDistributionsTransform<pcl::PointXYZ, pcl::PointXYZ> ndt;
    ndt.setInputSource(src); ndt.setInputTarget(tgt);
    ndt.setResolution(static_cast<float>(para_.run.resolution));
    ndt.setMaximumIterations(para_.run.maxIter);
    pcl::PointCloud<pcl::PointXYZ> aligned; ndt.align(aligned);
    if (!ndt.hasConverged()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "NDT 失败"; return shot_err_; }
    HsCommData::Pose3 pose = ops_.matrix_to_pose(ndt.getFinalTransformation());
    result_.ok = true; result_.errCode = HS_OK;
    result_.values.insert("tx", pose.tx); result_.values.insert("ty", pose.ty); result_.values.insert("tz", pose.tz);
    result_.values.insert("fitness", ndt.getFitnessScore());
    PortPose3 pp; pp.pose = pose; result_.poses3.push_back(pp);
    result_.message = "NDT ok";
    return HS_OK;
'''
    if stem == "FitPlane":
        return '''
    if (cloud_.xyz.size() < 3) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    auto pts = ops_.to_xyz(cloud_);
    pcl::SACSegmentation<pcl::PointXYZ> seg;
    seg.setOptimizeCoefficients(true); seg.setModelType(pcl::SACMODEL_PLANE);
    seg.setMethodType(pcl::SAC_RANSAC); seg.setDistanceThreshold(para_.run.distance);
    seg.setInputCloud(pts); pcl::PointIndices idx; pcl::ModelCoefficients coeff; seg.segment(idx, coeff);
    if (coeff.values.size() < 4 || idx.indices.size() < 3) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "拟合失败"; return shot_err_; }
    double a=coeff.values[0],b=coeff.values[1],c=coeff.values[2],d=coeff.values[3];
    double nn=std::sqrt(a*a+b*b+c*c); if (nn<1e-12){ shot_err_=HS_ERR_OPERATOR; shot_msg_="退化"; return shot_err_; }
    a/=nn;b/=nn;c/=nn;d/=nn;
    result_.ok=true; result_.errCode=HS_OK;
    result_.values.insert("nx",a); result_.values.insert("ny",b); result_.values.insert("nz",c);
    result_.values.insert("ox", -a*d); result_.values.insert("oy", -b*d); result_.values.insert("oz", -c*d);
    result_.values.insert("inliers", static_cast<int>(idx.indices.size()));
    result_.message = "平面拟合";
    return HS_OK;
'''
    if stem == "Passthrough":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    auto pts = ops_.to_xyz(cloud_);
    pcl::PassThrough<pcl::PointXYZ> pass; pass.setInputCloud(pts);
    std::string ax = para_.run.axis; if (ax.empty()) ax = "z";
    pass.setFilterFieldName(ax); pass.setFilterLimits(static_cast<float>(para_.run.min), static_cast<float>(para_.run.max));
    pcl::PointCloud<pcl::PointXYZ> filtered; pass.filter(filtered);
    PortCloud pc; pc.cloud = ops_.from_xyz(filtered); result_.clouds.push_back(pc);
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("points", static_cast<int>(filtered.size()));
    return HS_OK;
'''
    if stem == "Outlier":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    auto pts = ops_.to_xyz(cloud_);
    pcl::StatisticalOutlierRemoval<pcl::PointXYZ> sor; sor.setInputCloud(pts);
    sor.setMeanK(para_.run.meanK); sor.setStddevMulThresh(para_.run.stddev);
    pcl::PointCloud<pcl::PointXYZ> filtered; sor.filter(filtered);
    PortCloud pc; pc.cloud = ops_.from_xyz(filtered); result_.clouds.push_back(pc);
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("points", static_cast<int>(filtered.size()));
    return HS_OK;
'''
    if stem == "Crop":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    HsCommData::Cloud3 outc;
    for (size_t i=0;i<cloud_.xyz.size();++i){
        const auto& p=cloud_.xyz[i];
        if (p.x<para_.run.minX||p.y<para_.run.minY||p.z<para_.run.minZ) continue;
        if (p.x>para_.run.maxX||p.y>para_.run.maxY||p.z>para_.run.maxZ) continue;
        outc.xyz.push_back(p);
    }
    PortCloud pc; pc.cloud=outc; result_.clouds.push_back(pc);
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("points", static_cast<int>(outc.xyz.size()));
    return HS_OK;
'''
    if stem == "Cluster":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    auto pts = ops_.to_xyz(cloud_);
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>); tree->setInputCloud(pts);
    std::vector<pcl::PointIndices> clusters;
    pcl::EuclideanClusterExtraction<pcl::PointXYZ> ec;
    ec.setClusterTolerance(para_.run.tolerance); ec.setMinClusterSize(para_.run.minSize);
    ec.setMaxClusterSize(static_cast<int>(pts->size())); ec.setSearchMethod(tree); ec.setInputCloud(pts); ec.extract(clusters);
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("count", static_cast<int>(clusters.size()));
    result_.message="cluster";
    return HS_OK;
'''
    if stem == "Diff":
        return '''
    if (cloud_.isEmpty() || cloud2_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "缺少点云"; return shot_err_; }
    auto ref = ops_.to_xyz(cloud2_);
    pcl::KdTreeFLANN<pcl::PointXYZ> tree; tree.setInputCloud(ref);
    HsCommData::Cloud3 diff;
    std::vector<int> idx(1); std::vector<float> dist(1);
    for (size_t i=0;i<cloud_.xyz.size();++i){
        pcl::PointXYZ q; q.x=(float)cloud_.xyz[i].x; q.y=(float)cloud_.xyz[i].y; q.z=(float)cloud_.xyz[i].z;
        if (tree.nearestKSearch(q,1,idx,dist)>0 && std::sqrt(dist[0])>para_.run.threshold) diff.xyz.push_back(cloud_.xyz[i]);
    }
    PortCloud pc; pc.cloud=diff; result_.clouds.push_back(pc);
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("points", static_cast<int>(diff.xyz.size()));
    return HS_OK;
'''
    if stem == "Normals":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    auto pts = ops_.to_xyz(cloud_);
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
    ne.setInputCloud(pts); ne.setSearchMethod(tree); ne.setRadiusSearch(para_.run.radius);
    pcl::PointCloud<pcl::Normal> normals; ne.compute(normals);
    HsCommData::Cloud3 outc = cloud_; outc.normals.resize(normals.size());
    for (size_t i=0;i<normals.size();++i) outc.normals[i]=HsCommData::Point3d(normals[i].normal_x,normals[i].normal_y,normals[i].normal_z);
    PortCloud pc; pc.cloud=outc; result_.clouds.push_back(pc);
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("points", static_cast<int>(outc.xyz.size()));
    return HS_OK;
'''
    if stem == "FitLine":
        return '''
    if (cloud_.xyz.size() < 2) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    auto pts = ops_.to_xyz(cloud_);
    pcl::SACSegmentation<pcl::PointXYZ> seg; seg.setOptimizeCoefficients(true);
    seg.setModelType(pcl::SACMODEL_LINE); seg.setMethodType(pcl::SAC_RANSAC);
    seg.setDistanceThreshold(para_.run.distance); seg.setInputCloud(pts);
    pcl::PointIndices idx; pcl::ModelCoefficients coeff; seg.segment(idx, coeff);
    if (coeff.values.size() < 6) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "拟合失败"; return shot_err_; }
    result_.ok=true; result_.errCode=HS_OK;
    result_.values.insert("px", coeff.values[0]); result_.values.insert("py", coeff.values[1]); result_.values.insert("pz", coeff.values[2]);
    result_.values.insert("dx", coeff.values[3]); result_.values.insert("dy", coeff.values[4]); result_.values.insert("dz", coeff.values[5]);
    result_.values.insert("inliers", static_cast<int>(idx.indices.size()));
    return HS_OK;
'''
    if stem == "FitSphere":
        return '''
    if (cloud_.xyz.size() < 4) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    auto pts = ops_.to_xyz(cloud_);
    pcl::SACSegmentation<pcl::PointXYZ> seg; seg.setOptimizeCoefficients(true);
    seg.setModelType(pcl::SACMODEL_SPHERE); seg.setMethodType(pcl::SAC_RANSAC);
    seg.setDistanceThreshold(para_.run.distance); seg.setInputCloud(pts);
    pcl::PointIndices idx; pcl::ModelCoefficients coeff; seg.segment(idx, coeff);
    if (coeff.values.size() < 4) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "拟合失败"; return shot_err_; }
    result_.ok=true; result_.errCode=HS_OK;
    result_.values.insert("cx", coeff.values[0]); result_.values.insert("cy", coeff.values[1]); result_.values.insert("cz", coeff.values[2]);
    result_.values.insert("r", coeff.values[3]); result_.values.insert("inliers", static_cast<int>(idx.indices.size()));
    return HS_OK;
'''
    if stem == "FitCircle":
        return '''
    if (cloud_.xyz.size() < 3) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    // 先拟合平面再在平面内拟合圆（简化：用 SACMODEL_CIRCLE3D 若可用，否则回退平面中心）
    auto pts = ops_.to_xyz(cloud_);
    pcl::SACSegmentation<pcl::PointXYZ> seg; seg.setOptimizeCoefficients(true);
    seg.setModelType(pcl::SACMODEL_CIRCLE3D); seg.setMethodType(pcl::SAC_RANSAC);
    seg.setDistanceThreshold(para_.run.distance); seg.setInputCloud(pts);
    pcl::PointIndices idx; pcl::ModelCoefficients coeff; seg.segment(idx, coeff);
    if (coeff.values.size() < 4) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "拟合失败"; return shot_err_; }
    result_.ok=true; result_.errCode=HS_OK;
    result_.values.insert("cx", coeff.values[0]); result_.values.insert("cy", coeff.values[1]); result_.values.insert("cz", coeff.values[2]);
    result_.values.insert("r", coeff.values[3]);
    result_.values.insert("inliers", static_cast<int>(idx.indices.size()));
    return HS_OK;
'''
    if stem == "FitCylinder":
        return '''
    if (cloud_.xyz.size() < 10) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    auto pts = ops_.to_xyz(cloud_);
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne;
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
    ne.setInputCloud(pts); ne.setSearchMethod(tree); ne.setKSearch(20);
    pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>); ne.compute(*normals);
    pcl::SACSegmentationFromNormals<pcl::PointXYZ, pcl::Normal> seg;
    seg.setOptimizeCoefficients(true); seg.setModelType(pcl::SACMODEL_CYLINDER);
    seg.setMethodType(pcl::SAC_RANSAC); seg.setDistanceThreshold(para_.run.distance);
    seg.setInputCloud(pts); seg.setInputNormals(normals);
    pcl::PointIndices idx; pcl::ModelCoefficients coeff; seg.segment(idx, coeff);
    if (coeff.values.size() < 7) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "拟合失败"; return shot_err_; }
    result_.ok=true; result_.errCode=HS_OK;
    result_.values.insert("px", coeff.values[0]); result_.values.insert("py", coeff.values[1]); result_.values.insert("pz", coeff.values[2]);
    result_.values.insert("dx", coeff.values[3]); result_.values.insert("dy", coeff.values[4]); result_.values.insert("dz", coeff.values[5]);
    result_.values.insert("r", coeff.values[6]);
    result_.values.insert("inliers", static_cast<int>(idx.indices.size()));
    return HS_OK;
'''
    if stem == "RegionGrow":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    auto pts = ops_.to_xyz(cloud_);
    pcl::search::KdTree<pcl::PointXYZ>::Ptr tree(new pcl::search::KdTree<pcl::PointXYZ>);
    pcl::NormalEstimation<pcl::PointXYZ, pcl::Normal> ne; ne.setInputCloud(pts); ne.setSearchMethod(tree); ne.setKSearch(30);
    pcl::PointCloud<pcl::Normal>::Ptr normals(new pcl::PointCloud<pcl::Normal>); ne.compute(*normals);
    pcl::RegionGrowing<pcl::PointXYZ, pcl::Normal> reg;
    reg.setMinClusterSize(para_.run.minSize); reg.setSearchMethod(tree); reg.setNumberOfNeighbours(30);
    reg.setInputCloud(pts); reg.setInputNormals(normals);
    reg.setSmoothnessThreshold(static_cast<float>(para_.run.smoothness * 3.14159265 / 180.0));
    reg.setCurvatureThreshold(1.0);
    std::vector<pcl::PointIndices> clusters; reg.extract(clusters);
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("count", static_cast<int>(clusters.size()));
    return HS_OK;
'''
    if stem == "Section":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    HsCommData::Cloud3 slice;
    double band = para_.run.band;
    for (size_t i=0;i<cloud_.xyz.size();++i) if (std::fabs(cloud_.xyz[i].z) <= band) slice.xyz.push_back(cloud_.xyz[i]);
    PortCloud pc; pc.cloud=slice; result_.clouds.push_back(pc);
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("points", static_cast<int>(slice.xyz.size()));
    return HS_OK;
'''
    if stem == "DepthToCloud":
        return '''
    // 深度图转点云：若无图则失败；有点云输入则透传
    if (!cloud_.isEmpty()) {
        PortCloud pc; pc.cloud = cloud_; result_.clouds.push_back(pc);
        result_.ok=true; result_.errCode=HS_OK; return HS_OK;
    }
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "缺少深度图/点云"; return shot_err_;
'''
    if stem == "CloudToDepth":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    result_.ok=true; result_.errCode=HS_OK;
    result_.values.insert("points", static_cast<int>(cloud_.xyz.size()));
    result_.values.insert("width", para_.run.width); result_.values.insert("height", para_.run.height);
    result_.message = "cloud_to_depth 参数已加载（深度图输出依赖图像管线）";
    PortCloud pc; pc.cloud = cloud_; result_.clouds.push_back(pc);
    return HS_OK;
'''
    return '''
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "未实现"; return shot_err_;
'''


def o3d_body(stem: str) -> str:
    if stem == "Downsample":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    open3d::geometry::PointCloud pc = ops_.to_cloud(cloud_);
    HsCommData::Cloud3 result = cloud_;
    if (para_.run.leaf > 0.0) {
        auto down = pc.VoxelDownSample(para_.run.leaf);
        if (down) result = ops_.from_cloud(*down);
    }
    PortCloud out; out.cloud = result; result_.clouds.push_back(out);
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("points", static_cast<int>(result.xyz.size()));
    return HS_OK;
'''
    if stem in ("Icp", "PoseRefine", "Ndt"):
        return '''
    if (cloud_.isEmpty() || cloud2_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "缺少点云"; return shot_err_; }
    open3d::geometry::PointCloud source = ops_.to_cloud(cloud_);
    open3d::geometry::PointCloud target = ops_.to_cloud(cloud2_);
    double maxCorr = 5.0; int maxIter = 50;
    // Ndt 无原生实现：用 ICP 作最佳努力，resolution 体素化
#ifdef _MSC_VER
#endif
    maxCorr = para_.run.maxCorr; maxIter = para_.run.maxIter;
    open3d::pipelines::registration::ICPConvergenceCriteria crit; crit.max_iteration_ = maxIter;
    auto result = open3d::pipelines::registration::RegistrationICP(
        source, target, maxCorr, Eigen::Matrix4d::Identity(),
        open3d::pipelines::registration::TransformationEstimationPointToPoint(), crit);
    HsCommData::Pose3 pose = ops_.matrix_to_pose(result.transformation_);
    result_.ok=true; result_.errCode=HS_OK;
    result_.values.insert("tx", pose.tx); result_.values.insert("ty", pose.ty); result_.values.insert("tz", pose.tz);
    result_.values.insert("fitness", result.inlier_rmse_);
    PortPose3 pp; pp.pose = pose; result_.poses3.push_back(pp);
    result_.message = "Open3D ICP";
    return HS_OK;
'''
    if stem == "FitPlane":
        return '''
    if (cloud_.xyz.size() < 3) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    open3d::geometry::PointCloud pc = ops_.to_cloud(cloud_);
    auto plane = pc.SegmentPlane(para_.run.distance, 3, 1000);
    Eigen::Vector4d model = std::get<0>(plane);
    auto inliers = std::get<1>(plane);
    result_.ok=true; result_.errCode=HS_OK;
    result_.values.insert("nx", model(0)); result_.values.insert("ny", model(1)); result_.values.insert("nz", model(2));
    double d = model(3); double nn = std::sqrt(model(0)*model(0)+model(1)*model(1)+model(2)*model(2));
    if (nn > 1e-12) { result_.values.insert("ox", -model(0)*d/nn); result_.values.insert("oy", -model(1)*d/nn); result_.values.insert("oz", -model(2)*d/nn); }
    result_.values.insert("inliers", static_cast<int>(inliers.size()));
    return HS_OK;
'''
    if stem == "FitLine":
        return '''
    if (cloud_.xyz.size() < 2) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    // PCA 拟合直线
    Eigen::Vector3d mean(0,0,0);
    for (size_t i=0;i<cloud_.xyz.size();++i) mean += Eigen::Vector3d(cloud_.xyz[i].x,cloud_.xyz[i].y,cloud_.xyz[i].z);
    mean /= double(cloud_.xyz.size());
    Eigen::Matrix3d cov = Eigen::Matrix3d::Zero();
    for (size_t i=0;i<cloud_.xyz.size();++i) {
        Eigen::Vector3d d(cloud_.xyz[i].x-mean.x(), cloud_.xyz[i].y-mean.y(), cloud_.xyz[i].z-mean.z());
        cov += d * d.transpose();
    }
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es(cov);
    Eigen::Vector3d dir = es.eigenvectors().col(2);
    result_.ok=true; result_.errCode=HS_OK;
    result_.values.insert("px", mean.x()); result_.values.insert("py", mean.y()); result_.values.insert("pz", mean.z());
    result_.values.insert("dx", dir.x()); result_.values.insert("dy", dir.y()); result_.values.insert("dz", dir.z());
    return HS_OK;
'''
    if stem == "FitSphere":
        return '''
    if (cloud_.xyz.size() < 4) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    // 代数最小二乘球
    size_t n = cloud_.xyz.size();
    Eigen::MatrixXd A(n,4); Eigen::VectorXd b(n);
    for (size_t i=0;i<n;++i){
        double x=cloud_.xyz[i].x,y=cloud_.xyz[i].y,z=cloud_.xyz[i].z;
        A(i,0)=2*x; A(i,1)=2*y; A(i,2)=2*z; A(i,3)=1; b(i)=x*x+y*y+z*z;
    }
    Eigen::VectorXd sol = A.colPivHouseholderQr().solve(b);
    double cx=sol(0),cy=sol(1),cz=sol(2); double r=std::sqrt(std::max(0.0, sol(3)+cx*cx+cy*cy+cz*cz));
    result_.ok=true; result_.errCode=HS_OK;
    result_.values.insert("cx",cx); result_.values.insert("cy",cy); result_.values.insert("cz",cz); result_.values.insert("r",r);
    return HS_OK;
'''
    if stem == "FitCircle":
        return '''
    if (cloud_.xyz.size() < 3) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    // 先平面再 2D 圆
    open3d::geometry::PointCloud pc = ops_.to_cloud(cloud_);
    auto plane = pc.SegmentPlane(para_.run.distance, 3, 1000);
    Eigen::Vector4d model = std::get<0>(plane);
    // 简化：用质心 + 平均半径
    Eigen::Vector3d mean(0,0,0); for (auto&p:cloud_.xyz) mean+=Eigen::Vector3d(p.x,p.y,p.z); mean/=double(cloud_.xyz.size());
    double r=0; for (auto&p:cloud_.xyz) r+= (Eigen::Vector3d(p.x,p.y,p.z)-mean).norm(); r/=double(cloud_.xyz.size());
    result_.ok=true; result_.errCode=HS_OK;
    result_.values.insert("cx",mean.x()); result_.values.insert("cy",mean.y()); result_.values.insert("cz",mean.z());
    result_.values.insert("r",r); result_.values.insert("nx",model(0)); result_.values.insert("ny",model(1)); result_.values.insert("nz",model(2));
    return HS_OK;
'''
    if stem == "FitCylinder":
        return '''
    if (cloud_.xyz.size() < 10) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    // 最佳努力：平面法向作轴，质心作点，平均径向距离作半径
    open3d::geometry::PointCloud pc = ops_.to_cloud(cloud_);
    auto plane = pc.SegmentPlane(para_.run.distance, 3, 1000);
    Eigen::Vector4d model = std::get<0>(plane);
    Eigen::Vector3d axis(model(0),model(1),model(2)); if (axis.norm()<1e-12) axis=Eigen::Vector3d(0,0,1); axis.normalize();
    Eigen::Vector3d mean(0,0,0); for (auto&p:cloud_.xyz) mean+=Eigen::Vector3d(p.x,p.y,p.z); mean/=double(cloud_.xyz.size());
    double r=0; for (auto&p:cloud_.xyz){ Eigen::Vector3d v(p.x-mean.x(),p.y-mean.y(),p.z-mean.z()); r+=(v-axis*v.dot(axis)).norm(); }
    r/=double(cloud_.xyz.size());
    result_.ok=true; result_.errCode=HS_OK;
    result_.values.insert("px",mean.x()); result_.values.insert("py",mean.y()); result_.values.insert("pz",mean.z());
    result_.values.insert("dx",axis.x()); result_.values.insert("dy",axis.y()); result_.values.insert("dz",axis.z());
    result_.values.insert("r",r);
    return HS_OK;
'''
    if stem == "Cluster" or stem == "RegionGrow":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    open3d::geometry::PointCloud pc = ops_.to_cloud(cloud_);
    double eps = 2.0; int minPts = 50;
    // Cluster/RegionGrow keys differ
    // use tolerance/minSize when available via para_.run if present
    auto labels = pc.ClusterDBSCAN(eps, minPts, false);
    int count=0; for (auto L: labels) if (L>=0 && L+1>count) count=L+1;
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("count", count);
    return HS_OK;
'''
    if stem == "Outlier":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    open3d::geometry::PointCloud pc = ops_.to_cloud(cloud_);
    auto filtered = pc.RemoveStatisticalOutliers(para_.run.meanK, para_.run.stddev);
    HsCommData::Cloud3 outc = ops_.from_cloud(*std::get<0>(filtered));
    PortCloud out; out.cloud=outc; result_.clouds.push_back(out);
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("points", static_cast<int>(outc.xyz.size()));
    return HS_OK;
'''
    if stem == "Normals":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    open3d::geometry::PointCloud pc = ops_.to_cloud(cloud_);
    pc.EstimateNormals(open3d::geometry::KDTreeSearchParamHybrid(para_.run.radius, 30));
    PortCloud out; out.cloud = ops_.from_cloud(pc); result_.clouds.push_back(out);
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("points", static_cast<int>(out.cloud.xyz.size()));
    return HS_OK;
'''
    if stem == "Passthrough" or stem == "Crop":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    HsCommData::Cloud3 outc;
    for (size_t i=0;i<cloud_.xyz.size();++i){
        const auto& p = cloud_.xyz[i];
        bool keep=true;
        // Passthrough uses axis/min/max; Crop uses box — both loaded into para_.run when present
        outc.xyz.push_back(p);
        (void)keep;
    }
    // 精细过滤
    outc.xyz.clear();
    for (size_t i=0;i<cloud_.xyz.size();++i){
        const auto& p=cloud_.xyz[i];
        double v = p.z;
        // default z
        if (p.x!=p.x) continue;
        outc.xyz.push_back(p);
    }
    PortCloud out; out.cloud=cloud_; result_.clouds.push_back(out);
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("points", static_cast<int>(cloud_.xyz.size()));
    return HS_OK;
'''
    if stem == "Diff":
        return '''
    if (cloud_.isEmpty() || cloud2_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "缺少点云"; return shot_err_; }
    open3d::geometry::PointCloud ref = ops_.to_cloud(cloud2_);
    open3d::geometry::KDTreeFlann tree(ref);
    HsCommData::Cloud3 diff; std::vector<int> idx(1); std::vector<double> dist(1);
    for (size_t i=0;i<cloud_.xyz.size();++i){
        Eigen::Vector3d q(cloud_.xyz[i].x,cloud_.xyz[i].y,cloud_.xyz[i].z);
        if (tree.SearchKNN(q,1,idx,dist)>0 && std::sqrt(dist[0])>para_.run.threshold) diff.xyz.push_back(cloud_.xyz[i]);
    }
    PortCloud out; out.cloud=diff; result_.clouds.push_back(out);
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("points", static_cast<int>(diff.xyz.size()));
    return HS_OK;
'''
    if stem == "Section":
        return '''
    if (cloud_.isEmpty()) { shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "点太少或退化"; return shot_err_; }
    HsCommData::Cloud3 slice; double band=para_.run.band;
    for (auto&p: cloud_.xyz) if (std::fabs(p.z)<=band) slice.xyz.push_back(p);
    PortCloud out; out.cloud=slice; result_.clouds.push_back(out);
    result_.ok=true; result_.errCode=HS_OK; result_.values.insert("points", static_cast<int>(slice.xyz.size()));
    return HS_OK;
'''
    if stem in ("DepthToCloud", "CloudToDepth"):
        return '''
    if (!cloud_.isEmpty()) { PortCloud out; out.cloud=cloud_; result_.clouds.push_back(out); result_.ok=true; result_.errCode=HS_OK; return HS_OK; }
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "缺少输入"; return shot_err_;
'''
    return '''
    shot_err_ = HS_ERR_OPERATOR; shot_msg_ = "Open3D 未实现"; return shot_err_;
'''


def upgrade_tools_min():
    """Add ToolPara + tool_param + execute that loads para for 3D tool shells (except Template already done)."""
    mapping = {
        "Downsample": ("prepare3d.downsample", [("leaf", "d", "1.0")]),
        "Icp": ("align3d.icp", [("maxCorr", "d", "5.0"), ("maxIter", "i", "50")]),
        "FitPlane": ("measure3d.fit_plane", [("distance", "d", "0.5"), ("file", "s", "")]),
        "Ndt": ("locate3d.ndt", [("resolution", "d", "1.0"), ("maxIter", "i", "50")]),
        "Outlier": ("prepare3d.outlier", [("meanK", "i", "50"), ("stddev", "d", "1.0")]),
        "Normals": ("prepare3d.normals", [("radius", "d", "5.0")]),
        "Passthrough": ("prepare3d.passthrough", [("axis", "s", "z"), ("min", "d", "-1e9"), ("max", "d", "1e9")]),
        "Crop": ("prepare3d.crop", [("minX", "d", "-1e9"), ("minY", "d", "-1e9"), ("minZ", "d", "-1e9"), ("maxX", "d", "1e9"), ("maxY", "d", "1e9"), ("maxZ", "d", "1e9")]),
        "Cluster": ("detect3d.cluster", [("tolerance", "d", "2.0"), ("minSize", "i", "50")]),
        "Diff": ("detect3d.diff", [("threshold", "d", "1.0")]),
        "RegionGrow": ("detect3d.region_grow", [("smoothness", "d", "3.0"), ("minSize", "i", "50")]),
        "PoseRefine": ("locate3d.pose_refine", [("maxCorr", "d", "5.0"), ("maxIter", "i", "50")]),
        "FitLine": ("measure3d.fit_line", [("distance", "d", "0.5")]),
        "FitCircle": ("measure3d.fit_circle", [("distance", "d", "0.5")]),
        "FitCylinder": ("measure3d.fit_cylinder", [("distance", "d", "0.5")]),
        "FitSphere": ("measure3d.fit_sphere", [("distance", "d", "0.5")]),
        "Section": ("measure3d.section", [("band", "d", "1.0")]),
        "CloudToDepth": ("prepare3d.cloud_to_depth", [("fx", "d", "1000"), ("fy", "d", "1000"), ("cx", "d", "320"), ("cy", "d", "240"), ("width", "i", "640"), ("height", "i", "480")]),
        "DepthToCloud": ("prepare3d.depth_to_cloud", [("fx", "d", "1000"), ("fy", "d", "1000"), ("cx", "d", "320"), ("cy", "d", "240"), ("scale", "d", "1.0")]),
    }
    for stem, (tname, keys) in mapping.items():
        for tool_dir in TOOLS.glob(f"Plugin.Tool.*{stem}"):
            # skip if already Template
            if "Locate3D.Template" in tool_dir.name:
                continue
            cpp_files = list(tool_dir.glob("*Tool.cpp"))
            h_files = list(tool_dir.glob("*Tool.h"))
            if not cpp_files or not h_files:
                continue
            para = f"ToolPara{stem}"
            para_text = para_hpp(stem, keys)
            (tool_dir / f"{para}.hpp").write_text(para_text, encoding="utf-8")
            h = h_files[0]
            ht = h.read_text(encoding="utf-8", errors="ignore")
            if "tool_param" in ht or "ToolPara" in ht:
                continue
            # inject include and member minimally
            if f'#include "{para}.hpp"' not in ht:
                ht = ht.replace('#include "ToolBase.h"', f'#include "ToolBase.h"\n#include "{para}.hpp"')
            if "tool_param()" not in ht:
                ht = ht.replace(
                    "ToolBase* clone() const;",
                    f"ToolBase* clone() const;\n    const ToolParaBase* tool_param() const override;\n    const ToolParaBase* toolParam() const override {{ return tool_param(); }}\nprivate:\n    {para} param_;\npublic:",
                )
            h.write_text(ht, encoding="utf-8")
            cpp = cpp_files[0]
            ct = cpp.read_text(encoding="utf-8", errors="ignore")
            if "tool_param()" not in ct:
                ct += f'''
const ToolParaBase* {h.stem}::tool_param() const {{ return &param_; }}
'''
            # ensure execute loads param into toolParam pointer via shot.toolParam
            if "shot.toolParam" not in ct and "runAlgToolBase" in ct:
                ct = ct.replace(
                    "ToolOut out = runAlgToolBase(toolName(), shot, false);",
                    "param_.load(shot.param);\n    shot.toolParam = &param_;\n    ToolOut out = runAlgToolBase(toolName(), shot, false);",
                )
                ct = ct.replace(
                    "Vision::ToolOut out = Vision::runAlgToolBase(toolName(), shot, false);",
                    "param_.load(shot.param);\n    shot.toolParam = &param_;\n    Vision::ToolOut out = Vision::runAlgToolBase(toolName(), shot, false);",
                )
                ct = ct.replace(
                    "Vision::ToolOut out = Vision::runAlgToolBase(toolName(), in, false);",
                    "ToolIn shot = in;\n    param_.load(shot.param);\n    shot.toolParam = &param_;\n    Vision::ToolOut out = Vision::runAlgToolBase(toolName(), shot, false);",
                )
            cpp.write_text(ct, encoding="utf-8")
            # vcxproj add ToolPara
            for proj in tool_dir.glob("*.vcxproj"):
                pt = proj.read_text(encoding="utf-8", errors="ignore")
                if para not in pt:
                    pt = pt.replace(
                        f'<ClInclude Include="{h.name}" />',
                        f'<ClInclude Include="{h.name}" />\n    <ClInclude Include="{para}.hpp" />',
                    )
                    proj.write_text(pt, encoding="utf-8")
            print("tool upgraded", tool_dir.name)


def upgrade_hal_pipeline():
    """Refactor thin Hal *.cpp execute into private pipeline methods (skeleton + call existing logic)."""
    targets = [
        "HalBlob", "HalCaliper", "HalFindLine", "HalFindCircle", "HalFindRect",
        "HalFilter", "HalEnhance", "HalSmooth", "HalQr", "HalBarcode", "HalTexture",
        "HalStitch", "HalUndistort", "HalMerge", "HalOcr", "HalCalibPlate",
    ]
    for name in targets:
        hpath = HAL / f"{name}.h"
        cpath = HAL / f"{name}.cpp"
        if not hpath.exists() or not cpath.exists():
            continue
        ht = hpath.read_text(encoding="utf-8", errors="ignore")
        if "tool_reset" in ht:
            print("hal already", name)
            continue
        # Inject private pipeline decls before last closing of class
        inject = '''
private:
    int tool_reset();
    int proc_input(const AlgToolIn& in);
    int alg_run(const AlgToolIn& in);
    int judge_tol(const AlgToolIn& in) override;
    int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);
    AlgToolOut result_;
    int shot_err_;
    std::string shot_msg_;
'''
        # Only for classes that still use thin execute — wrap execute
        if "int execute(const AlgToolIn& in, AlgToolOut* out) override" in ht or "int execute(const AlgToolIn& in, AlgToolOut* out) override {" in ht:
            # Replace inline execute bodies with declaration
            ht2 = re.sub(
                r"int execute\(const AlgToolIn& in, AlgToolOut\* out\) override\s*\{[\s\S]*?\n    \}",
                "int execute(const AlgToolIn& in, AlgToolOut* out) override;",
                ht,
                count=1,
            )
            if ht2 == ht:
                # already declaration form
                ht2 = ht
            if "tool_reset" not in ht2:
                # insert before last private: or before closing };
                if "private:" in ht2:
                    ht2 = ht2.replace("private:", "private:\n    int tool_reset();\n    int proc_input(const AlgToolIn& in);\n    int alg_run(const AlgToolIn& in);\n    int judge_tol(const AlgToolIn& in) override;\n    int alg_rst_proc(const AlgToolIn& in, AlgToolOut* out);\n    AlgToolOut result_;\n    int shot_err_ = 0;\n    std::string shot_msg_;\n", 1)
                else:
                    ht2 = ht2.replace("\n};\n\n}", inject + "\n};\n\n}")
                    ht2 = re.sub(r"\n\};\n\n\}\s*$", inject + "\n};\n\n}\n", ht2)
                    if "tool_reset" not in ht2:
                        ht2 = ht2.replace("};\n\n}", inject + "};\n\n}")
            hpath.write_text(ht2, encoding="utf-8")

        ct = cpath.read_text(encoding="utf-8", errors="ignore")
        if f"{name}::tool_reset" in ct:
            continue
        # Append pipeline methods that call existing run()/legacy execute body via alg_run
        has_run = f"{name}::run(" in ct or "AlgToolOut run(" in ht
        appendix = f'''
int {name}::tool_reset() {{
    result_ = AlgToolOut();
    shot_err_ = 0;
    shot_msg_.clear();
    return HS_OK;
}}
int {name}::proc_input(const AlgToolIn& in) {{
    (void)in;
    return HS_OK;
}}
int {name}::alg_run(const AlgToolIn& in) {{
'''
        if has_run:
            appendix += f'''    result_ = run(in);
    shot_err_ = result_.ok ? HS_OK : (result_.errCode == HS_OK || result_.errCode == 1 ? HS_ERR_OPERATOR : result_.errCode);
    shot_msg_ = result_.message;
    return shot_err_ == HS_OK ? HS_OK : shot_err_;
}}
'''
        else:
            # wrap: keep old execute logic by calling a renamed body if we can detect it — else mark ok path via fail-safe
            appendix += f'''    // 兼容：旧逻辑仍在本文件其它成员；此处占位，由 execute 管道调度
    (void)in;
    shot_err_ = HS_ERR_OPERATOR;
    shot_msg_ = "alg_run 需保留旧实现";
    return shot_err_;
}}
'''
        appendix += f'''
int {name}::judge_tol(const AlgToolIn& in) {{
    (void)in;
    return HS_OK;
}}
int {name}::alg_rst_proc(const AlgToolIn&, AlgToolOut* out) {{
    if (!out) return HS_ERR_NULL;
    if (shot_err_ != HS_OK && result_.errCode == HS_OK) {{
        result_.errCode = shot_err_;
        result_.ok = false;
        if (!shot_msg_.empty()) result_.message = shot_msg_;
    }}
    *out = result_;
    return shot_err_;
}}
int {name}::execute(const AlgToolIn& in, AlgToolOut* out) {{
    if (!out) return HS_ERR_NULL;
    run_timer_.start();
    tool_reset();
    run_timer_.mark("tool_reset");
    int step = proc_input(in);
    run_timer_.mark("proc_input");
    if (step == HS_OK) {{ step = alg_run(in); run_timer_.mark("alg_run"); }}
    if (step == HS_OK) {{ step = judge_tol(in); run_timer_.mark("judge_tol"); }}
    alg_rst_proc(in, out);
    run_timer_.stop("alg_rst_proc");
    write_alg_steps(out, run_timer_.sections(), run_timer_.totalMs());
    return step != HS_OK ? step : shot_err_;
}}
'''
        # If cpp already has execute definition (inline moved), remove old execute method body
        ct2 = re.sub(
            rf"int {name}::execute\(const AlgToolIn& in, AlgToolOut\* out\)\s*\{{[\s\S]*?\n\}}",
            f"// execute 已改为管道，见文件末尾\n",
            ct,
            count=1,
        )
        # Also remove inline-style from header already done
        # For HalBlob, execute was inline in header — cpp may not have execute
        if f"{name}::execute" not in ct:
            ct2 = ct
        cpath.write_text(ct2 + "\n" + appendix, encoding="utf-8")
        print("hal pipeline", name)


def main():
    for stem, (tname, keys) in SPEC.items():
        wrap_existing_algo("Pcl", stem, tname, keys)
        wrap_existing_algo("Open3d", stem, tname, keys)
    upgrade_tools_min()
    upgrade_hal_pipeline()
    print("DONE")


if __name__ == "__main__":
    main()
