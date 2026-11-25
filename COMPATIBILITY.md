# nginx compatibility policy

This document defines what "compatible" means for this module, how the
promise is kept, and what to do when upstream nginx moves.

## Supported window

At any time, CI verifies four nginx versions:

| field | meaning | example (2026-11) | who updates it |
|---|---|---|---|
| `stable` | current stable line | 1.30.8 | upstream-sentinel (automated PR) |
| `mainline` | current mainline line | 1.31.10 | upstream-sentinel (automated PR) |
| `previous_stable` | the stable line before `stable` | 1.28.3 | upstream-sentinel (automated PR) |
| `floor` | oldest version still verified; compile + suite, no SLA | 1.24.0 | maintainers only, by hand |

Commitments inside the window:

1. **compile**: zero warnings with `-Werror`, gcc and clang;
2. **behavior**: the functional suite (`tests/run-tests.sh`) is green;
3. **response time**: a new `stable` or `mainline` release is detected by
   the next daily scan, validated and turned into a window PR by the same
   run; the target is a green window on `master` within a few working days.
   Merging that PR is the only step that waits for a person.  Allow for
   GitHub's scheduler: the on-the-hour schedules here ran roughly 8-10.5
   hours late before they were moved off the hour.

Versions outside the window are best-effort: issues are welcome, no active
adaptation is promised.  The `floor` marks how far back that best-effort
compatibility is at least verified (anything older than the floor is
untested; the pre-v2 tags in this repository cover ancient nginx lines).

## How the window is kept in sync

**Single source of truth:** `.github/nginx-versions.json`.

- `.github/workflows/tests.yml` generates its build matrix from that file
  via `.github/scripts/gen-matrix.py` (full both/http/stream variant matrix
  on `stable` + `mainline`; `previous_stable` and `floor` get the `both`
  variant).  A window bump therefore requires no workflow edit.
