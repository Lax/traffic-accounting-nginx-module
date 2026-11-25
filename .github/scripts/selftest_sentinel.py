#!/usr/bin/env python3
"""Offline self-test of upstream-sentinel.yml's own logic.

The code under test is taken verbatim from the workflow file -- the python
of the 'extract verdict' and 'apply validated window' steps, the PR body
block and the pending-PR lookup of the 'open window PR' step -- so this
tests what CI runs, not a copy of it.  Nothing here talks to GitHub: the
upstream change is synthetic, and it is produced by scan-upstream.py's own
`diff` so that the output contract of `diff` and the input contract of the
apply step are checked against each other.

Checks:
  extract verdict   turns a verdict.json into job outputs
  apply window      positive: JSON and demo tag move together, exactly one
                    Dockerfile line changes; negative: non x.y.z version,
                    missing or duplicated ARG NGX_TAG line, unchanged verdict
  PR body           renders with the evidence link and nothing unexpanded
  pending-PR lookup one window PR at a time, on crafted pull-request lists
  script syntax     bash -n on every run script of every workflow

Usage: selftest_sentinel.py            (from anywhere; exit 0 = all pass)
Needs jq, as the GitHub runner has it.
"""
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

import yaml

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
SENTINEL = os.path.join(ROOT, ".github", "workflows", "upstream-sentinel.yml")
FAILS = []


def ok(msg):
    print("PASS  %s" % msg)


def bad(msg):
    print("FAIL  %s" % msg)
    FAILS.append(msg)


def expect(label, condition, detail=""):
    if condition:
        ok(label)
    else:
        bad("%s%s" % (label, (": " + detail) if detail else ""))


def step_run(wf, name):
    for job in wf["jobs"].values():
        for step in job.get("steps", []):
            if step.get("name") == name:
                return step["run"]
    sys.exit("selftest: step %r not found in the sentinel workflow" % name)


def python_heredoc(script):
    m = re.search(r"<<'EOF'\n(.*?)\nEOF\n", script, re.S)
    if not m:
        sys.exit("selftest: python heredoc not found")
    return m.group(1) + "\n"


def run_python(code, cwd, env=None):
    return subprocess.run([sys.executable, "-c", code], cwd=cwd, env=env,
                          capture_output=True, text=True)


def next_patch(version):
    major, patch = version.rsplit(".", 1)
    return "%s.%d" % (major, int(patch) + 1)


def parse_github_output(path):
    """Read a $GITHUB_OUTPUT file: `key=value` lines and `key<<DELIM` blocks."""
    outputs = {}
    if not os.path.exists(path):
        return outputs
    lines = open(path).read().split("\n")
    i = 0
    while i < len(lines):
        m = re.match(r"(\w+)<<(\w+)$", lines[i])
        if m:
            key, delim = m.group(1), m.group(2)
            end = lines.index(delim, i + 1)          # raises if unterminated
            outputs[key] = "\n".join(lines[i + 1:end])
            i = end + 1
        elif "=" in lines[i]:
            key, value = lines[i].split("=", 1)
            outputs[key] = value
            i += 1
        else:
            i += 1
    return outputs


# ---- a synthetic upstream change, through the real `diff` ------------------
baseline = json.load(open(os.path.join(ROOT, ".github", "nginx-versions.json")))
discovered = {
    "stable": next_patch(baseline["stable"]),
    "mainline": next_patch(baseline["mainline"]),
    "previous_stable": baseline["previous_stable"],
}
work = tempfile.mkdtemp(prefix="sentinel-selftest-")
disc_path = os.path.join(work, "discovered.json")
base_path = os.path.join(work, "baseline.json")
with open(disc_path, "w") as f:
    json.dump(discovered, f)
with open(base_path, "w") as f:
    json.dump(baseline, f)
with open(os.path.join(work, "verdict.json"), "w") as f:
    subprocess.run([sys.executable, os.path.join(HERE, "scan-upstream.py"),
                    "diff", base_path, disc_path], stdout=f, check=True)
verdict = json.load(open(os.path.join(work, "verdict.json")))
expect("synthetic change is a change", verdict["changed"], json.dumps(verdict)[:120])
NEW_STABLE = discovered["stable"]
print("      synthetic upstream change: %s" % verdict["summary"])

wf = yaml.safe_load(open(SENTINEL))


