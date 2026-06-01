# Test Report: combined module cross-nginx compatibility

## Goal

Test whether the **combined** module (`ngx_http_accounting_module.so`, containing both HTTP and Stream modules) can be loaded on 3 nginx variants:

- http+stream nginx
- http-only nginx (no `--with-stream`)
- stream-only nginx (`--without-http`)

If the combined module works universally, the http-only/stream-only build options could be removed.

## Test Setup

3 containers via `Dockerfile.test`, each running a different nginx binary but all using the **same** combined `.so` built with `--with-compat --with-stream`.

| Container | Nginx build flags | Config | Expected |
|---|---|---|---|
| ta-test-http-stream | `--with-compat --with-stream` | `http{}` + `stream{}` | ✅ start |
| ta-test-http-only | `--with-compat` (no stream) | `http{}` only | ❌ dlopen fail |
| ta-test-stream-only | `--with-compat --without-http --with-stream` | `stream{}` only | ❌ dlopen fail |

## Results

All 3 tests passed as expected:

### 1. http+stream + combined ✅
```
[notice] start worker process 7
[notice] pid:7|start http traffic accounting
[notice] pid:7|start stream traffic accounting
```
→ Both HTTP and Stream accounting modules loaded successfully.

### 2. http-only + combined ❌
```
[emerg] dlopen() "...ngx_http_accounting_module.so" failed
  (Error relocating ... : ngx_stream_get_indexed_variable: symbol not found)
```
→ HTTP-only nginx lacks the Stream subsystem symbols.

### 3. stream-only + combined ❌
```
[emerg] dlopen() "...ngx_http_accounting_module.so" failed
  (Error relocating ... : ngx_http_get_indexed_variable: symbol not found)
```
→ Stream-only nginx lacks the HTTP subsystem symbols.

## Root Cause

The combined `.so` references symbols from both subsystems:
- `ngx_stream_get_indexed_variable` (from `src/stream/ngx_stream_accounting_module.c:290`)
- `ngx_http_get_indexed_variable` (from `src/http/ngx_http_accounting_module.c:300`)

Nginx uses `dlopen(RTLD_NOW)`, so any unresolved symbol causes immediate load failure.

## Conclusion

**http-only and stream-only build options cannot be removed.** The combined module only works on http+stream nginx. Users with single-protocol nginx builds must use the corresponding single-protocol module variant.
