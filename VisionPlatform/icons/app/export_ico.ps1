# Export app.png + multi-size app.ico from source PNG
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$src = 'C:\Users\70714\.cursor\projects\d-SVN\assets\vision-app-icon-v2.png'
$outDir = 'd:\SVN\通用工具平台开发\VisionPlatformTest\VisionPlatform\icons\app'
$pngPath = Join-Path $outDir 'app.png'
$icoPath = Join-Path $outDir 'app.ico'

$img = [System.Drawing.Image]::FromFile($src)

# 256 PNG for Qt window icon
$bmp256 = New-Object System.Drawing.Bitmap 256, 256
$g = [System.Drawing.Graphics]::FromImage($bmp256)
$g.Clear([System.Drawing.Color]::Transparent)
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
$g.DrawImage($img, 0, 0, 256, 256)
$g.Dispose()
$bmp256.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp256.Dispose()

$sizes = @(16, 24, 32, 48, 64, 128, 256)
$images = New-Object 'System.Collections.Generic.List[byte[]]'
foreach ($sz in $sizes) {
  $b = New-Object System.Drawing.Bitmap $sz, $sz
  $gg = [System.Drawing.Graphics]::FromImage($b)
  $gg.Clear([System.Drawing.Color]::Transparent)
  $gg.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $gg.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
  $gg.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $gg.DrawImage($img, 0, 0, $sz, $sz)
  $gg.Dispose()
  $tmp = New-Object IO.MemoryStream
  $b.Save($tmp, [System.Drawing.Imaging.ImageFormat]::Png)
  [void]$images.Add($tmp.ToArray())
  $tmp.Dispose()
  $b.Dispose()
}
$img.Dispose()

$ms = New-Object IO.MemoryStream
$bw = New-Object IO.BinaryWriter $ms
$bw.Write([uint16]0)
$bw.Write([uint16]1)
$bw.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
  $sz = $sizes[$i]
  $bytes = $images[$i]
  if ($sz -ge 256) { $bw.Write([byte]0); $bw.Write([byte]0) }
  else { $bw.Write([byte]$sz); $bw.Write([byte]$sz) }
  $bw.Write([byte]0)
  $bw.Write([byte]0)
  $bw.Write([uint16]1)
  $bw.Write([uint16]32)
  $bw.Write([uint32]$bytes.Length)
  $bw.Write([uint32]$offset)
  $offset += $bytes.Length
}
foreach ($bytes in $images) { $bw.Write($bytes) }
$bw.Flush()
[IO.File]::WriteAllBytes($icoPath, $ms.ToArray())
$bw.Dispose()
$ms.Dispose()

Write-Host "Wrote $pngPath"
Write-Host "Wrote $icoPath"
Get-Item $pngPath, $icoPath | ForEach-Object { Write-Host "$($_.Name) $($_.Length)" }
