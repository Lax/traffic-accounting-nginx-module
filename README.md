# Installation

Configure ngx_http_accounting_module as nginx module with ```--add-module``` when build nginx.

    cd /path/to/nginx-src/

    git clone https://github.com/Lax/ngx_http_accounting_module.git -b v0.2

    ./configure --add-module=/path/to/ngx_http_accounting_module

    make && make install

# Configuration

Edit your nginx.conf.

Example:

    http{
        http_accounting  on;   # turn on accounting function
        ...
        server {
            server_name example.com;
            http_accounting_id  accounting_id_str;   # set accounting_id based on server
            ...
            location / {
                http_accounting_id  accounting_id_str;    # set accounting_id based on location
                ...
            }
        }
    }

# Usage

This module write statistics to syslog. You should edit your syslog configuration.

For sample configuration / utils, see: [Lax/ngx_http_accounting_module-utils](http://github.com/Lax/ngx_http_accounting_module-utils)

# Branches

* master : new feathers
* v2-freeze-20110526 : works with nginx version(0.7.xx, 0.8.xx), nginx 0.9 is not tested. didn't work with nginx above 1.0.x.

# Author

Liu Lantao [Github](https://github.com/Lax)

# License

GPLv3
