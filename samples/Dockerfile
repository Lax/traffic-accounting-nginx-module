ARG MODULE_VARIANT=combined

FROM alpine:3.20 AS builder
ARG MODULE_VARIANT

RUN apk add --no-cache build-base pcre-dev zlib-dev openssl-dev linux-headers curl tar

WORKDIR /src

RUN curl -sL http://nginx.org/download/nginx-1.26.2.tar.gz | tar xz

RUN if [ "$MODULE_VARIANT" != "stream-only" ]; then \
        curl -sL https://github.com/openresty/echo-nginx-module/archive/master.tar.gz | tar xz -C /src/nginx-1.26.2; \
    fi

WORKDIR /src/nginx-1.26.2
COPY . traffic-accounting-nginx-module

RUN CONF_FLAGS="--with-compat" && \
    if [ "$MODULE_VARIANT" = "stream-only" ]; then \
        CONF_FLAGS="$CONF_FLAGS --without-http --with-stream"; \
    elif [ "$MODULE_VARIANT" = "combined" ]; then \
        CONF_FLAGS="$CONF_FLAGS --with-stream"; \
    fi && \
    if [ "$MODULE_VARIANT" != "stream-only" ]; then \
        CONF_FLAGS="$CONF_FLAGS --add-dynamic-module=echo-nginx-module-master"; \
    fi && \
    ./configure --prefix=/opt/nginx \
        $CONF_FLAGS \
        --add-dynamic-module=traffic-accounting-nginx-module \
        --http-log-path=/dev/stdout \
        --error-log-path=/dev/stderr \
    && make -s && make -s install

FROM alpine:3.20
ARG MODULE_VARIANT

RUN apk add --no-cache pcre zlib openssl

COPY --from=builder /opt/nginx /opt/nginx

COPY samples/nginx.conf /opt/nginx/conf/nginx.both.conf
COPY samples/nginx.http-only.conf /opt/nginx/conf/
COPY samples/nginx.stream-only.conf /opt/nginx/conf/
COPY samples/http.conf /opt/nginx/conf/
COPY samples/stream.conf /opt/nginx/conf/

RUN if [ "$MODULE_VARIANT" = "http-only" ]; then \
        CONF=nginx.http-only.conf; \
    elif [ "$MODULE_VARIANT" = "stream-only" ]; then \
        CONF=nginx.stream-only.conf; \
    else \
        CONF=nginx.both.conf; \
    fi && \
    cp /opt/nginx/conf/$CONF /opt/nginx/conf/nginx.conf && \
    ln -sf /dev/stdout /opt/nginx/logs/access.log && \
    ln -sf /dev/stderr /opt/nginx/logs/http-accounting.log && \
    ln -sf /dev/stderr /opt/nginx/logs/stream-accounting.log && \
    ln -sf /dev/stderr /opt/nginx/logs/error.log

EXPOSE 8080 8888 9999
STOPSIGNAL SIGTERM
ENTRYPOINT ["/opt/nginx/sbin/nginx", "-g", "daemon off;"]
