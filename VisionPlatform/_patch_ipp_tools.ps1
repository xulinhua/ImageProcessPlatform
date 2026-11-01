$ErrorActionPreference = 'Stop'
$root = 'C:\Git\other\ImageProcessPlatform\VisionPlatform\plugins\tools'

function Patch-CommonText([string]$t) {
    $t = $t -replace '#include "blob_tool.h"', '#include "BlobTool.h"'
    $t = $t -replace '#include "texture_tool.h"', '#include "TextureTool.h"'
    $t = $t -replace '#include "filter_tool.h"', '#include "FilterTool.h"'
    $t = $t -replace '#include "stitch_tool.h"', '#include "StitchTool.h"'
    $t = $t -replace '#include "rst_merge_tool.h"', '#include "RstMergeTool.h"'
    $t = $t -replace '#include "tool_base.h"', '#include "ToolBase.h"'
    $t = $t -replace '#include "alg_tool_base.h"', '#include "AlgToolBase.h"'
    $t = $t -replace '#include "tool_para_blob.hpp"', '#include "ToolParaBlob.hpp"'
    $t = $t -replace '#include "tool_para_texture.hpp"', '#include "ToolParaTexture.hpp"'
    $t = $t -replace '#include "tool_para_filter.hpp"', '#include "ToolParaFilter.hpp"'
    $t = $t -replace '#include "tool_para_stitch.hpp"', '#include "ToolParaStitch.hpp"'
    $t = $t -replace '#include "tool_para_rst_merge.hpp"', '#include "ToolParaRstMerge.hpp"'
    $t = $t -replace '#include "bond_version.h"', '#include "VisionVersion.h"'
    $t = $t -replace '#include "elapsed_timer.h"\r?\n', ''
    $t = $t -replace 'copy_instance_data_from', 'copyInstanceDataFrom'
    $t = $t -replace 'defaults_from\(', 'defaultsFrom('
    $t = $t -replace '@file blob_tool.cpp', '@file BlobTool.cpp'
    $t = $t -replace '@file texture_tool.cpp', '@file TextureTool.cpp'
    $t = $t -replace '@file filter_tool.cpp', '@file FilterTool.cpp'
    $t = $t -replace '@file stitch_tool.cpp', '@file StitchTool.cpp'
    $t = $t -replace '@file rst_merge_tool.cpp', '@file RstMergeTool.cpp'
    return $t
}

$shotBlock = @'
int Vision::{0}::shot(const Vision::ToolIn& in, const std::string& actionId, Vision::ToolOut* out, bool passImage)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    *out = Vision::ToolOut();
    attachAlg(in.toolDataDir);
    param_.load(in.param);
    if (param_.loadCode() != HS_OK) {
        out->ok = false;
        out->errCode = param_.loadCode();
        out->message = HsCommData::HsException(param_.loadCode()).errorMessage();
        return out->errCode;
    }
    Vision::AlgToolIn ain;
    fill_alg_in(in, &ain);
    const int code = run_attached_shot(in, actionId, &ain, passImage, out);
    log_result(out);
    if (!passImage && !Vision::toolTypeIsImageSource(type())) {
        out->io.images.clear();
    }
    return code;
}

void Vision::{0}::fill_alg_in(const Vision::ToolIn& in, Vision::AlgToolIn* ain) const
{
    build_alg_in(in, ain, {1});
    ain->param = &param_;
}

int Vision::{0}::check_param(std::string* detail) const
{
    if (param_.loadCode() != HS_OK) {
        if (detail != nullptr) {
            *detail = HsCommData::HsException(param_.loadCode()).errorMessage();
        }
        return param_.loadCode();
    }
    return HS_OK;
}

Vision::ToolOut Vision::{0}::execute(const Vision::ToolIn& in)
{
    Vision::ToolOut out;
    execute(in, &out);
    return out;
}
'@

