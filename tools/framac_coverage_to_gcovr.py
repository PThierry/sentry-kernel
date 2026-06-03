#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2026 Ledger SAS
# SPDX-License-Identifier: Apache-2.0

"""Aggregate Frama-C function coverage and export gcovr JSON.

This script:
1. Loads all Frama-C `*-coverage.json` files.
2. Collapses suffixed function names (e.g. `foo_12` -> `foo`) and sums calls.
3. Loads compiled kernel C files from `compile_commands.json`.
4. Extracts defined C functions from each compiled file via universal-ctags.
5. Emits a gcovr JSON report with function-level execution counts.
6. Emits a gcovr JSON summary file next to the main report.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from collections import defaultdict
from pathlib import Path
import re
from typing import Dict, Iterable, List, Set, Tuple


SUFFIX_RE = re.compile(r"^(?P<base>.+)_\d+$")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert Frama-C proof coverage to gcovr function JSON"
    )
    parser.add_argument(
        "--repo-root",
        default=".",
        help="Repository root (default: current directory)",
    )
    parser.add_argument(
        "--compile-commands",
        default="builddir_proof/compile_commands.json",
        help="Path to compile_commands.json (default: builddir_proof/compile_commands.json)",
    )
    parser.add_argument(
        "--coverage-glob",
        action="append",
        default=[],
        help=(
            "Glob pattern(s) for Frama-C coverage files, relative to repo root. "
            "Can be given multiple times. "
            "Default: builddir*/kernel/proof/**/*-coverage.json"
        ),
    )
    parser.add_argument(
        "--output",
        default="frama-c-kernel-function-coverage.gcovr.json",
        help="Output gcovr JSON path (default: frama-c-kernel-function-coverage.gcovr.json)",
    )
    parser.add_argument(
        "--summary-output",
        default=None,
        help=(
            "Output gcovr JSON summary path. "
            "Default: main output with '.summary.json' suffix"
        ),
    )
    parser.add_argument(
        "--ctags",
        default="ctags",
        help="ctags executable to use (default: ctags)",
    )
    return parser.parse_args()


def normalize_function_name(name: str) -> str:
    match = SUFFIX_RE.match(name)
    if match is None:
        return name
    return match.group("base")


def collect_coverage_files(repo_root: Path, globs: List[str]) -> List[Path]:
    patterns = globs or ["builddir*/kernel/proof/**/*-coverage.json"]
    coverage_files: Set[Path] = set()
    for pattern in patterns:
        for path in repo_root.glob(pattern):
            if path.is_file():
                coverage_files.add(path.resolve())
    return sorted(coverage_files)


def aggregate_calls(coverage_files: Iterable[Path]) -> Dict[str, int]:
    calls_by_function: Dict[str, int] = defaultdict(int)

    for coverage_file in coverage_files:
        with coverage_file.open("r", encoding="utf-8") as handle:
            payload = json.load(handle)

        for function_item in payload.get("defined-functions", []):
            if not isinstance(function_item, dict):
                continue
            for raw_name, function_data in function_item.items():
                if not isinstance(function_data, dict):
                    continue
                base_name = normalize_function_name(raw_name)
                call_count = int(function_data.get("calls", 0))
                calls_by_function[base_name] += call_count

    return dict(calls_by_function)


def resolve_source_path(entry: dict, repo_root: Path) -> Path | None:
    file_value = entry.get("file")
    directory_value = entry.get("directory")
    if not isinstance(file_value, str) or not isinstance(directory_value, str):
        return None

    file_path = Path(file_value)
    if file_path.is_absolute():
        candidate = file_path.resolve()
    else:
        candidate = (Path(directory_value) / file_path).resolve()

    rel = to_repo_relative_or_none(candidate, repo_root)
    if rel is None:
        return None

    # Keep only kernel source/include tree as requested.
    if not (rel.startswith("kernel/src/") or rel.startswith("kernel/include/")):
        return None

    if candidate.suffix != ".c" or not candidate.exists():
        return None

    return candidate


def load_compiled_kernel_sources(compile_commands: Path, repo_root: Path) -> List[Path]:
    with compile_commands.open("r", encoding="utf-8") as handle:
        entries = json.load(handle)

    sources: Set[Path] = set()
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        source = resolve_source_path(entry, repo_root)
        if source is not None:
            sources.add(source)

    return sorted(sources)


def to_repo_relative_or_none(path: Path, repo_root: Path) -> str | None:
    try:
        return path.resolve().relative_to(repo_root.resolve()).as_posix()
    except ValueError:
        return None


def extract_functions_with_ctags(ctags_bin: str, source_file: Path) -> List[Tuple[str, int]]:
    command = [
        ctags_bin,
        "--output-format=json",
        "--fields=+n",
        "--kinds-C=f",
        str(source_file),
    ]

    try:
        result = subprocess.run(
            command,
            check=True,
            capture_output=True,
            text=True,
        )
    except FileNotFoundError as exc:
        raise RuntimeError(f"ctags executable not found: {ctags_bin}") from exc
    except subprocess.CalledProcessError as exc:
        stderr = exc.stderr.strip() if exc.stderr else "unknown ctags error"
        raise RuntimeError(f"ctags failed for {source_file}: {stderr}") from exc

    functions: List[Tuple[str, int]] = []
    for line in result.stdout.splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            tag = json.loads(line)
        except json.JSONDecodeError:
            continue
        if tag.get("_type") != "tag" or tag.get("kind") != "function":
            continue
        name = tag.get("name")
        lineno = tag.get("line")
        if isinstance(name, str) and isinstance(lineno, int):
            functions.append((name, lineno))

    return functions


def find_function_span(source_text: List[str], start_line: int) -> Tuple[int, int]:
    """Find function body span from a ctags start line using brace matching.

    The scan starts at start_line and looks for the first opening brace, then
    balances braces until the body end. If no body is found, fallback to start.
    """

    start_idx = max(start_line - 1, 0)
    line_count = len(source_text)
    if start_idx >= line_count:
        return (start_line, start_line)

    open_found = False
    depth = 0
    body_start = start_line

    for idx in range(start_idx, line_count):
        line = source_text[idx]
        for ch in line:
            if ch == "{":
                if not open_found:
                    open_found = True
                    body_start = idx + 1
                depth += 1
            elif ch == "}" and open_found:
                depth -= 1
                if depth == 0:
                    return (body_start, idx + 1)

    return (start_line, start_line)


def to_repo_relative(path: Path, repo_root: Path) -> str:
    rel = to_repo_relative_or_none(path, repo_root)
    if rel is None:
        raise ValueError(f"path {path} is outside repository root {repo_root}")
    return rel


def build_gcovr_report(
    repo_root: Path,
    source_files: List[Path],
    aggregated_calls: Dict[str, int],
    ctags_bin: str,
) -> dict:
    files_payload = []

    for source_file in source_files:
        functions = extract_functions_with_ctags(ctags_bin, source_file)
        source_lines = source_file.read_text(encoding="utf-8", errors="replace").splitlines()
        if not functions:
            # Keep file entry for completeness, even if no function is found.
            files_payload.append(
                {
                    "file": to_repo_relative(source_file, repo_root),
                    "lines": [],
                    "functions": [],
                }
            )
            continue

        function_entries = []
        lines_map: Dict[int, dict] = {}
        for name, lineno in functions:
            calls = int(aggregated_calls.get(name, 0))
            line_start, line_end = find_function_span(source_lines, lineno)
            line_count = calls if calls > 0 else 0

            function_entries.append(
                {
                    "name": name,
                    "lineno": lineno,
                    "execution_count": calls,
                    "blocks_percent": 100.0 if calls > 0 else 0.0,
                }
            )

            # User rule: if a function is covered at least once, all its lines are covered.
            for line_number in range(line_start, line_end + 1):
                if line_number < 1:
                    continue
                existing = lines_map.get(line_number)
                if existing is None or line_count > existing["count"]:
                    lines_map[line_number] = {
                        "line_number": line_number,
                        "function_name": name,
                        "count": line_count,
                        "branches": [],
                    }

        function_entries.sort(key=lambda item: (item["name"], item["lineno"]))
        line_entries = [lines_map[k] for k in sorted(lines_map)]
        files_payload.append(
            {
                "file": to_repo_relative(source_file, repo_root),
                "lines": line_entries,
                "functions": function_entries,
            }
        )

    files_payload.sort(key=lambda item: item["file"])
    return {
        "gcovr/format_version": "0.14",
        "files": files_payload,
    }


def compute_percent(covered: int, total: int, nan_as_zero: bool) -> float | None:
    if total == 0:
        return 0.0 if nan_as_zero else None
    return round((covered * 100.0) / total, 1)


def build_gcovr_summary(report: dict) -> dict:
    file_summaries = []

    total_line = 0
    total_line_covered = 0
    total_fn = 0
    total_fn_covered = 0

    for file_entry in report.get("files", []):
        lines = file_entry.get("lines", [])
        functions = file_entry.get("functions", [])

        line_total = len(lines)
        line_covered = sum(1 for line in lines if int(line.get("count", 0)) > 0)
        fn_total = len(functions)
        fn_covered = sum(1 for fn in functions if int(fn.get("execution_count", 0)) > 0)

        total_line += line_total
        total_line_covered += line_covered
        total_fn += fn_total
        total_fn_covered += fn_covered

        file_summaries.append(
            {
                "filename": file_entry.get("file", ""),
                "line_total": line_total,
                "line_covered": line_covered,
                "line_percent": compute_percent(line_covered, line_total, nan_as_zero=False),
                "function_total": fn_total,
                "function_covered": fn_covered,
                "function_percent": compute_percent(fn_covered, fn_total, nan_as_zero=False),
                "branch_total": 0,
                "branch_covered": 0,
                "branch_percent": None,
            }
        )

    return {
        "root": ".",
        "gcovr/summary_format_version": "0.6",
        "files": file_summaries,
        "line_total": total_line,
        "line_covered": total_line_covered,
        "line_percent": compute_percent(total_line_covered, total_line, nan_as_zero=True),
        "function_total": total_fn,
        "function_covered": total_fn_covered,
        "function_percent": compute_percent(total_fn_covered, total_fn, nan_as_zero=True),
        "branch_total": 0,
        "branch_covered": 0,
        "branch_percent": 0.0,
    }


def main() -> int:
    args = parse_args()

    repo_root = Path(args.repo_root).resolve()
    compile_commands = (repo_root / args.compile_commands).resolve()
    output_path = (repo_root / args.output).resolve()
    summary_path = (
        (repo_root / args.summary_output).resolve()
        if args.summary_output
        else output_path.with_name(f"{output_path.stem}.summary.json")
    )

    if not compile_commands.exists():
        print(f"error: compile_commands not found: {compile_commands}", file=sys.stderr)
        return 2

    coverage_files = collect_coverage_files(repo_root, args.coverage_glob)
    if not coverage_files:
        print("error: no coverage files found from given glob(s)", file=sys.stderr)
        return 3

    aggregated_calls = aggregate_calls(coverage_files)
    source_files = load_compiled_kernel_sources(compile_commands, repo_root)
    if not source_files:
        print("error: no compiled kernel C file found in compile_commands", file=sys.stderr)
        return 4

    report = build_gcovr_report(
        repo_root=repo_root,
        source_files=source_files,
        aggregated_calls=aggregated_calls,
        ctags_bin=args.ctags,
    )

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", encoding="utf-8") as handle:
        json.dump(report, handle, indent=2, sort_keys=False)
        handle.write("\n")

    summary = build_gcovr_summary(report)
    summary_path.parent.mkdir(parents=True, exist_ok=True)
    with summary_path.open("w", encoding="utf-8") as handle:
        json.dump(summary, handle, indent=2, sort_keys=False)
        handle.write("\n")

    covered = sum(
        1
        for file_entry in report["files"]
        for fn in file_entry["functions"]
        if fn.get("execution_count", 0) > 0
    )
    total = sum(len(file_entry["functions"]) for file_entry in report["files"])
    print(f"coverage files: {len(coverage_files)}")
    print(f"compiled kernel sources: {len(source_files)}")
    print(f"function coverage: {covered}/{total}")
    print(f"written: {output_path}")
    print(f"summary written: {summary_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
