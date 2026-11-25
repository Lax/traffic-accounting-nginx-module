# Changelog

Releases before v3.2.0 are on the
[Releases](https://github.com/Lax/traffic-accounting-nginx-module/releases)
page.  This file starts with the first release that carries the changes
below.

## v3.2.0

A source release: build the module from source (README, Quickstart).  It
ships no prebuilt binaries -- see the end of this entry.

### Security and accounting integrity

These fixes come from the review merged as #80.  As there, each
description gives what is needed to judge exposure -- the impact, the
condition under which it applies, the severity -- and not how to trigger
it.

- **Denial of service -- high.**  With `accounting_id` derived from a
  client-controlled variable (as in the README examples), an
  unauthenticated client could grow the number of tracked ids without
  bound, and each period export then blocked the worker's event loop for
  longer as that number grew.  Deployments with no variable in
  `accounting_id` were not exposed to this.  Both ends are fixed: the
  number of tracked ids is bounded (`accounting_max_metrics`, below) and
  the export is linear in that number.
- **Undercounting -- medium.**  Under certain conditions traffic did not
  reach the books at all.  Every request and session is accounted now;
  one that carries no usable id goes to `__default__`.
- **Robustness -- low.**  A worker could crash when an allocation failed
  under memory pressure, and a lookup could settle on the wrong node
  under extreme hash-key collisions.  Allocation failures are handled,
  lookups compare names exactly, and a period rotation that fails logs
  an alert and is retried at the next interval.
- **Downstream data integrity -- low.**  Exported lines carried a
  trailing NUL byte, and id contents were not filtered before they
  entered the pipe-separated format.

### Changed behaviour -- read before upgrading

- **Unattributed traffic moved from `default` to `__default__`.**  Where
  `accounting_id` is not set, traffic used to be reported under
  `default`, the old default value of the directive.  It is now reported
  under `__default__`, so dashboards and alerts that pick up
  unattributed traffic by the name `default` have to be updated.  An
  explicit `accounting_id default;` keeps reporting under `default`.
- **Traffic with an empty or unset id is accounted** (under
  `__default__`); it used to be left out of the books.  Expect totals to
  rise accordingly.
- **New directive `accounting_max_metrics`** (`http`, `stream`; default
  16384, clamped to 16..1048576): how many distinct ids are tracked per
  reporting interval, per worker process.  Further ids are accounted
  under `__overflow__`, and an alert is logged once per interval.  Raise
  it if you legitimately serve more distinct ids than the default.
- **Accounting ids are normalized** before they are used as keys:
  truncated to 255 bytes, with control characters, `DEL` and the field
  separator `|` replaced by `_`.
- **`accounting_interval` is clamped** to 1s..30d.

### Development

The state of the repository at the time of this release:

- The nginx versions CI verifies form an explicit window
  (`.github/nginx-versions.json`): stable 1.30.8, mainline 1.31.10,
  previous stable 1.28.3 and floor 1.24.0, kept in step with upstream by
  a daily sentinel.  The policy is in [COMPATIBILITY.md](COMPATIBILITY.md).
- A `policy` job checks on every change that only the sentinel's
  `report` job holds a write-capable token, and that it can only open
  the window PR.  What keeps automation off `master` is measured and
  documented; the steps are in `tests/governance/`.
- A 17-case black-box functional suite, an ASAN soak and a load
  conservation harness, under `tests/`.
- The demo image is built on the official `nginx:<version>-alpine`
  image, and the docker-compose ELK stack runs on current images.

### Prebuilt binaries

This release ships none.  The `.so` files attached to v3.0.0 are
musl-linked x86-64 objects, which load into Alpine-style nginx builds
only, and that code has none of the fixes above.  Build from source.
