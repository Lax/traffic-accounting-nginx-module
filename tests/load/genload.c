/*
 * genload - deterministic HTTP load generator for traffic-accounting tests.
 *
 *   genload HOST PORT CONNS PIPELINE TOTAL [--id-mode unique|fixed] [--prefix P]
 *
 * - keep-alive connections, PIPELINE-deep request pipelining
 * - unique mode: every request carries a distinct ?id=<prefix><n> so each
 *   request lands in its own accounting metrics node
 * - counts exact HTTP/1.1 200 responses; non-zero exit unless all TOTAL
 *   requests were answered
 * - tracks the largest gap between two consecutive response reads
 *   (worker stall detector) and prints a per-second progress line
 *
 * Build: cc -O2 -o genload genload.c
 */
#define _GNU_SOURCE
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static char        *host, *id_prefix = "u";
static int          port, conns, pipeline, total, id_unique = 1;
static int          epfd;
static volatile int sent_total, ok_total, err_total, done;

static uint64_t     max_gap_us;
static double       start_us;

static double now_us(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e6 + ts.tv_nsec / 1e3;
}

typedef struct {
    int    fd;
    int    inflight;      /* requests written but not yet counted */
    char   buf[1 << 16];
    int    buf_len;
    int    scan_off;      /* how far the response scanner has consumed */
    double last_read_us;
} conn_t;

static conn_t *conns_arr;

/* interlocked id generator */
static pthread_mutex_t id_mu = PTHREAD_MUTEX_INITIALIZER;
static int next_id = 1;

static int make_request(char *out, int *out_len)
{
    int id;
    pthread_mutex_lock(&id_mu);
    id = next_id++;
    pthread_mutex_unlock(&id_mu);

    if (id_unique) {
        *out_len = snprintf(out, 512,
            "GET /?uid=%s%09d HTTP/1.1\r\nHost: load\r\n\r\n",
            id_prefix, id);
    } else {
        *out_len = snprintf(out, 512,
            "GET /?uid=%s HTTP/1.1\r\nHost: load\r\n\r\n", id_prefix);
    }
    return id <= total;
}

static void set_nb(int fd)
{
    int fl = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
}

static void open_conn(conn_t *c)
{
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = inet_addr(host);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
    connect(fd, (struct sockaddr *)&a, sizeof(a)); /* loopback: connects at once */
    set_nb(fd);

    memset(c, 0, sizeof(*c));
    c->fd = fd;
    c->last_read_us = now_us();

    struct epoll_event ev = { .events = EPOLLIN | EPOLLOUT, .data.ptr = c };
    epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
}

static void pump_writes(conn_t *c)
{
    char req[512];
    int req_len;
    char batch[64 * 512];
    int batch_len = 0;

    while (c->inflight < pipeline * 2) {
        int room = total - __atomic_load_n(&sent_total, __ATOMIC_RELAXED);
        if (room <= 0)
            break;
        if (!make_request(req, &req_len))
            break;
        memcpy(batch + batch_len, req, req_len);
        batch_len += req_len;
        c->inflight++;
        __atomic_fetch_add(&sent_total, 1, __ATOMIC_RELAXED);
        if (batch_len >= pipeline * 256)
            break;
    }
    if (batch_len) {
        int n = write(c->fd, batch, batch_len);
        (void)n; /* partial writes are retried on next EPOLLOUT via inflight slack */
    }
}

/*
 * Count responses by scanning for status-line markers. Bodies follow the
 * header block, so "\r\n\r\n" boundaries are NOT response boundaries; but
 * every response contains exactly one "HTTP/1.1 xxx" status line and the
 * test bodies ("ok") can never contain that byte sequence.
 */
#define MARK "HTTP/1.1 "
#define MARK_LEN 9

