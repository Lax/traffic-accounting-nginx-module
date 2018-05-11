# About

Monitor the incoming and outgoing traffic metrics in realtime for `NGINX` http subsystem.

Realtime traffic and status code monitor solution for NGINX, need less memory and cpu than realtime log analyzing.
Useful for http traffic accounting based on NGINX config logic ( by-location or by-server or by-user-defined-variable ).

## Why?

Real-time log analysis solution,
which requires multiple machines for storage and analysis,
are too heavy for application monitoring.

An cost-effective solution is needed to monitor the traffic metrics/status of application requests.
That solution should be accurate, sensitive, robust, light weight enough, and not affected by traffic peaks.

## How it works?

The context of this module keeps a list of **metrics** identified by `accounting_id`.

When a new **request** hits the server, the module will try to find its `accounting_id`, calculate statistics, and **aggregate** them into the corresponding metrics.

For every period (defined by `interval`), a timer event is triggered, these metrics are rotated and exported to log files or remote log servers.

---

# Installation
[![FOSSA Status](https://app.fossa.io/api/projects/git%2Bgithub.com%2FLax%2Fnginx-http-accounting-module.svg?type=shield)](https://app.fossa.io/projects/git%2Bgithub.com%2FLax%2Fnginx-http-accounting-module?ref=badge_shield)


Configure `nginx-http-accounting-module` as NGINX module with ```--add-module``` when build NGINX.

    cd /path/to/nginx-src/
    git clone https://github.com/Lax/nginx-http-accounting-module.git
    ./configure --add-module=nginx-http-accounting-module
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

        location /api {
            # for pc
            http_accounting_id  accounting_id_pc;
            # for mobile
            if ($http_user_agent ~* '(Android|webOS|iPhone|iPod|BlackBerry)') {
                http_accounting_id  accounting_id_mobile;
            }
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

http_accounting_log
--------------------
**syntax:** *http_accounting_log \</path/to/log/file> \[level]*

**default:** *-*

**context:** *http*

Configures logging.
Support both local `file` path, or `stderr`, or `syslog:`.
The second parameter is the log level.
For more details of supported params, refer to [this page from nginx.org](http://nginx.org/en/docs/ngx_core_module.html#error_log).

If not specified, accounting log will be written to `/dev/log`.

http_accounting_id
--------------------
**syntax:** *http_accounting_id \<accounting_id>*

**default:** *http_accounting_id default*

**context:** *server, location, if in location*

Sets the `accounting_id` string by user defined variable.
This string is used to determine which `metrics` a request/session should be aggregated to.

http_accounting_interval
------------------------
**syntax:** *http_accounting_interval \<seconds>*

**default:** *http_accounting_interval 60*

**context:** *http*

Specifies the reporting interval.  Defaults to 60 seconds.

http_accounting_perturb
------------------------
**syntax:** *http_accounting_perturb on | off*

**default:** *http_accounting_perturb off*

**context:** *http*

Randomly staggers the reporting interval by 20% from the usual time.

# Usage

This module can be configured to writes statistics to local file, remote log server or local syslog device.

To collect logs with syslog,
refer [Lax/ngx_http_accounting_module-utils](http://github.com/Lax/ngx_http_accounting_module-utils) to for sample configuration / utils.

Some open-source log-aggregation software such as logstash also support syslog input, which will help you establish a central log server.

## Metrics log format

    Apr  8 11:19:46 localhost NgxAccounting: pid:8555|from:1428463159|to:1428463186|accounting_id:default|requests:10|bytes_in:1400|bytes_out:223062|latency_ms:1873|upstream_latency_ms:1873|200:9|302:1

The output contains a list of k/v for the accounting metrics, in the sequence of:

|  key             |  meaning |
|------------------|----------|
| `pid`           | pid of nginx worker process |
| `from` / `to`   | metric was collected between these timestamps |
| `accounting_id` | identify the accounting unit, set with `http_accounting_id` directive |
| `requests`      | count of total requests processed in current period |
| `bytes_in`      | total bytes receiverd by the server |
| `bytes_out`     | total bytes send out by the server |
| `latency_ms`    | sum of `$request_time`, in `millisecond` |
| `upstream_latency_ms`  | sum of `$upstream_response_time`, in `millisecond` |
| `200` / `302` / `400` / `404` / `500` ... | count of requests with http status code `200`/`302`/`400`/`404`/`500`, etc |

# Branches

* master : main development branch.
* v2-freeze-20110526 : legacy release. works with nginx version(0.7.xx, 0.8.xx), nginx 0.9 is not tested. didn't work with nginx above 1.0.x.

# Contributing

1. Fork it ( https://github.com/Lax/nginx-http-accounting-module/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request

[Known issues](https://github.com/Lax/nginx-http-accounting-module/issues?q=)

# Author

Liu Lantao [Github@Lax](https://github.com/Lax)

[Contributors](https://github.com/Lax/nginx-http-accounting-module/graphs/contributors)

# License

[BSD-2-Clause](LICENSE)


[![FOSSA Status](https://app.fossa.io/api/projects/git%2Bgithub.com%2FLax%2Fnginx-http-accounting-module.svg?type=large)](https://app.fossa.io/projects/git%2Bgithub.com%2FLax%2Fnginx-http-accounting-module?ref=badge_large)
