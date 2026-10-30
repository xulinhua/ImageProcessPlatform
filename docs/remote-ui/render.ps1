$edge = "C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
$dir = $PSScriptRoot
$pages = @(
    @("control-main", 1640, 980),
    @("session", 1640, 980),
    @("file-transfer", 1280, 820),
    @("device-dialogs", 1270, 670),
    @("settings", 1000, 700),
    @("audit", 1320, 760),
    @("controlled-main", 1120, 860),
    @("confirm", 1440, 810)
)
foreach ($p in $pages) {
    $name = $p[0]
    $url = "file:///" + ($dir -replace '\\', '/') + "/$name.html"
    & $edge --headless=new --disable-gpu --hide-scrollbars --force-device-scale-factor=1.5 `
        "--window-size=$($p[1]),$($p[2])" "--screenshot=$dir\$name.png" $url 2>&1 | Out-Null
    Write-Output "$name.png"
}

# Write the PNGs back into every doc that marks an image with <!-- remote-ui/NAME.png -->
$pattern = '<!-- remote-ui/([\w-]+)\.png -->(\r?\n)!\[([^\]]*)\]\([^)]*\)'
foreach ($md in Get-ChildItem (Split-Path $dir -Parent) -Filter *.md) {
    $text = [IO.File]::ReadAllText($md.FullName)
    if ($text -notmatch '<!-- remote-ui/') { continue }
    $out = [regex]::Replace($text, $pattern, {
        param($m)
        $png = Join-Path $dir ($m.Groups[1].Value + ".png")
        $b64 = [Convert]::ToBase64String([IO.File]::ReadAllBytes($png))
        "<!-- remote-ui/$($m.Groups[1].Value).png -->$($m.Groups[2].Value)![$($m.Groups[3].Value)](data:image/png;base64,$b64)"
    })
    [IO.File]::WriteAllText($md.FullName, $out, (New-Object Text.UTF8Encoding($false)))
    Write-Output "embedded into $($md.Name)"
}
