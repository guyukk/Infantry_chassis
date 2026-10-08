"""Compare ARMCC-preprocessed retained functions with the saved source snapshot.

This records evidence for review, not a proof of hardware behavior. The runtime
protocol/calibration checks live in validate_runtime.py.
"""
import difflib
import argparse
import json
import re
import subprocess
import xml.etree.ElementTree as ET
from pathlib import Path
from zipfile import ZipFile

from audit import without_comments, read_source
from snapshot import PROJECTS, project_inventory
from project_paths import CACHE, ARTIFACTS, SNAPSHOTS, ARM_BIN

HERE = Path(__file__).resolve().parent
OUT = CACHE / "validation/current"
RESULTS = ARTIFACTS / "validation/latest"
BACKUP = SNAPSHOTS / "before-batch-01"
ARMCC = ARM_BIN / "armcc.exe"
REVIEWED_DIFFERENCES = {
    "chassis": {"MX_FREERTOS_Init", "UI_update", "cali_param_init", "get_flash_latitude",
                "chassis_behaviour_mode_set", "PID_Calc_Jump", "chassis_control_loop",
                "chassis_init", "chassis_set_contorl", "chassis_task", "gimbal_to_chassis"},
    "gimbal": {"main", "detect_init", "send_data_to_chassis"},
}


def functions(text):
    text = re.sub(r"^\s*#.*$", "", without_comments(text), flags=re.M)
    text = re.sub(r"__attribute__\s*\(\([^)]*\)\)", "", text)
    result = {}
    pattern = re.compile(r"\b([A-Za-z_]\w*)\s*\([^;{}]*\)\s*\{")
    pos = 0
    while True:
        match = pattern.search(text, pos)
        if not match:
            break
        start = match.end() - 1
        depth = 1
        end = start + 1
        # Strings do not contain braces in the selected firmware functions.
        while depth:
            if text[end] == "{":
                depth += 1
            elif text[end] == "}":
                depth -= 1
            end += 1
        if match.group(1) not in {"if", "for", "while", "switch"}:
            result[match.group(1)] = text[start:end]
        pos = end
    return result


def preprocess(root, project, rel, board):
    target = ET.parse(root / project).find("./Targets/Target")
    controls = target.find("./TargetOption/TargetArmAds/Cads/VariousControls")
    mdk = (root / project).parent
    command = [str(ARMCC), "-E", "--c99", "--cpu", "Cortex-M4" if board == "chassis" else "Cortex-M7"]
    command += ["-D" + item.strip() for item in controls.findtext("Define", "").split(",") if item.strip()]
    command += ["-I" + str((mdk / item.replace("\\", "/")).resolve())
                for item in controls.findtext("IncludePath", "").split(";") if item]
    command += [str(root / rel)]
    result = subprocess.run(command, cwd=mdk, capture_output=True, timeout=45)
    if result.returncode:
        raise RuntimeError(rel + "\n" + result.stderr.decode("gb18030", errors="replace"))
    return result.stdout.decode("gb18030", errors="replace")


def run():
    OUT.mkdir(parents=True, exist_ok=True)
    RESULTS.mkdir(parents=True, exist_ok=True)
    results = {}
    unexpected = []
    for board, (root, project) in PROJECTS.items():
        original = OUT / (board + "-original")
        original.mkdir(exist_ok=True)
        with ZipFile(BACKUP / (board + "-sources.zip")) as archive:
            # These are our own already-hashed local snapshots; verify paths first.
            for info in archive.infolist():
                destination = (original / info.filename).resolve()
                destination.relative_to(original.resolve())
            archive.extractall(original)
            _, entries = project_inventory(root, project)
            active = {Path(row["path"]).relative_to(root).as_posix()
                      for row in entries if row["enabled"] and Path(row["path"]).suffix == ".c"}
            local = ("application/", "bsp/", "components/", "Src/", "UI/") if board == "chassis" else ("User/", "Core/", "USB_DEVICE/")
            changed = sorted(name for name in active if name.startswith(local)
                             and (original / name).is_file())
        rows = []
        for rel in changed:
            before = functions(preprocess(original, project, rel, board))
            after = functions(preprocess(root, project, rel, board))
            # Limit to functions defined in this C file, excluding header inlines.
            source_names = set(functions(read_source(root / rel)))
            for name in sorted(source_names & before.keys() & after.keys()):
                a, b = before[name], after[name]
                identical = re.sub(r"\s+", "", a) == re.sub(r"\s+", "", b)
                rows.append({"source": rel, "function": name, "identical_body": identical})
                if not identical:
                    diff = "".join(difflib.unified_diff(a.splitlines(True), b.splitlines(True),
                                                       fromfile="before", tofile="after"))
                    (RESULTS / (board + "-" + name + ".diff")).write_text(diff, encoding="utf-8")
        results[board] = rows
        different = [r for r in rows if not r["identical_body"]]
        unexpected.extend(dict(board=board, **r) for r in different
                          if r["function"] not in (REVIEWED_DIFFERENCES[board]
                                                   if BACKUP.name == "before-batch-01" else set()))
        print(board, "retained function bodies:", len(rows), "identical:", len(rows) - len(different))
        print(json.dumps(different, ensure_ascii=False, indent=2))
    (RESULTS / "function-comparison.json").write_text(json.dumps(results, ensure_ascii=False, indent=2), encoding="utf-8")
    if unexpected:
        raise RuntimeError("Unreviewed function differences: " + json.dumps(unexpected, ensure_ascii=False))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", type=Path, default=BACKUP)
    parser.add_argument("--cache", type=Path, default=OUT)
    parser.add_argument("--results", type=Path, default=RESULTS)
    args = parser.parse_args()
    BACKUP, OUT, RESULTS = args.baseline.resolve(), args.cache.resolve(), args.results.resolve()
    run()
