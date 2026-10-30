# Tight-crop user icon (remove white margin) then export app.png / app.ico
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$srcPath = 'D:\360极速浏览器X下载\1789181756.png'
$outDir = 'd:\SVN\通用工具平台开发\VisionPlatformTest\VisionPlatform\icons\app'
$pngPath = Join-Path $outDir 'app.png'
$icoPath = Join-Path $outDir 'app.ico'
Copy-Item -Force $srcPath (Join-Path $outDir 'source-user.png')

function IsNearWhite([System.Drawing.Color]$c) {
  return ($c.R -gt 242 -and $c.G -gt 242 -and $c.B -gt 242)
}

$img = [System.Drawing.Bitmap]::FromFile($srcPath)
$w = $img.Width
$h = $img.Height

# Pass 1: center square by image height (icon is centered on landscape canvas)
$side0 = $h
$sx0 = [int](($w - $side0) / 2)
$sy0 = 0
$pass1 = $img.Clone((New-Object System.Drawing.Rectangle $sx0, $sy0, $side0, $side0), [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$img.Dispose()

# Pass 2: tight bounds excluding near-white
$pw = $pass1.Width
$ph = $pass1.Height
$minX = $pw; $minY = $ph; $maxX = -1; $maxY = -1
for ($y = 0; $y -lt $ph; $y += 2) {
  for ($x = 0; $x -lt $pw; $x += 2) {
    $c = $pass1.GetPixel($x, $y)
    if (-not (IsNearWhite $c)) {
      if ($x -lt $minX) { $minX = $x }
      if ($y -lt $minY) { $minY = $y }
      if ($x -gt $maxX) { $maxX = $x }
      if ($y -gt $maxY) { $maxY = $y }
    }
  }
}
if ($maxX -lt 0) { throw 'No icon content found' }

$pad = 2
$minX = [Math]::Max(0, $minX - $pad)
$minY = [Math]::Max(0, $minY - $pad)
$maxX = [Math]::Min($pw - 1, $maxX + $pad)
$maxY = [Math]::Min($ph - 1, $maxY + $pad)
$cw = $maxX - $minX + 1
$ch = $maxY - $minY + 1
$side = [int][Math]::Max($cw, $ch)
$cx = $minX + $cw / 2.0
$cy = $minY + $ch / 2.0
$sx = [int][Math]::Round($cx - $side / 2.0)
$sy = [int][Math]::Round($cy - $side / 2.0)
$sx = [Math]::Max(0, [Math]::Min($pw - $side, $sx))
$sy = [Math]::Max(0, [Math]::Min($ph - $side, $sy))
# if side exceeds remaining space, shrink
$side = [Math]::Min($side, [Math]::Min($pw - $sx, $ph - $sy))

Write-Host "Tight crop: $sx,$sy ${side}x${side} (from pass1 ${pw}x${ph})"
$crop = $pass1.Clone((New-Object System.Drawing.Rectangle $sx, $sy, $side, $side), [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$pass1.Dispose()

function MakeScaled([System.Drawing.Image]$src, [int]$sz) {
  $b = New-Object System.Drawing.Bitmap $sz, $sz
  $g = [System.Drawing.Graphics]::FromImage($b)
  $g.Clear([System.Drawing.Color]::Transparent)
  $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
  $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
  $g.DrawImage($src, 0, 0, $sz, $sz)
  $g.Dispose()
  return $b
}

$bmp256 = MakeScaled $crop 256
$bmp256.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp512 = MakeScaled $crop 512
$bmp512.Save((Join-Path $outDir 'app-512.png'), [System.Drawing.Imaging.ImageFormat]::Png)
$bmp512.Dispose()

$sizes = @(16, 24, 32, 48, 64, 128, 256)
$images = New-Object 'System.Collections.Generic.List[byte[]]'
foreach ($sz in $sizes) {
  $b = MakeScaled $crop $sz
  $tmp = New-Object IO.MemoryStream
  $b.Save($tmp, [System.Drawing.Imaging.ImageFormat]::Png)
  [void]$images.Add($tmp.ToArray())
  $tmp.Dispose(); $b.Dispose()
}
$crop.Dispose(); $bmp256.Dispose()

$ms = New-Object IO.MemoryStream
$bw = New-Object IO.BinaryWriter $ms
$bw.Write([uint16]0); $bw.Write([uint16]1); $bw.Write([uint16]$sizes.Count)
$offset = 6 + 16 * $sizes.Count
for ($i = 0; $i -lt $sizes.Count; $i++) {
  $sz = $sizes[$i]; $bytes = $images[$i]
  if ($sz -ge 256) { $bw.Write([byte]0); $bw.Write([byte]0) } else { $bw.Write([byte]$sz); $bw.Write([byte]$sz) }
  $bw.Write([byte]0); $bw.Write([byte]0)
  $bw.Write([uint16]1); $bw.Write([uint16]32)
  $bw.Write([uint32]$bytes.Length); $bw.Write([uint32]$offset)
  $offset += $bytes.Length
}
foreach ($bytes in $images) { $bw.Write($bytes) }
$bw.Flush()
[IO.File]::WriteAllBytes($icoPath, $ms.ToArray())
$bw.Dispose(); $ms.Dispose()

Get-Item $pngPath, $icoPath | ForEach-Object { "$($_.Name) $($_.Length)" }
