/**
 * @file install-remote.ps1
 * @brief 给中控台、被控端、中继加上防火墙入站规则。需要管理员。
 */
#Requires -RunAsAdministrator
$vp = Split-Path -Parent $PSScriptRoot
$root = Join-Path (Split-Path -Parent $vp) "VisionRemote"
if (-not (Test-Path "$root\bin\x64\Release\RemoteMaster.exe")) {
    $root = $vp
}
$bin = Join-Path $root "bin\x64\Release"
$rules = @(
    @{ Name = "RemoteMaster 19100"; Exe = "RemoteMaster.exe"; Port = 19100 },
    @{ Name = "RemoteClient 19102"; Exe = "RemoteClient.exe"; Port = 19102 },
    @{ Name = "RemoteClient HTTP"; Exe = "RemoteClient.exe"; Port = 19180 },
    @{ Name = "RemoteRelay 19200"; Exe = "RemoteRelay.exe"; Port = 19200 }
)
foreach ($rule in $rules) {
    $exe = Join-Path $bin $rule.Exe
    netsh advfirewall firewall delete rule name="$($rule.Name)" | Out-Null
    if (Test-Path $exe) {
        netsh advfirewall firewall add rule name="$($rule.Name)" dir=in action=allow program="$exe" protocol=TCP localport=$($rule.Port) profile=any
    }
}
Write-Host "firewall rules updated for $bin"
