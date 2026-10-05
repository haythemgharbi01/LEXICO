from __future__ import annotations

import csv
import os
import pathlib
import shutil
import subprocess
import time

ROOT = pathlib.Path(__file__).resolve().parents[1]
COMPILER = ROOT / "SourceCode" / "Applications" / ("lexico.exe" if os.name == "nt" else "lexico")
CASES = ROOT / "research" / "evaluation_cases"
RESULTS = ROOT / "research" / "evaluation_results.csv"

MUTATIONS = [
    ("missing-semicolon", "missing_semicolon_01", "create int total set value 2\nprint total;\n"),
    ("missing-semicolon", "missing_semicolon_02", "create int total set value 2;\nprint total\n"),
    ("missing-semicolon", "missing_semicolon_03", "create int total set value 2\nprint total\n"),
    ("missing-semicolon", "missing_semicolon_04", "create int total set value 2\n"),
    ("missing-semicolon", "missing_semicolon_05", "print total\n"),
    ("keyword-case", "keyword_case_01", "Create int total set value 2;\nprint total;\n"),
    ("keyword-case", "keyword_case_02", "create INT total set value 2;\nprint total;\n"),
    ("keyword-case", "keyword_case_03", "create int total SET value 2;\nprint total;\n"),
    ("keyword-case", "keyword_case_04", "create int total set VALUE 2;\nprint total;\n"),
    ("keyword-case", "keyword_case_05", "create int total set value 2;\nPRINT total;\n"),
    ("multi-word-operator", "multi_word_01", "create int total set value 2;\nif total is equal 2 then\n> print total;\n"),
    ("multi-word-operator", "multi_word_02", "create int total set value 2;\nif total is not equal 3 then\n> print total;\n"),
    ("multi-word-operator", "multi_word_03", "create int total set value 2;\nif total is greater 1 then\n> print total;\n"),
    ("multi-word-operator", "multi_word_04", "create int total set value 2;\nif total is less 3 then\n> print total;\n"),
    ("multi-word-operator", "multi_word_05", "create int total set value 2;\nset total to total divided 1;\nprint total;\n"),
    ("multi-word-operator", "multi_word_06", "create int total set value 2;\nif total is equal 2 then\n> print total;\n"),
    ("layout-mismatch", "layout_01", "create int total set value 2;\nif total is equal to 2 then\n> print total;\n>>> print total;\n"),
    ("layout-mismatch", "layout_02", "create int total set value 2;\nif total is equal to 2 then\n> print total;\n>> print total;\n"),
    ("layout-mismatch", "layout_03", "create int total set value 2;\nif total is equal to 2 then\n> if total is equal to 2 then\n>>> print total;\n"),
    ("layout-mismatch", "layout_04", "create int total set value 2;\nif total is equal to 2 then\n> print total;\n>>> if total is equal to 2 then\n>>>> print total;\n"),
    ("layout-mismatch", "layout_05", "create int total set value 2;\nif total is equal to 2 then\n> print total;\n>> if total is equal to 2 then\n> print total;\n"),
    ("file-context", "file_context_01", "write \"not-open\" in file;\n"),
    ("file-context", "file_context_02", "read file into missing_target;\n"),
    ("file-context", "file_context_03", "clear file;\n"),
    ("file-context", "file_context_04", "close file;\n"),
    ("file-context", "file_context_05", "set file title to \"renamed.txt\";\n"),
]


def run(source: pathlib.Path, autofix: bool) -> tuple[int, float, str]:
    command = [str(COMPILER)] + (["--autofix"] if autofix else []) + [str(source)]
    start = time.perf_counter()
    try:
        result = subprocess.run(command, input="", capture_output=True, text=True, timeout=45)
        elapsed = time.perf_counter() - start
        text = (result.stderr + "\n" + result.stdout).replace("\r", " ").replace("\n", " ").strip()
        return result.returncode, elapsed, text[:2000]
    except subprocess.TimeoutExpired:
        return 124, time.perf_counter() - start, "evaluation timeout"


def main() -> int:
    if not COMPILER.exists():
        raise SystemExit(f"compiler not found: {COMPILER}")
    CASES.mkdir(parents=True, exist_ok=True)
    for old in CASES.glob("*"):
        if old.is_file():
            old.unlink()

    seed = CASES / "valid_seed.lx"
    seed.write_text("create int total set value 2;\nprint total;\n", encoding="utf-8")
    seed_rc, seed_time, seed_diag = run(seed, False)
    if seed_rc != 0:
        raise SystemExit(f"valid seed failed: {seed_diag}")

    rows = []
    for category, name, text in MUTATIONS:
        source = CASES / f"{name}.lx"
        source.write_text(text, encoding="utf-8")
        baseline_rc, baseline_time, baseline_diag = run(source, False)
        autofix_rc, autofix_time, autofix_diag = run(source, True)
        rows.append({
            "category": category,
            "case": name,
            "baseline_failed": baseline_rc != 0,
            "baseline_returncode": baseline_rc,
            "baseline_seconds": f"{baseline_time:.3f}",
            "baseline_diagnostic": baseline_diag,
            "autofix_recovered": baseline_rc != 0 and autofix_rc == 0,
            "autofix_returncode": autofix_rc,
            "autofix_seconds": f"{autofix_time:.3f}",
            "autofix_diagnostic": autofix_diag,
            "cost": "unknown; compiler does not record provider usage",
        })

    with RESULTS.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)

    baseline_failures = sum(row["baseline_failed"] for row in rows)
    recovered = sum(row["autofix_recovered"] for row in rows)
    print(f"seed_passed=True seed_seconds={seed_time:.3f}")
    print(f"trials={len(rows)} baseline_rejected={baseline_failures} autofix_recovered={recovered}")
    print(f"results={RESULTS}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
