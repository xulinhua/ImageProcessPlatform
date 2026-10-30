function New-GuidSuffix([int]$n) {
    return ('{0:X3}' -f $n).ToUpper().PadLeft(3,'0')
}
function Guid-For([int]$n) {
    return "{B4E91A10-7C22-4F18-9A01-A0B1C2D3E5$(New-GuidSuffix $n)}"
}
function Write-Utf8([string]$Path, [string]$Content) {
    $dir = Split-Path $Path -Parent
    if ($dir -and !(Test-Path $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }
    [System.IO.File]::WriteAllText($Path, $Content.Replace("`n", "`r`n"), [System.Text.UTF8Encoding]::new($false))
}
function Tool-Vcxproj([string]$Guid, [string]$TargetName, [string]$RootNs, [string[]]$Sources, [string[]]$Headers, [bool]$IsAlg, [string]$Kind) {
    $outDir = if ($IsAlg) { '$(SolutionDir)bin\$(Platform)\$(Configuration)\plugins\algos\' } else { '$(SolutionDir)bin\$(Platform)\$(Configuration)\plugins\tools\' }
    $props = ''
    if ($Kind -eq 'pcl') { $props = '    <Import Project="..\..\..\third_party\PCL\pcl.props" Condition="Exists(''..\..\..\third_party\PCL\pcl.props'')" />' }
    if ($Kind -eq 'open3d') { $props = '    <Import Project="..\..\..\third_party\Open3D\open3d.props" Condition="Exists(''..\..\..\third_party\Open3D\open3d.props'')" />' }
    $refs = if ($IsAlg) {
@'
    <ProjectReference Include="..\..\..\VisionAlgToolBase\VisionAlgToolBase.vcxproj"><Project>{B4E91A10-7C22-4F18-9A01-A0B1C2D3E441}</Project></ProjectReference>
    <ProjectReference Include="..\..\..\VisionCommon\VisionCommon.vcxproj"><Project>{B4E91A10-7C22-4F18-9A01-A0B1C2D3E402}</Project></ProjectReference>
'@
    } else {
@'
    <ProjectReference Include="..\..\..\VisionAlgToolBase\VisionAlgToolBase.vcxproj"><Project>{B4E91A10-7C22-4F18-9A01-A0B1C2D3E441}</Project></ProjectReference>
    <ProjectReference Include="..\..\..\VisionToolBase\VisionToolBase.vcxproj"><Project>{B4E91A10-7C22-4F18-9A01-A0B1C2D3E440}</Project></ProjectReference>
    <ProjectReference Include="..\..\..\VisionCommon\VisionCommon.vcxproj"><Project>{B4E91A10-7C22-4F18-9A01-A0B1C2D3E402}</Project></ProjectReference>
'@
    }
    $cl = ($Sources | ForEach-Object { "    <ClCompile Include=`"$_`" />" }) -join "`r`n"
    $hi = ($Headers | ForEach-Object { "    <ClInclude Include=`"$_`" />" }) -join "`r`n"
    $depth = if ($IsAlg) { '..\..\..\..' } else { '..\..\..' }
    @"
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|Win32"><Configuration>Debug</Configuration><Platform>Win32</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Release|Win32"><Configuration>Release</Configuration><Platform>Win32</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>{$Guid}</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>$RootNs</RootNamespace>
    <WindowsTargetPlatformVersion>8.1</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Debug|Win32'" Label="Configuration"><ConfigurationType>DynamicLibrary</ConfigurationType><UseDebugLibraries>true</UseDebugLibraries><PlatformToolset>v143</PlatformToolset><LanguageStandard>stdcpp17</LanguageStandard><CharacterSet>Unicode</CharacterSet></PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|Win32'" Label="Configuration"><ConfigurationType>DynamicLibrary</ConfigurationType><UseDebugLibraries>false</UseDebugLibraries><PlatformToolset>v143</PlatformToolset><LanguageStandard>stdcpp17</LanguageStandard><CharacterSet>Unicode</CharacterSet><WholeProgramOptimization>true</WholeProgramOptimization></PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Debug|x64'" Label="Configuration"><ConfigurationType>DynamicLibrary</ConfigurationType><UseDebugLibraries>true</UseDebugLibraries><PlatformToolset>v143</PlatformToolset><LanguageStandard>stdcpp17</LanguageStandard><CharacterSet>Unicode</CharacterSet><UseOfMfc>false</UseOfMfc></PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|x64'" Label="Configuration"><ConfigurationType>DynamicLibrary</ConfigurationType><UseDebugLibraries>false</UseDebugLibraries><PlatformToolset>v143</PlatformToolset><LanguageStandard>stdcpp17</LanguageStandard><CharacterSet>Unicode</CharacterSet><WholeProgramOptimization>true</WholeProgramOptimization></PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings" />
  <ImportGroup Label="PropertySheets">
    <Import Project="`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props" Condition="exists('`$(UserRootDir)\Microsoft.Cpp.`$(Platform).user.props')" Label="LocalAppDataPlatform" />
    <Import Project="$depth\vision.props" />
$props
  </ImportGroup>
  <PropertyGroup>
    <TargetName>$TargetName</TargetName>
    <OutDir>$outDir</OutDir>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <AdditionalIncludeDirectories>`$(ProjectDir);`$(SolutionDir)VisionPluginAPI;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
    <Link>
      <AdditionalDependencies>VisionAlgToolBase.lib;VisionToolBase.lib;VisionCommon.lib;%(AdditionalDependencies)</AdditionalDependencies>
      <AdditionalLibraryDirectories>`$(SolutionDir)bin/`$(Platform)/`$(Configuration)/;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
    </Link>
  </ItemDefinitionGroup>
  <ItemGroup>
$cl
  </ItemGroup>
  <ItemGroup>
$hi
  </ItemGroup>
  <ItemGroup>
$refs
  </ItemGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
"@
}
