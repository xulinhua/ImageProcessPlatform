# Writes PCL alg execute bodies into bodies/pcl/*.cpp (overlay after scaffold)
$dir = Join-Path $PSScriptRoot 'bodies\pcl'
New-Item -ItemType Directory -Path $dir -Force | Out-Null
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$tpl = Join-Path $Root 'plugins\algos\Plugin.Alg.Pcl.Prepare3D.Downsample\PclDownsample.cpp'
$tplText = [IO.File]::ReadAllText($tpl)
$classes = @('Crop','Outlier','Passthrough','Normals','DepthToCloud','CloudToDepth','Template','Ndt','PoseRefine','FitCylinder','FitLine','FitCircle','Section','Cluster','Diff','RegionGrow')
foreach ($c in $classes) {
    $t = $tplText.Replace('Downsample', $c).Replace('VISION_DS_USE_PCL', "VISION_PCL_${c}_USE")
    $t = $t -replace 'return fail\(HS_ERR_OPERATOR, "未安装 PCL", out\);', 'return fail(HS_ERR_OPERATOR, "未安装 PCL", out);'
    $outPath = Join-Path $dir "$c.cpp"
    [IO.File]::WriteAllText($outPath, $t, [Text.UTF8Encoding]::new($false))
}
Write-Host "PCL body stubs from Downsample: $($classes.Count)"
