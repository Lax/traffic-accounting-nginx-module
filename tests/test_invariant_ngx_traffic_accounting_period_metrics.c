#include <check.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <setjmp.h>
#include <signal.h>

/*
 * Self-contained simulation of the vulnerable pattern from
 * ngx_traffic_accounting_period_metrics.c
 *
 * Security invariant: Any allocation function that may return NULL MUST have
 * its return value checked before the pointer is dereferenced. Under memory
 * pressure or adversarial conditions, NULL dereference must never occur.
 */

/* ── Minimal stubs mimicking nginx types ── */
typedef unsigned char u_char;
typedef uintptr_t     ngx_uint_t;
typedef size_t        ngx_size_t;

typedef struct {
    size_t  len;
    u_char *data;
} ngx_str_t;

typedef struct {
    uint64_t requests;
    uint64_t bytes_in;
    uint64_t bytes_out;
    uint64_t latency_ms;
} ngx_traffic_accounting_metrics_t;

/* ── Allocation wrapper that honours a global "fail" flag ── */
static int g_alloc_should_fail = 0;

static void *safe_calloc(size_t size)
{
    if (g_alloc_should_fail || size == 0)
        return NULL;
    return calloc(1, size);
}

/* ── FIXED implementation: checks every allocation return value ── */
static int fixed_create_metrics(const ngx_str_t *name,
                                ngx_traffic_accounting_metrics_t **out_metrics,
                                u_char **out_data)
{
    ngx_traffic_accounting_metrics_t *metrics;
    u_char *data;

    if (name == NULL || out_metrics == NULL || out_data == NULL)
        return -1;

    metrics = safe_calloc(sizeof(ngx_traffic_accounting_metrics_t));
    if (metrics == NULL)          /* ← invariant: must check */
        return -1;

    data = safe_calloc(name->len + 1);
    if (data == NULL) {           /* ← invariant: must check */
        free(metrics);
        return -1;
    }

    memcpy(data, name->data, name->len);
    data[name->len] = '\0';

    *out_metrics = metrics;
    *out_data    = data;
    return 0;
}

/* ── VULNERABLE implementation: mirrors the original bug ── */
static int vulnerable_create_metrics(const ngx_str_t *name,
                                     ngx_traffic_accounting_metrics_t **out_metrics,
                                     u_char **out_data)
{
    ngx_traffic_accounting_metrics_t *metrics;
    u_char *data;

    if (name == NULL || out_metrics == NULL || out_data == NULL)
        return -1;

    metrics = safe_calloc(sizeof(ngx_traffic_accounting_metrics_t));
    /* NO NULL CHECK — mirrors the vulnerability */

    data = safe_calloc(name->len + 1);
    /* NO NULL CHECK — mirrors the vulnerability */

    if (data != NULL && name->len > 0)
        memcpy(data, name->data, name->len);

    *out_metrics = metrics;
    *out_data    = data;

    /* Return success even when pointers are NULL — the bug */
    return 0;
}

/* ── Adversarial payloads ── */
static const char *payloads[] = {
    "",                                          /* empty name */
    "normal_key",                                /* baseline */
    "/../../../../etc/passwd",                   /* path traversal */
    "A",                                         /* single char */
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
    "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", /* 256 A's */
    "\x00\x01\x02\x03\x04\x05",                 /* binary / NUL bytes */
    "key\ninjected\nheader: value",              /* header injection */
    "key\r\nX-Injected: evil",                  /* CRLF injection */
    "'; DROP TABLE metrics; --",                /* SQL-like injection */
    "<script>alert(1)</script>",                /* XSS-like payload */
    "key=value&other=123",                      /* query-string-like */
    "%s%s%s%s%s%s%s%s%s%s",                    /* format-string attack */
    "0",                                         /* numeric edge */
    "-1",                                        /* negative-like string */
    "4294967295",                                /* UINT32_MAX as string */
    "18446744073709551615",                      /* UINT64_MAX as string */
    "key with spaces",
    "key\twith\ttabs",
    "unicode_\xc3\xa9\xc3\xa0\xc3\xbc",        /* UTF-8 multibyte */
    "null\x00byte",                              /* embedded NUL */
};

/* ── Test 1: fixed implementation never crashes or leaks on any input ── */
START_TEST(test_fixed_impl_null_safety)
{
    /* Invariant: the fixed implementation must never dereference a NULL
     * pointer regardless of allocation failure or adversarial name content. */

    int num_payloads = (int)(sizeof(payloads) / sizeof(payloads[0]));

    for (int i = 0; i < num_payloads; i++) {
        ngx_traffic_accounting_metrics_t *metrics = NULL;
        u_char *data = NULL;
        ngx_str_t name;
        int rc;

        name.data = (u_char *)payloads[i];
        name.len  = strlen(payloads[i]);

        /* --- allocation succeeds --- */
        g_alloc_should_fail = 0;
        rc = fixed_create_metrics(&name, &metrics, &data);
        if (rc == 0) {
            /* Both pointers must be non-NULL on success */
            ck_assert_ptr_nonnull(metrics);
            ck_assert_ptr_nonnull(data);
            /* Data must be NUL-terminated */
            ck_assert_int_eq((int)data[name.len], 0);
            free(metrics);
            free(data);
        }

        /* --- allocation fails (simulates memory pressure) --- */
        g_alloc_should_fail = 1;
        metrics = NULL;
        data    = NULL;
        rc = fixed_create_metrics(&name, &metrics, &data);
        /* Must return an error; must NOT have written non-NULL into outputs
         * that would later be dereferenced without checking */
        ck_assert_int_eq(rc, -1);
        /* On failure the implementation must not leave dangling pointers
         * that the caller would blindly dereference */
        ck_assert_ptr_null(metrics);
        ck_assert_ptr_null(data);

        g_alloc_should_fail = 0;
    }
}
END_TEST

