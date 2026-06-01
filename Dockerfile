ARG NGINX_BRANCH=mainline
ARG MODULE_SRC

FROM nginx:${NGINX_BRANCH}-alpine AS builder

RUN apk add --no-cache build-base linux-headers pcre-dev zlib-dev openssl-dev wget

ARG MODULE_NAME
ARG MODULE_SRC

WORKDIR /build

COPY . /build/repo/

RUN nginx_version=$(nginx -v 2>&1 | cut -d'/' -f2) && \
    wget -q "https://nginx.org/download/nginx-${nginx_version}.tar.gz" && \
    tar xzf "nginx-${nginx_version}.tar.gz" && \
    mv "nginx-${nginx_version}" nginx-src

RUN mkdir -p /build/artifacts && \
    module_src="${MODULE_SRC:-$MODULE_NAME}" && \
    variants_file="/build/repo/.build-variants" && \
    if [ -f "$variants_file" ] && grep -q "^${MODULE_NAME}|" "$variants_file"; then \
      grep "^${MODULE_NAME}|" "$variants_file" | while IFS='|' read -r _ variant flags; do \
        [ -z "$variant" ] && continue; \
        cp -r /build/nginx-src "/build/nginx-${variant}" && \
        cd "/build/nginx-${variant}" && \
        ./configure --with-compat $flags --add-dynamic-module=/build/repo/${module_src} && \
        make modules && \
        for f in objs/*.so; do \
          [ -f "$f" ] || continue; \
          cp "$f" "/build/artifacts/${variant}-$(basename "$f")"; \
        done; \
      done; \
    else \
      cd /build/nginx-src && \
      ./configure --with-compat --add-dynamic-module=/build/repo/${module_src} && \
      make modules && \
      cp objs/*.so /build/artifacts/; \
    fi

FROM scratch AS artifacts
COPY --from=builder /build/artifacts/ /
