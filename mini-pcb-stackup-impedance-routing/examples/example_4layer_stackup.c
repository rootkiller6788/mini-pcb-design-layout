#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pcb_stackup.h"
#include "pcb_material.h"

int main(void)
{
    printf("=== PCB 4-Layer Stackup Design Example ===\n\n");

    PcbStackup s = stackup_standard_4layer();
    stackup_build(&s);
    stackup_print(&s);

    double asym = stackup_symmetry_check(&s);
    printf("Symmetry: %.2f%% asymmetry\n\n", asym);

    LayerImpedanceTarget targets[4];
    double heights[4];
    targets[0].signal_layer_index = 1;
    targets[0].target_z0_ohm = 50.0;
    targets[0].trace_width_um = 350.0;
    stackup_impedance_feasibility(&s, targets, 1, heights);
    printf("For 50 Ohm on TOP layer: required height ~ %.0f um\n", heights[0]);
    printf("  (Actual prepreg = 200 um)\n");

    double C_plane = stackup_plane_capacitance(&s, 5000.0);
    printf("\nPlane capacitance (50x100mm): %.2f nF\n", C_plane);

    double I_trace = stackup_trace_current_capacity(0.35, 35.0, 10.0, 0);
    printf("Trace current capacity (0.35mm, 1oz, +10C): %.2f A\n", I_trace);

    printf("\n=== Done ===\n");
    return 0;
}
