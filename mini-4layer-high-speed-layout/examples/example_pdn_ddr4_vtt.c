/* example_pdn_ddr4_vtt.c - PDN design for DDR4 VDD/VTT power rails
 *
 * Demonstrates complete PDN analysis for DDR4-3200 1.2V core rail
 * and 0.6V VTT termination rail on a 4-layer PCB. Covers L6 canonical
 * problem: multi-rail PDN design with decap selection optimization.
 */
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "hs_pdn.h"
#include "hs_stackup.h"

static void print_pdn_result(const hs_pdn_result_t *r) {
    printf("  Target impedance: %.2f mOhm\n", r->target_impedance_ohm * 1e3);
    printf("  DC IR drop: %.2f mV (%.2f%%)\n", 
           r->dc_voltage_drop_v * 1e3, r->dc_voltage_drop_percent);
    printf("  Plane capacitance: %.2f nF\n", r->plane_capacitance_nf);
    printf("  First plane resonance: %.1f MHz\n", 
           r->first_plane_resonance_hz * 1e-6);
    printf("  Max PDN impedance: %.2f mOhm at %.1f MHz\n",
           r->max_impedance_ohm * 1e3, r->max_impedance_freq_hz * 1e-6);
    printf("  Decaps required: %d\n", r->num_decaps_required);
    printf("  Compliant with Z_target: %s\n", r->is_compliant ? "YES" : "NO");
    printf("  Est. SSN amplitude: %.1f mV\n", r->ssn_voltage_mv);
}

