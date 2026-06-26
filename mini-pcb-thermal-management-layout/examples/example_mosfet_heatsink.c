/**
 * example_mosfet_heatsink.c - MOSFET Heatsink Selection Example
 *
 * L6/L7: Power MOSFET thermal design with forced convection heatsink.
 * Demonstrates: junction temperature analysis, heatsink selection,
 * thermal runaway check for paralleled MOSFETs.
 *
 * Scenario: Two parallel MOSFETs in a motor drive application.
 * Each dissipates 3W. Ambient = 55 C (inside motor enclosure).
 * Forced air cooling at 2 m/s from a system fan.
 */

#include <stdio.h>
#include <math.h>
#include "pcb_thermal_defs.h"
#include "pcb_thermal_analysis.h"
#include "pcb_thermal_material.h"
#include "pcb_thermal_design.h"

int main(void) {
    printf("============================================================\n");
    printf(" MOSFET Motor Drive Thermal Design Example\n");
    printf("============================================================\n\n");

    double ta_c = 55.0;
    ambient_conditions_t amb = ambient_default();
    amb.ambient_temp_c = ta_c;

    printf("System Parameters:\n");
    printf("  Ambient temperature: %.0f C (enclosed motor drive)\n", ta_c);
    printf("  Forced airflow: 2.0 m/s from system fan\n\n");

    /* MOSFET: TO-220 package with heatsink
     * IRFZ44N or similar: Rjc = 1.5 C/W, Tj_max = 175 C
     * Each MOSFET handles ~3W in a switching application */
    double power_per_mosfet = 3.0;

    heat_source_t mosfet = {
        .center = {.x_mm = 0.0, .y_mm = 0.0, .z_mm = 0.0},
        .width_mm = 10.0,
        .length_mm = 15.0,
        .height_mm = 4.5,
        .power_w = power_per_mosfet,
        .package = PKG_TO220,
        .r_jc = 1.5,
        .max_tj = 175.0,
        .has_heatsink = 0,
        .has_thermal_vias = 0
    };

    printf("MOSFET Parameters:\n");
    printf("  Package: TO-220\n");
    printf("  Rjc = %.1f C/W\n", mosfet.r_jc);
    printf("  Tj_max = %.0f C\n", mosfet.max_tj);
    printf("  Power per MOSFET = %.1f W\n\n", power_per_mosfet);

    /* --- Step 1: Without heatsink --- */
    printf("--- Step 1: Without Heatsink ---\n");
    /* TO-220 in free air: Rja ~ 62 C/W */
    double rja_free_air = 62.0;
    double tj_free_air = ta_c + power_per_mosfet * rja_free_air;
    printf("  Rja (TO-220 free air) = %.1f C/W\n", rja_free_air);
    printf("  Tj (no heatsink) = %.1f C\n", tj_free_air);
    printf("  Assessment: %s\n\n", (tj_free_air > mosfet.max_tj) ? "FAILS - Heatsink Required" : "OK");

    /* --- Step 2: Estimate convection coefficient --- */
    printf("--- Step 2: Forced Convection Analysis ---\n");
    double h_forced;
    int ret = forced_convection_coefficient(2.0, 50.0, &amb, &h_forced);
    printf("  Forced convection h (2 m/s, 50mm plate) = %.1f W/m2-K\n", h_forced);

    /* Also show natural convection for comparison */
    double h_nat;
    ret = natural_convection_coefficient(80.0, ta_c, 50.0,
                CONV_CORR_NAT_VERT_PLATE, &amb, &h_nat);
    printf("  Natural convection h (80 C surface) = %.1f W/m2-K\n", h_nat);
    printf("  Forced/Natural ratio = %.1fx\n\n", h_forced / h_nat);

    /* --- Step 3: Heatsink Selection --- */
    printf("--- Step 3: Heatsink Selection ---\n");
    /* Required Rsa: Tj = Ta + P*(Rjc + Rcs + Rsa_required)
     * Target Tj = 120 C (conservative) for reliability
     * Rsa_needed = (120 - 55)/3 - 1.5 - 0.5 = 21.67 - 2.0 = 19.67 C/W */
    double target_tj = 120.0;
    double r_cs = 0.5;  /* Thermal grease TIM */
    double r_sa_needed = (target_tj - ta_c) / power_per_mosfet - mosfet.r_jc - r_cs;
    printf("  Target Tj = %.0f C (%.0f C margin from max)\n", target_tj, mosfet.max_tj - target_tj);
    printf("  Required Rsa = %.1f C/W\n", r_sa_needed);

    /* Select heatsink from catalog */
    heat_sink_model_t selected_heatsink;
    ret = heatsink_select_from_catalog(r_sa_needed, 40.0, 30.0, 40.0, 2.0, &selected_heatsink);
    if (ret == THERMAL_OK) {
        printf("  Selected: %.0f x %.0f mm, %.0f mm height, %d fins\n",
               selected_heatsink.base_width_mm, selected_heatsink.base_length_mm,
               selected_heatsink.fin_height_mm, selected_heatsink.num_fins);
        printf("  Rsa (forced, 2m/s) = %.1f C/W\n", selected_heatsink.r_sa_forced);

        double tj_final = ta_c + power_per_mosfet *
                          (mosfet.r_jc + r_cs + selected_heatsink.r_sa_forced);
        printf("  Final Tj = %.1f C\n", tj_final);
        printf("  Margin = %.1f C\n\n", mosfet.max_tj - tj_final);
    } else {
        printf("  No suitable heatsink found in catalog.\n\n");
    }

    /* --- Step 4: Thermal Runaway Check for Parallel MOSFETs --- */
    printf("--- Step 4: Parallel MOSFET Thermal Runaway Check ---\n");
    heat_source_t mosfets_parallel[2] = {
        {.power_w = 3.0, .max_tj = 175.0, .width_mm = 10.0, .length_mm = 15.0},
        {.power_w = 3.0, .max_tj = 175.0, .width_mm = 10.0, .length_mm = 15.0}
    };

    double r_thermal = mosfet.r_jc + r_cs + selected_heatsink.r_sa_forced;
    int runaway_ret = thermal_runaway_check(mosfets_parallel, 2, r_thermal, ta_c);
    printf("  Devices: 2 parallel MOSFETs, 3W each\n");
    printf("  R_thermal per device = %.1f C/W\n", r_thermal);
    printf("  Thermal runaway risk: %s\n",
           (runaway_ret == THERMAL_OK) ? "NONE (stable operation)" : "WARNING - Investigate");

    /* What if one MOSFET hogs current */
    mosfets_parallel[0].power_w = 4.5;  /* One MOSFET takes 75 percent */
    mosfets_parallel[1].power_w = 1.5;
    int runaway_ret2 = thermal_runaway_check(mosfets_parallel, 2, r_thermal, ta_c);
    printf("  With 75/25 percent current split: %s\n",
           (runaway_ret2 == THERMAL_OK) ? "Still stable" : "Runaway risk detected!");

    /* --- Step 5: Transient analysis --- */
    printf("\n--- Step 5: Transient Thermal Response ---\n");
    /* Time constants for the thermal path */
    double mass_mosfet_g = 2.0;   /* TO-220 mass ~2g */
    double cp_si = 712.0;         /* Silicon specific heat */
    double mass_heatsink_g = selected_heatsink.weight_g > 0 ?
                              selected_heatsink.weight_g : 50.0;
    double cp_al = 896.0;

    /* Lumped tau estimate */
    double tau_mosfet = thermal_time_constant(r_thermal, mass_mosfet_g, cp_si);
    double tau_heatsink = thermal_time_constant(r_thermal, mass_heatsink_g, cp_al);
    printf("  MOSFET thermal time constant: %.1f s\n", tau_mosfet);
    printf("  Heatsink thermal time constant: %.1f s\n", tau_heatsink);

    /* Temperature after 10s (motor startup surge) */
    double t_10s = transient_lumped_capacitance(ta_c, ta_c,
                     mass_mosfet_g + mass_heatsink_g * 0.2,  /* Partial heatsink thermal mass */
                     (cp_si + cp_al) / 2.0,
                     h_forced, selected_heatsink.surface_area_mm2 > 0 ?
                     selected_heatsink.surface_area_mm2 : 2000.0,
                     10.0);
    printf("  Estimated T after 10s surge: %.1f C\n", t_10s);

    /* --- Summary --- */
    printf("\n============================================================\n");
    printf(" DESIGN SUMMARY - MOSFET Motor Drive\n");
    printf("============================================================\n");
    printf("  Component:    2x TO-220 MOSFETs\n");
    printf("  Power:        %.1f W each (%.1f W total)\n",
           power_per_mosfet, power_per_mosfet * 2);
    printf("  Cooling:      Forced air heatsink (2 m/s)\n");
    printf("  Heatsink:     %.0fx%.0fx%.0f mm\n",
           selected_heatsink.base_width_mm, selected_heatsink.base_length_mm,
           selected_heatsink.fin_height_mm);
    printf("  Rsa:          %.1f C/W (forced)\n", selected_heatsink.r_sa_forced);
    printf("  Final Tj:     %.1f C\n",
           ta_c + power_per_mosfet * (mosfet.r_jc + r_cs + selected_heatsink.r_sa_forced));
    printf("  Thermal runaway: %s\n",
           (runaway_ret == THERMAL_OK) ? "Passed" : "Check");
    printf("  Design status: %s\n\n", (ret == THERMAL_OK) ? "ACCEPTED" : "REVIEW");

    return 0;
}
