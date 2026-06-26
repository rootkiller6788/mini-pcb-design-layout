#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pcb_impedance.h"
#include "pcb_transmission_line.h"
#include "pcb_material.h"

int main(void)
{
    printf("=== 50 Ohm Microstrip Design Example ===\n\n");

    MicrostripGeometry geo;
    memset(&geo, 0, sizeof(geo));
    geo.trace.dielectric_height_m = 200e-6;
    geo.trace.dielectric_er = 4.2;
    geo.trace.loss_tangent = 0.02;
    geo.trace.trace_thickness_m = 35e-6;
    geo.trace.conductor_sigma = 5.8e7;

    double w_50 = impedance_microstrip_w_for_z0(50.0, 200.0, 4.2, 35.0, NULL);
    printf("For 50 Ohm on 0.2mm FR-4 prepreg:\n");
    printf("  Required trace width: %.0f um (%.1f mil)\n\n", w_50, w_50/25.4);

    geo.trace.trace_width_m = w_50 * 1e-6;

    printf("Formula comparison for w=%.0f um, h=200 um, er=4.2:\n", w_50);
    ImpedanceResult r_ipc = impedance_microstrip_ipc(&geo);
    ImpedanceResult r_hj  = impedance_microstrip_hammerstad(&geo);
    ImpedanceResult r_whe = impedance_microstrip_wheeler(&geo);
    printf("  IPC-2141A:        Z0 = %.2f ohm\n", r_ipc.z0_ohm);
    printf("  Hammerstad-Jensen: Z0 = %.2f ohm\n", r_hj.z0_ohm);
    printf("  Wheeler:           Z0 = %.2f ohm\n", r_whe.z0_ohm);

    printf("\nSensitivity analysis (+/-10%% on each parameter):\n");
    ImpedanceSensitivity sens = impedance_sensitivity(&geo, 20.0, 10.0, 0.2, 3.5);
    printf("  dZ/dw = %.2f ohm/um\n", sens.dZ_dw);
    printf("  dZ/dh = %.2f ohm/um\n", sens.dZ_dh);
    printf("  dZ/d_er = %.2f ohm\n", sens.dZ_der);
    printf("  dZ/dt = %.2f ohm/um\n", sens.dZ_dt);
    printf("  Worst-case delta Z = %.2f ohm\n", sens.worst_case_delta_Z);

    printf("\nDifferential pair (100 Ohm target):\n");
    DiffImpedanceResult d100 = impedance_standard_100ohm_diff_fr4(200.0, 1.0, w_50);
    printf("  Z_diff = %.1f ohm, Z_odd = %.1f ohm, k = %.3f\n",
           d100.z_diff_ohm, d100.z_odd_ohm, d100.coupling_coeff);

    printf("\n=== Done ===\n");
    return 0;
}
