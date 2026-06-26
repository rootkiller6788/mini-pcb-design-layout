/**
 * @file example_thermal.c
 * @brief End-to-end example: Thermal analysis and via optimization
 *
 * L6 Canonical Problem: Hot spot mitigation through thermal via placement
 * and strategic component spacing.
 */
#include "placement_core.h"
#include "placement_constraint.h"
#include "placement_thermal.h"
#include "placement_util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

int main(void) {
    printf("=== Example: Thermal Analysis and Via Optimization ===\n\n");
    Board board;
    placement_board_init(&board, "ThermalTestBoard", 100.0, 80.0, 4);
    placement_board_add_layer(&board, 0, "Top", 1, 0, 2.0, 0.07);
    placement_board_add_layer(&board, 1, "GND", 0, 1, 1.0, 0.035);
    placement_board_add_layer(&board, 2, "PWR", 0, 1, 1.0, 0.035);
    placement_board_add_layer(&board, 3, "Bottom", 1, 0, 2.0, 0.07);
    PlacementResult result;
    if (!placement_result_init(&result, &board, 5, 0)) {
        fprintf(stderr, "Failed\n"); return 1;
    }
    /* 5 heat-dissipating components */
    const char* names[] = {"U1_CPU","U2_PA","U3_LDO","U4_DRV","U5_FPGA"};
    double powers[] = {3.0, 8.0, 1.5, 2.5, 4.0};
    double thetas[] = {25.0, 15.0, 50.0, 30.0, 20.0};
    for (uint32_t i = 0; i < 5; i++) {
        placement_component_init(&result.components[i], names[i],
                                COMP_CAT_DIGITAL_IC, PKG_QFP_64);
        result.components[i].comp_id = i + 1;
        result.components[i].body.width = 10 + i * 2;
        result.components[i].body.height = 10 + i * 2;
        result.components[i].power_dissipation_W = powers[i];
        result.components[i].theta_JA_C_per_W = thetas[i];
        result.components[i].theta_JC_C_per_W = thetas[i] * 0.2;
        result.components[i].max_junction_temp_C = 125.0;
    }
    result.component_count = 5;
    /* Place in a line with varying spacing */
    for (uint32_t i = 0; i < 5; i++)
        placement_component_set_position(&result.components[i],
                                         15.0 + i * 18.0, 40.0, 0.0);
    /* Build and solve thermal network */
    ThermalNetwork tn; memset(&tn, 0, sizeof(tn));
    if (!placement_thermal_build_network(&tn, &result, 25.0)) {
        fprintf(stderr, "Network build failed\n"); return 1;
    }
    printf("Thermal network: %u nodes, %u edges\n", tn.node_count, tn.edge_count);
    if (!placement_thermal_solve_steady_state(&tn)) {
        fprintf(stderr, "Solve failed\n"); return 1;
    }
    /* Report junction temperatures */
    printf("\n--- Junction Temperatures ---\n");
    for (uint32_t i = 1; i < tn.node_count; i++) {
        double tj = placement_thermal_junction_temp(
            &result.components[0], &tn);
        /* Find correct component */
        for (uint32_t c = 0; c < result.component_count; c++) {
            if (result.components[c].comp_id == tn.nodes[i].comp_id) {
                tj = placement_thermal_junction_temp(&result.components[c], &tn);
                break;
            }
        }
        printf("  Node %u (comp_id=%u): T=%.1f C\n",
               tn.nodes[i].node_id, tn.nodes[i].comp_id, tj);
    }
    /* Temperature distribution map */
    printf("\n--- Temperature Distribution (C) ---\n");
    for (int row = 8; row >= 0; row--) {
        printf("  ");
        for (int col = 0; col < 10; col++) {
            double tx = 5.0 + col * 10.0;
            double ty = 5.0 + row * 10.0;
            double T = 25.0;
            /* Superpose contributions from all sources */
            for (uint32_t s = 0; s < result.component_count; s++) {
                if (result.components[s].power_dissipation_W > 0) {
                    T += placement_thermal_temperature_at(
                        tx, ty,
                        result.components[s].position.x,
                        result.components[s].position.y,
                        result.components[s].power_dissipation_W,
                        0.3, 1.6, 10.0, 0.0) - 25.0;
                }
            }
            printf("%5.1f ", T);
        }
        printf("\n");
    }
    /* Hot spot detection */
    Point2D spots[8];
    uint32_t n_spots = placement_thermal_detect_hotspots(&result, &tn, 5.0, spots, 8);
    printf("\nHot spots detected: %u\n", n_spots);
    for (uint32_t i = 0; i < n_spots && i < 8; i++)
        printf("  Spot %u at (%.1f, %.1f)\n", i+1, spots[i].x, spots[i].y);
    /* Via optimization for hottest component */
    double max_grad = placement_thermal_max_gradient(&tn);
    printf("Max thermal gradient: %.3f C/mm\n", max_grad);
    /* Recommend vias for the PA (highest power) */
    Rect2D via_area = {{30, 30}, 20, 20};
    int32_t vias = placement_thermal_vias_required(
        &result.components[1], 125.0, 25.0, 0.3, 1.6, via_area, 0.3);
    printf("\nThermal vias recommended for %s: %d\n",
           vias >= 0 ? names[1] : "N/A", vias >= 0 ? vias : -vias);
    if (vias < 0)
        printf("  (Does not fit in available area — need %d vias)\n", -vias);
    /* Spreading resistance */
    double R_sp = placement_thermal_spreading_resistance(
        &result.components[1], 0.3, 1.6);
    printf("Spreading resistance for %s: %.2f C/W\n", names[1], R_sp);
    /* Single via resistance */
    ThermalVia tv; memset(&tv, 0, sizeof(tv));
    tv.outer_diameter_mm = 0.4; tv.drill_diameter_mm = 0.3;
    tv.plating_thickness_um = 25; tv.via_count = 1;
    double R_via = placement_thermal_via_resistance(&tv, 1.6);
    printf("Single via resistance: %.2f C/W\n", R_via);
    placement_thermal_network_free(&tn);
    placement_result_free(&result);
    printf("\nDone.\n");
    return 0;
}