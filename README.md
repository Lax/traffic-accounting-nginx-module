# Installation

Configure ngx_http_accounting_module as nginx module with ```--add-module``` when build nginx.

    cd /path/to/nginx-src/

    git clone https://github.com/Lax/ngx_http_accounting_module.git -b v1.0

    ./configure --add-module=ngx_http_accounting_module

    make && make install

# Configuration

Edit your nginx.conf.

Example:

```nginx
http{
    # turn on accounting function
    http_accounting  on;
    ...
    server {
        server_name example.com;
        
        # set accounting_id based on server, use variable
        http_accounting_id  $http_host;
        ...
        location / {
            # set accounting_id based on location
            http_accounting_id  accounting_id_str;
            ...
        }
    }
}
```

# Directives

http_accounting
--------------------
**syntax:** *http_accounting on | off*

**default:** *http_accounting off*

**context:** *http*

http_accounting_id
--------------------
**syntax:** *http_accounting_id <accounting_id>*

**default:** *http_accounting_id default*

**content:** *server, location*

Specifies current request belongs to which accounting_id.

This directive was first introduced in the v0.1 release, and can use variable in v1.0 and above.

# Usage

This module write statistics to syslog. You should edit your syslog configuration.

For sample configuration / utils, see: [Lax/ngx_http_accounting_module-utils](http://github.com/Lax/ngx_http_accounting_module-utils)

## Sample syslog output

    Apr  8 11:19:46 localhost NgxAccounting: pid:8555|from:1428463159|to:1428463186|accounting_id:default|requests:10|bytes_in:1400|bytes_out:223062|latency_ms:1873|upstream_latency_ms:1873|200:9|302:1

The output contains a list of k/v for the accounting metrics, in the sequence of:

|  key             |  meaning |
|------------------|----------|
| `pid`           | pid of nginx worker process |
| `from` / `to`   | metric was collected between these timestamps |
| `accounting_id` | identify for the accounting unit, you name it with `http_accounting_id` directive |
| `requests`      | count of total requests processed |
| `bytes_in`      | total bytes receiverd by the server |
| `bytes_out`     | total bytes send out by the server |
| `latency_ms`    | a sum of `$request_time`, in `millisecond` |
| `upstream_latency_ms`  | a sum of `$upstream_response_time`, in `millisecond` |
| `200` / `302` / `400` / `404` / `500` ... | count of requests which response is with http code `200`/`302`/`400`/`404`/`500`, etc |

# Branches

* master : new feathers
* v2-freeze-20110526 : works with nginx version(0.7.xx, 0.8.xx), nginx 0.9 is not tested. didn't work with nginx above 1.0.x.

# Author

Liu Lantao [Github](https://github.com/Lax)

# License

GPLv3
