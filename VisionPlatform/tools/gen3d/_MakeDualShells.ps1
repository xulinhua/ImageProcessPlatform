function Write-DualShell([string]$Class, [string]$ToolName, [string]$Display, [string]$TypeEnum, [string]$Desc, [string]$ErrMsg, [string[]]$ParamLines, [string[]]$PortLines) {
    $out = Join-Path $PSScriptRoot "shell\dual\${Class}Tool.cpp"
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine("#include `"${Class}Tool.h`"")
    [void]$sb.AppendLine("#include `"AlgToolBase.h`"")
    [void]$sb.AppendLine("#include `"VisionVersion.h`"")
    [void]$sb.AppendLine("#include <HsCommData/HsError.h>")
    [void]$sb.AppendLine("")
    [void]$sb.AppendLine("namespace Vision {")
    [void]$sb.AppendLine("namespace {")
    [void]$sb.AppendLine("const VerNoteRaw kVerNotes[] = { { `"1.0.0.0`", `"$Display`" } };")
    [void]$sb.AppendLine("}")
    [void]$sb.AppendLine("std::string ${Class}Tool::toolName() const { return `"$ToolName`"; }")
    [void]$sb.AppendLine("ToolType ${Class}Tool::type() const { return ToolType::$TypeEnum; }")
    [void]$sb.AppendLine("std::string ${Class}Tool::displayName() const { return `"$Display`"; }")
    [void]$sb.AppendLine("std::string ${Class}Tool::version() const { return `"1.0.0.0`"; }")
    [void]$sb.AppendLine("std::vector<VerNote> ${Class}Tool::versionNotes() const { return verNotesFrom(kVerNotes, 1); }")
    [void]$sb.AppendLine("std::string ${Class}Tool::description() const { return `"$Desc`"; }")
    [void]$sb.AppendLine("std::vector<ParamField> ${Class}Tool::paramFields() const")
    [void]$sb.AppendLine("{")
    [void]$sb.AppendLine("    std::vector<ParamField> f;")
    foreach ($line in $ParamLines) { [void]$sb.AppendLine("    $line") }
    [void]$sb.AppendLine("    return f;")
    [void]$sb.AppendLine("}")
    [void]$sb.AppendLine("ParamMap ${Class}Tool::defaultParams() const { return defaultsFrom(paramFields()); }")
    [void]$sb.AppendLine("std::vector<PortDesc> ${Class}Tool::ports() const")
    [void]$sb.AppendLine("{")
    [void]$sb.AppendLine("    std::vector<PortDesc> p;")
    foreach ($line in $PortLines) { [void]$sb.AppendLine("    $line") }
    [void]$sb.AppendLine("    return p;")
    [void]$sb.AppendLine("}")
    [void]$sb.AppendLine("ToolOut ${Class}Tool::execute(const ToolIn& in)")
    [void]$sb.AppendLine("{")
    [void]$sb.AppendLine("    ToolIn shot = in;")
    [void]$sb.AppendLine("    if (shot.algLib != AlgLibPcl && shot.algLib != AlgLibOpen3d) { shot.algLib = AlgLibPcl; }")
    [void]$sb.AppendLine("    ToolOut out = runAlgToolBase(toolName(), shot, false);")
    [void]$sb.AppendLine("    if (out.errCode == HS_ERR_NO_ALG) { out.message = `"$ErrMsg`"; }")
    [void]$sb.AppendLine("    return out;")
    [void]$sb.AppendLine("}")
    [void]$sb.AppendLine("ToolBase* ${Class}Tool::clone() const")
    [void]$sb.AppendLine("{")
    [void]$sb.AppendLine("    ${Class}Tool* p = new ${Class}Tool();")
    [void]$sb.AppendLine("    p->copyInstanceDataFrom(*this);")
    [void]$sb.AppendLine("    return p;")
    [void]$sb.AppendLine("}")
    [void]$sb.AppendLine("}")
    [IO.File]::WriteAllText($out, $sb.ToString(), (New-Object System.Text.UTF8Encoding $false))
}

$inOut = @('p.push_back(PortDesc(PortDir::In, PortData::Cloud));', 'p.push_back(PortDesc(PortDir::Out, PortData::Cloud));')
$fitP = @('p.push_back(PortDesc(PortDir::In, PortData::Cloud));', 'p.push_back(PortDesc(PortDir::Out, PortData::Shape3));', 'PortDesc inlier(PortDir::Out, PortData::Cloud); inlier.slot.name = "inlier"; p.push_back(inlier);', 'p.push_back(PortDesc(PortDir::Out, PortData::Tuple));', 'p.push_back(PortDesc(PortDir::Out, PortData::Logic));')
$icpP = @('p.push_back(PortDesc(PortDir::In, PortData::Cloud));', 'PortDesc tmpl(PortDir::In, PortData::Cloud); tmpl.slot.name = "template"; p.push_back(tmpl);', 'p.push_back(PortDesc(PortDir::Out, PortData::Pose3));', 'p.push_back(PortDesc(PortDir::Out, PortData::Tuple));', 'p.push_back(PortDesc(PortDir::Out, PortData::Logic));')

Write-DualShell Crop prepare3d.crop '\u70b9\u4e91\u88c1\u526a' Prepare3D '\u8f74\u5bf9\u9f50\u76d2\u88c1\u526a\u70b9\u4e91' '\u672a\u767b\u8bb0\u70b9\u4e91\u7b97\u6cd5' @(
    'f.push_back(paramDouble("minX", "\u6700\u5c0fX", -1000.0, true, 0.0, false, 0.0));',
    'f.push_back(paramDouble("minY", "\u6700\u5c0fY", -1000.0, true, 0.0, false, 0.0));',
    'f.push_back(paramDouble("minZ", "\u6700\u5c0fZ", -1000.0, true, 0.0, false, 0.0));',
    'f.push_back(paramDouble("maxX", "\u6700\u5927X", 1000.0, true, 0.0, false, 0.0));',
    'f.push_back(paramDouble("maxY", "\u6700\u5927Y", 1000.0, true, 0.0, false, 0.0));',
    'f.push_back(paramDouble("maxZ", "\u6700\u5927Z", 1000.0, true, 0.0, false, 0.0));'
) $inOut

Write-Host 'Dual shells OK (Crop sample; run Fix-AllDualShells for full set)'
