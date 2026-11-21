FROM quay.io/centos/centos:stream9 AS builder

RUN yum install gcc make pcre-devel zlib-devel openssl-devel -y \
    && yum clean all

ENV PREFIX /opt/nginx
# 1.30.7 (2026-11-18) is the current stable release; it carries the same
# security batch as mainline 1.31.9.  Bump this line when the upstream
# sentinel opens its weekly window PR.
ENV NGX_VER 1.30.7

ENV WORKDIR /src
ENV NGX_SRC_DIR ${WORKDIR}/nginx-${NGX_VER}
ENV NGX_URL https://nginx.org/download/nginx-${NGX_VER}.tar.gz
ENV NGX_HTTP_ECHO_URL https://github.com/openresty/echo-nginx-module/archive/master.tar.gz

WORKDIR ${WORKDIR}

RUN tar zxf `curl -SLOs -w'%{filename_effective}' ${NGX_URL}` -C ${WORKDIR} \
    && tar zxf `curl -SLJOs -w'%{filename_effective}' ${NGX_HTTP_ECHO_URL}` -C ${NGX_SRC_DIR}

WORKDIR ${NGX_SRC_DIR}
ADD . traffic-accounting-nginx-module
RUN ./configure --prefix=${PREFIX} \
    --with-stream \
    --add-dynamic-module=traffic-accounting-nginx-module \
    --add-dynamic-module=echo-nginx-module-master \
    --http-log-path=/dev/stdout \
    --error-log-path=/dev/stderr \
    && make -s && make -s install


FROM quay.io/centos/centos:stream9

ENV PREFIX /opt/nginx
ENV CONFIG_VER $(date)

COPY --from=builder ${PREFIX} ${PREFIX}

WORKDIR ${PREFIX}

RUN ln -sf /dev/stdout ${PREFIX}/logs/access.log \
    && ln -sf /dev/stderr ${PREFIX}/logs/http-accounting.log \
    && ln -sf /dev/stderr ${PREFIX}/logs/stream-accounting.log \
    && ln -sf /dev/stderr ${PREFIX}/logs/error.log \
    && ln -sf ../usr/share/zoneinfo/Asia/Shanghai /etc/localtime

ADD samples/nginx.conf ${PREFIX}/conf/nginx.conf
ADD samples/http.conf ${PREFIX}/conf/http.conf
ADD samples/stream.conf ${PREFIX}/conf/stream.conf

EXPOSE 8080
EXPOSE 8888
EXPOSE 9999
STOPSIGNAL SIGTERM
ENTRYPOINT ["./sbin/nginx", "-g", "daemon off;"]
