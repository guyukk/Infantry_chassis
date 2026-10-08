"""Capture source/configuration hashes and Keil file lists without editing firmware."""
import argparse
import csv
import hashlib
import json
import zipfile
from datetime import datetime, timezone
from pathlib import Path
import xml.etree.ElementTree as ET

from project_paths import ROOT, PROJECTS, build_artifact
EXTENSIONS = {".c", ".h", ".cpp", ".hpp", ".s", ".lib", ".uvprojx",
              ".uvoptx", ".ioc", ".ld", ".sct", ".bat", ".cmake"}
SKIP = {".git", ".vscode", ".agents", ".codex", "preparation", "AI_Workspace",
        "Project_Maintainer", "build", "__pycache__"}


def source_files(root):
    output_dirs = {build_artifact(project_root, project, ".axf").parent
                   for project_root, project in PROJECTS.values() if project_root == root}
    return sorted(p for p in root.rglob("*") if p.is_file()
                  and not SKIP.intersection(p.relative_to(root).parts)
                  and not any(parent in p.parents for parent in output_dirs)
                  and (p.suffix.lower() in EXTENSIONS or p.name in {"AGENTS.md", ".mxproject"}))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def project_inventory(root, project):
    tree = ET.parse(root / project)
    rows = []
    targets = []
    for target in tree.findall("./Targets/Target"):
        name = target.findtext("TargetName")
        common = target.find("./TargetOption/TargetCommonOption")
        controls = target.find("./TargetOption/TargetArmAds/Cads/VariousControls")
        targets.append({"target": name, "device": common.findtext("Device"),
                        "pack": common.findtext("PackID"),
                        "compiler": target.findtext("pCCUsed"),
                        "defines": controls.findtext("Define"),
                        "include_paths": controls.findtext("IncludePath"),
                        "output_directory": common.findtext("OutputDirectory")})
        for group in target.findall("./Groups/Group"):
            group_include = group.findtext("./GroupOption/CommonProperty/IncludeInBuild", "1")
            for file in group.findall("./Files/File"):
                raw = file.findtext("FilePath", "")
                path = (root / Path(project).parent / raw.replace("\\", "/")).resolve()
                included = file.findtext("./FileOption/CommonProperty/IncludeInBuild", "2")
                enabled = (group_include != "0") if included == "2" else (included != "0")
                rows.append({"target": name, "group": group.findtext("GroupName"),
                             "path": str(path), "exists": path.is_file(), "enabled": enabled})
    return targets, rows


def capture(output):
    output.mkdir(parents=True, exist_ok=False)
    summary = {"captured_at": datetime.now(timezone.utc).isoformat(), "projects": {}}
    for name, (root, project) in PROJECTS.items():
        paths = source_files(root)
        hashes = {p.relative_to(root).as_posix(): digest(p) for p in paths}
        (output / (name + "-sha256.json")).write_text(
            json.dumps(hashes, ensure_ascii=False, indent=2), encoding="utf-8")
        with zipfile.ZipFile(output / (name + "-sources.zip"), "w", zipfile.ZIP_DEFLATED) as archive:
            for path in paths:
                archive.write(path, path.relative_to(root).as_posix())
        targets, rows = project_inventory(root, project)
        with (output / (name + "-keil-files.csv")).open("w", encoding="utf-8-sig", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=["target", "group", "path", "exists", "enabled"])
            writer.writeheader()
            writer.writerows(rows)
        summary["projects"][name] = {"root": str(root), "project": project,
                                     "snapshot_file_count": len(paths), "targets": targets,
                                     "keil_entry_count": len(rows),
                                     "missing_entries": [r for r in rows if not r["exists"]]}
    (output / "baseline.json").write_text(
        json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False, indent=2))


def verify(output, result_path=None):
    changed = []
    for name, (root, _) in PROJECTS.items():
        hashes = json.loads((output / (name + "-sha256.json")).read_text(encoding="utf-8"))
        archive_path = output / (name + "-sources.zip")
        with zipfile.ZipFile(archive_path) as archive:
            corrupt = archive.testzip()
            if corrupt:
                raise RuntimeError("Corrupt archive member: " + corrupt)
            for rel, expected in hashes.items():
                if hashlib.sha256(archive.read(rel)).hexdigest() != expected:
                    raise RuntimeError("Snapshot hash mismatch: " + rel)
                current = root / rel
                if not current.is_file() or digest(current) != expected:
                    changed.append(name + ":" + rel)
        added = set(p.relative_to(root).as_posix() for p in source_files(root)) - set(hashes)
        changed.extend(name + ":ADDED:" + rel for rel in sorted(added))
    generated = [item for item in changed if ":MDK-ARM/RTE/" in item
                 or re_generated_scatter(item)]
    guidance = [item for item in changed if item.endswith(":AGENTS.md")]
    manual = sorted(set(changed) - set(generated) - set(guidance))
    result = {"snapshot_integrity": "OK", "changed_source_or_config": changed,
              "keil_generated_changes": generated, "project_guidance_changes": guidance,
              "manual_firmware_source_or_config_changes": manual}
    destination = result_path or output / "verification.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return bool(manual)


def re_generated_scatter(item):
    return item in {"chassis:ADDED:MDK-ARM/standard_tpye_c/standard_tpye_c.sct",
                    "gimbal:ADDED:MDK-ARM/CtrlBoard-H7_IMU/CtrlBoard-H7_IMU.sct"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("mode", choices=["capture", "verify"])
    parser.add_argument("output", type=Path)
    parser.add_argument("--result", type=Path, help="Write verification outside an immutable old snapshot")
    args = parser.parse_args()
    if args.mode == "capture":
        capture(args.output.resolve())
    else:
        raise SystemExit(verify(args.output.resolve(), args.result.resolve() if args.result else None))
