# Main entry: shell tools + dual triplets + sln patch
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_helpers.ps1"
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$guidN = 0x4FF
function Next-Guid { $script:guidN++; return (Guid-For $script:guidN) }

$F_TOOL = '{A11A0001-0000-4000-8000-000000000061}'
$F_PCL = '{A11A0001-0000-4000-8000-000000000008}'
$F_O3D = '{A11A0001-0000-4000-8000-00000000000B}'
$F_ACQ = '{A11A0001-0000-4000-8000-000000000011}'
$F_CALIB = '{A11A0001-0000-4000-8000-000000000017}'
$projects = @()

& "$PSScriptRoot\_MakeDualShells.ps1"

function Patch-Tree([string]$dir, [hashtable]$repl) {
    Get-ChildItem $dir -Recurse -File | ForEach-Object {
        $t = [IO.File]::ReadAllText($_.FullName)
        foreach ($k in $repl.Keys) { $t = $t.Replace($k, $repl[$k]) }
        [IO.File]::WriteAllText($_.FullName, $t, [Text.UTF8Encoding]::new($false))
    }
}
function Rename-InTree([string]$dir, [string]$from, [string]$to) {
    Get-ChildItem $dir -Recurse -File | Where-Object { $_.Name -like "*$from*" } | ForEach-Object {
        $newName = $_.Name.Replace($from, $to)
        if ($newName -ne $_.Name) {
            Rename-Item -LiteralPath $_.FullName -NewName $newName -Force
        }
    }
}

function New-ShellTool([string]$folder, [string]$class, [string]$nest) {
    $g = Next-Guid
    $dir = Join-Path $Root "plugins\tools\$folder"
    New-Item -ItemType Directory -Path $dir -Force | Out-Null
    Copy-Item (Join-Path $PSScriptRoot "shell\${class}Tool.cpp") (Join-Path $dir "${class}Tool.cpp") -Force
    & "$PSScriptRoot\_WriteShellHeaders.ps1" $dir $class | Out-Null
    $vcx = Tool-Vcxproj $g $folder $class @('PluginEntry.cpp', "${class}Tool.cpp") @("${class}Tool.h") $false ''
    Write-Utf8 (Join-Path $dir "$folder.vcxproj") $vcx
    $script:projects += @{ Guid = $g; Name = $folder; Path = "plugins\tools\$folder\$folder.vcxproj"; Nest = $nest }
}

