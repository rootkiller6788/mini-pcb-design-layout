/**
 * example_ldo_cooling.c - LDO Voltage Regulator Thermal Design Example
 *
 * L6/L7: Complete thermal design for an LDO regulator.
 * Demonstrates: copper pour sizing, junction temperature calculation,
 * derating analysis, and lifetime estimation.
 *
 * Scenario: 3.3V LDO powering a GPS receiver module from a 5V supply.
 * Load current: 500mA, Power dissipation: (5-3.3)*0.5 = 0.85W.
 * SOT-223 package on FR4, 1oz copper.
 */

#include <stdio.h>
#include <math.h>
#include "pcb_thermal_defs.h"
#include "pcb_thermal_analysis.h"
#include "pcb_thermal_material.h"
#include "pcb_thermal_design.h"

int main(void) {
    printf("============================================================\n");
    printf(" LDO Regulator Thermal Design Example\n");
    printf("============================================================\n\n");

    /* Setup the LDO thermal scenario */
    double vin = 5.0, vout = 3.3, i_load = 0.5;
    double power_w = (vin - vout) * i_load;
    printf("Operating Point:\n");
    printf("  Vin = %.1f V, Vout = %.1f V, I_load = %.2f A\n", vin, vout, i_load);
    printf("  Power dissipation = %.3f W\n\n", power_w);

    /* LDO in SOT-223 package:
     * Rjc typically 15-25 C/W for SOT-223 with thermal pad.
     * Rja without copper pour: ~50-80 C/W.
     * Max junction temperature: 125 C for commercial grade. */
    heat_source_t ldo = {
        .center = {.x_mm = 25.0, .y_mm = 25.0, .z_mm = 0.0},
        .width_mm = 6.5,
        .length_mm = 7.0,
        .height_mm = 1.6,
        .power_w = power_w,
        .package = PKG_SOT23,  /* SOT-223 is similar thermally */
        .r_jc = 20.0,           /* Junction-to-case (thermal pad soldered) */
        .max_tj = 125.0,
        .has_heatsink = 0,
        .has_thermal_vias = 0
    };

    /* Ambient: 45 C (enclosed consumer electronics) */
    double ta_c = 45.0;

    printf("Component Parameters:\n");
    printf("  Package: SOT-223 with exposed thermal pad\n");
    printf("  Rjc = %.1f C/W\n", ldo.r_jc);
    printf("  Tj_max = %.0f C\n", ldo.max_tj);
    printf("  Ta = %.0f C\n\n", ta_c);

    /* --- Step 1: Worst-case analysis (no thermal management) --- */
    printf("--- Step 1: Worst-Case (no copper pour) ---\n");
    double rja_no_pour = 80.0;  /* Typical SOT-223 Rja without pour */
    double tj_no_pour = ta_c + power_w * rja_no_pour;
    printf("  Rja (no pour) = %.1f C/W\n", rja_no_pour);
    printf("  Tj = %.1f C\n", tj_no_pour);
    printf("  Margin = %.1f C %s\n\n", ldo.max_tj - tj_no_pour,
           (tj_no_pour > ldo.max_tj) ? "<<< EXCEEDED!" : "OK");

    /* --- Step 2: Copper pour sizing --- */
    printf("--- Step 2: Copper Pour Sizing ---\n");
    double pour_area, r_pour;
    int ret = copper_pour_sizing(power_w, ta_c, ldo.max_tj, ldo.r_jc,
                                  0.35, 1.6, 10.0, CU_1OZ,
                                  &pour_area, &r_pour);
    if (ret == THERMAL_OK) {
        printf("  Required copper pour area: %.0f mm^2 (%.1f x %.1f mm square)\n",
               pour_area, sqrt(pour_area), sqrt(pour_area));
        printf("  Achieved R_ambient: %.1f C/W\n", r_pour);

        double tj_with_pour = ta_c + power_w * (ldo.r_jc + r_pour);
        printf("  Tj with pour: %.1f C\n", tj_with_pour);
        printf("  Margin: %.1f C\n\n", ldo.max_tj - tj_with_pour);
    } else {
        printf("  Copper pour insufficient - consider additional cooling.\n\n");
    }

    /* --- Step 3: Add thermal vias if needed --- */
    printf("--- Step 3: Thermal Via Assessment ---\n");
    /* Assume we can fit a 10x10mm via array under the component */
    thermal_via_geometry_t via_geom = {
        .drill_diameter_mm = 0.3,
        .pad_diameter_mm = 0.6,
        .plating_thickness_mm = 0.025,
        .pitch_mm = 0.8,
        .num_vias = 16,
        .rows = 4,
        .cols = 4,
        .is_hexagonal = 0,
        .is_filled = 0,
        .fill_k = 0.0,
        .via_length_mm = 1.6
    };

    double r_single = thermal_via_single_resistance(&via_geom, 1.6, 385.0);
    double r_array = thermal_via_array_resistance(&via_geom, 1.6, 385.0);
    printf("  Via array: %d vias (4x4 grid, d=0.3mm, pitch=0.8mm)\n", via_geom.num_vias);
    printf("  Single via R_theta = %.1f C/W\n", r_single);
    printf("  Array R_theta (with efficiency) = %.1f C/W\n", r_array);

    /* Combined with copper pour: parallel path */
    double r_combined = thermal_resistance_parallel(r_pour, r_array);
    double tj_with_vias = ta_c + power_w * (ldo.r_jc + r_combined);
    printf("  Combined R (pour || vias) = %.1f C/W\n", r_combined);
    printf("  Tj with pour + vias = %.1f C\n", tj_with_vias);
    printf("  Improvement over bare FR4: %.1fx\n\n", rja_no_pour / (ldo.r_jc + r_combined));

    /* --- Step 4: Derating and reliability --- */
    printf("--- Step 4: Reliability Assessment ---\n");
    double derating = thermal_derating_factor(tj_with_vias, ldo.max_tj, ta_c);
    printf("  Derating factor = %.2f (%.1f C margin)\n", derating, ldo.max_tj - tj_with_vias);

    /* Arrhenius lifetime estimate:
     * Assume rated lifetime 50000h at Tj=105 C.
     * Actual Tj=~70 C -> extended lifetime */
    double lifetime = thermal_lifetime_estimate(tj_with_vias, 105.0, 50000.0, 0.7);
    printf("  Estimated lifetime (Ea=0.7eV): %.0f hours (%.1f years)\n",
           lifetime, lifetime / 8760.0);

    /* --- Summary --- */
    printf("\n============================================================\n");
    printf(" DESIGN SUMMARY\n");
    printf("============================================================\n");
    printf("  Component:    3.3V LDO Regulator (%.2f W)\n", power_w);
    printf("  Package:      SOT-223 with exposed pad\n");
    printf("  Cooling:      Copper pour + thermal vias\n");
    printf("  Copper pour:  %.0f mm^2\n", pour_area);
    printf("  Thermal vias: %d vias (4x4, 0.3mm drill)\n", via_geom.num_vias);
    printf("  Final Tj:     %.1f C (max %.0f C)\n", tj_with_vias, ldo.max_tj);
    printf("  Margin:       %.1f C\n", ldo.max_tj - tj_with_vias);
    printf("  Action:       %s\n\n", (derating > 0.3) ? "APPROVED" : "REDESIGN REQUIRED");

    return 0;
}
