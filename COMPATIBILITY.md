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
  it -- nothing on the PR page says so, and the PR shows "no checks".  The
  sentinel therefore dispatches the full `tests` matrix on the PR branch
  itself and links that run from the PR body.  Once someone pushes to the PR
  branch, the PR's own checks run as usual.  Merge a window PR only when a
  `tests` run on its current commit is green.
- Branch protection on `master` asks for one review and no status checks,
  so "green tests run before merge" is a discipline here, not an enforced
  gate.
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
