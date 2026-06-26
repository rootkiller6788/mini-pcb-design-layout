/**
 * @file example_greedy.c
 * @brief End-to-end example: Greedy sequential placement on a mixed-signal board
 *
 * L6 Canonical Problem: Mixed-signal PCB component placement with
 * analog/digital domain separation using greedy strategy.
 */
#include "placement_core.h"
#include "placement_constraint.h"
#include "placement_optimizer.h"
#include "placement_strategy.h"
#include "placement_util.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Example: Greedy Mixed-Signal PCB Placement ===\n\n");
    Board board;
    placement_board_init(&board, "MixedSignalBoard", 100.0, 80.0, 4);
    placement_board_add_layer(&board, 0, "Top", 1, 0, 1.0, 0.035);
    placement_board_add_layer(&board, 1, "GND", 0, 1, 1.0, 0.035);
    placement_board_add_layer(&board, 2, "PWR", 0, 1, 1.0, 0.035);
    placement_board_add_layer(&board, 3, "Bottom", 1, 0, 1.0, 0.035);
    PlacementResult result;
    if (!placement_result_init(&result, &board, 12, 8)) {
        fprintf(stderr, "Failed\n"); return 1;
    }
    const char* names[] = {"U1_MCU","U2_FPGA","U3_OPAMP","U4_ADC",
        "R1","R2","C1","C2","J1_USB","J2_HDR","VR1","VR2"};
    ComponentCategory cats[] = {COMP_CAT_DIGITAL_IC,COMP_CAT_DIGITAL_IC,
        COMP_CAT_ANALOG_IC,COMP_CAT_ANALOG_IC,COMP_CAT_PASSIVE,COMP_CAT_PASSIVE,
        COMP_CAT_PASSIVE,COMP_CAT_PASSIVE,COMP_CAT_CONNECTOR,COMP_CAT_CONNECTOR,
        COMP_CAT_POWER,COMP_CAT_POWER};
    PackageType pkgs[] = {PKG_QFP_64,PKG_BGA_256,PKG_SOIC_8,PKG_TSSOP_20,
        PKG_SMD_0603,PKG_SMD_0603,PKG_SMD_0805,PKG_SMD_0805,
        PKG_DIP_8,PKG_DIP_16,PKG_SOT_223,PKG_QFN_16};
    double ws[] = {12,17,5,7,1.6,1.6,2,2,12,20,7,5};
    double hs[] = {12,17,4,7,0.8,0.8,1.2,1.2,10,8,7,5};
    double ps[] = {0.3,1.0,0.05,0.02,0,0,0,0,0,0,0.5,2.0};
    for (uint32_t i = 0; i < 12; i++) {
        placement_component_init(&result.components[i], names[i], cats[i], pkgs[i]);
        result.components[i].comp_id = i + 1;
        result.components[i].body.width = ws[i];
        result.components[i].body.height = hs[i];
        result.components[i].power_dissipation_W = ps[i];
        placement_component_add_pad(&result.components[i], 1, "P1", 0, 0, 0.3, 0.3);
        if (i == 8 || i == 9) {
            result.components[i].is_fixed = 1;
            result.components[i].priority = 0;
        }
    }
    result.component_count = 12;
    const char* netn[] = {"SPI_MOSI","SPI_CLK","ANLG_IN","ANLG_REF",
        "VDD_3V3","VDD_1V8","USB_DP","USB_DN"};
    for (uint32_t n = 0; n < 8; n++) {
        placement_net_init(&result.nets[n], n + 1, netn[n]);
        result.nets[n].pin_count = 2;
    }
    result.net_count = 8;
    result.components[0].net_ids[0] = 1; result.components[1].net_ids[0] = 1;
    result.components[2].net_ids[0] = 3; result.components[3].net_ids[0] = 3;
    result.components[4].net_ids[0] = 5; result.components[5].net_ids[0] = 6;
    result.components[8].net_ids[0] = 7; result.components[9].net_ids[0] = 8;
    result.components[0].net_ids[0] = 1;
    placement_component_set_position(&result.components[8], 90, 10, 270);
    placement_component_set_position(&result.components[9], 10, 70, 90);
    printf("Running greedy placement strategy...\n");
    uint32_t placed = placement_strategy_greedy(&result);
    printf("Placed %u / %u components\n", placed, result.component_count);
    double hpwl = placement_cost_hpwl(&result);
    double overlap = placement_cost_overlap(&result);
    double thermal = placement_cost_thermal(&result, 25.0);
    printf("HPWL: %.1f mm  Overlap: %.2f  Thermal: %.2f\n", hpwl, overlap, thermal);
    ConstraintResult cr = placement_constraint_check_all_spacing(&result, IPC_LEVEL_B);
    printf("Spacing violations: %u  All clear: %s\n", cr.violation_count,
           cr.all_clear ? "YES" : "NO");
    placement_constraint_result_free(&cr);
    placement_util_export_csv(&result, "example_greedy_placement.csv");
    printf("Exported to example_greedy_placement.csv\n");
    placement_util_print_summary(&result);
    placement_result_free(&result);
    printf("Done.\n");
    return 0;
}