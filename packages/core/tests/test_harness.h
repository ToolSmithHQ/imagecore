/*
 * Minimal test harness for imagecore.
 * Shared macros and helpers used by all test files.
 */

#ifndef IC_TEST_HARNESS_H
#define IC_TEST_HARNESS_H

#include "imagecore.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

/* ── Counters (defined in test_main.cpp) ─────────────────────────────── */

extern int tests_passed;
extern int tests_failed;

/* ── Test macro: auto-registers and runs via static constructor ───────── */

#define TEST(name) \
    static void test_##name(); \
    static struct Register_##name { \
        Register_##name() { \
            printf("  TEST %-40s ", #name); \
            test_##name(); \
            printf("PASS\n"); \
            tests_passed++; \
        } \
    } register_##name; \
    static void test_##name()

/* ── Assertion macro ─────────────────────────────────────────────────── */

#define ASSERT(cond) \
    do { if (!(cond)) { \
        printf("FAIL\n    Assertion failed: %s\n    at %s:%d\n", \
               #cond, __FILE__, __LINE__); \
        tests_failed++; \
        return; \
    } } while(0)

/* ── Helper: create a simple RGBA test image (gradient pattern) ──────── */

static inline ICImage make_test_image(uint32_t w, uint32_t h) {
    ICImage img;
    img.width = w;
    img.height = h;
    img.stride = w * 4;
    img.pixels = static_cast<uint8_t*>(malloc(img.stride * h));

    for (uint32_t y = 0; y < h; y++) {
        for (uint32_t x = 0; x < w; x++) {
            uint8_t* p = img.pixels + y * img.stride + x * 4;
            p[0] = static_cast<uint8_t>((x * 255) / w);
            p[1] = static_cast<uint8_t>((y * 255) / h);
            p[2] = 128;
            p[3] = 255;
        }
    }
    return img;
}

/* ── Sample test infrastructure (deferred execution) ─────────────────── */

extern const char* g_samples_dir;

typedef void (*TestFunc)();
struct SampleTest { const char* name; TestFunc func; };
extern SampleTest g_sample_tests[];
extern int g_sample_test_count;

#define SAMPLE_TEST(name) \
    static void stest_##name(); \
    static struct RegisterSample_##name { \
        RegisterSample_##name() { \
            g_sample_tests[g_sample_test_count++] = { #name, stest_##name }; \
        } \
    } reg_sample_##name; \
    static void stest_##name()

/* ── Helper: load a file from the samples directory ──────────────────── */

static inline uint8_t* load_file(const char* filename, size_t* out_len) {
    if (!g_samples_dir) return nullptr;

    char path[1024];
    snprintf(path, sizeof(path), "%s/%s", g_samples_dir, filename);

    FILE* f = fopen(path, "rb");
    if (!f) return nullptr;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) { fclose(f); return nullptr; }

    uint8_t* buf = static_cast<uint8_t*>(malloc(static_cast<size_t>(size)));
    if (!buf) { fclose(f); return nullptr; }

    size_t read = fread(buf, 1, static_cast<size_t>(size), f);
    fclose(f);

    if (read != static_cast<size_t>(size)) { free(buf); return nullptr; }

    *out_len = static_cast<size_t>(size);
    return buf;
}

#endif /* IC_TEST_HARNESS_H */
