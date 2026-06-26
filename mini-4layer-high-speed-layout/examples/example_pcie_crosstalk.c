/* example_pcie_crosstalk.c - PCIe Gen4 16 GT/s crosstalk budget analysis
 *
 * Demonstrates crosstalk analysis for a PCIe Gen4 16 GT/s link on 4-layer PCB,
 * including NEXT/FEXT computation, power-sum crosstalk budget, and
 * guard trace optimization. Covers L6 canonical problem.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "hs_crosstalk.h"
#include "hs_stackup.h"
#include "hs_impedance.h"
#include "hs_transmission.h"

int main(void) {
    printf("=== PCIe Gen4 16 GT/s Crosstalk Budget Analysis ===\n\n");
    
    /* PCIe Gen4 parameters */
    double bit_rate = 16e9;       /* 16 GT/s */
    double ui_ps = 62.5;         /* Unit interval = 62.5 ps */
    double rise_time = 25e-12;   /* 25 ps typical for PCIe Gen4 */
    double amplitude_v = 0.8;    /* 800 mVpp differential */
    
    /* PCB stackup: 4-layer, Megtron 6, 62 mil total */
    hs_stackup_t stackup;
    hs_stackup_init_default(&stackup);
    
    const hs_material_props_t *mat = hs_material_get_props(HS_MAT_MEGTRON_6);
    
    /* Solve for 42.5 Ohm single-ended (85 Ohm differential for PCIe) */
    hs_impedance_geometry_t geo;
    memset(&geo, 0, sizeof(geo));
    geo.dielectric_height = stackup.prepreg_thickness[0];
    geo.dielectric_constant = mat->er;
    geo.trace_thickness = stackup.layers[0].copper_thickness;
    
    hs_impedance_result_t imp_result;
    hs_solve_trace_width(42.5, &geo, &imp_result);
    
    double trace_w = geo.trace_width;
    double er_eff = imp_result.epsilon_eff;
    double z0 = imp_result.z0_single;
    
    printf("Stackup: 4L M6, Trace: %.2f mils, Z0=%.1f Ohm, Er_eff=%.2f\n\n",
           trace_w / 2.54e-5, z0, er_eff);
    
    /* Analyze crosstalk for multiple spacing values */
    printf("=== Single Aggressor NEXT/FEXT vs Spacing ===\n");
    printf("%-12s %-12s %-12s %-12s %-12s\n",
           "Spacing(mil)", "NEXT(dB)", "FEXT(dB)", "K_b", "K_f");
    printf("-------------------------------------------------------------\n");
    
    double spacings[] = {5.0, 8.0, 12.0, 16.0, 20.0, 30.0};
    double next_vals[6];
    
    for (int i = 0; i < 6; i++) {
        double s_mils = spacings[i];
        double s_m = s_mils * 2.54e-5;
        
        hs_coupled_pair_t pair;
        memset(&pair, 0, sizeof(pair));
        pair.trace_width_m = trace_w;
        pair.trace_thickness_m = geo.trace_thickness;
        pair.edge_spacing_m = s_m;
        pair.height_to_plane_m = geo.dielectric_height;
        pair.er_eff = er_eff;
        pair.z0_ohm = z0;
        pair.coupling_length_m = 0.10; /* 10 cm parallel run */
        pair.aggressor_rise_time_s = rise_time;
        pair.aggressor_amplitude_v = amplitude_v;
        
        hs_coupling_params_t cp;
        hs_coupling_analyze(&pair, &cp);
        
        hs_crosstalk_result_t result;
        hs_crosstalk_analyze_pair(&pair, &result);
        
        printf("%-12.0f %-12.1f %-12.1f %-12.4f %-12.4f\n",
               s_mils, result.next_db, result.fext_db,
               cp.backward_crosstalk_coeff, cp.forward_crosstalk_coeff);
        
        next_vals[i] = result.next_db;
    }
    
    /* Power-sum crosstalk with 4 aggressors (typical PCIe x4) */
    printf("\n=== Multi-Aggressor Crosstalk Budget (PCIe x4) ===\n");
    
    /* Simulate 4 aggressors at progressively closer spacing */
    double next_4agg[] = {-32.0, -35.0, -38.0, -40.0};
    double ps_coherent = hs_power_sum_crosstalk_db(next_4agg, 4, 1);
    double ps_rms = hs_power_sum_crosstalk_db(next_4agg, 4, 0);
    
    printf("4 Aggressors: NEXT(dB) = {%.1f, %.1f, %.1f, %.1f}\n",
           next_4agg[0], next_4agg[1], next_4agg[2], next_4agg[3]);
    printf("Power-sum (coherent worst-case): %.1f dB\n", ps_coherent);
    printf("Power-sum (RMS random-phase):    %.1f dB\n", ps_rms);
    
    /* PCIe Gen4 specification: NEXT must be < -32 dB per lane pair */
    double pcie_spec = -32.0;
    printf("\nPCIe Gen4 NEXT spec: < %.0f dB\n", pcie_spec);
    printf("Coherent PS NEXT: %.1f dB -> %s\n",
           ps_coherent, (ps_coherent <= pcie_spec) ? "PASS" : "FAIL");
    
    /* Guard trace analysis */
    printf("\n=== Guard Trace Effectiveness ===\n");
    double guard_w = trace_w;
    double guard_spacing = 0.3e-3; /* 12 mils guard to victim */
    double orig_spacing = 0.5e-3;  /* 20 mils agg to victim */
    
    double reduction = hs_guard_trace_reduction_db(guard_w, guard_spacing, orig_spacing);
    printf("Guard trace width: %.2f mils\n", guard_w / 2.54e-5);
    printf("Expected NEXT reduction: %.1f dB\n", reduction);
    
    /* Differential crosstalk immunity */
    double cmrr = 30.0; /* typical PCIe receiver CMRR */
    double se_next = -30.0;
    double diff_next = hs_diff_crosstalk_immunity(se_next, cmrr);
    printf("\n=== Differential Immunity ===\n");
    printf("Single-ended NEXT: %.1f dB\n", se_next);
    printf("Receiver CMRR: %.1f dB\n", cmrr);
    printf("Differential NEXT: %.1f dB (%.0f dB improvement)\n",
           diff_next, se_next - diff_next);
    
    /* Critical length check */
    double t_pd = imp_result.delay_ps_per_mm * 1e-12 / 1e-3;
    double crit_len = hs_critical_length(rise_time, t_pd);
    printf("\nCritical length at %.0f ps rise: %.1f mm (%.1f inches)\n",
           rise_time * 1e12, crit_len * 1e3, crit_len / 0.0254);
    printf("Routes longer than %.1f mm must be treated as transmission lines.\n",
           crit_len * 1e3);
    
    printf("\n=== PCIe Gen4 Crosstalk Analysis Complete ===\n");
    return 0;
}
