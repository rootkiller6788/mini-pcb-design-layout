/**
 * @file example_bend_analysis.c
 * @brief End-to-end example: Flex bend reliability analysis for a smartphone
 *        hinge flex design (dynamic flex).
 *
 * Simulates a 2-layer flex cable in a folding phone hinge —
 * 200,000 open/close cycles over product life at 25°C to 45°C.
 *
 * L6 Canonical Problem: Dynamic flex life prediction per IPC-2223.
 */

#include "flex_bend.h"
#include "flex_material.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    printf("=== Flex Bend Analysis: Foldable Phone Hinge Flex ===\n\n");

    /* Design parameters — 2-layer flex in hinge */
    flex_bend_params_t params;
    params.config                    = FLEX_BEND_U_SHAPE;
    params.grain_orientation         = FLEX_BEND_PERPENDICULAR_GRAIN;
    params.bend_radius_mm            = 1.5;   /* Sub-2mm radius for compact hinge */
    params.bend_angle_deg            = 180.0; /* Full fold */
    params.total_thickness_mm        = 0.12;  /* 120 μm total (thin flex) */
    params.copper_thickness_total_um = 36.0;  /* 2 × 18 μm Cu */
    params.copper_elongation_limit_percent = 16.0; /* RA copper */
    params.num_layers                = 2;
    params.is_dynamic                = 1;     /* Dynamic — repeated folding */
    params.expected_cycles           = 200000.0;
    params.operating_temp_min_c      = 25.0;
    params.operating_temp_max_c      = 45.0;
    params.youngs_modulus_copper_mpa = 117000.0;

    flex_bend_result_t result;
    if (flex_bend_analyze(&params, &result) != 0) {
        printf("ERROR: Analysis failed.\n");
        return 1;
    }

    printf("  Design: 2-layer flex, 0.12 mm thick, U-bend\n");
    printf("  Bend radius: %.2f mm\n", params.bend_radius_mm);
    printf("  IPC-2223 minimum radius: %.2f mm\n", result.min_bend_radius_mm);
    printf("  Safety factor: %.2f×\n", result.safety_factor);
    printf("  IPC-2223 compliant: %s\n",
           result.is_compliant_ipc2223 ? "YES" : "NO");
    printf("\n  Maximum copper strain: %.2f%%\n",
           result.max_copper_strain_percent);
    printf("  Strain safety factor: %.2f×\n", result.strain_safety_factor);
    printf("\n  Estimated cycles to failure: %.0f\n",
           result.estimated_cycles_to_failure);
    printf("  Required cycles: %.0f\n", params.expected_cycles);
    printf("  Life margin: %.1f×\n",
           result.estimated_cycles_to_failure / params.expected_cycles);
    printf("\n  Failure mode: %s\n", result.failure_description);

    /* Temperature derating for max operating temp */
    double life_85c = flex_cycles_temperature_derate(
        result.estimated_cycles_to_failure, 45.0, 0.8);
    printf("\n  At 45°C: %.0f cycles (Arrhenius derated)\n", life_85c);

    /* Summary */
    printf("\n=== Verdict: %s ===\n",
           (result.is_compliant_ipc2223 &&
            result.estimated_cycles_to_failure > params.expected_cycles)
           ? "PASS — Design meets requirements" : "FAIL — Redesign needed");

    return 0;
}
