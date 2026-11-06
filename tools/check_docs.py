#!/usr/bin/env python3
"""Documentation consistency checks for the BlitzQuake tree.

Checks performed:
  1. Every relative markdown link resolves to an existing file/directory.
  2. Every "<!-- mmd: name.mmd -->" marker in a markdown file points at an
     existing docs/diagrams/ diagram, is immediately followed by a mermaid
     fence, and the embedded diagram matches the .mmd source (ignoring
     "%%" comment and blank lines).
  3. Every docs/diagrams/*.mmd file is referenced from at least one page.
  4. AGENTS.md references docs/README.md (the documentation entry point).

Usage:
    python3 tools/check_docs.py

Exit status is non-zero when any check fails. No third-party dependencies.
"""

from __future__ import annotations

import re
import sys
import urllib.parse
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
DIAGRAMS = REPO / "docs" / "diagrams"

LINK_RE = re.compile(r"\[[^\]]*\]\(\s*([^)\s]+)(?:\s+\"[^\"]*\")?\s*\)")
# Markers must be alone on their line (examples in prose/code are ignored).
MMD_RE = re.compile(r"^\s*<!--\s*mmd:\s*(\S+)\s*-->\s*$")
FENCE_OPEN_RE = re.compile(r"^```mermaid\s*$")
FENCE_ANY_RE = re.compile(r"^```")
FENCE_CLOSE_RE = re.compile(r"^```\s*$")

SKIP_SCHEMES = ("http://", "https://", "ftp://", "mailto:", "tel:")
errors: list[str] = []
stats = {"md": 0, "links": 0, "mmd_refs": 0, "mmd_files": 0}


def iter_markdown() -> list[Path]:
    files = []
    for path in sorted(REPO.rglob("*.md")):
        if ".git" in path.parts:
            continue
        files.append(path)
    return files


def rel(path: Path) -> str:
    try:
        return path.relative_to(REPO).as_posix()
    except ValueError:
        return path.as_posix()


def normalized_mermaid(text: str) -> str:
    """Diagram body without %% comments, fences and blank lines."""
    lines = []
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith("%%"):
            continue
        if line.startswith("```"):
            continue
        lines.append(line)
    return "\n".join(lines)


def check_links(path: Path, lines: list[str]) -> None:
    for lineno, line in enumerate(lines, start=1):
        for match in LINK_RE.finditer(line):
            target = match.group(1)
            if target.startswith("#") or target.startswith(SKIP_SCHEMES):
                continue
            if "://" in target:
                continue
            target = urllib.parse.unquote(target.split("#", 1)[0])
            if not target:
                continue
            stats["links"] += 1
            resolved = (path.parent / target).resolve()
            if not resolved.exists():
                errors.append(
                    f"{rel(path)}:{lineno}: broken link '{target}'"
                    f" -> {resolved}"
                )


def check_mmd_markers(path: Path, lines: list[str]) -> set[str]:
    referenced: set[str] = set()
    in_fence = False
    for lineno, line in enumerate(lines, start=1):
        stripped = line.strip()
        match = MMD_RE.match(line)
        if not in_fence and match:
            name = match.group(1)
            referenced.add(name)
            stats["mmd_refs"] += 1
            src = DIAGRAMS / name if "/" not in name else (path.parent / name)
            if not src.exists():
                errors.append(
                    f"{rel(path)}:{lineno}: mmd source '{name}' not found"
                    f" (looked at {rel(src)})"
                )
                continue

            # The fence must follow the marker, separated only by blank lines.
            idx = lineno  # 0-based index of the line after the marker
            while idx < len(lines) and not lines[idx].strip():
                idx += 1
            if idx >= len(lines) or not FENCE_OPEN_RE.match(lines[idx].strip()):
                errors.append(
                    f"{rel(path)}:{lineno}: marker for {name} is not followed by"
                    " a ```mermaid fence"
                )
                continue
            body: list[str] = []
            idx += 1
            while idx < len(lines) and not FENCE_CLOSE_RE.match(lines[idx].strip()):
                body.append(lines[idx])
                idx += 1
            if idx >= len(lines):
                errors.append(
                    f"{rel(path)}:{lineno}: mermaid fence for {name} is not closed"
                )
                continue

            embedded = normalized_mermaid("\n".join(body))
            source = normalized_mermaid(src.read_text(encoding="utf-8"))
            if embedded != source:
                errors.append(
                    f"{rel(path)}:{lineno}: embedded diagram differs from"
                    f" docs/diagrams/{name} (update both copies)"
                )
            continue

        if FENCE_ANY_RE.match(stripped):
            if not in_fence:
                in_fence = True
            elif FENCE_CLOSE_RE.match(stripped):
                in_fence = False
    return referenced


def main() -> int:
    md_files = iter_markdown()
    if not md_files:
        errors.append("no markdown files found")
    referenced_all: set[str] = set()

    for path in md_files:
        stats["md"] += 1
        text = path.read_text(encoding="utf-8")
        lines = text.splitlines()
        check_links(path, lines)
        referenced_all |= check_mmd_markers(path, lines)

    mmd_files = sorted(DIAGRAMS.glob("*.mmd")) if DIAGRAMS.is_dir() else []
    stats["mmd_files"] = len(mmd_files)
    for mmd in mmd_files:
        if mmd.name not in referenced_all:
            errors.append(
                f"docs/diagrams/{mmd.name}: not referenced by any"
                " '<!-- mmd: ... -->' marker"
            )

    agents = REPO / "AGENTS.md"
    if not agents.exists():
        errors.append("AGENTS.md is missing")
    elif "docs/README.md" not in agents.read_text(encoding="utf-8"):
        errors.append("AGENTS.md does not reference docs/README.md")

    print(
        f"check_docs: {stats['md']} markdown files, {stats['links']} links,"
        f" {stats['mmd_refs']} mmd references, {stats['mmd_files']} diagrams"
    )
    if errors:
        print(f"\n{len(errors)} problem(s):", file=sys.stderr)
        for err in errors:
            print(f"  {err}", file=sys.stderr)
        return 1
    print("check_docs: OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
