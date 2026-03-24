/*
 * imagecore test suite entry point.
 *
 * Tests are split across files by concern:
 *   test_format_detection.cpp  — magic byte detection, getImageInfo, null handling
 *   test_codec_roundtrips.cpp  — encode/decode round-trips, cross-format conversion
 *   test_lossless_jpeg.cpp     — lossless JPEG rotate, crop, EXIF strip
 *   test_pixel_ops.cpp         — crop, resize, rotate, flip
 *   test_sample_images.cpp     — real sample image file tests (deferred)
 *
 * Build: cmake -S . -B build -DIC_BUILD_TESTS=ON && cmake --build build
 * Run:   ./build/test_imagecore [path/to/samples/]
 */

#include "test_harness.h"

/* Global counters (declared extern in test_harness.h) */
int tests_passed = 0;
int tests_failed = 0;

/* Sample test infrastructure (declared extern in test_harness.h) */
const char* g_samples_dir = nullptr;
SampleTest g_sample_tests[64];
int g_sample_test_count = 0;

int main(int argc, char** argv) {
    if (argc > 1) {
        g_samples_dir = argv[1];
    }

    printf("\n  imagecore test suite\n");
    printf("  ====================\n\n");

    /* Phase 1+2 tests ran via static constructors (TEST macro) */

    /* Phase 3: Sample tests (deferred — need g_samples_dir set first) */
    if (g_samples_dir) {
        printf("\n  -- Sample image tests (from %s) --\n", g_samples_dir);
        for (int i = 0; i < g_sample_test_count; i++) {
            printf("  TEST %-40s ", g_sample_tests[i].name);
            g_sample_tests[i].func();
            printf("PASS\n");
            tests_passed++;
        }
    } else {
        printf("\n  Sample tests skipped (run with: ./test_imagecore path/to/samples/)\n");
    }

    printf("\n  Results: %d passed, %d failed\n\n", tests_passed, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}