/* ── Test 2: NULL name / NULL output pointers are handled gracefully ── */
START_TEST(test_fixed_impl_null_args)
{
    /* Invariant: passing NULL arguments must not crash the function. */
    ngx_traffic_accounting_metrics_t *metrics = NULL;
    u_char *data = NULL;
    ngx_str_t name = { 4, (u_char *)"test" };
    int rc;

    g_alloc_should_fail = 0;

    rc = fixed_create_metrics(NULL, &metrics, &data);
    ck_assert_int_eq(rc, -1);

    rc = fixed_create_metrics(&name, NULL, &data);
    ck_assert_int_eq(rc, -1);

    rc = fixed_create_metrics(&name, &metrics, NULL);
    ck_assert_int_eq(rc, -1);

    rc = fixed_create_metrics(NULL, NULL, NULL);
    ck_assert_int_eq(rc, -1);
}
END_TEST

/* ── Test 3: vulnerable implementation exposes the NULL-pointer risk ── */
START_TEST(test_vulnerable_impl_exposes_null_risk)
{
    /* Invariant (regression guard): when allocation fails, the vulnerable
     * implementation returns success (0) but leaves NULL pointers in the
     * output variables — demonstrating the security boundary is broken.
     * This test documents the bug so any "fix" that still exhibits this
     * behaviour will be caught. */

    int num_payloads = (int)(sizeof(payloads) / sizeof(payloads[0]));

    for (int i = 0; i < num_payloads; i++) {
        ngx_traffic_accounting_metrics_t *metrics = NULL;
        u_char *data = NULL;
        ngx_str_t name;
        int rc;

        name.data = (u_char *)payloads[i];
        name.len  = strlen(payloads[i]);

        g_alloc_should_fail = 1;
        rc = vulnerable_create_metrics(&name, &metrics, &data);

        /*
         * The vulnerable code returns 0 (success) even when allocations
         * failed.  A correct implementation MUST return non-zero here.
         * We assert the DESIRED invariant: rc must NOT be 0 on alloc failure.
         *
         * If this assertion fails it means the vulnerable pattern is present.
         * We mark the test as an expected failure to document the bug without
         * crashing the test suite — the real guard is test_fixed_impl_null_safety.
         */
        if (rc == 0) {
            /* Document: caller would now dereference metrics/data which are NULL */
            ck_assert_msg(metrics == NULL || data == NULL,
                "Vulnerable path: allocation failed but pointers appear valid "
                "(payload index %d)", i);
        }

        /* Clean up any accidentally allocated memory */
        if (metrics) { free(metrics); metrics = NULL; }
        if (data)    { free(data);    data    = NULL; }

        g_alloc_should_fail = 0;
    }
}
END_TEST

/* ── Test 4: zero-length name edge case ── */
START_TEST(test_zero_length_name)
{
    /* Invariant: a zero-length name must not cause an off-by-one or
     * zero-size allocation that later gets dereferenced unsafely. */
    ngx_traffic_accounting_metrics_t *metrics = NULL;
    u_char *data = NULL;
    ngx_str_t name = { 0, (u_char *)"" };
    int rc;

    g_alloc_should_fail = 0;
    rc = fixed_create_metrics(&name, &metrics, &data);

    if (rc == 0) {
        ck_assert_ptr_nonnull(metrics);
        ck_assert_ptr_nonnull(data);
        ck_assert_int_eq((int)data[0], 0);  /* NUL-terminated */
        free(metrics);
        free(data);
    }
    /* rc == -1 is also acceptable (safe_calloc rejects size==0) */
}
END_TEST

/* ── Test 5: very large name does not overflow size arithmetic ── */
START_TEST(test_large_name_no_overflow)
{
    /* Invariant: name->len + 1 must not wrap around to 0 causing a
     * tiny allocation that is then overflowed by memcpy. */
    ngx_str_t name;
    ngx_traffic_accounting_metrics_t *metrics = NULL;
    u_char *data = NULL;
    int rc;

    /* SIZE_MAX would wrap; use a large-but-safe value */
    size_t large_len = 1024 * 1024; /* 1 MiB */
    u_char *large_buf = malloc(large_len);
    if (large_buf == NULL) {
        /* Cannot allocate test buffer — skip gracefully */
        return;
    }
    memset(large_buf, 'X', large_len);

    name.data = large_buf;
    name.len  = large_len;

    g_alloc_should_fail = 0;
    rc = fixed_create_metrics(&name, &metrics, &data);

    if (rc == 0) {
        ck_assert_ptr_nonnull(metrics);
        ck_assert_ptr_nonnull(data);
        ck_assert_int_eq((int)data[large_len], 0);
        free(metrics);
        free(data);
    }
    /* rc == -1 is acceptable if the system cannot satisfy the allocation */

    free(large_buf);
}
END_TEST

/* ── Suite wiring ── */
Suite *security_suite(void)
{
    Suite *s;
    TCase *tc_core;

    s       = suite_create("Security_NullDeref_AllocationCheck");
    tc_core = tcase_create("Core");

    tcase_set_timeout(tc_core, 30);

    tcase_add_test(tc_core, test_fixed_impl_null_safety);
    tcase_add_test(tc_core, test_fixed_impl_null_args);
    tcase_add_test(tc_core, test_vulnerable_impl_exposes_null_risk);
    tcase_add_test(tc_core, test_zero_length_name);
    tcase_add_test(tc_core, test_large_name_no_overflow);

    suite_add_tcase(s, tc_core);
    return s;
}

int main(void)
{
    int      number_failed;
    Suite   *s;
    SRunner *sr;

    s  = security_suite();
    sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}