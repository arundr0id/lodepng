/*
 * Demonstrates the integer overflow in Adam7_getpassvalues
 * without needing an actual PNG file.
 *
 * Compile: g++ -o overflow_demo overflow_demo.cpp
 * Run:     ./overflow_demo
 *
 * Shows how unsigned 32-bit arithmetic produces wrong offsets
 * compared to correct 64-bit arithmetic.
 */

#include <cstdio>
#include <cstdint>
#include <cstddef>

static const unsigned ADAM7_IX[7] = { 0, 4, 0, 2, 0, 1, 0 };
static const unsigned ADAM7_IY[7] = { 0, 0, 4, 0, 2, 0, 1 };
static const unsigned ADAM7_DX[7] = { 8, 8, 4, 4, 2, 2, 1 };
static const unsigned ADAM7_DY[7] = { 8, 8, 8, 4, 4, 2, 2 };

void demo_overflow(unsigned w, unsigned h, unsigned bpp) {
    printf("=== Adam7 overflow demo: %ux%u, bpp=%u ===\n\n", w, h, bpp);

    unsigned passw[7], passh[7];
    
    // Buggy: unsigned 32-bit (as in lodepng)
    unsigned filter_passstart_buggy[8];
    unsigned padded_passstart_buggy[8];
    unsigned passstart_buggy[8];
    
    // Correct: size_t 64-bit
    size_t filter_passstart_correct[8];
    size_t padded_passstart_correct[8];
    size_t passstart_correct[8];

    for(unsigned i = 0; i < 7; i++) {
        passw[i] = (w + ADAM7_DX[i] - ADAM7_IX[i] - 1) / ADAM7_DX[i];
        passh[i] = (h + ADAM7_DY[i] - ADAM7_IY[i] - 1) / ADAM7_DY[i];
        if(passw[i] == 0) passh[i] = 0;
        if(passh[i] == 0) passw[i] = 0;
    }

    filter_passstart_buggy[0] = 0;
    padded_passstart_buggy[0] = 0;
    passstart_buggy[0] = 0;
    filter_passstart_correct[0] = 0;
    padded_passstart_correct[0] = 0;
    passstart_correct[0] = 0;

    for(unsigned i = 0; i < 7; i++) {
        // BUGGY: exact lodepng code using unsigned arithmetic
        filter_passstart_buggy[i + 1] = filter_passstart_buggy[i]
            + ((passw[i] && passh[i]) ? passh[i] * (1u + (passw[i] * bpp + 7u) / 8u) : 0);
        padded_passstart_buggy[i + 1] = padded_passstart_buggy[i]
            + passh[i] * ((passw[i] * bpp + 7u) / 8u);
        passstart_buggy[i + 1] = passstart_buggy[i]
            + (passh[i] * passw[i] * bpp + 7u) / 8u;

        // CORRECT: using size_t
        filter_passstart_correct[i + 1] = filter_passstart_correct[i]
            + ((passw[i] && passh[i]) ? (size_t)passh[i] * (1u + (passw[i] * bpp + 7u) / 8u) : 0);
        padded_passstart_correct[i + 1] = padded_passstart_correct[i]
            + (size_t)passh[i] * ((passw[i] * bpp + 7u) / 8u);
        passstart_correct[i + 1] = passstart_correct[i]
            + ((size_t)passh[i] * passw[i] * bpp + 7u) / 8u;

        int overflow_filter = (filter_passstart_buggy[i+1] != (unsigned)filter_passstart_correct[i+1]);
        int overflow_padded = (padded_passstart_buggy[i+1] != (unsigned)padded_passstart_correct[i+1]);
        int overflow_raw    = (passstart_buggy[i+1] != (unsigned)passstart_correct[i+1]);

        printf("Pass %u: %ux%u\n", i, passw[i], passh[i]);
        if(overflow_filter || overflow_padded || overflow_raw) {
            printf("  ** OVERFLOW DETECTED **\n");
            printf("  filter_passstart: buggy=0x%08X  correct=0x%016lX\n",
                   filter_passstart_buggy[i+1], (unsigned long)filter_passstart_correct[i+1]);
            printf("  padded_passstart: buggy=0x%08X  correct=0x%016lX\n",
                   padded_passstart_buggy[i+1], (unsigned long)padded_passstart_correct[i+1]);
            printf("  passstart:        buggy=0x%08X  correct=0x%016lX\n",
                   passstart_buggy[i+1], (unsigned long)passstart_correct[i+1]);
        } else {
            printf("  OK (no overflow)\n");
        }
        printf("\n");
    }

    printf("Impact: postProcessScanlines() and Adam7_deinterlace() use these\n");
    printf("wrong offsets as buffer indices, causing heap buffer overflow.\n");
    printf("With controlled PNG data, this enables arbitrary write -> RCE.\n");
}

int main() {
    // 65536x65536 RGBA (bpp=32) - overflows in passes 4-6
    demo_overflow(65536, 65536, 32);
    printf("\n");
    
    // 32768x65536 RGB (bpp=24) - also overflows
    demo_overflow(32768, 65536, 24);
    
    return 0;
}
