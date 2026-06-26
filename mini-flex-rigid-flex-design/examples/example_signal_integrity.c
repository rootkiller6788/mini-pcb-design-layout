/**
 * @file example_signal_integrity.c
 * @brief End-to-end example: Impedance control and loss budget for a
 *        USB 3.2 Gen 2 (10 Gbps) signal on LCP flex.
 *
 * Designs and analyzes a 90Ω differential pair on LCP flex substrate,
 * computing impedance, loss budget, and maximum usable trace length.
 *
 * L6 Canonical Problem: High-speed signal integrity on flex.
 * L7 Application: USB 3.2 / Thunderbolt over flex interconnects.
 */

#include "flex_signal_integrity.h"
#include "flex_material.h"
#include <stdio.h>
#include <math.h>

int main(void) {
    printf("=== Signal Integrity: USB 3.2 10 Gbps on LCP Flex ===\n\n");

    /* Target: 90Ω differential microstrip on LCP */
    flex_tl_params_t params;
    params.tl_type                  = FLEX_TL_DIFF_MICROSTRIP;
    params.trace_width_um           = 80.0;    /* Narrow trace for flex */
    params.trace_thickness_um       = 18.0;    /* 0.5 oz Cu */
    params.dielectric_thickness_um  = 50.0;    /* Thin LCP substrate */
    params.dielectric_constant      = 2.9;     /* LCP εr */
    params.loss_tangent             = 0.002;   /* LCP low loss */
    params.trace_spacing_um         = 120.0;   /* Diff pair spacing */
    params.coverlay_thickness_um    = 25.0;    /* PI coverlay */
    params.coverlay_dk             = 3.4;     /* Coverlay εr */
    params.solder_mask_thickness_um = 0.0;
    params.solder_mask_dk          = 0.0;
    params.upper_dielectric_um      = 50.0;
    params.lower_dielectric_um      = 50.0;
    params.copper_roughness_um      = 0.15;    /* RA low-profile Cu */
    params.frequency_hz             = 5.0e9;   /* 5 GHz (Nyquist for 10 Gbps) */
    params.has_coverlay             = 1;
    params.has_solder_mask          = 0;

    flex_tl_result_t result;
    if (flex_tl_analyze(&params, &result) != 0) {
        printf("ERROR: Analysis failed.\n");
        return 1;
    }

    printf("  Transmission Line Parameters:\n");
    printf("  -----------------------------\n");
    printf("  Trace width:      %.0f μm\n", params.trace_width_um);
    printf("  Trace spacing:    %.0f μm\n", params.trace_spacing_um);
    printf("  Substrate:        LCP (εr=%.1f, tanδ=%.3f)\n",
           params.dielectric_constant, params.loss_tangent);
    printf("\n  Differential Z0:  %.1f Ω (target: 90 Ω)\n",
           result.differential_impedance);
    printf("  Single-ended Z0:  %.1f Ω\n",
           result.characteristic_impedance);
    printf("  Effective DK:     %.3f\n", result.effective_dk);
    printf("\n  --- Loss Budget at 5 GHz ---\n");
    printf("  Conductor loss:   %.4f dB/mm\n",
           result.conductor_loss_db_per_mm);
    printf("  Dielectric loss:  %.4f dB/mm\n",
           result.dielectric_loss_db_per_mm);
    printf("  Total loss:       %.4f dB/mm\n",
           result.total_loss_db_per_mm);
    printf("  Roughness factor: %.3f\n",
           result.copper_roughness_factor);
    printf("\n  --- Timing ---\n");
    printf("  Propagation delay: %.2f ps/mm\n",
           result.propagation_delay_ps_per_mm);
    printf("  Wavelength @ 5 GHz: %.1f mm\n",
           result.wavelength_mm);
    printf("  Skin depth:         %.2f μm\n",
           result.skin_depth_um);

    /* Maximum trace length for 3 dB loss budget */
    double max_length_3db = 3.0 / result.total_loss_db_per_mm;
    printf("\n  === Maximum trace lengths ===\n");
    printf("  For 3 dB loss:     %.0f mm\n", max_length_3db);
    printf("  For 6 dB loss:     %.0f mm\n",
           6.0 / result.total_loss_db_per_mm);

    /* Critical length for 35 ps rise time (typical 10 Gbps) */
    double l_crit = flex_critical_length_mm(35.0,
        result.propagation_delay_ps_per_mm);
    printf("\n  Critical length (tr=35 ps): %.0f mm\n", l_crit);

    printf("\n=== Verdict ===\n");
    if (result.differential_impedance >= 80.0 &&
        result.differential_impedance <= 100.0) {
        printf("Impedance within USB 3.2 spec (76.5-103.5 Ω).\n");
    } else {
        printf("WARNING: Impedance outside USB 3.2 target.\n");
    }
    printf("Max usable length at 3 dB loss: %.0f mm.\n", max_length_3db);

    return 0;
}
