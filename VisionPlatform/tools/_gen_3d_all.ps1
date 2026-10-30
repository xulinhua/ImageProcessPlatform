# Generate remaining 3D tool/alg plugins and patch VisionPlatform.sln
$ErrorActionPreference = "Stop"
$vp = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not (Test-Path "$vp\VisionPlatform.sln")) { $vp = "C:\Git\other\ImageProcessPlatform\VisionPlatform" }
$script:guidN = 0xE500
$script:slnEntries = @()

function Next-Guid {
  $g = "{B4E91A10-7C22-4F18-9A01-A0B1C2D3{0:X4}}" -f $script:guidN
  $script:guidN++
  return $g
}

function Write-Utf8($path, $text) {
  $dir = Split-Path $path -Parent
  if (-not (Test-Path $dir)) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
  [System.IO.File]::WriteAllText($path, $text, [System.Text.UTF8Encoding]::new($false))
}

function Tool-Vcx($dir, $name, $guid, $cls) {
  Write-Utf8 (Join-Path $dir "$name.vcxproj") @"
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|Win32"><Configuration>Debug</Configuration><Platform>Win32</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Release|Win32"><Configuration>Release</Configuration><Platform>Win32</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>$guid</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>$($name -replace '\.','')</RootNamespace>
    <WindowsTargetPlatformVersion>8.1</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Debug|x64'" Label="Configuration"><ConfigurationType>DynamicLibrary</ConfigurationType><UseDebugLibraries>true</UseDebugLibraries><PlatformToolset>v143</PlatformToolset><LanguageStandard>stdcpp17</LanguageStandard><CharacterSet>Unicode</CharacterSet></PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|x64'" Label="Configuration"><ConfigurationType>DynamicLibrary</ConfigurationType><UseDebugLibraries>false</UseDebugLibraries><PlatformToolset>v143</PlatformToolset><LanguageStandard>stdcpp17</LanguageStandard><CharacterSet>Unicode</CharacterSet><WholeProgramOptimization>true</WholeProgramOptimization></PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Debug|Win32'" Label="Configuration"><ConfigurationType>DynamicLibrary</ConfigurationType><UseDebugLibraries>true</UseDebugLibraries><PlatformToolset>v143</PlatformToolset><LanguageStandard>stdcpp17</LanguageStandard><CharacterSet>Unicode</CharacterSet></PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|Win32'" Label="Configuration"><ConfigurationType>DynamicLibrary</ConfigurationType><UseDebugLibraries>false</UseDebugLibraries><PlatformToolset>v143</PlatformToolset><LanguageStandard>stdcpp17</LanguageStandard><CharacterSet>Unicode</CharacterSet></PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ImportGroup Label="PropertySheets">
    <Import Project="..\..\..\vision.props" />
  </ImportGroup>
  <PropertyGroup>
    <TargetName>$name</TargetName>
    <OutDir>`$(SolutionDir)bin\`$(Platform)\`$(Configuration)\plugins\tools\</OutDir>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile><AdditionalIncludeDirectories>`$(ProjectDir);`$(SolutionDir)VisionPluginAPI;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories></ClCompile>
    <Link>
      <AdditionalDependencies>VisionToolBase.lib;VisionAlgToolBase.lib;VisionCommon.lib;%(AdditionalDependencies)</AdditionalDependencies>
      <AdditionalLibraryDirectories>`$(SolutionDir)bin\`$(Platform)\`$(Configuration)\;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
    </Link>
  </ItemDefinitionGroup>
  <ItemGroup>
    <ClCompile Include="PluginEntry.cpp" /><ClCompile Include="$cls.cpp" />
  </ItemGroup>
  <ItemGroup><ClInclude Include="$cls.h" /></ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\..\..\VisionAlgToolBase\VisionAlgToolBase.vcxproj"><Project>{B4E91A10-7C22-4F18-9A01-A0B1C2D3E441}</Project></ProjectReference>
    <ProjectReference Include="..\..\..\VisionToolBase\VisionToolBase.vcxproj"><Project>{B4E91A10-7C22-4F18-9A01-A0B1C2D3E440}</Project></ProjectReference>
    <ProjectReference Include="..\..\..\VisionCommon\VisionCommon.vcxproj"><Project>{B4E91A10-7C22-4F18-9A01-A0B1C2D3E402}</Project></ProjectReference>
  </ItemGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
"@
}

function Alg-Vcx($dir, $name, $guid, $cls, $lib) {
  $imp = if ($lib -eq 'Pcl') {
    '    <Import Project="..\..\..\third_party\PCL\pcl.props" Condition="Exists(''..\..\..\third_party\PCL\pcl.props'')" />'
  } else {
    '    <Import Project="..\..\..\third_party\Open3D\open3d.props" Condition="Exists(''..\..\..\third_party\Open3D\open3d.props'')" />'
  }
  Write-Utf8 (Join-Path $dir "$name.vcxproj") @"
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|Win32"><Configuration>Debug</Configuration><Platform>Win32</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Release|Win32"><Configuration>Release</Configuration><Platform>Win32</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>$guid</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>$($name -replace '\.','')</RootNamespace>
    <WindowsTargetPlatformVersion>8.1</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Debug|x64'" Label="Configuration"><ConfigurationType>DynamicLibrary</ConfigurationType><UseDebugLibraries>true</UseDebugLibraries><PlatformToolset>v143</PlatformToolset><LanguageStandard>stdcpp17</LanguageStandard><CharacterSet>Unicode</CharacterSet></PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|x64'" Label="Configuration"><ConfigurationType>DynamicLibrary</ConfigurationType><UseDebugLibraries>false</UseDebugLibraries><PlatformToolset>v143</PlatformToolset><LanguageStandard>stdcpp17</LanguageStandard><CharacterSet>Unicode</CharacterSet><WholeProgramOptimization>true</WholeProgramOptimization></PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Debug|Win32'" Label="Configuration"><ConfigurationType>DynamicLibrary</ConfigurationType><UseDebugLibraries>true</UseDebugLibraries><PlatformToolset>v143</PlatformToolset><LanguageStandard>stdcpp17</LanguageStandard><CharacterSet>Unicode</CharacterSet></PropertyGroup>
  <PropertyGroup Condition="'`$(Configuration)|`$(Platform)'=='Release|Win32'" Label="Configuration"><ConfigurationType>DynamicLibrary</ConfigurationType><UseDebugLibraries>false</UseDebugLibraries><PlatformToolset>v143</PlatformToolset><LanguageStandard>stdcpp17</LanguageStandard><CharacterSet>Unicode</CharacterSet></PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ImportGroup Label="PropertySheets">
    <Import Project="..\..\..\vision.props" />
$imp
  </ImportGroup>
  <PropertyGroup>
    <TargetName>$name</TargetName>
    <OutDir>`$(SolutionDir)bin\`$(Platform)\`$(Configuration)\plugins\algos\</OutDir>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile><AdditionalIncludeDirectories>`$(ProjectDir);`$(SolutionDir)VisionPluginAPI;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories></ClCompile>
    <Link>
      <AdditionalDependencies>VisionAlgToolBase.lib;VisionToolBase.lib;VisionCommon.lib;%(AdditionalDependencies)</AdditionalDependencies>
      <AdditionalLibraryDirectories>`$(SolutionDir)bin\`$(Platform)\`$(Configuration)\;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
    </Link>
  </ItemDefinitionGroup>
  <ItemGroup>
    <ClCompile Include="PluginEntry.cpp" /><ClCompile Include="$cls.cpp" />
  </ItemGroup>
  <ItemGroup><ClInclude Include="$cls.h" /></ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\..\..\VisionAlgToolBase\VisionAlgToolBase.vcxproj"><Project>{B4E91A10-7C22-4F18-9A01-A0B1C2D3E441}</Project></ProjectReference>
    <ProjectReference Include="..\..\..\VisionCommon\VisionCommon.vcxproj"><Project>{B4E91A10-7C22-4F18-9A01-A0B1C2D3E402}</Project></ProjectReference>
  </ItemGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
"@
}

function Add-Sln($name, $rel, $guid, $parent) {
  $script:slnEntries += [pscustomobject]@{ Name=$name; Rel=$rel; Guid=$guid; Parent=$parent }
}

# Parent folders in sln
$TOOL3D = "{A11A0001-0000-4000-8000-000000000061}"
$PCLF = "{A11A0001-0000-4000-8000-000000000008}"
$O3DF = "{A11A0001-0000-4000-8000-00000000000B}"
$ACQF = "{A11A0001-0000-4000-8000-000000000011}"

Write-Host "Generating into $vp"
