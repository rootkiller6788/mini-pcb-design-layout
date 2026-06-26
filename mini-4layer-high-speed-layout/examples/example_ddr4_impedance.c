/* example_ddr4_impedance.c - DDR4 SODIMM 4-layer stackup impedance design
 * 
 * Demonstrates designing a complete 4-layer PCB stackup for DDR4-3200 memory,
 * including 50 Ohm single-ended and 100 Ohm differential impedance targets.
 * Covers L6 canonical problem: impedance-controlled PCB design for DDR4.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "hs_stackup.h"
#include "hs_impedance.h"

int main(void) {
    printf("=== DDR4-3200 4-Layer PCB Impedance Design ===\n\n");
    
    /* Step 1: Define the 4-layer stackup */
    hs_stackup_t stackup;
    hs_stackup_init_default(&stackup);
    
    /* Use Megtron 6 for better high-frequency performance at 1.6 GHz */
    hs_stackup_init_config(&stackup, HS_CONFIG_SIG_GND_PWR_SIG, 62.0, HS_MAT_MEGTRON_6);
    hs_stackup_print(&stackup, 'i');
    
    const hs_material_props_t *mat = hs_material_get_props(HS_MAT_MEGTRON_6);
    printf("\nMaterial: %s, er=%.2f, tanD=%.4f\n\n", mat->name, mat->er, mat->tan_d);
    
    /* Step 2: Solve for 50 Ohm single-ended microstrip on top layer */
    hs_impedance_geometry_t geo_se;
    memset(&geo_se, 0, sizeof(geo_se));
    geo_se.dielectric_height = stackup.prepreg_thickness[0];
    geo_se.dielectric_constant = mat->er;
    geo_se.trace_thickness = stackup.layers[0].copper_thickness;
    
    hs_impedance_result_t result_se;
    int rc = hs_solve_trace_width(50.0, &geo_se, &result_se);
    if (rc == 0) {
        printf("50 Ohm Single-Ended Microstrip (Top Layer):\n");
        printf("  Trace width: %.3f mm (%.1f mils)\n", 
               geo_se.trace_width * 1e3, geo_se.trace_width / 2.54e-5);
        printf("  Z0: %.1f Ohm\n", result_se.z0_single);
        printf("  Effective er: %.2f\n", result_se.epsilon_eff);
        printf("  Delay: %.2f ps/mm\n", result_se.delay_ps_per_mm);
        printf("  Capacitance: %.1f pF/m\n", result_se.capacitance_pf_per_m);
    }
    
    /* Step 3: Solve for 100 Ohm differential pair */
    hs_impedance_geometry_t geo_diff = geo_se;
    geo_diff.spacing = 0.15e-3; /* 0.15 mm edge-to-edge initial guess */
    
    hs_impedance_result_t result_diff;
    rc = hs_diff_microstrip_impedance(&geo_diff, &result_diff);
    
    /* Iterate spacing to achieve 100 Ohm target */
    double tol = 0.01;
    for (int iter = 0; iter < 20; iter++) {
        rc = hs_diff_microstrip_impedance(&geo_diff, &result_diff);
        if (rc != 0) break;
        double err = result_diff.z0_diff - 100.0;
        if (fabs(err) < tol) break;
        geo_diff.spacing *= (1.0 + 0.05 * (err > 0 ? 1.0 : -1.0));
        if (geo_diff.spacing < 0.05e-3) geo_diff.spacing = 0.05e-3;
    }
    
    printf("\n100 Ohm Differential Pair (Top Layer):\n");
    printf("  Trace width: %.3f mm\n", geo_diff.trace_width * 1e3);
    printf("  Spacing (edge-to-edge): %.3f mm (%.1f mils)\n",
           geo_diff.spacing * 1e3, geo_diff.spacing / 2.54e-5);
    printf("  Z_diff: %.1f Ohm\n", result_diff.z0_diff);
    printf("  Z_odd: %.1f Ohm\n", result_diff.z0_odd);
    printf("  Z_even: %.1f Ohm\n", result_diff.z0_even);
    printf("  Z_common: %.1f Ohm\n", result_diff.z0_common);
    
    /* Step 4: Compute skin depth at DDR4-3200 fundamental (1.6 GHz) */
    double skin = hs_skin_depth(1.6e9, 1.72e-8, 1.0);
    printf("\nSkin Effect at 1.6 GHz:\n");
    printf("  Skin depth: %.2f um\n", skin * 1e6);
    printf("  Trace thickness: %.0f um\n", geo_se.trace_thickness * 1e6);
    printf("  Current flows in skin depth -> AC resistance increase\n");
    
    /* Step 5: Bandwidth estimation */
    double bw = hs_bandwidth_from_rise_time(50e-12, 0);
    printf("\nDDR4-3200 Signal Bandwidth:\n");
    printf("  Rise time: 50 ps (typical)\n");
    printf("  Bandwidth (Gaussian): %.1f GHz\n", bw * 1e-9);
    double lambda = hs_wavelength(bw, result_se.epsilon_eff);
    printf("  Wavelength at BW: %.1f mm\n", lambda * 1e3);
    printf("  lambda/10: %.1f mm (critical for TL treatment)\n", lambda * 1e2);
    
    printf("\n=== DDR4-3200 Impedance Design Complete ===\n");
    return 0;
}
