/**
 * @file bench_bend.c
 * @brief Microbenchmark: Bend analysis latency for design optimization loops.
 *
 * Measures the throughput of flex_bend_analyze() for parametric design
 * space exploration — useful when auto-optimizing stackup parameters.
 */

#include "flex_bend.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

int main(void) {
    flex_bend_params_t bp;
    memset(&bp, 0, sizeof(bp));
    bp.config = FLEX_BEND_SINGLE;
    bp.bend_radius_mm = 2.0;
    bp.bend_angle_deg = 90.0;
    bp.total_thickness_mm = 0.3;
    bp.copper_elongation_limit_percent = 16.0;
    bp.youngs_modulus_copper_mpa = 117000.0;
    bp.num_layers = 1;
    bp.is_dynamic = 1;
    bp.copper_thickness_total_um = 35.0;

    flex_bend_result_t result;
    const int iterations = 100000;

    clock_t start = clock();
    for (int i = 0; i < iterations; i++) {
        flex_bend_analyze(&bp, &result);
    }
    clock_t end = clock();

    double elapsed = (double)(end - start) / CLOCKS_PER_SEC;
    printf("Bench: flex_bend_analyze()\n");
    printf("  Iterations: %d\n", iterations);
    printf("  Total time: %.3f s\n", elapsed);
    printf("  Per call:   %.3f μs\n", (elapsed / iterations) * 1.0e6);

    return 0;
}
