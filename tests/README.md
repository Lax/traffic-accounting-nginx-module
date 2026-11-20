# traffic-accounting-nginx-module — test infrastructure

This directory contains a self-contained test infrastructure for the module:
it builds stock nginx (from nginx.org) with the module statically linked in,
runs a functional black-box suite against the exported accounting log, and
drives load/soak tests for conservation and memory-safety checks.

Everything is parameterized by the **module source tree**, so the exact same
suite can be run against any branch, commit or pull request without touching
the working tree:

```sh
# 1. build (variants: both | http | stream; add --asan and/or --werror)
tests/lib/build-nginx.sh -s /path/to/module-source -o /tmp/build/pr -v both --werror

# 2. functional suite (17 tests; stream tests auto-skip on http-only builds)
tests/run-tests.sh /tmp/build/pr/nginx both

# 3. load conservation (exact request accounting across rotations)
tests/load/run-conservation.sh /tmp/build/pr/nginx --total 90000 --interval 5

# 4. export-stall / O(n^2) regression check
tests/load/run-conservation.sh /tmp/build/pr/nginx --total 20000 --interval 5 \
    --max-metrics 1000000 --label stall
# watch worst_export_spread_s in the output: seconds between the first and
# last line of one export batch. A growing spread with growing cardinality
# means the export loop stalls the worker event loop again.

# 5. ASAN soak (build with --asan first)
tests/load/run-soak.sh /tmp/build/pr-asan/nginx --total 30000 --rotations 6
```

Layout
------

| path | purpose |
|---|---|
| `lib/build-nginx.sh` | builds nginx + module from any source tree (cached upstream tarball, concurrent-safe) |
| `lib/harness.sh` | shared helpers: nginx lifecycle, port picking, config boilerplate, assertions |
| `lib/parseacct.py` | parser for exported accounting lines (sum / ids / status / per-batch timestamp spread) |
| `lib/rawreq.py` | raw-byte HTTP sender (HTTP/1.0 without Host, header values with `\|`) |
| `run-tests.sh` | functional suite runner |
| `t/t01..t17` | functional tests (see below) |
| `load/genload.c` | epoll-based pipelined load generator with exact response accounting |
| `load/run-conservation.sh` | conservation + export-spread measurement |
| `load/run-soak.sh` | ASAN soak across many rotations |

Functional tests
----------------

| test | verifies |
|---|---|
| t01 basic-http | fixed id counted once per request, statuses/bytes exported |
| t02 variable-id | `$arg_uid`-derived ids counted separately |
| t03 default-bucket-merge | all four classes of unattributed traffic merge into `__default__`; attributed traffic separate |
| t04 truncation-255 | ids >255 bytes truncated to 255 and still accounted (old builds dropped them silently = billing bypass) |
| t05 sanitize-separators | `\|` (query + Host header) sanitized to `_`; parser-rejected (400) requests still accounted; no raw separator/control bytes in any id |
| t06 collision-case-pair | case-variant ids (same `ngx_hash_key_lc`, different bytes) stay separate nodes with exact counts |
| t07 cap-overflow | `accounting_max_metrics` cap: excess ids land in `__overflow__`, one-time alert |
| t08 default-exempt-from-cap | `__default__` gets its own node even past the cap |
| t09 empty-period | idle periods rotate cleanly, later traffic counted |
| t10 graceful-export | unrotated period exported at graceful shutdown |
| t11 config-clamps | interval clamped to [1s, 30d], max_metrics to [16, 1048576] with warnings |
| t12 multi-worker | per-worker periods, totals conserved |
| t13 reload | SIGHUP mid-period: no crash, totals conserved across retiring+new workers |
| t14 stream-basic | stream sessions counted (`sessions`, `bytes_out`, status 200) |
| t15 stream-cap | stream cap + overflow with per-connection `$remote_port` ids |
| t16 reserved-merge | organic ids equal to reserved words merge into the reserved buckets, never split |
| t17 export-bytes-clean | no NUL byte inside exported lines (old builds wrote a trailing NUL that corrupted downstream parsers) |

Notes
-----

- `accounting_id` resolves a value as a variable only when the value is a
  single `$variable`; `prefix-$var` concatenation is treated as a literal.
- Control/DEL bytes cannot reach module variables through nginx's own request
  parsers (rejected with 400); those requests must still be accounted under
  `__default__` (verified by t05). Sanitizing them inside the module is
  defense in depth.
- Any TCP probe against a stream server is itself a session; stream tests use
  `wait_master` instead of a port probe for that reason.
