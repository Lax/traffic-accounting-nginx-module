FROM gcc:14-bookworm
WORKDIR /src
COPY . .
RUN cc -I test/ -I src/ -Wall -Werror \
    test/test_null_alloc.c -o /tmp/test_null_alloc
CMD ["/tmp/test_null_alloc"]