int main(void) {
    printf("=== DDR4-3200 PDN Design for 4-Layer PCB ===\n\n");
    
    /* === Rail 1: VDD (1.2V core) === */
    printf("--- Rail 1: VDD (1.2V Core) ---\n\n");
    
    hs_pdn_network_t vdd_network;
    memset(&vdd_network, 0, sizeof(vdd_network));
    
    /* VRM for 1.2V core */
    hs_vrm_init_typical(&vdd_network.vrm, 1.2, 10.0);
    printf("VRM: %.1fV, %.0fA, BW=%.0fkHz, Z_out=%.1fmOhm\n",
           vdd_network.vrm.output_voltage_v,
           vdd_network.vrm.max_current_a,
           vdd_network.vrm.bandwidth_hz * 1e-3,
           vdd_network.vrm.output_impedance_ohm * 1e3);
    
    /* Plane pair: 80x60 mm, FR-4 standard, 0.25 mm core separation */
    vdd_network.plane.plane_width_m = 0.08;
    vdd_network.plane.plane_height_m = 0.06;
    vdd_network.plane.separation_m = 0.25e-3;
    vdd_network.plane.dielectric_er = 4.2;
    vdd_network.plane.copper_thickness_m = 35e-6;
    
    double c_plane = hs_plane_pair_capacitance(&vdd_network.plane);
    printf("Plane pair: %.0fx%.0fmm, C_plane=%.2f nF\n",
           vdd_network.plane.plane_width_m * 1e3,
           vdd_network.plane.plane_height_m * 1e3,
           c_plane * 1e9);
    
    /* Target impedance: 1.2V, 5% ripple, 5A transient, 50% margin */
    double z_target = hs_pdn_target_impedance(1.2, 0.05, 5.0, 1.5);
    printf("Z_target (5%% ripple, 5A step, 50%% margin): %.2f mOhm\n\n",
           z_target * 1e3);
    
    /* Decap selection for VDD rail */
    hs_decap_t vdd_bulk[6], vdd_ceramic[12];
    vdd_network.bulk_caps = vdd_bulk;
    vdd_network.ceramic_caps = vdd_ceramic;
    
    int n_bulk, n_ceramic;
    int rc = hs_decap_select(&vdd_network, 6, 12, z_target, 1e9,
                              &n_bulk, &n_ceramic);
    
    printf("Decap selection result: %s\n", rc == 0 ? "COMPLIANT" : 
           (rc == 1 ? "PARTIALLY COMPLIANT" : "INFEASIBLE"));
    printf("Bulk caps selected: %d\n", n_bulk);
    printf("Ceramic caps selected: %d\n", n_ceramic);
    
    /* Print selected capacitor details */
    for (int i = 0; i < n_bulk; i++) {
        hs_decap_t *d = &vdd_bulk[i];
        double srf = hs_decap_srf(d);
        printf("  Bulk[%d]: %.1fuF %s, ESR=%.0fmOhm, ESL=%.1fnH, SRF=%.1fMHz\n",
               i, d->capacitance_f * 1e6, d->package_code,
               d->esr_ohm * 1e3, d->esl_h * 1e9, srf * 1e-6);
    }
    for (int i = 0; i < n_ceramic; i++) {
        hs_decap_t *d = &vdd_ceramic[i];
        double srf = hs_decap_srf(d);
        printf("  Ceramic[%d]: %.0fnF %s, ESR=%.0fmOhm, ESL=%.1fnH, SRF=%.1fMHz\n",
               i, d->capacitance_f * 1e9, d->package_code,
               d->esr_ohm * 1e3, d->esl_h * 1e9, srf * 1e-6);
    }
    
    /* Frequency sweep analysis */
    hs_pdn_impedance_point_t vdd_profile[80];
    hs_pdn_result_t vdd_result;
    hs_pdn_analyze(&vdd_network, 1e3, 1e9, 80, vdd_profile, &vdd_result);
    
    printf("\nPDN Analysis Results:\n");
    print_pdn_result(&vdd_result);
    
    /* Print key impedance data points */
    printf("\nImpedance Profile (key points):\n");
    printf("%-12s %-16s\n", "Frequency", "Impedance(mOhm)");
    printf("--------------------------------\n");
    int points[] = {0, 10, 20, 40, 60, 79};
    for (int i = 0; i < 6; i++) {
        int idx = points[i];
        printf("%-12s %-16.2f\n", 
               (vdd_profile[idx].frequency_hz >= 1e6) ? 
                 (vdd_profile[idx].frequency_hz >= 1e9 ?
                  "X GHz" : "XXX MHz") : "XXX kHz",
               vdd_profile[idx].impedance_ohm * 1e3);
    }
    /* Print actual values */
    for (int i = 0; i < 6; i++) {
        int idx = points[i];
        double f = vdd_profile[idx].frequency_hz;
        const char *unit;
        if (f >= 1e9) { f /= 1e9; unit = "GHz"; }
        else if (f >= 1e6) { f /= 1e6; unit = "MHz"; }
        else { f /= 1e3; unit = "kHz"; }
        printf("  %7.2f %-4s: %8.2f mOhm\n", f, unit,
               vdd_profile[idx].impedance_ohm * 1e3);
    }
    
    /* SSN estimation for DDR4 data bus */
    printf("\n=== SSN Analysis for DDR4 x32 Data Bus ===\n");
    double v_ssn_core = hs_ssn_estimate(32, 0.020, 0.25e-9, 0.8e-9);
    printf("Core SSN (32 drivers, 20mA, 250ps edge, 0.8nH): %.1f mV\n",
           v_ssn_core * 1e3);
    
    /* SSN with lower inductance (better decoupling) */
    double v_ssn_opt = hs_ssn_estimate(32, 0.020, 0.25e-9, 0.3e-9);
    printf("Core SSN (optimized, 0.3nH): %.1f mV (%.0f%% reduction)\n",
           v_ssn_opt * 1e3,
           (1.0 - v_ssn_opt / v_ssn_core) * 100.0);
    
    /* Check plane resonance avoidance */
    double f_res = hs_plane_resonance_frequency(&vdd_network.plane);
    printf("\nPlane cavity resonance: %.1f MHz\n", f_res * 1e-6);
    printf("DDR4-3200 fundamental: 1.6 GHz\n");
    if (f_res < 1.6e9 && f_res > 0.5e9) {
        printf("WARNING: Plane resonance near DDR4 fundamental - add decaps!\n");
    } else {
        printf("Plane resonance is outside critical band.\n");
    }
    
    /* === Rail 2: VTT (0.6V termination) === */
    printf("\n\n--- Rail 2: VTT (0.6V Termination) ---\n\n");
    
    hs_pdn_network_t vtt_network;
    memset(&vtt_network, 0, sizeof(vtt_network));
    
    hs_vrm_init_typical(&vtt_network.vrm, 0.6, 5.0);
    vtt_network.plane = vdd_network.plane; /* Same plane pair */
    
    /* VTT is less demanding: lower current, higher Z_target acceptable */
    double z_target_vtt = hs_pdn_target_impedance(0.6, 0.05, 2.0, 1.0);
    printf("VTT Z_target (5%% ripple, 2A step): %.1f mOhm\n\n",
           z_target_vtt * 1e3);
    
    hs_decap_t vtt_bulk[4], vtt_ceramic[8];
    vtt_network.bulk_caps = vtt_bulk;
    vtt_network.ceramic_caps = vtt_ceramic;
    
    int nb_vtt, nc_vtt;
    rc = hs_decap_select(&vtt_network, 4, 8, z_target_vtt, 1e9,
                          &nb_vtt, &nc_vtt);
    
    printf("VTT decap selection: %s\n", rc == 0 ? "COMPLIANT" : "PARTIAL");
    printf("Bulk: %d, Ceramic: %d\n", nb_vtt, nc_vtt);
    
    printf("\n=== DDR4 PDN Design Complete ===\n");
    printf("Summary: VDD rail uses %d bulk + %d ceramic decaps\n", n_bulk, n_ceramic);
    printf("         VTT rail uses %d bulk + %d ceramic decaps\n", nb_vtt, nc_vtt);
    
    return 0;
}