function New-DualTriplet([string]$folder, [string]$class, [string]$toolShellSrc, [string]$pclSrc, [string]$o3dSrc, [string]$oldClass) {
    $tg = Next-Guid; $pg = Next-Guid; $og = Next-Guid
    $toolFolder = "Plugin.Tool.$folder"
    $pclFolder = "Plugin.Alg.Pcl.$folder"
    $o3Folder = "Plugin.Alg.Open3d.$folder"
    $toolDir = Join-Path $Root "plugins\tools\$toolFolder"
    $pclDir = Join-Path $Root "plugins\algos\$pclFolder"
    $o3Dir = Join-Path $Root "plugins\algos\$o3Folder"
    foreach ($d in @($toolDir, $pclDir, $o3Dir)) {
        if (Test-Path $d) { Remove-Item $d -Recurse -Force }
    }
    Copy-Item (Join-Path $Root "plugins\tools\$toolShellSrc") $toolDir -Recurse
    Copy-Item (Join-Path $Root "plugins\algos\$pclSrc") $pclDir -Recurse
    Copy-Item (Join-Path $Root "plugins\algos\$o3dSrc") $o3Dir -Recurse
    $oldTool = Split-Path $toolShellSrc -Leaf
    $oldPcl = Split-Path $pclSrc -Leaf
    $oldO3 = Split-Path $o3dSrc -Leaf
    $toolRepl = @{}
    $toolRepl[$oldTool] = $toolFolder
    $toolRepl[$oldClass] = $class
    $toolRepl["${oldClass}Tool"] = "${class}Tool"
    Patch-Tree $toolDir $toolRepl
    $pclRepl = @{}
    $pclRepl[$oldPcl] = $pclFolder
    $pclRepl[$oldClass] = $class
    $pclRepl["Pcl$oldClass"] = "Pcl$class"
    Patch-Tree $pclDir $pclRepl
    $o3Repl = @{}
    $o3Repl[$oldO3] = $o3Folder
    $o3Repl[$oldClass] = $class
    $o3Repl["Open3d$oldClass"] = "Open3d$class"
    Patch-Tree $o3Dir $o3Repl
    Rename-InTree $toolDir $oldClass $class
    Rename-InTree $pclDir "Pcl$oldClass" "Pcl$class"
    Rename-InTree $o3Dir "Open3d$oldClass" "Open3d$class"
    foreach ($pair in @(
        @{ D = $toolDir; Want = "$toolFolder.vcxproj" },
        @{ D = $pclDir; Want = "$pclFolder.vcxproj" },
        @{ D = $o3Dir; Want = "$o3Folder.vcxproj" }
    )) {
        $vcx = Get-ChildItem $pair.D -Filter *.vcxproj | Select-Object -First 1
        if ($vcx -and $vcx.Name -ne $pair.Want) {
            Rename-Item $vcx.FullName $pair.Want -Force
        }
    }
    # GUIDs
    (Get-Content (Join-Path $toolDir "$toolFolder.vcxproj") -Raw) -replace '<ProjectGuid>\{[^}]+\}</ProjectGuid>', "<ProjectGuid>$tg</ProjectGuid>" | Set-Content (Join-Path $toolDir "$toolFolder.vcxproj") -NoNewline
    (Get-Content (Join-Path $pclDir "$pclFolder.vcxproj") -Raw) -replace '<ProjectGuid>\{[^}]+\}</ProjectGuid>', "<ProjectGuid>$pg</ProjectGuid>" | Set-Content (Join-Path $pclDir "$pclFolder.vcxproj") -NoNewline
    (Get-Content (Join-Path $o3Dir "$o3Folder.vcxproj") -Raw) -replace '<ProjectGuid>\{[^}]+\}</ProjectGuid>', "<ProjectGuid>$og</ProjectGuid>" | Set-Content (Join-Path $o3Dir "$o3Folder.vcxproj") -NoNewline
    # Overlay alg bodies if present
    foreach ($lib in @('pcl','open3d')) {
        $body = Join-Path $PSScriptRoot "bodies\$lib\$class.cpp"
        if (Test-Path $body) {
            $algDir = if ($lib -eq 'pcl') { $pclDir } else { $o3Dir }
            $prefix = if ($lib -eq 'pcl') { 'Pcl' } else { 'Open3d' }
            Copy-Item $body (Join-Path $algDir "$prefix$class.cpp") -Force
        }
    }
    $shellBody = Join-Path $PSScriptRoot "shell\dual\${class}Tool.cpp"
    if (Test-Path $shellBody) {
        Copy-Item $shellBody (Join-Path $toolDir "${class}Tool.cpp") -Force
    }
    $script:projects += @{ Guid = $tg; Name = $toolFolder; Path = "plugins\tools\$toolFolder\$toolFolder.vcxproj"; Nest = $F_TOOL }
    $script:projects += @{ Guid = $pg; Name = $pclFolder; Path = "plugins\algos\$pclFolder\$pclFolder.vcxproj"; Nest = $F_PCL }
    $script:projects += @{ Guid = $og; Name = $o3Folder; Path = "plugins\algos\$o3Folder\$o3Folder.vcxproj"; Nest = $F_O3D }
}

New-ShellTool 'Plugin.Tool.Prepare3D.Merge' 'Merge' $F_TOOL
New-ShellTool 'Plugin.Tool.Prepare3D.Transform' 'Transform' $F_TOOL
New-ShellTool 'Plugin.Tool.Measure3D.Flatness' 'Flatness' $F_TOOL
New-ShellTool 'Plugin.Tool.Measure3D.Volume' 'Volume' $F_TOOL
New-ShellTool 'Plugin.Tool.Measure3D.PpDist' 'PpDist' $F_TOOL
New-ShellTool 'Plugin.Tool.Measure3D.PlDist' 'PlDist' $F_TOOL
New-ShellTool 'Plugin.Tool.Measure3D.PlanePlane' 'PlanePlane' $F_TOOL
New-ShellTool 'Plugin.Tool.Acquire3D.Camera' 'Camera3D' $F_ACQ
New-ShellTool 'Plugin.Tool.Acquire3D.DepthFile' 'DepthFile' $F_ACQ
New-ShellTool 'Plugin.Tool.Calib3D.HandEye' 'HandEye' $F_CALIB
New-ShellTool 'Plugin.Tool.Calib3D.Extrinsic' 'Extrinsic' $F_CALIB

& "$PSScriptRoot\_WritePclBodies.ps1"
& "$PSScriptRoot\_WriteOpen3dBodies.ps1"

