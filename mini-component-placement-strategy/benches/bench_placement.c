/**
 * @file bench_placement.c
 * @brief Performance benchmarks for PCB component placement strategies
 *
 * Benchmarks measure:
 *   - Wire length estimation throughput (HPWL, Steiner)
 *   - Constraint checking speed (spacing, boundary, thermal)
 *   - Strategy execution time vs problem size
 *   - Thermal network solve scaling
 */
#include "placement_core.h"
#include "placement_constraint.h"
#include "placement_optimizer.h"
#include "placement_strategy.h"
#include "placement_thermal.h"
#include "placement_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define BENCH_ITER 100
#define BENCH_WARMUP 10

static double now_ms(void) {
    return (double)clock() * 1000.0 / CLOCKS_PER_SEC;
}

static PlacementResult* create_bench_problem(uint32_t n_comps, uint32_t n_nets) {
    Board b; placement_board_init(&b, "Bench", 200.0, 150.0, 4);
    PlacementResult* r = calloc(1, sizeof(PlacementResult));
    if (!placement_result_init(r, &b, n_comps, n_nets)) { free(r); return NULL; }
    RandomState rng; placement_util_random_init(&rng, 12345);
    for (uint32_t i = 0; i < n_comps; i++) {
        char des[8]; snprintf(des, 8, "C%d", i+1);
        placement_component_init(&r->components[i], des,
                                COMP_CAT_PASSIVE, PKG_SMD_0603, 1.6, 0.8, 0);
        r->components[i].comp_id = i + 1;
        r->components[i].body.width = 1.6 + placement_util_random_uniform(&rng) * 10;
        r->components[i].body.height = 0.8 + placement_util_random_uniform(&rng) * 8;
        r->components[i].power_dissipation_W = placement_util_random_uniform(&rng) * 2;
        placement_component_add_pad(&r->components[i], 1, "P", 0, 0, 0.3, 0.3);
        double bx = placement_util_random_uniform(&rng) * 190 + 5;
        double by = placement_util_random_uniform(&rng) * 140 + 5;
        placement_component_set_position(&r->components[i], bx, by, 0);
    }
    r->component_count = n_comps;
    for (uint32_t n = 0; n < n_nets; n++) {
        char nm[8]; snprintf(nm, 8, "N%d", n+1);
        placement_net_init(&r->nets[n], n+1, nm);
        r->nets[n].pin_count = 2 + placement_util_random_int(&rng, 0, 3);
        /* Connect random components to this net */
        for (uint32_t p = 0; p < r->nets[n].pin_count && p < n_comps; p++) {
            uint32_t ci = placement_util_random_int(&rng, 0, (int32_t)n_comps - 1);
            r->components[ci].net_ids[0] = n + 1;
        }
    }
    r->net_count = n_nets;
    return r;
}

static void bench_hpwl(void) {
    printf("--- HPWL Wire Length Estimation ---\n");
    const uint32_t sizes[] = {10, 50, 100, 200, 500};
    for (int si = 0; si < 5; si++) {
        uint32_t N = sizes[si];
        PlacementResult* r = create_bench_problem(N, N / 2);
        if (!r) continue;
        /* Warmup */
        for (int w = 0; w < BENCH_WARMUP; w++)
            placement_cost_hpwl(r);
        double t0 = now_ms();
        for (int i = 0; i < BENCH_ITER; i++)
            placement_cost_hpwl(r);
        double t1 = now_ms();
        double avg_us = (t1 - t0) * 1000.0 / BENCH_ITER;
        printf("  N=%4u components: %8.1f us/call\n", N, avg_us);
        placement_result_free(r); free(r);
    }
}

static void bench_steiner(void) {
    printf("\n--- Steiner Tree Estimation ---\n");
    const uint32_t sizes[] = {10, 50, 100, 200};
    for (int si = 0; si < 4; si++) {
        uint32_t N = sizes[si];
        PlacementResult* r = create_bench_problem(N, N / 2);
        if (!r) continue;
        for (int w = 0; w < BENCH_WARMUP; w++)
            placement_cost_steiner(r);
        double t0 = now_ms();
        for (int i = 0; i < BENCH_ITER; i++)
            placement_cost_steiner(r);
        double t1 = now_ms();
        double avg_us = (t1 - t0) * 1000.0 / BENCH_ITER;
        printf("  N=%4u components: %8.1f us/call\n", N, avg_us);
        placement_result_free(r); free(r);
    }
}

