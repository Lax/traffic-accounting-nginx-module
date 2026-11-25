# Demo image: official nginx base + this module (and echo-nginx-module) as
# dynamic modules compiled against the image's own nginx source.
#
# Why not compile nginx here: the official image is built with
# --with-compat and --with-stream, so third-party .so modules compiled
# against the matching nginx source load into it (verified: load_module,
# accounting, echo all work).  nginx security updates therefore come from
# rebasing on a newer official tag instead of recompiling nginx here.
#
# NGX_TAG should track the window stable tested by CI (see COMPATIBILITY.md);
# bump it when the upstream-sentinel workflow opens its window PR.
ARG NGX_TAG=1.30.8-alpine

FROM nginx:${NGX_TAG} AS builder

RUN apk add --no-cache build-base curl pcre2-dev zlib-dev openssl-dev

ADD . /opt/ta

WORKDIR /src
# echo-nginx-module pinned to its latest release (v0.66) for reproducible
# builds; bump deliberately, not by tracking master.
RUN curl -fsSL -o /tmp/nginx.tar.gz \
        https://nginx.org/download/nginx-${NGINX_VERSION}.tar.gz \
    && curl -fsSL -o /tmp/echo.tar.gz -L \
        https://github.com/openresty/echo-nginx-module/archive/refs/tags/v0.66.tar.gz \
    && tar -xzf /tmp/nginx.tar.gz --strip-components=1 \
    && tar -xzf /tmp/echo.tar.gz \
    && ./configure --with-compat --with-stream \
        --add-dynamic-module=/opt/ta \
        --add-dynamic-module=echo-nginx-module-0.66 \
    && make modules

FROM nginx:${NGX_TAG}

COPY --from=builder /src/objs/*.so /usr/lib/nginx/modules/

# accounting logs follow the docker convention: container logs are visible
# via `docker logs` (metrics also go to logstash over syslog, see confs);
# the sample confs resolve "html" relative to the prefix, which in the
# official image is /usr/share/nginx/html
RUN ln -sf /dev/stderr /var/log/nginx/http-accounting.log \
    && ln -sf /dev/stderr /var/log/nginx/stream-accounting.log \
    && ln -sfn /usr/share/nginx/html /etc/nginx/html

COPY samples/nginx.conf /etc/nginx/nginx.conf
COPY samples/http.conf /etc/nginx/http.conf
COPY samples/stream.conf /etc/nginx/stream.conf

EXPOSE 8080
EXPOSE 8888
EXPOSE 9999
STOPSIGNAL SIGTERM