- `.github/workflows/upstream-sentinel.yml` runs daily, off the hour:
  - parses the maintained lines from [nginx.org/en/download.html](https://nginx.org/en/download.html)
    (the page's own Mainline / Stable / Legacy sections are the authority;
    `.github/scripts/scan-upstream.py` has an offline selftest);
  - no change -> summary only;
  - change -> builds and runs the functional suite against the new
    versions, then (green) opens a PR that applies the window to the JSON
    *and* to the demo image tag, or (red) files an issue with the failing
    logs and leaves the baseline untouched for the next run to re-report;
  - at most one window PR is open at a time: while one is pending, later
    runs add their verdict as a comment on it instead of opening another;
  - the sentinel never merges anything -- a human reviews the PR.
- **CI evidence for a freshly opened window PR is not on the PR.**  The
  sentinel opens it with the workflow token, and the `pull_request`-triggered
  `tests` run on that event parks in `action_required` until a human approves
  it -- `gh pr checks` reports no checks on it.  The sentinel therefore
  dispatches the full `tests` matrix on the PR branch itself and links that
  run from the PR body.  Once someone pushes to the PR branch, the PR's own
  checks run as usual.  Merge a window PR only when a `tests` run on its
  current commit is green.
- What stops the sentinel from writing `master`, and what does not stop
  it, is in [What keeps automation off master](#what-keeps-automation-off-master)
  below.
- `.github/workflows/mainline-tip.yml` builds the module against the nginx
  development tip (GitHub mirror) nightly, as an early warning for API
  changes that have not been released yet.  Its log states the version of
  the nginx it actually built.  A red tip job is a signal to investigate
  upstream, never a merge blocker.
- Additional jobs on the current `stable` only: clang build, `--with-compat`
  build, dynamic-module smoke, ASAN soak, load conservation.
- The demo image (`samples/Dockerfile`) pins `NGX_TAG` to the same window
  stable on the official `nginx:<version>-alpine` image.  The sentinel moves
  it together with the JSON, and the `version window` CI job fails whenever
  the two disagree, so a hand-made window bump cannot leave the demo behind.

To run the whole compatibility check locally:

```sh
tests/lib/build-nginx.sh -s . -o /tmp/b -v both --nginx-version 1.30.8 --werror
tests/run-tests.sh /tmp/b/nginx both
```

## What keeps automation off `master`

One workflow holds a write-capable token: `upstream-sentinel`, and only its
`report` job (every other job and workflow is read-only).  That job opens a
PR on a new branch, comments on a pending one, files an issue, and dispatches
the `tests` matrix on that branch.

Measured on 2026-11-25, with a workflow token holding `contents: write`
against a throw-away branch carrying the same protection rules as `master`.
The instrument is kept in the repository -- not installed as a workflow --
at `tests/governance/workflow-token-probe.yml`, with the steps to repeat
it; the valid run is
[45533274170](https://github.com/Lax/traffic-accounting-nginx-module/actions/runs/45533274170)
(`PROBE_` lines in its job log):

| attempted write | outcome |
|---|---|
| push to an unprotected branch (control) | accepted |
| push to the protected branch | rejected: `GH006: Protected branch update failed` -- `Changes must be made through a pull request` |
| open a PR onto the protected branch, then merge it | PR opened; merge rejected: `At least 1 approving review is required by reviewers with write access` |
| push a commit that adds a workflow file, to an unprotected branch | rejected: `refusing to allow a GitHub App to create or update workflow` -- it needs the `workflows` permission |

So the sentinel cannot put anything on `master` by itself, and cannot change
its own instructions.  Two checkers keep that true, both run by the
`policy` job of `tests.yml` on every change:

- `.github/scripts/check_workflow_policy.py` -- write-level permissions
  only on that one job, no `workflows` permission anywhere, only the listed
  triggers, and in the privileged job no merging, tagging, releasing, direct
  API calls, downloads or installs, and no `git push` except the new window
  branch.  Its self-test plants a violation for every rule.
- `.github/scripts/selftest_sentinel.py` -- the sentinel's own logic, taken
  verbatim from the workflow file and run offline against a synthetic
  upstream change: what it applies, what it refuses, the PR body, the
  one-pending-PR lookup, and the syntax of every run script.

What this does **not** cover:

- **Administrators bypass the protection** (`enforce_admins` is off).  A
  person with the admin role, or anything using an admin token, can push to
  or merge into `master` without a review.  That is the one path by which
  content reaches `master` unreviewed.  Whether it is used is a matter of
  who holds the token, not something the sentinel or the `policy` job can
  prevent.
- **No status check is required to merge.**  Branch protection asks for one
  approving review and nothing else, so a PR can be merged while `tests` is
  red.  "Green before merge" is a discipline here, not an enforced gate.
- **Tags and releases are not protected.**  Branch protection covers the
  `master` branch and the repository has no rulesets, so by its permissions
  (`contents: write` covers refs) the sentinel's token could create tags and
  releases.  This was not measured.  What is enforced is that the
  `policy` job fails if the privileged scripts ever gain a command to do it.
- **The repository default for workflows without a `permissions:` block is
  read/write.**  Every workflow here declares its permissions, and the
  `policy` job fails on one that does not.

## Module/nginx coupling surface

Everything the module touches in nginx, in decreasing age-stability:

- **Very stable, relied upon**: module scaffolding (`NGX_MODULE_V1`,
  `ngx_command_t`, standard conf slots), phase-handler registration into
  `cmcf->phases[NGX_HTTP_LOG_PHASE|NGX_STREAM_LOG_PHASE].handlers`,
  config/variable APIs (`ngx_http_get_variable_index`,
  `ngx_http_get_indexed_variable` and the stream equivalents),
  `ngx_conf_get_module_*_conf`, rbtree, `ngx_hash_key_lc`,
  `ngx_calloc/ngx_free`, `ngx_add_timer`, `ngx_timeofday`,
  `ngx_log_set_log`, `ngx_slprintf`.
- **Public structs read directly** (the only realistic future breakage
  points): `r->request_length`, `r->connection->sent`, `r->err_status`,
  `r->headers_out.status`, `r->start_sec/start_msec`,
  `r->upstream_states[].response_time`, `s->received`, `s->status`,
  `s->start_sec/start_msec`, `s->upstream_states[].response_time`.
  nginx adds struct fields freely but has rarely removed or reshaped these;
  still, any *new* direct field access in a PR must name the nginx version
  the field appeared in and link the docs/commit.
- **Memory model**: all accounting state is `ngx_calloc`-owned with an
  explicit destroy path -- deliberately not tied to any nginx pool, so
  pool-implementation changes upstream cannot affect the module.

All `#if (nginx_version ...)` guards live in
`src/ngx_traffic_accounting_compat.h`, one macro per adaptation, with a
comment naming the nginx version that changed the API.  New guards go
there; nowhere else.

## Dynamic modules (.so)

nginx refuses to load a `.so` whose `NGX_MODULE_SIGNATURE` (computed from
the nginx version *and* configure flags) does not match the running
binary.  Consequences, by design:

- a prebuilt `.so` is only valid for the exact nginx version and configure
  flags it was built against -- release artifacts must name the target
  nginx version;
- the module's primary supported integration is static linking from source
  (`--add-module`), which sidesteps ABI entirely;
- `--with-compat` widens the signature to the core set and is verified by
  its own CI job, so downstream `--with-compat` builds work;
- the dynamic build (single `.so` registering both the HTTP and STREAM
  accounting modules, see README "Installation") is covered by
  `tests/dynamic-smoke.sh`: load, account, reload.
