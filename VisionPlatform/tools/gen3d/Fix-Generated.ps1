# Re-apply dual shells + alg toolName map after Build-Remaining3d.ps1
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
. "$PSScriptRoot\_MakeDualShells-Full.ps1"

$map = @{
    Crop='prepare3d.crop'; Outlier='prepare3d.outlier'; Passthrough='prepare3d.passthrough'; Normals='prepare3d.normals'
    DepthToCloud='prepare3d.depth_to_cloud'; CloudToDepth='prepare3d.cloud_to_depth'
    Template='locate3d.template'; Ndt='locate3d.ndt'; PoseRefine='locate3d.pose_refine'
    FitCylinder='measure3d.fit_cylinder'; FitLine='measure3d.fit_line'; FitCircle='measure3d.fit_circle'; Section='measure3d.section'
    Cluster='detect3d.cluster'; Diff='detect3d.diff'; RegionGrow='detect3d.region_grow'
}
foreach ($kv in $map.GetEnumerator()) {
    $class = $kv.Key
    $toolName = $kv.Value
    $shell = Join-Path $PSScriptRoot "shell\dual\${class}Tool.cpp"
    if (!(Test-Path $shell)) { Write-Warning "missing $shell"; continue }
    Get-ChildItem (Join-Path $Root 'plugins\tools') -Directory -Filter "Plugin.Tool.*.$class" -ErrorAction SilentlyContinue | ForEach-Object {
        Copy-Item $shell (Join-Path $_.FullName "${class}Tool.cpp") -Force
    }
    foreach ($lib in @('Pcl','Open3d')) {
        Get-ChildItem (Join-Path $Root 'plugins\algos') -Directory -Filter "Plugin.Alg.$lib.*.$class" -ErrorAction SilentlyContinue | ForEach-Object {
            $h = Get-ChildItem $_.FullName -Filter "${lib}${class}.h" | Select-Object -First 1
            if ($h) {
                $t = [IO.File]::ReadAllText($h.FullName)
                $t = [regex]::Replace($t, 'return "([^"]+)";', "return `"$toolName`";", 1)
                [IO.File]::WriteAllText($h.FullName, $t, [Text.UTF8Encoding]::new($false))
            }
        }
    }
}
Write-Host 'Fix-Generated done'
