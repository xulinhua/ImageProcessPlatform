# Copy template plugins and patch names. Dot-source _helpers.ps1 first.
$ErrorActionPreference = 'Stop'
. "$PSScriptRoot\_helpers.ps1"
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$guidN = 0x4FF
function Next-Guid { script:guidN++; return Guid-For $guidN }

function Copy-ToolFrom([string]$srcRel, [string]$dstFolder, [string]$oldClass, [string]$newClass) {
    $src = Join-Path $Root $srcRel
    $dst = Join-Path $Root "plugins\tools\$dstFolder"
    if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }
    Copy-Item $src $dst -Recurse
    Get-ChildItem $dst -Recurse -File | ForEach-Object {
        $c = [IO.File]::ReadAllText($_.FullName)
        $c = $c.Replace($oldClass, $newClass).Replace($oldClass.ToUpper(), $newClass.ToUpper())
        $oldProj = Split-Path $src -Leaf
        $c = $c.Replace($oldProj, $dstFolder)
        if ($_.Extension -eq '.vcxproj') { $c = $c -replace '<ProjectGuid>\{[^}]+\}</ProjectGuid>', "<ProjectGuid>{$(Next-Guid)}</ProjectGuid>" }
        if ($_.Name -like "*$oldClass*") {
            $newName = $_.Name.Replace($oldClass, $newClass)
            Rename-Item $_.FullName $newName -Force
            $_.FullName = Join-Path $_.DirectoryName $newName
        }
        [IO.File]::WriteAllText($_.FullName, $c, [Text.UTF8Encoding]::new($false))
    }
}

function Copy-AlgFrom([string]$srcRel, [string]$dstFolder, [string]$oldClass, [string]$newClass, [string]$lib) {
    $src = Join-Path $Root $srcRel
    $dst = Join-Path $Root "plugins\algos\$dstFolder"
    if (Test-Path $dst) { Remove-Item $dst -Recurse -Force }
    Copy-Item $src $dst -Recurse
    Get-ChildItem $dst -Recurse -File | ForEach-Object {
        $c = [IO.File]::ReadAllText($_.FullName)
        $c = $c.Replace($oldClass, $newClass)
        $oldProj = Split-Path $src -Leaf
        $c = $c.Replace($oldProj, $dstFolder)
        if ($_.Extension -eq '.vcxproj') { $c = $c -replace '<ProjectGuid>\{[^}]+\}</ProjectGuid>', "<ProjectGuid>{$(Next-Guid)}</ProjectGuid>" }
        if ($_.Name -like "*$oldClass*") {
            $newName = $_.Name.Replace($oldClass, $newClass)
            Rename-Item $_.FullName $newName -Force
        }
        [IO.File]::WriteAllText($_.FullName, $c, [Text.UTF8Encoding]::new($false))
    }
}

Write-Host "Scaffold copy uses GUID from E500 after manual patch"