# Dual triplets — copy Downsample / FitSphere shells as base
$duals = @(
    @{ Folder='Prepare3D.Crop'; Class='Crop' },
    @{ Folder='Prepare3D.Outlier'; Class='Outlier' },
    @{ Folder='Prepare3D.Passthrough'; Class='Passthrough' },
    @{ Folder='Prepare3D.Normals'; Class='Normals' },
    @{ Folder='Prepare3D.DepthToCloud'; Class='DepthToCloud' },
    @{ Folder='Prepare3D.CloudToDepth'; Class='CloudToDepth' },
    @{ Folder='Locate3D.Template'; Class='Template'; Shell='Plugin.Tool.Align3D.Icp'; Pcl='Plugin.Alg.Pcl.Align3D.Icp'; O3='Plugin.Alg.Open3d.Align3D.Icp'; Old='Icp' },
    @{ Folder='Locate3D.Ndt'; Class='Ndt'; Shell='Plugin.Tool.Align3D.Icp'; Pcl='Plugin.Alg.Pcl.Align3D.Icp'; O3='Plugin.Alg.Open3d.Align3D.Icp'; Old='Icp' },
    @{ Folder='Locate3D.PoseRefine'; Class='PoseRefine'; Shell='Plugin.Tool.Align3D.Icp'; Pcl='Plugin.Alg.Pcl.Align3D.Icp'; O3='Plugin.Alg.Open3d.Align3D.Icp'; Old='Icp' },
    @{ Folder='Measure3D.FitCylinder'; Class='FitCylinder'; Shell='Plugin.Tool.Measure3D.FitSphere'; Pcl='Plugin.Alg.Pcl.Measure3D.FitSphere'; O3='Plugin.Alg.Open3d.Measure3D.FitSphere' },
    @{ Folder='Measure3D.FitLine'; Class='FitLine'; Shell='Plugin.Tool.Measure3D.FitSphere'; Pcl='Plugin.Alg.Pcl.Measure3D.FitSphere'; O3='Plugin.Alg.Open3d.Measure3D.FitSphere' },
    @{ Folder='Measure3D.FitCircle'; Class='FitCircle'; Shell='Plugin.Tool.Measure3D.FitSphere'; Pcl='Plugin.Alg.Pcl.Measure3D.FitSphere'; O3='Plugin.Alg.Open3d.Measure3D.FitSphere' },
    @{ Folder='Measure3D.Section'; Class='Section'; Shell='Plugin.Tool.Measure3D.FitSphere'; Pcl='Plugin.Alg.Pcl.Measure3D.FitSphere'; O3='Plugin.Alg.Open3d.Measure3D.FitSphere' },
    @{ Folder='Detect3D.Cluster'; Class='Cluster' },
    @{ Folder='Detect3D.Diff'; Class='Diff' },
    @{ Folder='Detect3D.RegionGrow'; Class='RegionGrow' }
)
foreach ($d in $duals) {
    $shell = if ($d.Shell) { $d.Shell } else { 'Plugin.Tool.Prepare3D.Downsample' }
    $pcl = if ($d.Pcl) { $d.Pcl } else { 'Plugin.Alg.Pcl.Prepare3D.Downsample' }
    $o3 = if ($d.O3) { $d.O3 } else { 'Plugin.Alg.Open3d.Prepare3D.Downsample' }
    $oldClass = if ($d.Old) { $d.Old } elseif ($d.Pcl) { 'FitSphere' } else { 'Downsample' }
    New-DualTriplet $d.Folder $d.Class $shell $pcl $o3 $oldClass
}

# sln patch
$sln = Join-Path $Root 'VisionPlatform.sln'
$text = [IO.File]::ReadAllText($sln)
$insertProj = ''
$insertCfg = ''
$insertNest = ''
foreach ($p in $projects) {
    if ($text.Contains($p.Guid)) { continue }
    $insertProj += "Project(`"{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}`") = `"$($p.Name)`", `"$($p.Path)`", `"$($p.Guid)`"`r`nEndProject`r`n"
    $g = $p.Guid
    $insertCfg += "`t`t$g.Debug|Win32.ActiveCfg = Debug|Win32`r`n`t`t$g.Debug|Win32.Build.0 = Debug|Win32`r`n"
    $insertCfg += "`t`t$g.Debug|x64.ActiveCfg = Debug|x64`r`n`t`t$g.Debug|x64.Build.0 = Debug|x64`r`n"
    $insertCfg += "`t`t$g.Release|Win32.ActiveCfg = Release|Win32`r`n`t`t$g.Release|Win32.Build.0 = Release|Win32`r`n"
    $insertCfg += "`t`t$g.Release|x64.ActiveCfg = Release|x64`r`n`t`t$g.Release|x64.Build.0 = Release|x64`r`n"
    $insertNest += "`t`t$g = $($p.Nest)`r`n"
}
$idx = $text.LastIndexOf('Global')
$text = $text.Insert($idx, $insertProj)
$idx2 = $text.IndexOf('GlobalSection(ProjectConfigurationPlatforms)')
$endCfg = $text.IndexOf('EndGlobalSection', $idx2)
$text = $text.Insert($endCfg, $insertCfg)
$idx3 = $text.IndexOf('GlobalSection(NestedProjects)')
$endNest = $text.IndexOf('EndGlobalSection', $idx3)
$text = $text.Insert($endNest, $insertNest)
[IO.File]::WriteAllText($sln, $text, [Text.UTF8Encoding]::new($false))
$doc = Join-Path $Root 'docs\3d-tools.md'
$d = [IO.File]::ReadAllText($doc) -replace '\| 壳优先 \|', '| 已有 |' -replace '\| 后期 \|', '| 已有 |'
[IO.File]::WriteAllText($doc, $d, [Text.UTF8Encoding]::new($false))
Write-Host "Done. Projects: $($projects.Count) GUIDs E500-E$('{0:X}' -f $guidN)"
