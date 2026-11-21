#!/usr/bin/env python3
"""Discover the maintained nginx release lines from nginx.org.

The supported-version window of this module (see COMPATIBILITY.md) is
reviewed against upstream by the upstream-sentinel workflow:

    scan-upstream.py scan
        fetch https://nginx.org/en/download.html and print the discovered
        window as JSON on stdout (exit 1 on any deviation from the expected
        page structure -- the workflow goes red so the breakage is visible).

    scan-upstream.py diff BASELINE DISCOVERED
        compare a baseline file (.github/nginx-versions.json) with a
        discovered window; print a JSON verdict on stdout:
            {"changed": bool, "changed_versions": [...],
             "proposed": {...}, "summary": "..."}
        exit 0 unless the input files are invalid (exit 2).  Never exits
        non-zero on a change: the decision is the caller's.

    scan-upstream.py selftest
        run offline fixture assertions for the parser (used by CI and
        hand-edits of this script).

The page's own section headings ("Mainline version" / "Stable version" /
"Legacy versions") are the authority for what is maintained; the script
deliberately does not derive branch roles from version-number parity.
"""
import json
import re
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone

DOWNLOAD_URL = "https://nginx.org/en/download.html"

# Only versions behind a real download link count; prose references like
# "see nginx-1.28.3 notes" inside a section must not be taken as a release.
DOWNLOAD_RE = re.compile(r'href="/download/nginx-(\d+\.\d+\.\d+)\.tar\.gz"')


class PageStructureError(Exception):
    pass


def extract_section(html, heading, next_headings):
    """Return the text between one <h4> heading and the next known heading."""
    start = html.find("<h4>%s</h4>" % heading)
    if start < 0:
        raise PageStructureError("section not found: %r" % heading)
    start += len("<h4>%s</h4>" % heading)
    end = len(html)
    for nxt in next_headings:
        pos = html.find("<h4>%s</h4>" % nxt, start)
        if pos >= 0:
            end = min(end, pos)
    return html[start:end]


def parse_versions(section_text):
    """Unique versions behind download links, in page order; the page lists
    only the newest release of each line, first entry = latest of the
    section."""
    seen = []
    for m in DOWNLOAD_RE.finditer(section_text):
        v = m.group(1)
        if v not in seen:
            seen.append(v)
    return seen


def parse_window(html):
    """-> dict with mainline / stable / previous_stable."""
    sections = {}
    headings = ["Mainline version", "Stable version", "Legacy versions",
                "Source Code"]
    for i, heading in enumerate(headings[:-1]):
        sections[heading] = extract_section(html, heading, headings[i + 1:])

    mainline = parse_versions(sections["Mainline version"])
    stable = parse_versions(sections["Stable version"])
    legacy = parse_versions(sections["Legacy versions"])

    if not mainline or not stable or not legacy:
        raise PageStructureError("a section lists no versions")
    if len(mainline) > 1 or len(stable) > 1:
        raise PageStructureError("mainline/stable sections list multiple "
                                 "lines: %r %r" % (mainline, stable))

    def key(v):
        return tuple(int(x) for x in v.split("."))

    prev = legacy[0]
    if key(stable[0]) <= key(prev):
        raise PageStructureError("stable %s is not newer than its "
                                 "predecessor %s" % (stable[0], prev))
    if key(mainline[0]) == key(stable[0]):
        raise PageStructureError("mainline equals stable (%s)" % mainline[0])
    if key(mainline[0]) == key(prev):
        raise PageStructureError("mainline equals previous_stable (%s)"
                                 % mainline[0])

    return {
        "mainline": mainline[0],
        "stable": stable[0],
        "previous_stable": prev,
        "legacy_lines": legacy,
    }


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": "nginx-versions-scan"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return resp.read().decode("utf-8", "replace")


def cmd_scan(url):
    try:
        html = fetch(url)
    except urllib.error.HTTPError as e:
        print("fetch failed: HTTP %s from %s" % (e.code, url), file=sys.stderr)
        return 1
    except (urllib.error.URLError, OSError) as e:
        print("fetch failed: %s" % e, file=sys.stderr)
        return 1
    window = parse_window(html)
    window["scanned_at"] = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    window["source"] = url
    print(json.dumps(window, indent=2))
    return 0


def cmd_diff(baseline_path, discovered_path):
    try:
        with open(baseline_path) as f:
            baseline = json.load(f)
        with open(discovered_path) as f:
            discovered = json.load(f)
    except (OSError, ValueError) as e:
        print("invalid input: %s" % e, file=sys.stderr)
        return 2

    for field in ("floor", "previous_stable", "stable", "mainline"):
        if field not in baseline:
            print("invalid baseline: missing %r" % field, file=sys.stderr)
            return 2
    for field in ("stable", "mainline", "previous_stable"):
        if field not in discovered:
            print("invalid discovery: missing %r" % field, file=sys.stderr)
            return 2

    window_fields = ("stable", "mainline", "previous_stable")
    changed = [f for f in window_fields if baseline[f] != discovered[f]]

    proposed = dict(baseline)
    proposed.update({f: discovered[f] for f in window_fields})
    if changed:
        proposed["window_updated"] = datetime.now(timezone.utc).strftime("%Y-%m-%d")

    summary = "no window change" if not changed else \
        "; ".join("%s: %s -> %s" % (f, baseline[f], discovered[f]) for f in changed)
    print(json.dumps({
        "changed": bool(changed),
        "changed_fields": changed,
        "changed_versions": sorted({discovered[f] for f in changed}),
        "proposed": proposed,
        "summary": summary,
    }, indent=2))
    return 0