static void scan_responses(conn_t *c)
{
    char *b = c->buf;
    int   len = c->buf_len;

    while (c->scan_off + 12 <= len) {
        char *m = memmem(b + c->scan_off, len - c->scan_off, MARK, MARK_LEN);
        if (!m)
            break;
        int off = (int)(m - b);
        if (off + 12 > len)     /* status code not fully received yet */
            break;
        if (memcmp(m, "HTTP/1.1 200", 12) == 0)
            __atomic_fetch_add(&ok_total, 1, __ATOMIC_RELAXED);
        else
            __atomic_fetch_add(&err_total, 1, __ATOMIC_RELAXED);
        c->inflight--;
        c->scan_off = off + MARK_LEN;
    }
    /* compaction is safe: the loop guarantees no uncounted complete marker
     * exists in [scan_off, len); a partial marker at the tail survives */
    if (c->scan_off > 0) {
        memmove(b, b + c->scan_off, len - c->scan_off);
        c->buf_len = len - c->scan_off;
        c->scan_off = 0;
    }
}

static void drain_reads(conn_t *c)
{
    for (;;) {
        int n = read(c->fd, c->buf + c->buf_len, sizeof(c->buf) - c->buf_len);
        if (n > 0) {
            double now = now_us();
            double gap = now - c->last_read_us;
            if (gap > max_gap_us)
                max_gap_us = (uint64_t)gap;
            c->last_read_us = now;
            c->buf_len += n;
            scan_responses(c);
            continue;
        }
        if (n == 0) {
            done = 1;
            return;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        if (errno != EINTR) {
            done = 1;
            return;
        }
    }
}

int main(int argc, char **argv)
{
    if (argc < 6) {
        fprintf(stderr, "usage: %s HOST PORT CONNS PIPELINE TOTAL [--id-mode unique|fixed] [--prefix P]\n", argv[0]);
        return 2;
    }
    host = argv[1];
    port = atoi(argv[2]);
    conns = atoi(argv[3]);
    pipeline = atoi(argv[4]);
    total = atoi(argv[5]);
    for (int i = 6; i < argc; i++) {
        if (!strcmp(argv[i], "--id-mode") && i + 1 < argc) {
            id_unique = !strcmp(argv[++i], "unique");
        } else if (!strcmp(argv[i], "--prefix") && i + 1 < argc) {
            id_prefix = argv[++i];
        }
    }

    epfd = epoll_create1(0);
    conns_arr = calloc(conns, sizeof(conn_t));
    for (int i = 0; i < conns; i++)
        open_conn(&conns_arr[i]);

    start_us = now_us();
    double last_tick = start_us;
    int last_ok = 0;

    for (;;) {
        struct epoll_event evs[256];
        int n = epoll_wait(epfd, evs, 256, 1000);
        for (int i = 0; i < n; i++) {
            conn_t *c = evs[i].data.ptr;
            if (evs[i].events & EPOLLIN)
                drain_reads(c);
            if (evs[i].events & EPOLLOUT)
                pump_writes(c);
        }
        double now = now_us();
        if (now - last_tick >= 1e6) {
            fprintf(stderr, "t=%.0fs sent=%d ok=%d err=%d rps=%d\n",
                    (now - start_us) / 1e6,
                    __atomic_load_n(&sent_total, __ATOMIC_RELAXED),
                    __atomic_load_n(&ok_total, __ATOMIC_RELAXED),
                    __atomic_load_n(&err_total, __ATOMIC_RELAXED),
                    __atomic_load_n(&ok_total, __ATOMIC_RELAXED) - last_ok);
            last_ok = __atomic_load_n(&ok_total, __ATOMIC_RELAXED);
            last_tick = now;
        }
        if (__atomic_load_n(&ok_total, __ATOMIC_RELAXED) +
            __atomic_load_n(&err_total, __ATOMIC_RELAXED) >= total)
            break;
        if (done)
            break;
    }

    double dur = now_us() - start_us;
    printf("RESULT sent=%d ok=%d err=%d duration_s=%.2f rps=%.0f max_read_gap_ms=%.1f\n",
           sent_total,
           __atomic_load_n(&ok_total, __ATOMIC_RELAXED),
           __atomic_load_n(&err_total, __ATOMIC_RELAXED),
           dur / 1e6,
           __atomic_load_n(&ok_total, __ATOMIC_RELAXED) / (dur / 1e6),
           max_gap_us / 1000.0);
    return (__atomic_load_n(&ok_total, __ATOMIC_RELAXED) == total) ? 0 : 1;
}
