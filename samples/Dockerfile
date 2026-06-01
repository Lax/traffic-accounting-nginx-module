ARG MODULE_VARIANT=combined

ARG BUILDER_IMAGE=ghcr.io/lax/traffic-accounting-nginx-module/builder:alpine-3.20

FROM ${BUILDER_IMAGE} AS builder
ARG MODULE_VARIANT
ARG NGINX_VERSION=1.31.1
ARG ECHO_NGINX_MODULE_VERSION=echo-nginx-module-v20260601-snapshot
ARG APK_MIRROR

RUN if [ -n "$APK_MIRROR" ]; then \
        sed -i "s|dl-cdn.alpinelinux.org|${APK_MIRROR}|g" /etc/apk/repositories; \
    fi

WORKDIR /src

RUN curl -sL http://nginx.org/download/nginx-${NGINX_VERSION}.tar.gz | tar xz

WORKDIR /src/nginx-${NGINX_VERSION}
COPY config traffic-accounting-nginx-module/config
COPY src traffic-accounting-nginx-module/src

RUN CONF_FLAGS="--with-compat" && \
    if [ "$MODULE_VARIANT" = "stream-only" ]; then \
        CONF_FLAGS="$CONF_FLAGS --without-http --with-stream"; \
    elif [ "$MODULE_VARIANT" = "combined" ]; then \
        CONF_FLAGS="$CONF_FLAGS --with-stream"; \
    fi && \
    ./configure --prefix=/opt/nginx \
        $CONF_FLAGS \
        --add-dynamic-module=traffic-accounting-nginx-module \
        --http-log-path=/dev/stdout \
        --error-log-path=/dev/stderr \
    && make -s && make -s install

RUN if [ "$MODULE_VARIANT" != "stream-only" ]; then \
        mkdir -p /opt/nginx/modules && \
        curl -sL -o /opt/nginx/modules/ngx_http_echo_module.so \
        https://github.com/Lax/echo-nginx-module/releases/download/${ECHO_NGINX_MODULE_VERSION}/ngx_http_echo_module-nginx-${NGINX_VERSION}.so; \
    fi

FROM alpine:3.20
ARG MODULE_VARIANT
ARG APK_MIRROR

RUN if [ -n "$APK_MIRROR" ]; then \
        sed -i "s|dl-cdn.alpinelinux.org|${APK_MIRROR}|g" /etc/apk/repositories; \
    fi

RUN apk add --no-cache pcre zlib openssl

COPY --from=builder /opt/nginx /opt/nginx

COPY samples/nginx.conf /opt/nginx/conf/nginx.combined.conf
COPY samples/nginx.http-only.conf /opt/nginx/conf/
COPY samples/nginx.stream-only.conf /opt/nginx/conf/
COPY samples/http.conf /opt/nginx/conf/
COPY samples/stream.conf /opt/nginx/conf/

RUN mkdir -p /opt/nginx/logs && \
    if [ "$MODULE_VARIANT" = "http-only" ]; then \
        CONF=nginx.http-only.conf; \
    elif [ "$MODULE_VARIANT" = "stream-only" ]; then \
        CONF=nginx.stream-only.conf; \
    else \
        CONF=nginx.combined.conf; \
    fi && \
    cp /opt/nginx/conf/$CONF /opt/nginx/conf/nginx.conf && \
    ln -sf /dev/stdout /opt/nginx/logs/access.log && \
    ln -sf /dev/stderr /opt/nginx/logs/http-accounting.log && \
    ln -sf /dev/stderr /opt/nginx/logs/stream-accounting.log && \
    ln -sf /dev/stderr /opt/nginx/logs/error.log

RUN adduser -D nginx && \
    chown -R nginx:nginx /opt/nginx/logs /opt/nginx/conf

WORKDIR /opt/nginx

USER nginx

EXPOSE 8080 8888 9999
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=2 CMD wget -qO- http://localhost:8080/ || exit 1
STOPSIGNAL SIGTERM
ENTRYPOINT ["/opt/nginx/sbin/nginx", "-g", "daemon off;"]