static void bench_spacing_check(void) {
    printf("\n--- Spacing Constraint Check ---\n");
    const uint32_t sizes[] = {10, 50, 100, 200};
    for (int si = 0; si < 4; si++) {
        uint32_t N = sizes[si];
        PlacementResult* r = create_bench_problem(N, N / 2);
        if (!r) continue;
        double t0 = now_ms();
        ConstraintResult cr = placement_constraint_check_all_spacing(r, IPC_LEVEL_B);
        double t1 = now_ms();
        printf("  N=%4u components: %8.1f ms  (%u violations)\n",
               N, t1 - t0, cr.violation_count);
        placement_constraint_result_free(&cr);
        placement_result_free(r); free(r);
    }
}

static void bench_thermal_solve(void) {
    printf("\n--- Thermal Network Solve ---\n");
    const uint32_t sizes[] = {5, 10, 20, 30};
    for (int si = 0; si < 4; si++) {
        uint32_t N = sizes[si];
        PlacementResult* r = create_bench_problem(N, 0);
        if (!r) continue;
        for (uint32_t i = 0; i < N; i++)
            r->components[i].power_dissipation_W = 1.0 + (double)i * 0.5;
        ThermalNetwork tn; memset(&tn, 0, sizeof(tn));
        placement_thermal_build_network(&tn, r, 25.0);
        double t0 = now_ms();
        placement_thermal_solve_steady_state(&tn);
        double t1 = now_ms();
        printf("  N=%4u heat sources: %8.1f ms  (%u nodes)\n",
               N, t1 - t0, tn.node_count);
        placement_thermal_network_free(&tn);
        placement_result_free(r); free(r);
    }
}

static void bench_strategy_scaling(void) {
    printf("\n--- Greedy Strategy Scaling ---\n");
    const uint32_t sizes[] = {10, 20, 50, 100};
    for (int si = 0; si < 4; si++) {
        uint32_t N = sizes[si];
        uint32_t nets = N / 2;
        if (nets < 1) nets = 1;
        PlacementResult* r = create_bench_problem(N, nets);
        if (!r) continue;
        /* Un-place all components for a fresh strategy run */
        for (uint32_t i = 0; i < N; i++) {
            if (!r->components[i].is_fixed)
                r->components[i].is_placed = 0;
        }
        double t0 = now_ms();
        placement_strategy_greedy(r);
        double t1 = now_ms();
        printf("  N=%4u components: %8.1f ms\n", N, t1 - t0);
        placement_result_free(r); free(r);
    }
}

static void bench_quadtree(void) {
    printf("\n--- Quadtree Spatial Index ---\n");
    const uint32_t sizes[] = {50, 100, 200, 500};
    for (int si = 0; si < 4; si++) {
        uint32_t N = sizes[si];
        PlacementResult* r = create_bench_problem(N, 0);
        if (!r) continue;
        QuadTree qt; memset(&qt, 0, sizeof(qt));
        double t0 = now_ms();
        placement_util_quadtree_build(&qt, r, 8);
        double t1 = now_ms();
        Rect2D qr = {{50, 30}, 100, 80};
        uint32_t results[256];
        double t2 = now_ms();
        for (int i = 0; i < BENCH_ITER; i++)
            placement_util_quadtree_query(&qt, &qr, results, 256);
        double t3 = now_ms();
        double avg_us = (t3 - t2) * 1000.0 / BENCH_ITER;
        printf("  N=%4u: build %7.1f ms, query %8.1f us\n",
               N, t1 - t0, avg_us);
        placement_util_quadtree_free(&qt);
        placement_result_free(r); free(r);
    }
}

static void bench_csv_io(void) {
    printf("\n--- CSV Import/Export ---\n");
    PlacementResult* r = create_bench_problem(100, 50);
    if (!r) return;
    const char* fname = "_bench_export.csv";
    double t0 = now_ms();
    placement_util_export_csv(r, fname);
    double t1 = now_ms();
    printf("  Export 100 components: %8.1f ms\n", t1 - t0);
    double t2 = now_ms();
    placement_util_import_csv(r, fname);
    double t3 = now_ms();
    printf("  Import 100 components: %8.1f ms\n", t3 - t2);
    remove(fname);
    placement_result_free(r); free(r);
}

int main(void) {
    printf("=== PCB Component Placement Strategy Benchmarks ===\n\n");
    bench_hpwl();
    bench_steiner();
    bench_spacing_check();
    bench_thermal_solve();
    bench_strategy_scaling();
    bench_quadtree();
    bench_csv_io();
    printf("\n=== Benchmarks Complete ===\n");
    return 0;
}