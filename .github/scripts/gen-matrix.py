#!/usr/bin/env python3
"""Turn the nginx version window (.github/nginx-versions.json) into the CI
build matrix.

Single source of truth: .github/workflows/tests.yml reads the matrix and the
stable version from this script, and the upstream-sentinel workflow updates
the same JSON file -- a window bump therefore needs exactly one file change.

Variant coverage:
  stable, mainline        both + http + stream  (full variant matrix)
  previous_stable, floor  both only             (compile + behavior regression)

Usage:
  gen-matrix.py                    print the matrix JSON on stdout
  gen-matrix.py --github-output    also append matrix=/stable= to
                                   $GITHUB_OUTPUT (multiline-safe form)
"""
import json
import os
import re
import sys

BASELINE = os.path.join(os.path.dirname(__file__), "..", "nginx-versions.json")
FULL_VARIANTS = ("both", "http", "stream")

VERSION_SHAPE = re.compile(r"^\d+\.\d+\.\d+$")


def version_key(v):
    return tuple(int(x) for x in v.split("."))


def valid_version(v):
    """nginx release tarballs are named nginx-<maj>.<min>.<pat> with plain
    decimal components; reject anything else early (e.g. "1.30", "1.30.07",
    "1.24.0.1") so CI fails here, not with a confusing 404 later."""
    if not isinstance(v, str) or not VERSION_SHAPE.match(v):
        return False
    return v == ".".join(str(int(p)) for p in v.split("."))


def load_window():
    with open(BASELINE) as f:
        window = json.load(f)
    fields = ["floor", "previous_stable", "stable", "mainline"]
    missing = [f for f in fields if f not in window]
    if missing:
        raise SystemExit("invalid window file, missing: %s" % missing)
    for field in fields:
        if not valid_version(window[field]):
            raise SystemExit("invalid version in %s: %r" % (field, window[field]))
    if version_key(window["stable"]) < version_key(window["floor"]):
        raise SystemExit("stable %s older than floor %s"
                         % (window["stable"], window["floor"]))
    return window


def build_matrix(window):
    entries = []          # [{nginx, variants}]
    by_version = {}

    def add(version, variants):
        if version in by_version:
            by_version[version]["variants"].update(variants)
            return
        entry = {"nginx": version, "variants": set(variants)}
        by_version[version] = entry
        entries.append(entry)

    add(window["stable"], FULL_VARIANTS)
    add(window["mainline"], FULL_VARIANTS)
    add(window["previous_stable"], ("both",))
    add(window["floor"], ("both",))

    include = [{"nginx": e["nginx"], "variant": v}
               for e in entries
               for v in ("both", "http", "stream") if v in e["variants"]]
    return {"include": include}


def main():
    github_output = "--github-output" in sys.argv[1:]
    window = load_window()
    matrix_json = json.dumps(build_matrix(window), separators=(",", ":"))
    stable = window["stable"]

    print(matrix_json)

    if github_output:
        out_path = os.environ.get("GITHUB_OUTPUT")
        if not out_path:
            raise SystemExit("--github-output requires GITHUB_OUTPUT")
        with open(out_path, "a") as out:
            # multiline form: heredoc-delimited value survives json quoting
            out.write("matrix<<MATRIX_EOF\n%s\nMATRIX_EOF\n" % matrix_json)
            out.write("stable=%s\n" % stable)


if __name__ == "__main__":
    main()
