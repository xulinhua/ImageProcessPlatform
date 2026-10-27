# -*- coding: utf-8 -*-
import os
ROOT = r"d:\Git\VisionPlatformTest\VisionPlatform"
sln_path = os.path.join(ROOT, "VisionPlatform.sln")
text = open(sln_path, encoding="utf-8").read()

drop = [
    "B4E91A10-7C22-4F18-9A01-A0B1C2D3E404",
    "B4E91A10-7C22-4F18-9A01-A0B1C2D3E410",
    "B4E91A10-7C22-4F18-9A01-A0B1C2D3E433",
    "B4E91A10-7C22-4F18-9A01-A0B1C2D3E434",
]
lines = text.splitlines()
out = []
i = 0
while i < len(lines):
    line = lines[i]
    if any(g in line for g in drop) and (line.strip().startswith("Project(") or ".ActiveCfg" in line or ".Build.0" in line or line.strip().startswith("{B4E91A10")):
        if line.strip().startswith("Project("):
            # skip until EndProject
            while i < len(lines) and "EndProject" not in lines[i]:
                i += 1
        i += 1
        continue
    out.append(line)
    i += 1
text = "\n".join(out) + "\n"

rows = open(os.path.join(ROOT, "tools", "_projects.txt"), encoding="utf-8").read().splitlines()
proj_lines = []
cfg_lines = []
nest_lines = []
CLASS = "{A11A0001-0000-4000-8000-000000000002}"
IMG = "{A11A0001-0000-4000-8000-000000000003}"
for row in rows:
    name, rel, guid = row.split("|")
    proj_lines.append('Project("{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}") = "%s", "%s", "{%s}"' % (name, rel, guid))
    proj_lines.append("EndProject")
    for cfg, plat in (("Debug", "Win32"), ("Debug", "x64"), ("Release", "Win32"), ("Release", "x64")):
        cfg_lines.append("\t\t{%s}.%s|%s.ActiveCfg = %s|%s" % (guid, cfg, plat, cfg, plat))
        cfg_lines.append("\t\t{%s}.%s|%s.Build.0 = %s|%s" % (guid, cfg, plat, cfg, plat))
    parent = CLASS if name in ("VisionToolBase", "VisionAlgorBase") else IMG
    nest_lines.append("\t\t{%s} = %s" % (guid, parent))

text = text.replace("Global\n", "\n".join(proj_lines) + "\nGlobal\n", 1)
# insert configs before EndGlobalSection of ProjectConfigurationPlatforms
needle = "\tEndGlobalSection\n\tGlobalSection(SolutionProperties)"
text = text.replace(needle, "\n".join(cfg_lines) + "\n" + needle, 1)
needle2 = "\tEndGlobalSection\nEndGlobal"
text = text.replace(needle2, "\n".join(nest_lines) + "\n" + needle2, 1)
open(sln_path, "w", encoding="utf-8", newline="\n").write(text)
print("sln patched", text.count("Project("))
