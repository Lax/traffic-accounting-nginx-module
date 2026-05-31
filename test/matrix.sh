# Matrix definitions for module modes M1-M4
#
# Each mode defines:
#   IMAGE  - Docker runtime image tag (from Dockerfile.test)
#   MODS   - space-separated list of available modules (http/stream)
#   LOAD   - load_module directive(s) for nginx.conf
#   DESC   - human-readable description

declare -A IMAGE MODS LOAD DESC

IMAGE[m1]=acc-test:rt
MODS[m1]=http
LOAD[m1]="load_module modules/ngx_http_accounting_module.so;"
DESC[m1]="HTTP only (http-only build)"

IMAGE[m2]=acc-test:rt
MODS[m2]=stream
LOAD[m2]="load_module modules/ngx_stream_accounting_module.so;"
DESC[m2]="Stream only (stream-only build)"

IMAGE[m3]=acc-test:rt
MODS[m3]="http stream"
LOAD[m3]="load_module modules/ngx_http_accounting_module.so;
load_module modules/ngx_stream_accounting_module.so;"
DESC[m3]="HTTP + Stream separate (both build)"

IMAGE[m4]=acc-test:rt
MODS[m4]=http
LOAD[m4]="load_module modules/ngx_http_accounting_module-with-stream.so;"
DESC[m4]="HTTP with-stream (both build, combined .so)"
