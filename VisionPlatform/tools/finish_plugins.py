# -*- coding: utf-8 -*-
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_plugins as g

ROOT = g.ROOT

# image plugins
print("imported", len(g.IMAGES))

CUST = [
    ("Tool.Result.Merge", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E458", False),
    ("Tool.Logic.Judge", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E459", False),
    ("Tool.Comm.Plc", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E45A", False),
    ("Tool.Camera.File", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E45B", True),
    ("Tool.Camera.Device", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E45C", True),
]
projects = []
for dll, guid, tool_name, typ, title, desc, pass_image, caps, fields in g.IMAGES:
    projects.append((dll, "plugins\\tools\\%s\\%s.vcxproj" % (dll, dll), guid))

for dll, guid, cam in CUST:
    refs = g.tool_refs()
    lib = "VisionToolBase.lib;VisionCommon.lib;"
    inc = "$(SolutionDir)VisionPluginAPI;"
    if cam:
        refs.append(("..\\..\\VisionGrabFile\\VisionGrabFile.vcxproj", g.GRAB))
        lib += "VisionGrabFile.lib;"
        inc += "$(SolutionDir)VisionGrabFile;$(SolutionDir)VisionGrabFile\\grabber;"
    vcx = g.plugin_vcx(guid, dll.replace(".", ""), dll, "plugins\\tools\\",
                       ["PluginEntry.cpp"], [], refs, False, "", inc, lib)
    g.write(os.path.join(ROOT, "plugins", "tools", dll, dll + ".vcxproj"), vcx)
    projects.append((dll, "plugins\\tools\\%s\\%s.vcxproj" % (dll, dll), guid))

ALGOS = [
    ("Algo.Halcon.Enhance", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E460", "HalconEnhance"),
    ("Algo.Halcon.Smooth", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E461", "HalconSmooth"),
    ("Algo.Halcon.Template", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E462", "HalconTemplate"),
    ("Algo.Halcon.FindLine", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E463", "HalconFindLine"),
    ("Algo.Halcon.FindCircle", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E464", "HalconFindCircle"),
    ("Algo.Halcon.Caliper", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E465", "HalconCaliper"),
    ("Algo.Halcon.Blob", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E466", "HalconBlob"),
    ("Algo.Halcon.Texture", "B4E91A10-7C22-4F18-9A01-A0B1C2D3E467", "HalconTexture"),
]
for dll, guid, cls in ALGOS:
    folder = os.path.join(ROOT, "plugins", "algos", dll)
    cpp = '''#include "PluginEntry.h"
#include "%s.h"

VISION_PLUGIN_EXPORT int VisionPlugin_apiVersion()
{
    return 1;
}

VISION_PLUGIN_EXPORT void VisionPlugin_register(Vision::IPluginHost* host)
{
    if (host) {
        host->addAlgor(new Vision::%s());
    }
}
''' % (cls, cls)
    g.write(os.path.join(folder, "PluginEntry.cpp"), cpp)
    refs = [
        ("..\\..\\VisionAlgorBase\\VisionAlgorBase.vcxproj", g.ALGOR_BASE),
        ("..\\..\\VisionHalconCommon\\VisionHalconCommon.vcxproj", g.HALCON_COMMON),
        ("..\\..\\VisionCommon\\VisionCommon.vcxproj", g.COMMON),
    ]
    src = ["PluginEntry.cpp", "..\\..\\VisionAlgoHalcon\\%s.cpp" % cls]
    vcx = g.plugin_vcx(guid, dll.replace(".", ""), dll, "plugins\\algos\\",
                       src, [], refs, True, "",
                       "$(SolutionDir)VisionPluginAPI;$(SolutionDir)VisionAlgoHalcon;$(HALCONROOT)\\include;$(HALCONROOT)\\include\\halconcpp;",
                       "halcon.lib;halconcpp.lib;VisionAlgorBase.lib;VisionHalconCommon.lib;VisionCommon.lib;")
    g.write(os.path.join(folder, dll + ".vcxproj"), vcx)
    projects.append((dll, "plugins\\algos\\%s\\%s.vcxproj" % (dll, dll), guid))

projects.append(("VisionToolBase", "VisionToolBase\\VisionToolBase.vcxproj", g.TOOL_BASE))
projects.append(("VisionAlgorBase", "VisionAlgorBase\\VisionAlgorBase.vcxproj", g.ALGOR_BASE))
projects.append(("VisionHalconCommon", "VisionHalconCommon\\VisionHalconCommon.vcxproj", g.HALCON_COMMON))

# strip Translate from Halcon sources
for name in os.listdir(os.path.join(ROOT, "VisionAlgoHalcon")):
    if not name.endswith((".cpp", ".h")):
        continue
    path = os.path.join(ROOT, "VisionAlgoHalcon", name)
    text = open(path, encoding="utf-8").read()
    text2 = re.sub(r"Translate::tr\(([^)]*)\)", r"\1", text)
    text2 = text2.replace('#include "Translate.h"\r\n', "")
    text2 = text2.replace('#include "Translate.h"\n', "")
    if text2 != text:
        g.write(path, text2)
        print("stripped", name)

print("projects", len(projects))
open(os.path.join(ROOT, "tools", "_projects.txt"), "w", encoding="utf-8").write(
    "\n".join("%s|%s|%s" % p for p in projects))
