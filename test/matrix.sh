declare -A IMAGE MODS LOAD DESC

IMAGE[m1]=acc-test:rt
MODS[m1]="http stream"
LOAD[m1]="load_module modules/ngx_http_accounting_module.so;"
DESC[m1]="HTTP + Stream combined (both blocks)"

IMAGE[m2]=acc-test:rt
MODS[m2]=http
LOAD[m2]="load_module modules/ngx_http_accounting_module.so;"
DESC[m2]="HTTP only (no stream block, test NULL check)"

IMAGE[m3]=acc-test:rt
MODS[m3]=stream
LOAD[m3]="load_module modules/ngx_http_accounting_module.so;"
DESC[m3]="Stream only (no http block, test NULL check)"
