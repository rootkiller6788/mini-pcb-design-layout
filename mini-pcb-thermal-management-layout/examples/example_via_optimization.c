/**
 * example_via_optimization.c - Thermal Via Array Optimization Example
 *
 * L6: Optimize thermal via placement under a power component.
 * Demonstrates: via array resistance, fill analysis, footprint optimization,
 * effective Z-conductivity, and improvement factor over bare FR4.
 *
 * Scenario: GaN power transistor with 10W dissipation on a 4-layer PCB.
 * The component has a 8x8mm thermal pad area for vias.
 */

#include <stdio.h>
#include <math.h>
#include "pcb_thermal_defs.h"
#include "pcb_thermal_analysis.h"
#include "pcb_thermal_via.h"
#include "pcb_thermal_material.h"

int main(void) {
    printf("============================================================\n");
    printf(" Thermal Via Array Optimization Example\n");
    printf("============================================================\n\n");

    /* Scenario: GaN power transistor on 4-layer PCB
     * 4-layer stack: SIG_TOP / GND (solid) / PWR (solid) / SIG_BOT
     * Total thickness: 1.6mm
     * Component thermal pad: 8x8mm for vias */
    double power_w = 10.0;
    double via_length_mm = 1.6;
    double k_cu = 385.0;
    double footprint_w = 8.0;
    double footprint_l = 8.0;
    double plate_mm = 0.025;

    printf("Design Parameters:\n");
    printf("  Component power: %.0f W\n", power_w);
    printf("  PCB thickness: %.1f mm (4-layer)\n", via_length_mm);
    printf("  Available via footprint: %.0f x %.0f mm\n", footprint_w, footprint_l);
    printf("  Via plating: %.0f um\n\n", plate_mm * 1000.0);

    /* --- Step 1: Single via baseline --- */
    printf("--- Step 1: Single Via Baseline ---\n");
    thermal_via_geometry_t single_via = {
        .drill_diameter_mm = 0.3,
        .plating_thickness_mm = plate_mm,
        .pitch_mm = 0.8,
        .num_vias = 1,
        .is_filled = 0,
        .via_length_mm = via_length_mm
    };

    double r_single = thermal_via_single_resistance(&single_via, via_length_mm, k_cu);
    double a_cross = thermal_via_cross_section_area(&single_via);
    printf("  Single via (d=0.3mm, unfilled):\n");
    printf("    Cross-section area: %.4f mm^2\n", a_cross);
    printf("    Thermal resistance: %.1f C/W\n\n", r_single);

    /* --- Step 2: Filled vs Unfilled comparison --- */
    printf("--- Step 2: Filled vs Unfilled Comparison ---\n");
    double d_drill[] = {0.2, 0.3, 0.4, 0.5, 0.6};
    printf("  %-8s %-12s %-12s %-10s\n", "Drill(mm)", "R_unfilled", "R_filled", "Improve");
    printf("  %-8s %-12s %-12s %-10s\n", "--------", "----------", "--------", "-------");
    for (int i = 0; i < 5; i++) {
        double r_u, r_f, imp;
        thermal_via_fill_analysis(d_drill[i], plate_mm, 58.0, &r_u, &r_f, &imp);
        printf("  %-8.2f %-12.1f %-12.1f %-10.1fx\n", d_drill[i], r_u, r_f, imp);
    }
    printf("\n");

    /* --- Step 3: Optimize via array for footprint --- */
    printf("--- Step 3: Via Array Optimization ---\n");
    thermal_via_geometry_t optimized;
    int ret = thermal_via_optimize_array(footprint_w, footprint_l,
                                          via_length_mm, k_cu, plate_mm,
                                          &optimized);
    if (ret == THERMAL_OK) {
        printf("  Optimal configuration:\n");
        printf("    Drill diameter: %.2f mm\n", optimized.drill_diameter_mm);
        printf("    Grid: %d rows x %d cols = %d vias\n",
               optimized.rows, optimized.cols, optimized.num_vias);
        printf("    Pitch: %.2f mm\n", optimized.pitch_mm);
        printf("    Single via R_theta: %.1f C/W\n", optimized.r_theta_single);
        printf("    Array R_theta: %.1f C/W\n", optimized.r_theta_array);

        /* Temperature rise with this array */
        double delta_t = power_w * optimized.r_theta_array;
        printf("    Delta-T for %.0fW: %.1f C\n\n", power_w, delta_t);
    } else {
        printf("  Optimization failed (ret=%d)\n\n", ret);
    }

    /* --- Step 4: Effective Z-conductivity --- */
    printf("--- Step 4: Effective Through-Plane Conductivity ---\n");

    /* Different via configurations and their effect on k_z */
    struct { int n_vias; double d_mm; const char *desc; } configs[] = {
        {0,  0.0, "FR4 only (no vias)"},
        {4,  0.3, "Sparse (4 vias, 0.3mm)"},
        {9,  0.3, "Moderate (9 vias, 0.3mm)"},
        {16, 0.3, "Dense (16 vias, 0.3mm)"},
        {25, 0.3, "Very dense (25 vias, 0.3mm)"},
        {16, 0.5, "Dense large (16 vias, 0.5mm)"}
    };

    double k_fr4 = 0.25;  /* Through-plane FR4 conductivity */
    printf("  %-30s %8s %10s %8s\n", "Configuration", "k_eff_z", "Improve", "R_theta");
    printf("  %-30s %8s %10s %8s\n", "------------", "-------", "-------", "------");

    for (int i = 0; i < 6; i++) {
        if (configs[i].n_vias == 0) {
            double r_bare = via_length_mm / (k_fr4 * footprint_w * footprint_l) * 1.0e3;
            printf("  %-30s %8.3f %10s %8.1f\n",
                   configs[i].desc, k_fr4, "1.0x", r_bare);
        } else {
            thermal_via_geometry_t geom = {
                .drill_diameter_mm = configs[i].d_mm,
                .plating_thickness_mm = plate_mm,
                .pitch_mm = configs[i].d_mm * 2.5,
                .num_vias = configs[i].n_vias,
                .rows = (int)sqrt(configs[i].n_vias),
                .cols = (int)sqrt(configs[i].n_vias),
                .is_filled = 0,
                .via_length_mm = via_length_mm
            };
            geom.rows = geom.cols = (int)sqrt(configs[i].n_vias);
            if (geom.rows * geom.cols < configs[i].n_vias) geom.cols++;

            double k_eff_z = thermal_via_effective_kz(&geom, k_cu, k_fr4);
            double r = thermal_via_array_resistance(&geom, via_length_mm, k_cu);
            double impr = thermal_via_improvement_factor(&geom, via_length_mm,
                            footprint_w * footprint_l, k_cu, k_fr4);
            printf("  %-30s %8.3f %9.1fx %8.1f\n",
                   configs[i].desc, k_eff_z, impr, r);
        }
    }
    printf("\n");

    /* --- Step 5: Efficiency analysis --- */
    printf("--- Step 5: Via Array Efficiency Analysis ---\n");
    printf("  Efficiency vs. via count (d=0.3mm, pitch=0.8mm, L=1.6mm):\n");
    int counts[] = {1, 4, 9, 16, 25, 36, 64, 100};
    printf("  %-10s %12s %12s\n", "N_vias", "Ideal R", "Actual R");
    printf("  %-10s %12s %12s\n", "------", "-------", "---------");
    for (int i = 0; i < 8; i++) {
        thermal_via_geometry_t via_test = single_via;
        via_test.num_vias = counts[i];
        via_test.rows = via_test.cols = (int)sqrt(counts[i]);
        if (via_test.rows * via_test.cols < counts[i]) via_test.cols++;

        double r_ideal = r_single / counts[i];
        double r_actual = thermal_via_array_resistance(&via_test, via_length_mm, k_cu);
        printf("  %-10d %12.1f %12.1f\n", counts[i], r_ideal, r_actual);
    }

    /* --- Summary --- */
    printf("\n============================================================\n");
    printf(" VIA OPTIMIZATION SUMMARY\n");
    printf("============================================================\n");
    printf("  Application:   GaN power transistor (%.0fW)\n", power_w);
    printf("  Footprint:     %.0f x %.0f mm\n", footprint_w, footprint_l);
    printf("  Optimal vias:  %d (%.1fmm drill, %.2fmm pitch)\n",
           optimized.num_vias, optimized.drill_diameter_mm, optimized.pitch_mm);
    printf("  R_theta array: %.1f C/W\n", optimized.r_theta_array);
    printf("  Delta-T:       %.1f C (%.0fW load)\n",
           power_w * optimized.r_theta_array, power_w);
    printf("  Improvement:   %.0fx over bare FR4\n",
           thermal_via_improvement_factor(&optimized, via_length_mm,
               footprint_w * footprint_l, k_cu, k_fr4));
    printf("  Recommendation: %s\n\n",
           (power_w * optimized.r_theta_array < 50.0) ?
           "ACCEPTABLE" : "Increase via count or use filled vias");

    return 0;
}
