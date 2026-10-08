"""Shared project and evidence paths, independent of the shell working directory."""
import json
from pathlib import Path
import xml.etree.ElementTree as ET

HERE = Path(__file__).resolve().parent
ROOT = next(parent for parent in HERE.parents
            if (parent / "MDK-ARM/standard_tpye_c.uvprojx").is_file())
WORKSPACE = ROOT / "AI_Workspace"
CONFIG = json.loads((HERE / "projects.json").read_text(encoding="utf-8-sig"))
PROJECTS = {name: ((ROOT / item["root"]).resolve(), item["project"])
            for name, item in CONFIG["projects"].items()}
KEIL = Path(CONFIG["keil_executable"])
ARM_BIN = KEIL.parent.parent / "ARM/ARMCC/bin"
SNAPSHOTS = WORKSPACE / "snapshots"
ARTIFACTS = WORKSPACE / "artifacts"
CACHE = WORKSPACE / "cache"
DEPENDENCIES = CACHE / "dependencies/packages"


def build_artifact(root, project, extension):
    target_name = next(item["target"] for item in CONFIG["projects"].values()
                       if item["project"] == project)
    targets = ET.parse(root / project).findall("./Targets/Target")
    target = next(item for item in targets if item.findtext("TargetName") == target_name)
    common = target.find("./TargetOption/TargetCommonOption")
    output = common.findtext("OutputDirectory", "").replace("\\", "/")
    return ((root / project).parent / output /
            (common.findtext("OutputName") + extension)).resolve()
