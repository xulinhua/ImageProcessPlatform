$dir = Join-Path $PSScriptRoot 'bodies\open3d'
New-Item -ItemType Directory -Path $dir -Force | Out-Null
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$tpl = Join-Path $Root 'plugins\algos\Plugin.Alg.Open3d.Prepare3D.Downsample\Open3dDownsample.cpp'
$tplText = [IO.File]::ReadAllText($tpl)
$classes = @('Crop','Outlier','Passthrough','Normals','DepthToCloud','CloudToDepth','Template','Ndt','PoseRefine','FitCylinder','FitLine','FitCircle','Section','Cluster','Diff','RegionGrow')
foreach ($c in $classes) {
    $t = $tplText.Replace('Downsample', $c).Replace('Open3dDownsample', "Open3d$c").Replace('VISION_DS_USE_OPEN3D', "VISION_O3_${c}_USE")
    [IO.File]::WriteAllText((Join-Path $dir "$c.cpp"), $t, [Text.UTF8Encoding]::new($false))
}
Write-Host "Open3D body stubs: $($classes.Count)"
