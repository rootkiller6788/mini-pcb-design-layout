/**
 * @file example_sa.c
 * @brief End-to-end example: Thermal-aware Simulated Annealing placement
 *
 * L6 Canonical Problem: Power electronics thermal management through
 * optimal component placement. Demonstrates SA optimization with
 * thermal cost weighting.
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

int main(void) {
    printf("=== Example: Thermal-Aware Simulated Annealing ===\n\n");
    Board board;
    placement_board_init(&board, "PowerBoard", 120.0, 80.0, 4);
    placement_board_add_layer(&board, 0, "Top", 1, 0, 2.0, 0.07);
    placement_board_add_layer(&board, 1, "GND", 0, 1, 2.0, 0.07);
    placement_board_add_layer(&board, 2, "PWR", 0, 1, 2.0, 0.07);
    placement_board_add_layer(&board, 3, "Bottom", 1, 0, 2.0, 0.07);
    PlacementResult result;
    if (!placement_result_init(&result, &board, 10, 6)) {
        fprintf(stderr, "Failed\n"); return 1;
    }
    /* 4 power MOSFETs with high heat dissipation */
    for (uint32_t i = 0; i < 4; i++) {
        char des[8]; snprintf(des, 8, "Q%d", i+1);
        placement_component_init(&result.components[i], des, COMP_CAT_POWER, PKG_TO_220);
        result.components[i].comp_id = i + 1;
        result.components[i].body.width = 10; result.components[i].body.height = 15;
        result.components[i].power_dissipation_W = 3.0 + i * 1.0;
        result.components[i].max_junction_temp_C = 150.0;
        result.components[i].theta_JA_C_per_W = 40.0;
        result.components[i].theta_JC_C_per_W = 2.0;
        placement_component_add_pad(&result.components[i], 1, "D", 0, -5, 1, 1);
        placement_component_add_pad(&result.components[i], 2, "G", -2, 5, 0.5, 0.5);
        placement_component_add_pad(&result.components[i], 3, "S", 2, 5, 0.5, 0.5);
    }
    /* 2 Controller ICs */
    for (uint32_t i = 0; i < 2; i++) {
        uint32_t idx = 4 + i;
        const char* icn[] = {"U1_PWM","U2_ADC"};
        placement_component_init(&result.components[idx], icn[i],
                                COMP_CAT_DIGITAL_IC, PKG_QFP_32);
        result.components[idx].comp_id = idx + 1;
        result.components[idx].body.width = 7;
        result.components[idx].body.height = 7;
        result.components[idx].power_dissipation_W = 0.2;
        placement_component_add_pad(&result.components[idx], 1, "P1", 0, 0, 0.3, 0.3);
    }
    /* 4 passives */
    for (uint32_t i = 0; i < 4; i++) {
        uint32_t idx = 6 + i;
        char des[8]; snprintf(des, 8, "%s%d", i<2?"R":"C", i+1);
        placement_component_init(&result.components[idx], des,
                                COMP_CAT_PASSIVE, PKG_SMD_0805);
        result.components[idx].comp_id = idx + 1;
        result.components[idx].body.width = 2;
        result.components[idx].body.height = 1.2;
        result.components[idx].power_dissipation_W = 0.1;
        placement_component_add_pad(&result.components[idx], 1, "T1", 0, 0, 0.3, 0.3);
    }
    result.component_count = 10;
    for (uint32_t n = 0; n < 6; n++) {
        char nm[8]; snprintf(nm, 8, "NET%d", n+1);
        placement_net_init(&result.nets[n], n+1, nm);
        result.nets[n].pin_count = 2;
    }
    result.net_count = 6;
    /* Connect components via nets */
    result.components[0].net_ids[0] = 1; result.components[4].net_ids[0] = 1;
    result.components[1].net_ids[0] = 2; result.components[4].net_ids[0] = 2;
    result.components[2].net_ids[0] = 3; result.components[5].net_ids[0] = 3;
    result.components[3].net_ids[0] = 4; result.components[5].net_ids[0] = 4;
    result.components[0].net_ids[1] = 5; result.components[1].net_ids[1] = 5;
    result.components[2].net_ids[2] = 6; result.components[3].net_ids[2] = 6;
    /* Random initial placement */
    RandomState rng; placement_util_random_init(&rng, 4242);
    for (uint32_t i = 0; i < 10; i++) {
        double x = 10 + placement_util_random_uniform(&rng) * 100;
        double y = 10 + placement_util_random_uniform(&rng) * 60;
        placement_component_set_position(&result.components[i], x, y, 0);
    }
    double init_thermal = placement_cost_thermal(&result, 40.0);
    printf("Initial thermal cost (T_amb=40C): %.2f\n", init_thermal);
    ThermalNetwork tn; memset(&tn, 0, sizeof(tn));
    placement_thermal_build_network(&tn, &result, 40.0);
    placement_thermal_solve_steady_state(&tn);
    double max_tj_before = 40.0;
    for (uint32_t i = 1; i < tn.node_count; i++)
        if (tn.nodes[i].temperature_C > max_tj_before)
            max_tj_before = tn.nodes[i].temperature_C;
    printf("Max T_j before: %.1f C\n", max_tj_before);
    placement_thermal_network_free(&tn);
    /* SA optimization */
    SAConfig sa;
    memset(&sa, 0, sizeof(sa));
    sa.initial_temperature = 500; sa.final_temperature = 0.01;
    sa.schedule = COOLING_EXPONENTIAL; sa.alpha = 0.92;
    sa.moves_per_temperature = 50; sa.max_iterations = 1000;
    sa.random_seed = 42; sa.swap_probability = 0.4; sa.max_move_distance = 15;
    printf("\nRunning SA (thermal-aware)...\n");
    uint32_t iters = placement_strategy_simulated_annealing(&result, &sa);
    printf("Completed %u iterations\n", iters);
    double final_thermal = placement_cost_thermal(&result, 40.0);
    printf("Final thermal cost: %.2f\n", final_thermal);
    ThermalNetwork tn2; memset(&tn2, 0, sizeof(tn2));
    placement_thermal_build_network(&tn2, &result, 40.0);
    placement_thermal_solve_steady_state(&tn2);
    double max_tj_after = 40.0;
    for (uint32_t i = 1; i < tn2.node_count; i++)
        if (tn2.nodes[i].temperature_C > max_tj_after)
            max_tj_after = tn2.nodes[i].temperature_C;
    printf("Max T_j after: %.1f C\n", max_tj_after);
    printf("Temperature reduction: %.1f C\n", max_tj_before - max_tj_after);
    Point2D spots[4];
    uint32_t n_spots = placement_thermal_detect_hotspots(&result, &tn2, 5.0, spots, 4);
    printf("Hot spots: %u\n", n_spots);
    placement_thermal_network_free(&tn2);
    double hpwl = placement_cost_hpwl(&result);
    printf("Final HPWL: %.1f mm\n", hpwl);
    placement_util_export_csv(&result, "example_sa_placement.csv");
    printf("Exported to example_sa_placement.csv\n");
    placement_result_free(&result);
    return 0;
}