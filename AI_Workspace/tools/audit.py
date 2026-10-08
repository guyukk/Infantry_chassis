"""Index local C/H files and linker evidence; this is not a C reachability proof."""
import argparse
import csv
import json
import re
import shutil
import hashlib
import difflib
import zipfile
from datetime import datetime
from pathlib import Path
from snapshot import PROJECTS, project_inventory
from project_paths import build_artifact

LOCAL_DIRS = {
    "chassis": ["application", "bsp", "components", "Src", "Inc", "UI"],
    "gimbal": ["User", "Core", "USB_DEVICE"],
}


def read_source(path):
    raw = path.read_bytes()
    for encoding in ("utf-8-sig", "gb18030"):
        try:
            return raw.decode(encoding)
        except UnicodeDecodeError:
            pass
    return raw.decode("utf-8", errors="replace")


def without_comments(text):
    # Preserve string literals and line numbers while removing comments.
    pattern = r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*'
    return re.sub(pattern, lambda m: (re.sub(r"[^\n]", " ", m.group())
                                      if m.group().startswith(("//", "/*")) else m.group()), text)


def write_csv(path, rows, columns):
    with path.open("w", encoding="utf-8-sig", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=columns)
        writer.writeheader()
        writer.writerows(rows)


def run(output):
    summary = {}
    build_metrics = {}
    build_results = json.loads((output / "build/results.json").read_text(encoding="utf-8-sig"))
    if isinstance(build_results, dict):
        build_results = [build_results]
    for board, (root, project) in PROJECTS.items():
        _, entries = project_inventory(root, project)
        enabled = {Path(e["path"]).resolve() for e in entries if e["enabled"]}
        paths = []
        for directory in LOCAL_DIRS[board]:
            paths.extend(p for p in (root / directory).rglob("*")
                         if p.is_file() and p.suffix.lower() in {".c", ".h"}
                         and not {"build", "host"}.intersection(p.relative_to(root).parts))
        includes = []
        rows = []
        for path in sorted(paths):
            text = without_comments(read_source(path))
            rel = path.relative_to(root).as_posix()
            for match in re.finditer(r'^\s*#\s*include\s*[<"]([^>"\n]+)', text, re.M):
                includes.append({"source": rel, "line": text.count("\n", 0, match.start()) + 1,
                                 "include": match.group(1), "basename": Path(match.group(1)).name})
            rows.append({"path": rel, "lines": len(text.splitlines()),
                         "in_keil_file_list": path.resolve() in enabled,
                         "kind": "translation_unit" if path.suffix.lower() == ".c" else "header"})
        for row in rows:
            row["include_references_by_basename"] = sum(i["basename"] == Path(row["path"]).name for i in includes)
        write_csv(output / (board + "-local-files.csv"), rows,
                  ["path", "lines", "in_keil_file_list", "kind", "include_references_by_basename"])
        write_csv(output / (board + "-includes.csv"), includes, ["source", "line", "include", "basename"])
        output_name = Path(project).stem
        map_path = build_artifact(root, project, ".map")
        removed = []
        sizes = []
        if map_path.is_file():
            shutil.copy2(map_path, output / (board + ".map"))
            map_text = read_source(map_path)
            for match in re.finditer(r'^\s*Removing (.+?)\((.*?)\), \((\d+) bytes\)\.', map_text, re.M):
                removed.append({"object": match.group(1), "section": match.group(2), "bytes": int(match.group(3))})
            components = map_text.split("Image component sizes", 1)[-1].split("Library Member Name", 1)[0]
            for match in re.finditer(r'^\s*(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s+(\S+\.o)\s*$', components, re.M):
                values = list(map(int, match.groups()[:6]))
                sizes.append(dict(zip(["code", "code_data", "ro_data", "rw_data", "zi_data", "debug", "object"], values + [match.group(7)])))
        write_csv(output / (board + "-linker-removed.csv"), removed, ["object", "section", "bytes"])
        write_csv(output / (board + "-object-sizes.csv"), sizes, ["code", "code_data", "ro_data", "rw_data", "zi_data", "debug", "object"])
        log_text = read_source(output / "build" / (board + ".log"))
        diagnostics = []
        for line in log_text.splitlines():
            match = re.match(r'(.+?)\((\d+)\): warning:\s+#([^:]+):\s*(.*)', line)
            if match:
                diagnostics.append({"file": match.group(1), "line": int(match.group(2)),
                                    "code": match.group(3), "message": match.group(4)})
        write_csv(output / (board + "-warnings.csv"), diagnostics, ["file", "line", "code", "message"])
        status = re.search(r'(\d+) Error\(s\), (\d+) Warning\(s\)', log_text)
        program = re.search(r'Program Size: Code=(\d+) RO-data=(\d+) RW-data=(\d+) ZI-data=(\d+)', log_text)
        result = next(r for r in build_results if r["Board"] == board)
        image_path = map_path.with_suffix(".axf")
        # PowerShell's round-trip timestamps carry 7 fractional digits; Python 3.9 accepts 6.
        started_text = re.sub(r'(\.\d{6})\d+(?=[+-]|$)', r'\1', result["Started"])
        started = datetime.fromisoformat(started_text).timestamp()
        success = (result["ExitCode"] in (0, 1) and status and int(status.group(1)) == 0
                   and image_path.is_file() and image_path.stat().st_mtime >= started - 2)
        if not success or not program:
            raise RuntimeError("Baseline build not verified: " + board)
        for extension in (".axf", ".hex"):
            artifact = map_path.with_suffix(extension)
            if artifact.is_file():
                shutil.copy2(artifact, output / (board + extension))
        build_metrics[board] = {"success": bool(success), "errors": int(status.group(1)),
                                "warnings": int(status.group(2)),
                                "code_bytes": int(program.group(1)), "ro_data_bytes": int(program.group(2)),
                                "rw_data_bytes": int(program.group(3)), "zi_data_bytes": int(program.group(4)),
                                "image_sha256": hashlib.sha256(image_path.read_bytes()).hexdigest(),
                                "warning_codes": {code: sum(d["code"] == code for d in diagnostics)
                                                  for code in sorted(set(d["code"] for d in diagnostics))}}
        with zipfile.ZipFile(output / (board + "-sources.zip")) as archive:
            generated = "MDK-ARM/RTE/_" + output_name + "/RTE_Components.h"
            if generated in archive.namelist() and (root / generated).is_file():
                before = archive.read(generated).decode("utf-8-sig").splitlines(True)
                after = (root / generated).read_text(encoding="utf-8-sig").splitlines(True)
                (output / (board + "-rte-build-change.diff")).write_text(
                    "".join(difflib.unified_diff(before, after, fromfile="before", tofile="after")), encoding="utf-8")
                shutil.copy2(root / generated, output / (board + "-RTE_Components-after.h"))
        summary[board] = {"local_c_h_files": len(rows), "local_c_lines": sum(r["lines"] for r in rows if r["kind"] == "translation_unit"),
                          "c_files_not_in_keil": [r["path"] for r in rows if r["kind"] == "translation_unit" and not r["in_keil_file_list"]],
                          "linker_removed_sections": len(removed)}
    (output / "audit-summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")
    (output / "build-metrics.json").write_text(json.dumps(build_metrics, ensure_ascii=False, indent=2), encoding="utf-8")
    print(json.dumps(summary, ensure_ascii=False, indent=2))
    print(json.dumps(build_metrics, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    run(args.output.resolve())