def scratch(verdict_obj):
    """A tree as the report job sees it after checkout + artifact download."""
    d = tempfile.mkdtemp(prefix="sentinel-tree-")
    os.makedirs(os.path.join(d, "scan-artifacts"))
    os.makedirs(os.path.join(d, ".github"))
    os.makedirs(os.path.join(d, "samples"))
    with open(os.path.join(d, "scan-artifacts/verdict.json"), "w") as f:
        json.dump(verdict_obj, f)
    shutil.copy(os.path.join(ROOT, ".github", "nginx-versions.json"),
                os.path.join(d, ".github", "nginx-versions.json"))
    shutil.copy(os.path.join(ROOT, "samples", "Dockerfile"), os.path.join(d, "samples", "Dockerfile"))
    return d


try:
    # ---- extract verdict ---------------------------------------------------
    script = step_run(wf, "extract verdict")
    out_file = os.path.join(work, "github_output")
    env = dict(os.environ, RUNNER_TEMP=work, GITHUB_OUTPUT=out_file)
    r = run_python(python_heredoc(script), work, env)
    outputs = parse_github_output(out_file)
    expect("extract verdict exits 0", r.returncode == 0, r.stderr.strip()[-200:])
    expect("extract verdict: changed=true", outputs.get("changed") == "true", repr(outputs.get("changed")))
    want_versions = " ".join(sorted(set(verdict["changed_versions"])))
    expect("extract verdict: lists the changed versions",
           outputs.get("versions") == want_versions, "%r != %r" % (outputs.get("versions"), want_versions))
    expect("extract verdict: carries the summary", verdict["summary"] in outputs.get("summary", ""),
           repr(outputs.get("summary")))

    # ---- apply validated window --------------------------------------------
    apply_py = python_heredoc(step_run(wf, "apply validated window"))
    d = scratch(verdict)
    r = run_python(apply_py, d)
    expect("apply: exits 0 on the synthetic verdict", r.returncode == 0, r.stderr.strip()[-300:])
    win = json.load(open(os.path.join(d, ".github", "nginx-versions.json")))
    docker = open(os.path.join(d, "samples", "Dockerfile")).read()
    expect("apply: window JSON takes stable=%s mainline=%s" % (NEW_STABLE, discovered["mainline"]),
           win.get("stable") == NEW_STABLE and win.get("mainline") == discovered["mainline"], json.dumps(win)[:160])
    expect("apply: window JSON keeps floor and _readme",
           win.get("floor") == baseline["floor"] and win.get("_readme") == baseline.get("_readme"))
    tag = re.search(r"(?m)^ARG NGX_TAG=(\S+)$", docker)
    expect("apply: demo tag follows the stable", tag is not None and tag.group(1) == NEW_STABLE + "-alpine",
           repr(tag and tag.group(1)))
    before = open(os.path.join(ROOT, "samples", "Dockerfile")).read().splitlines()
    after = docker.splitlines()
    differ = [(a, b) for a, b in zip(before, after) if a != b]
    expect("apply: exactly one Dockerfile line changes",
           len(before) == len(after) and len(differ) == 1 and "NGX_TAG=" in differ[0][0],
           str(differ[:3]))

    def refused(label, verdict_obj, needle, dockerfile=None):
        d = scratch(verdict_obj)
        if dockerfile is not None:
            with open(os.path.join(d, "samples", "Dockerfile"), "w") as f:
                f.write(dockerfile)
        r = run_python(apply_py, d)
        expect(label, r.returncode != 0 and needle in r.stderr,
               "rc=%d stderr=%s" % (r.returncode, r.stderr.strip()[-160:]))
        return d

    d = refused("apply refuses a non x.y.z stable",
                dict(verdict, proposed=dict(verdict["proposed"], stable="1.30")),
                "refusing to apply a non x.y.z stable")
    expect("... and leaves the JSON untouched",
           json.load(open(os.path.join(d, ".github", "nginx-versions.json"))) == baseline)
    refused("apply refuses a Dockerfile without the ARG NGX_TAG line", verdict,
            "expected exactly one 'ARG NGX_TAG=' line, found 0", dockerfile="FROM nginx\n")
    dockerfile_two = open(os.path.join(ROOT, "samples", "Dockerfile")).read() + "ARG NGX_TAG=9.9.9-alpine\n"
    refused("apply refuses a Dockerfile with two ARG NGX_TAG lines", verdict,
            "expected exactly one 'ARG NGX_TAG=' line, found 2", dockerfile=dockerfile_two)
    refused("apply refuses an unchanged verdict", dict(verdict, changed=False),
            "no longer reports a change")

    # ---- PR body -----------------------------------------------------------
    script = step_run(wf, "open window PR")
    m = re.search(r"^\s*\{\n(.*?)\n\s*\} > pr-body\.md\n", script, re.S | re.M)
    if not m:
        sys.exit("selftest: PR body block not found")
    body_block = re.sub(r"\$\{\{.*?\}\}", want_versions, m.group(0))
    d = scratch(verdict)
    env = dict(os.environ,
               SUMMARY=verdict["summary"], STABLE=NEW_STABLE,
               BR="nginx-window-20260101-1", GITHUB_SERVER_URL="https://github.com",
               GITHUB_REPOSITORY="o/r", GITHUB_RUN_ID="1",
               RUN_URL="https://github.com/o/r/actions/runs/1")
    r = subprocess.run(["bash", "-c", body_block], cwd=d, env=env, capture_output=True, text=True)
    body_path = os.path.join(d, "pr-body.md")
    body = open(body_path).read() if (r.returncode == 0 and os.path.exists(body_path)) else ""
    expect("PR body renders", r.returncode == 0 and body.strip(), r.stderr.strip()[-200:])
    for label, needle in [
        ("the summary", verdict["summary"]),
        ("the demo tag", "ARG NGX_TAG=%s-alpine" % NEW_STABLE),
        ("the link to the matrix run",
         "https://github.com/o/r/actions/workflows/tests.yml?query=branch%3Anginx-window-20260101-1"),
        ("why the matrix is dispatched explicitly", "action_required"),
        ("the official image to check", "https://hub.docker.com/_/nginx/tags?name=%s-alpine" % NEW_STABLE),
        ("each validated version", "- nginx " + NEW_STABLE),
    ]:
        expect("PR body has %s" % label, needle in body, repr(needle))
    expect("PR body has nothing unexpanded", "${{" not in body)

    # ---- pending-PR lookup: one window PR at a time --------------------------
    m = re.search(r"^existing=\$\(.*?\)\n", script, re.S | re.M)
    jq = re.search(r"--jq '(\[.*)'", m.group(0).replace("\\\n", " ")) if m else None
    if not jq:
        sys.exit("selftest: pending-PR lookup not found")
    cases = [
        ("no PRs at all", [], ""),
        ("only unrelated PRs",
         [{"number": 7, "headRefName": "feat/x"}, {"number": 9, "headRefName": "fix/y"}], ""),
        ("one window PR", [{"number": 8, "headRefName": "nginx-window-20260101-5"}], "8"),
        ("unrelated first, then a window PR",
         [{"number": 7, "headRefName": "feat/x"}, {"number": 91, "headRefName": "nginx-window-a"}], "91"),
        ("a look-alike prefix is not a window PR",
         [{"number": 93, "headRefName": "nginx-windowx"}, {"number": 94, "headRefName": "nginx-window-ok"}], "94"),
    ]
    for label, prs, want in cases:
        r = subprocess.run(["jq", "-r", jq.group(1)], input=json.dumps(prs),
                           capture_output=True, text=True)
        expect("pending-PR lookup: %s -> %r" % (label, want), r.returncode == 0 and r.stdout.strip() == want,
               "rc=%d out=%r err=%s" % (r.returncode, r.stdout.strip(), r.stderr.strip()[-120:]))

    # ---- script syntax -------------------------------------------------------
    n_scripts = 0
    broken = []
    wf_dir = os.path.join(ROOT, ".github", "workflows")
    for name in sorted(os.listdir(wf_dir)):
        if not name.endswith((".yml", ".yaml")):
            continue
        doc = yaml.safe_load(open(os.path.join(wf_dir, name)))
        for job_name, job in doc["jobs"].items():
            for index, step in enumerate(job.get("steps") or []):
                if "run" not in step:
                    continue
                n_scripts += 1
                # GitHub substitutes ${{ }} with literal text before bash sees it
                prepared = re.sub(r"\$\{\{.*?\}\}", "XPR", step["run"])
                s = subprocess.run(["bash", "-n"], input=prepared, capture_output=True, text=True)
                if s.returncode != 0:
                    broken.append("%s/%s[%d]: %s" % (name, job_name, index, s.stderr.strip()))
    expect("bash -n clean on all %d run scripts" % n_scripts, not broken, "; ".join(broken))
finally:
    shutil.rmtree(work, ignore_errors=True)

print("---")
print("%d failure(s)" % len(FAILS))
sys.exit(1 if FAILS else 0)
