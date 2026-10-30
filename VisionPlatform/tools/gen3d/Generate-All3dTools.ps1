# Generates remaining 3D vision tool plugins. Run from VisionPlatform root.
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (Test-Path (Join-Path $PSScriptRoot '_helpers.ps1')) { . (Join-Path $PSScriptRoot '_helpers.ps1') }
else { . (Join-Path $PSScriptRoot 'gen3d\_helpers.ps1') }

$guidN = 0x500
function Next-Guid { script:guidN++; return Guid-For $guidN }

$FOLDER_TOOL = '{A11A0001-0000-4000-8000-000000000061}'
$FOLDER_PCL = '{A11A0001-0000-4000-8000-000000000008}'
$FOLDER_O3D = '{A11A0001-0000-4000-8000-00000000000B}'
$FOLDER_ACQ = '{A11A0001-0000-4000-8000-000000000011}'
$FOLDER_CALIB = '{A11A0001-0000-4000-8000-000000000017}'

$projects = @()

function Add-Project([hashtable]$p) { $script:projects += $p }

function New-ToolPlugin([string]$Folder, [string]$Class, [string]$ToolName, [string]$Display, [string]$TypeEnum,
    [string]$Desc, [string]$Note, [string[]]$ExtraCpp, [string]$NestFolder) {
    $g = Next-Guid
    $projName = $Folder
    $dir = Join-Path $Root "plugins\tools\$projName"
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    $cppName = "${Class}Tool.cpp"
    $hName = "${Class}Tool.h"
    $bodyPath = Join-Path $PSScriptRoot "shell\${Class}Tool.cpp"
    if (Test-Path $bodyPath) {
        Copy-Item $bodyPath (Join-Path $dir $cppName) -Force
    } else {
        throw "Missing shell body: $bodyPath"
    }
    $h = @"
#ifndef VISION_${Class.ToUpper()}_TOOL_H
#define VISION_${Class.ToUpper()}_TOOL_H
#include "ToolBase.h"
namespace Vision {
class ${Class}Tool : public ToolBase {
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
"@
    Write-Utf8 (Join-Path $dir $hName) $h
    $entry = @"
#include "PluginEntry.h"
#include "${Class}Tool.h"
VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion() { return 1; }
VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (host) { host->addTool(new Vision::${Class}Tool()); }
}
"@
    Write-Utf8 (Join-Path $dir 'PluginEntry.cpp') $entry
    $vcx = Tool-Vcxproj $g $projName $Class @('PluginEntry.cpp', $cppName) @($hName) $false ''
    Write-Utf8 (Join-Path $dir "$projName.vcxproj") $vcx
    Add-Project @{ Guid = $g; Name = $projName; Path = "plugins\tools\$projName\$projName.vcxproj"; Nest = $NestFolder }
}

function New-DualPlugin([string]$Category, [string]$Class, [string]$ToolName, [string]$Display, [string]$TypeEnum,
    [string]$Desc, [string]$Note, [string]$ToolFolder) {
    $tg = Next-Guid; $pg = Next-Guid; $og = Next-Guid
    $toolProj = "Plugin.Tool.$ToolFolder"
    $pclProj = "Plugin.Alg.Pcl.$ToolFolder"
    $o3dProj = "Plugin.Alg.Open3d.$ToolFolder"
    $toolDir = Join-Path $Root "plugins\tools\$toolProj"
    $pclDir = Join-Path $Root "plugins\algos\$pclProj"
    $o3dDir = Join-Path $Root "plugins\algos\$o3dProj"

    foreach ($pair in @(
        @{ Dir = $toolDir; Proj = $toolProj; G = $tg; IsAlg = $false; Kind = '' },
        @{ Dir = $pclDir; Proj = $pclProj; G = $pg; IsAlg = $true; Kind = 'pcl' },
        @{ Dir = $o3dDir; Proj = $o3dProj; G = $og; IsAlg = $true; Kind = 'open3d' }
    )) {
        New-Item -ItemType Directory -Path $pair.Dir -Force | Out-Null
    }

    $shellBody = Join-Path $PSScriptRoot "shell\dual\$ClassTool.cpp"
    if (!(Test-Path $shellBody)) { throw "Missing dual shell: $shellBody" }
    Copy-Item $shellBody (Join-Path $toolDir "${Class}Tool.cpp") -Force

    $toolH = @"
#ifndef VISION_${Class.ToUpper()}_TOOL_H
#define VISION_${Class.ToUpper()}_TOOL_H
#include "ToolBase.h"
namespace Vision {
class ${Class}Tool : public ToolBase {
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
"@
    Write-Utf8 (Join-Path $toolDir "${Class}Tool.h") $toolH
    Write-Utf8 (Join-Path $toolDir 'PluginEntry.cpp') @"
#include "PluginEntry.h"
#include "${Class}Tool.h"
VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion() { return 1; }
VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (host) { host->addTool(new Vision::${Class}Tool()); }
}
"@

    foreach ($lib in @('Pcl', 'Open3d')) {
        $algDir = if ($lib -eq 'Pcl') { $pclDir } else { $o3dDir }
        $algClass = "${lib}${Class}"
        $body = Join-Path $PSScriptRoot "bodies\$($lib.ToLower())\$Class.cpp"
        if (!(Test-Path $body)) { throw "Missing alg body: $body" }
        Copy-Item $body (Join-Path $algDir "$algClass.cpp") -Force
        $algH = @"
#ifndef VISION_${algClass.ToUpper()}_H
#define VISION_${algClass.ToUpper()}_H
#include "AlgToolBase.h"
namespace Vision {
class $algClass : public AlgToolBase {
public:
    std::string toolName() const { return "$ToolName"; }
    AlgLibKind algLib() const { return $(if ($lib -eq 'Pcl') { 'AlgLibPcl' } else { 'AlgLibOpen3d' }); }
    std::string version() const { return std::string("1.0.0.0"); }
    std::vector<VerNote> versionNotes() const;
    int execute(const AlgToolIn& in, AlgToolOut* out) override;
    AlgToolBase* clone() const { return new $algClass(); }
};
}
#endif
"@
        Write-Utf8 (Join-Path $algDir "$algClass.h") $algH
        Write-Utf8 (Join-Path $algDir 'PluginEntry.cpp') @"
#include "PluginEntry.h"
#include "$algClass.h"
VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion() { return 1; }
VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (host) { host->addAlg(new Vision::$algClass()); }
}
"@
    }

    $vcxT = Tool-Vcxproj $tg $toolProj $Class @('PluginEntry.cpp', "${Class}Tool.cpp") @("${Class}Tool.h") $false ''
    Write-Utf8 (Join-Path $toolDir "$toolProj.vcxproj") $vcxT
    $vcxP = Tool-Vcxproj $pg $pclProj "PluginAlgPcl$Class" @('PluginEntry.cpp', "Pcl${Class}.cpp") @("Pcl${Class}.h") $true 'pcl'
    Write-Utf8 (Join-Path $pclDir "$pclProj.vcxproj") $vcxP
    $vcxO = Tool-Vcxproj $og $o3dProj "PluginAlgOpen3d$Class" @('PluginEntry.cpp', "Open3d${Class}.cpp") @("Open3d${Class}.h") $true 'open3d'
    Write-Utf8 (Join-Path $o3dDir "$o3dProj.vcxproj") $vcxO

    Add-Project @{ Guid = $tg; Name = $toolProj; Path = "plugins\tools\$toolProj\$toolProj.vcxproj"; Nest = $FOLDER_TOOL }
    Add-Project @{ Guid = $pg; Name = $pclProj; Path = "plugins\algos\$pclProj\$pclProj.vcxproj"; Nest = $FOLDER_PCL }
    Add-Project @{ Guid = $og; Name = $o3dProj; Path = "plugins\algos\$o3dProj\$o3dProj.vcxproj"; Nest = $FOLDER_O3D }
}

# --- shell-only ---
New-ToolPlugin 'Plugin.Tool.Prepare3D.Merge' 'Merge' 'prepare3d.merge' '点云合并' 'Prepare3D' '合并多路点云' '点云合并' @() $FOLDER_TOOL
New-ToolPlugin 'Plugin.Tool.Prepare3D.Transform' 'Transform' 'prepare3d.transform' '坐标变换' 'Prepare3D' '刚体变换点云' '坐标变换' @() $FOLDER_TOOL
New-ToolPlugin 'Plugin.Tool.Measure3D.Flatness' 'Flatness' 'measure3d.flatness' '平面度' 'Measure3D' '相对拟合面峰谷/RMS' '平面度' @() $FOLDER_TOOL
New-ToolPlugin 'Plugin.Tool.Measure3D.Volume' 'Volume' 'measure3d.volume' '体积' 'Measure3D' '相对基准面体积' '体积' @() $FOLDER_TOOL
New-ToolPlugin 'Plugin.Tool.Measure3D.PpDist' 'PpDist' 'measure3d.pp_dist' '点点距离' 'Measure3D' '两点距离' '点点距离' @() $FOLDER_TOOL
New-ToolPlugin 'Plugin.Tool.Measure3D.PlDist' 'PlDist' 'measure3d.pl_dist' '点线距离' 'Measure3D' '点到直线距离' '点线距离' @() $FOLDER_TOOL
New-ToolPlugin 'Plugin.Tool.Measure3D.PlanePlane' 'PlanePlane' 'measure3d.plane_plane' '面面测量' 'Measure3D' '两面距离与夹角' '面面测量' @() $FOLDER_TOOL
New-ToolPlugin 'Plugin.Tool.Acquire3D.Camera' 'Camera3D' 'acquire3d.camera' '三维相机' 'Acquire3D' '三维相机采集' '三维相机' @() $FOLDER_ACQ
New-ToolPlugin 'Plugin.Tool.Acquire3D.DepthFile' 'DepthFile' 'acquire3d.depth_file' '深度图文件' 'Acquire3D' '深度图文件入口' '深度图文件' @() $FOLDER_ACQ
New-ToolPlugin 'Plugin.Tool.Calib3D.HandEye' 'HandEye' 'calib3d.hand_eye' '手眼标定' 'Calib3D' '手眼矩阵求解' '手眼标定' @() $FOLDER_CALIB
New-ToolPlugin 'Plugin.Tool.Calib3D.Extrinsic' 'Extrinsic' 'calib3d.extrinsic' '多相机外参' 'Calib3D' '外参合成' '外参' @() $FOLDER_CALIB

# --- dual ---
$duals = @(
    @{ Cat='Prepare3D'; Class='Crop'; Name='prepare3d.crop'; Disp='点云裁剪'; Folder='Prepare3D.Crop' },
    @{ Cat='Prepare3D'; Class='Outlier'; Name='prepare3d.outlier'; Disp='离群点滤波'; Folder='Prepare3D.Outlier' },
    @{ Cat='Prepare3D'; Class='Passthrough'; Name='prepare3d.passthrough'; Disp='直通滤波'; Folder='Prepare3D.Passthrough' },
    @{ Cat='Prepare3D'; Class='Normals'; Name='prepare3d.normals'; Disp='法向估计'; Folder='Prepare3D.Normals' },
    @{ Cat='Prepare3D'; Class='DepthToCloud'; Name='prepare3d.depth_to_cloud'; Disp='深度图转点云'; Folder='Prepare3D.DepthToCloud' },
    @{ Cat='Prepare3D'; Class='CloudToDepth'; Name='prepare3d.cloud_to_depth'; Disp='点云转深度图'; Folder='Prepare3D.CloudToDepth' },
    @{ Cat='Locate3D'; Class='Template'; Name='locate3d.template'; Disp='点云模板匹配'; Folder='Locate3D.Template' },
    @{ Cat='Locate3D'; Class='Ndt'; Name='locate3d.ndt'; Disp='NDT 配准'; Folder='Locate3D.Ndt' },
    @{ Cat='Locate3D'; Class='PoseRefine'; Name='locate3d.pose_refine'; Disp='位姿精修'; Folder='Locate3D.PoseRefine' },
    @{ Cat='Measure3D'; Class='FitCylinder'; Name='measure3d.fit_cylinder'; Disp='拟合圆柱'; Folder='Measure3D.FitCylinder' },
    @{ Cat='Measure3D'; Class='FitLine'; Name='measure3d.fit_line'; Disp='拟合直线'; Folder='Measure3D.FitLine' },
    @{ Cat='Measure3D'; Class='FitCircle'; Name='measure3d.fit_circle'; Disp='拟合圆'; Folder='Measure3D.FitCircle' },
    @{ Cat='Measure3D'; Class='Section'; Name='measure3d.section'; Disp='截面轮廓'; Folder='Measure3D.Section' },
    @{ Cat='Detect3D'; Class='Cluster'; Name='detect3d.cluster'; Disp='点云聚类'; Folder='Detect3D.Cluster' },
    @{ Cat='Detect3D'; Class='Diff'; Name='detect3d.diff'; Disp='点云差分'; Folder='Detect3D.Diff' },
    @{ Cat='Detect3D'; Class='RegionGrow'; Name='detect3d.region_grow'; Disp='区域生长'; Folder='Detect3D.RegionGrow' }
)
foreach ($d in $duals) {
    New-DualPlugin $d.Cat $d.Class $d.Name $d.Disp $d.Cat "$($d.Disp)工具" $d.Disp $d.Folder
}

# Patch solution
$sln = Join-Path $Root 'VisionPlatform.sln'
$text = [System.IO.File]::ReadAllText($sln)
$insertProj = ''
$insertCfg = ''
$insertNest = ''
foreach ($p in $projects) {
    if ($text -match [regex]::Escape($p.Guid)) { continue }
    $insertProj += "Project(`"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}`") = `"$($p.Name)`", `"$($p.Path)`", `"$($p.Guid)`"`r`nEndProject`r`n"
    $insertCfg += "`t`t$($p.Guid).Debug|Win32.ActiveCfg = Debug|Win32`r`n"
    $insertCfg += "`t`t$($p.Guid).Debug|Win32.Build.0 = Debug|Win32`r`n"
    $insertCfg += "`t`t$($p.Guid).Debug|x64.ActiveCfg = Debug|x64`r`n"
    $insertCfg += "`t`t$($p.Guid).Debug|x64.Build.0 = Debug|x64`r`n"
    $insertCfg += "`t`t$($p.Guid).Release|Win32.ActiveCfg = Release|Win32`r`n"
    $insertCfg += "`t`t$($p.Guid).Release|Win32.Build.0 = Release|Win32`r`n"
    $insertCfg += "`t`t$($p.Guid).Release|x64.ActiveCfg = Release|x64`r`n"
    $insertCfg += "`t`t$($p.Guid).Release|x64.Build.0 = Release|x64`r`n"
    $insertNest += "`t`t$($p.Guid) = $($p.Nest)`r`n"
}
if ($insertProj) {
    $text = $text -replace '(?m)^(Project\("\{8BC9CEB8).*\r?\nGlobal)', ($insertProj + '$1' + "`r`nGlobal")
    # fix botched replace - use simpler anchor
}
# Simpler sln patch: insert before Global
$anchor = "Global"
$idx = $text.LastIndexOf("Global")
if ($idx -lt 0) { throw 'sln Global not found' }
$text = $text.Insert($idx, $insertProj)
$idx2 = $text.IndexOf("GlobalSection(ProjectConfigurationPlatforms)")
if ($idx2 -lt 0) { throw 'ProjectConfigurationPlatforms not found' }
$endCfg = $text.IndexOf("EndGlobalSection", $idx2)
$text = $text.Insert($endCfg, $insertCfg)
$idx3 = $text.IndexOf("GlobalSection(NestedProjects)")
if ($idx3 -lt 0) { throw 'NestedProjects not found' }
$endNest = $text.IndexOf("EndGlobalSection", $idx3)
$text = $text.Insert($endNest, $insertNest)
[System.IO.File]::WriteAllText($sln, $text, [System.Text.UTF8Encoding]::new($false))

# Update docs
$doc = Join-Path $Root 'docs\3d-tools.md'
$docText = [System.IO.File]::ReadAllText($doc)
$docText = $docText -replace '\| 壳优先 \|', '| 已有 |'
$docText = $docText -replace '\| 后期 \|', '| 已有 |'
[System.IO.File]::WriteAllText($doc, $docText, [System.Text.UTF8Encoding]::new($false))

Write-Host "Generated $($projects.Count) projects. GUID range from E501 to E$('{0:X3}' -f $guidN)"