# --- selftest fixtures -------------------------------------------------------
# Modeled on the real https://nginx.org/en/download.html structure.

FIXTURE_OK = """
<h2>nginx: download</h2>
<center><h4>Mainline version</h4></center>
<table width="100%"><tr>
<td><a href="/download/nginx-1.31.9.tar.gz">nginx-1.31.9</a>
    <a href="/download/nginx-1.31.9.tar.gz.asc">pgp</a></td>
<td><a href="/download/nginx-1.31.9.zip">nginx/Windows-1.31.9</a></td></tr></table>
<center><h4>Stable version</h4></center>
<table width="100%"><tr>
<td><a href="/download/nginx-1.30.7.tar.gz">nginx-1.30.7</a>
    <a href="/download/nginx-1.30.7.tar.gz.asc">pgp</a></td>
<td><a href="/download/nginx-1.30.7.zip">nginx/Windows-1.30.7</a></td></tr></table>
<center><h4>Legacy versions</h4></center>
<table width="100%"><tr><td><a href="/download/nginx-1.28.3.tar.gz">nginx-1.28.3</a></td>
<td><a href="/download/nginx-1.28.3.zip">nginx/Windows-1.28.3</a></td></tr></table>
<table width="100%"><tr><td><a href="/download/nginx-1.26.3.tar.gz">nginx-1.26.3</a></td>
<td><a href="/download/nginx-1.26.3.zip">nginx/Windows-1.26.3</a></td></tr></table>
<table width="100%"><tr><td><a href="/download/nginx-1.24.0.tar.gz">nginx-1.24.0</a></td></tr></table>
<center><h4>Source Code</h4></center><p>trunk, etc.</p>
"""

FIXTURE_NO_SECTIONS = "<h2>nginx: download</h2><p>layout changed</p>"

FIXTURE_EMPTY_MAINLINE = """
<center><h4>Mainline version</h4></center>
<table width="100%"></table>
<center><h4>Stable version</h4></center>
<table width="100%"><tr><td><a href="/download/nginx-1.30.7.tar.gz">nginx-1.30.7</a></td></tr></table>
<center><h4>Legacy versions</h4></center>
<table width="100%"><tr><td><a href="/download/nginx-1.28.3.tar.gz">nginx-1.28.3</a></td></tr></table>
<center><h4>Source Code</h4></center>
"""

# a prose mention is not a release: no download link in the section
FIXTURE_PROSE_MAINLINE = """
<center><h4>Mainline version</h4></center>
<p>see the nginx-1.33.0 notes somewhere else</p>
<center><h4>Stable version</h4></center>
<table width="100%"><tr><td><a href="/download/nginx-1.30.7.tar.gz">nginx-1.30.7</a></td></tr></table>
<center><h4>Legacy versions</h4></center>
<table width="100%"><tr><td><a href="/download/nginx-1.28.3.tar.gz">nginx-1.28.3</a></td></tr></table>
<center><h4>Source Code</h4></center>
"""

# mainline line that equals the first legacy line (stale page state)
FIXTURE_MAINLINE_EQ_PREV = """
<center><h4>Mainline version</h4></center>
<table width="100%"><tr><td><a href="/download/nginx-1.28.3.tar.gz">nginx-1.28.3</a></td></tr></table>
<center><h4>Stable version</h4></center>
<table width="100%"><tr><td><a href="/download/nginx-1.30.7.tar.gz">nginx-1.30.7</a></td></tr></table>
<center><h4>Legacy versions</h4></center>
<table width="100%"><tr><td><a href="/download/nginx-1.28.3.tar.gz">nginx-1.28.3</a></td></tr></table>
<center><h4>Source Code</h4></center>
"""


def cmd_selftest():
    w = parse_window(FIXTURE_OK)
    assert w == {"mainline": "1.31.9", "stable": "1.30.7",
                 "previous_stable": "1.28.3",
                 "legacy_lines": ["1.28.3", "1.26.3", "1.24.0"]}, w

    bad_fixtures = ((FIXTURE_NO_SECTIONS, "missing sections"),
                    (FIXTURE_EMPTY_MAINLINE, "empty mainline"),
                    (FIXTURE_PROSE_MAINLINE, "prose-only version mention"),
                    (FIXTURE_MAINLINE_EQ_PREV, "mainline equals previous"))
    for bad, why in bad_fixtures:
        try:
            parse_window(bad)
        except PageStructureError:
            pass
        else:
            raise AssertionError("expected PageStructureError (%s)" % why)

    print("selftest: %d fixture groups OK" % (1 + len(bad_fixtures)))
    return 0


def main():
    args = sys.argv[1:]
    if args == ["selftest"]:
        return cmd_selftest()
    if args and args[0] == "scan":
        rest = args[1:]
        url = DOWNLOAD_URL
        if rest:
            if len(rest) != 2 or rest[0] != "--url":
                print(__doc__, file=sys.stderr)
                return 2
            url = rest[1]
        return cmd_scan(url)
    if len(args) == 3 and args[0] == "diff":
        return cmd_diff(args[1], args[2])
    print(__doc__, file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main())
