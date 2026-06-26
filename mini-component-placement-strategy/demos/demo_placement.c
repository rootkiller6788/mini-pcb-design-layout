/**
 * @file demo_placement.c
 * @brief Interactive demo: PCB component placement strategy comparison
 *
 * Demonstrates all 6 placement strategies side-by-side on identical
 * problem instances, comparing wire length, thermal cost, and runtime.
 *
 * L7 Application: Electronics manufacturing — pick-and-place optimization
 * for PCB assembly lines (relevant to Toyota production, iPhone manufacturing,
 * Detroit automotive electronics, Tesla Gigafactory PCB lines).
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
#include <math.h>
#include <time.h>

typedef struct {
    const char* name;
    double hpwl;
    double thermal;
    double overlap;
    double total_cost;
    uint32_t iterations;
    double time_ms;
} StrategyResult;

static void run_strategy(PlacementResult* base, StrategyType type,
                         StrategyResult* sr) {
    /* Copy the base placement */
    Board board_copy = base->board;
    PlacementResult r;
    placement_result_init(&r, &board_copy, base->component_count, base->net_count);
    r.component_count = base->component_count;
    r.net_count = base->net_count;
    /* Deep copy components */
    for (uint32_t i = 0; i < base->component_count; i++) {
        r.components[i] = base->components[i];
        /* Reset position for non-fixed components */
        if (!r.components[i].is_fixed) {
            r.components[i].is_placed = 0;
            r.components[i].position.x = 0;
            r.components[i].position.y = 0;
        }
    }
    for (uint32_t i = 0; i < base->net_count; i++)
        r.nets[i] = base->nets[i];
    StrategyConfig sc;
    placement_strategy_config_init(&sc, type);
    clock_t start = clock();
    sr->iterations = placement_strategy_execute(&r, &sc);
    clock_t end = clock();
    sr->time_ms = (double)(end - start) * 1000.0 / CLOCKS_PER_SEC;
    sr->hpwl = placement_cost_hpwl(&r);
    sr->thermal = placement_cost_thermal(&r, 25.0);
    sr->overlap = placement_cost_overlap(&r);
    CostWeights w; placement_cost_weights_init_default(&w);
    PlacementCost pc = placement_cost_compute_total(&r, &w, 25.0, 10.0, 0.8);
    sr->total_cost = pc.total_cost;
    placement_result_free(&r);
}