function Inject-Shot([string]$path, [string]$className, [string]$roiPort, [bool]$passImage) {
    $t = Get-Content -Raw -Encoding UTF8 $path
    $t = Patch-CommonText $t
    if ($t -notmatch 'HsCommData/HsException') {
        $t = $t -replace '#include <HsCommData/HsError.h>', "#include <HsCommData/HsError.h>`n#include <HsCommData/HsException.h>"
    }
    $fillPort = if ($roiPort) { "`"$roiPort`"" } else { 'std::string()' }
    $block = ($shotBlock -f $className) -replace 'build_alg_in\(in, ain, \{1\}\)', "build_alg_in(in, ain, $fillPort)"
    if ($t -notmatch '::shot\(') {
        $t = $t -replace '(?s)int Vision::' + [regex]::Escape($className) + '::execute\(const Vision::ToolIn& in, Vision::ToolOut\* nRetOut\).*?return nRetOut->errCode;\s*\}', @"
int Vision::$className::execute(const Vision::ToolIn& in, Vision::ToolOut* out)
{
    return shot(in, std::string(), out, $($passImage.ToString().ToLower()));
}
"@
        $t = $t + "`n" + $block
    }
    # strip timer execute in action if present
    $t = $t -replace '(?s)int Vision::' + [regex]::Escape($className) + '::action\(.*?return out->errCode;\s*\}', @"
int Vision::$className::action(const std::string& id, const Vision::ToolIn& in, Vision::ToolOut* out)
{
    if (out == nullptr) {
        return HS_ERR_NULL;
    }
    return shot(in, id, out, $($passImage.ToString().ToLower()));
}
"@
    Set-Content -Encoding UTF8 -NoNewline $path $t
}

# Headers: add bridges from bond to tmpl style
function Patch-Header([string]$path, [string]$className) {
    if (-not (Test-Path $path)) { return }
    $t = Get-Content -Raw -Encoding UTF8 $path
    $t = $t -replace '#include "tool_base.h"', '#include "ToolBase.h"'
    $t = $t -replace '#include "tool_para_', '#include "ToolPara'
    $t = $t -replace 'ToolPara(\w+)\.hpp', 'ToolPara$1.hpp'
    if ($t -notmatch 'toolName\(\) const override \{ return tool_name') {
        $bridge = @"

private: // 基类桥
    std::string toolName() const override { return tool_name(); }
    std::string displayName() const override { return display_name(); }
    std::vector<VerNote> versionNotes() const override { return version_notes(); }
    ParamMap defaultParams() const override { return default_params(); }
    std::vector<ParamField> paramFields() const override { return cfg_param(); }
    ToolCaps capabilities() const override { return cfg_roi(); }
    const ToolParaBase* toolParam() const override { return tool_param(); }
    ToolOut execute(const ToolIn& in) override;
"@
        $t = $t -replace '\};\s*\}\s*#endif', ($bridge + "`n};`n}`n`n#endif")
    }
    if ($t -notmatch 'fill_alg_in') {
        $t = $t -replace '(private:\s*\r?\n\s*ToolPara\w+ param_;)', "`$1`n    void fill_alg_in(const ToolIn& in, AlgToolIn* ain) const;`n    int shot(const ToolIn& in, const std::string& actionId, ToolOut* out, bool passImage);`n    int check_param(std::string* detail) const override;"
    }
    Set-Content -Encoding UTF8 -NoNewline $path $t
}

Inject-Shot "$root\Plugin.Tool.Detect.Blob\BlobTool.cpp" 'BlobTool' 'BlobKey::roi' $false
Inject-Shot "$root\Plugin.Tool.Detect.Texture\TextureTool.cpp" 'TextureTool' 'roi' $false
Inject-Shot "$root\Plugin.Tool.Preprocess.Filter\FilterTool.cpp" 'FilterTool' 'FilterKey::roi' $true
Inject-Shot "$root\Plugin.Tool.Preprocess.Stitch\StitchTool.cpp" 'StitchTool' '' $true
Inject-Shot "$root\Plugin.Tool.Rst.Merge\RstMergeTool.cpp" 'RstMergeTool' 'roi' $false

Patch-Header "$root\Plugin.Tool.Detect.Texture\TextureTool.h" 'TextureTool'
Patch-Header "$root\Plugin.Tool.Preprocess.Filter\FilterTool.h" 'FilterTool'
Patch-Header "$root\Plugin.Tool.Preprocess.Stitch\StitchTool.h" 'StitchTool'
Patch-Header "$root\Plugin.Tool.Rst.Merge\RstMergeTool.h" 'RstMergeTool'

Write-Host 'patched alg tools'