int main(void) {
    printf("=== PCB Component Placement Strategy Comparison ===\n\n");
    printf("L7 Application: PCB Assembly Optimization\n");
    printf("Relevance: Electronics manufacturing (ISO 9001), automotive\n");
    printf("(Detroit/Toyota ECU), consumer electronics (iPhone), industrial\n");
    printf("(Siemens PLC), aerospace (Boeing 787 avionics)\n\n");
    /* Create a realistic 20-component mixed-signal board */
    Board board;
    placement_board_init(&board, "DemoBoard", 120.0, 90.0, 6);
    board.thickness_mm = 1.6;
    PlacementResult base;
    if (!placement_result_init(&base, &board, 20, 10)) {
        fprintf(stderr, "Init failed\n"); return 1;
    }
    /* Diverse component mix */
    const char* des[] = {"U1","U2","U3","U4","U5","U6","U7","U8","U9","U10",
                         "R1","R2","R3","R4","C1","C2","C3","C4","J1","J2"};
    ComponentCategory ccat[] = {
        COMP_CAT_DIGITAL_IC,COMP_CAT_DIGITAL_IC,COMP_CAT_ANALOG_IC,
        COMP_CAT_ANALOG_IC,COMP_CAT_POWER,COMP_CAT_POWER,
        COMP_CAT_RF,COMP_CAT_CRYSTAL_OSC,COMP_CAT_SENSOR,COMP_CAT_ESD_PROTECT,
        COMP_CAT_PASSIVE,COMP_CAT_PASSIVE,COMP_CAT_PASSIVE,COMP_CAT_PASSIVE,
        COMP_CAT_PASSIVE,COMP_CAT_PASSIVE,COMP_CAT_PASSIVE,COMP_CAT_PASSIVE,
        COMP_CAT_CONNECTOR,COMP_CAT_CONNECTOR};
    PackageType cpkg[] = {
        PKG_QFP_100,PKG_BGA_256,PKG_SOIC_8,PKG_TSSOP_20,PKG_TO_220,PKG_QFN_32,
        PKG_QFN_16,PKG_SMD_1812,PKG_QFN_16,PKG_SC_70,
        PKG_SMD_0603,PKG_SMD_0603,PKG_SMD_0805,PKG_SMD_1206,
        PKG_SMD_0603,PKG_SMD_0805,PKG_SMD_1206,PKG_SMD_1210,
        PKG_DIP_8,PKG_DIP_16};
    double cw[] = {14,17,5,7,10,5,4,5,4,2,1.6,1.6,2,3.2,1.6,2,3.2,3.2,12,20};
    double ch[] = {14,17,4,7,15,5,4,5,4,1.2,0.8,0.8,1.2,1.6,0.8,1.2,1.6,2.5,10,8};
    double cpw[] = {0.5,2,0.1,0.15,3,1.5,0.3,0.01,0.05,0.01,0,0,0,0,0,0,0,0,0,0};
    for (uint32_t i = 0; i < 20; i++) {
        placement_component_init(&base.components[i], des[i], ccat[i], cpkg[i]);
        base.components[i].comp_id = i + 1;
        base.components[i].body.width = cw[i]; base.components[i].body.height = ch[i];
        base.components[i].power_dissipation_W = cpw[i];
        placement_component_add_pad(&base.components[i], 1, "P1", 0, 0, 0.3, 0.3);
        /* Connectors are fixed at edges */
        if (i >= 18) {
            base.components[i].is_fixed = 1;
            base.components[i].priority = 0;
        }
    }
    base.component_count = 20;
    for (uint32_t n = 0; n < 10; n++) {
        char nm[8]; snprintf(nm, 8, "NET%d", n+1);
        placement_net_init(&base.nets[n], n+1, nm);
        base.nets[n].pin_count = 2;
    }
    base.net_count = 10;
    /* Fixed connector positions */
    placement_component_set_position(&base.components[18], 110, 10, 270);
    placement_component_set_position(&base.components[19], 10, 80, 90);
    /* Run all strategies */
    const char* snames[] = {"Greedy","Simulated Annealing","Force-Directed",
                            "Min-Cut Partition","Genetic Algorithm","Clustering"};
    StrategyType stypes[] = {STRATEGY_GREEDY,STRATEGY_SIMULATED_ANNEALING,
        STRATEGY_FORCE_DIRECTED,STRATEGY_PARTITION_BISECTION,
        STRATEGY_GENETIC,STRATEGY_CLUSTERING};
    StrategyResult results[6];
    printf("%-22s %10s %10s %10s %10s %10s %10s\n",
           "Strategy","HPWL(mm)","Thermal","Overlap","TotCost","Iters","Time(ms)");
    printf("%.22s %.10s %.10s %.10s %.10s %.10s %.10s\n",
           "----------------------","----------","----------",
           "----------","----------","----------","----------");
    for (int s = 0; s < 6; s++) {
        results[s].name = snames[s];
        run_strategy(&base, stypes[s], &results[s]);
        printf("%-22s %10.1f %10.2f %10.2f %10.2f %10u %10.1f\n",
               results[s].name, results[s].hpwl, results[s].thermal,
               results[s].overlap, results[s].total_cost,
               results[s].iterations, results[s].time_ms);
    }
    /* Find best */
    int best = 0;
    for (int s = 1; s < 6; s++)
        if (results[s].total_cost < results[best].total_cost) best = s;
    printf("\nBest strategy: %s (total cost = %.2f)\n",
           results[best].name, results[best].total_cost);
    /* Pareto analysis */
    printf("\n--- Multi-Objective Analysis ---\n");
    ParetoFront pf; placement_pareto_front_init(&pf, 10);
    for (int s = 0; s < 6; s++) {
        ObjectivePoint op = {results[s].hpwl, results[s].thermal, 0, (uint32_t)s};
        if (placement_pareto_front_insert(&pf, &op))
            printf("  %-22s is Pareto-optimal\n", results[s].name);
    }
    ObjectivePoint ref = {2000, 500, 100, 0};
    double hv = placement_pareto_hypervolume(&pf, &ref);
    printf("Hypervolume indicator: %.0f\n", hv);
    placement_pareto_front_free(&pf);
    placement_result_free(&base);
    printf("\nDemo complete.\n");
    return 0;
}