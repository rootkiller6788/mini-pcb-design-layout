#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pcb_routing.h"
#include "pcb_transmission_line.h"
#include "pcb_via.h"
#include "pcb_signal_integrity.h"

int main(void)
{
    printf("=== DDR5 Routing & SI Analysis Example ===\n\n");

    /* Setup transmission line for DDR5: typical 40 Ohm single-ended */
    TraceGeometry geo;
    memset(&geo, 0, sizeof(geo));
    geo.trace_width_m = 200e-6;
    geo.trace_thickness_m = 35e-6;
    geo.dielectric_height_m = 100e-6;
    geo.dielectric_er = 4.2;
    geo.loss_tangent = 0.02;
    geo.conductor_sigma = 5.8e7;

    TransmissionLine tl = tl_from_geometry(&geo, 3.2e9);
    printf("Transmission line at 3.2 GHz:\n");
    printf("  Z0 = %.1f ohm, vp = %.2e m/s, tpd = %.2f ps/mm\n\n",
           tl.z0_real, tl.vp, tl.delay_per_m * 1e12 * 1e3);

    /* DDR5 fly-by bus check */
    RouteSegment segs[4];
    for (int i = 0; i < 4; i++) {
        segs[i].length_mm = 25.0 + i * 12.0;
        segs[i].width_um = 200.0;
        segs[i].layer = 3;
    }

    Ddr4FlyByBus bus = routing_ddr4_flyby_check(3200.0, segs, 4, &tl);
    printf("DDR5 Fly-By Bus (3200 MT/s):\n");
    for (int i = 0; i < bus.num_ranks; i++) {
        printf("  Rank %d: %.0f mm, skew = %.1f ps\n",
               i, bus.trace_lengths_mm[i], bus.skew_per_rank_ps[i]);
    }
    printf("  Timing OK: %s\n\n", bus.timing_ok ? "YES" : "NO");

    /* Via design for DDR5 */
    ViaDimensions v_ddr5 = via_ddr5_design(1.6);
    double L_via = via_inductance(&v_ddr5);
    double C_via = via_capacitance(&v_ddr5, 4.2, 0.035);
    printf("DDR5 Via (0.25mm drill, 1.6mm board):\n");
    printf("  L = %.3f nH, C = %.3f pF\n\n", L_via, C_via);

    /* PRBS15 generation for SI testing */
    int bits[128];
    si_prbs_generate(PRBS15, bits, 128);
    printf("PRBS15 pattern (first 16 bits): ");
    for (int i = 0; i < 16; i++) printf("%d", bits[i]);
    printf("\n");

    /* Eye diagram from sample waveform */
    Waveform wf;
    wf.num_points = 100;
    WaveformPoint pts[100];
    for (int i = 0; i < 100; i++) {
        pts[i].time_ps = i * 20.0;
        pts[i].voltage_v = (i % 10 < 5) ? 0.8 : 0.2;
    }
    wf.points = pts;
    EyeDiagram eye = si_eye_diagram(&wf, 200.0);
    printf("\nEye Diagram (ideal NRZ, 5 Gbps):\n");
    printf("  Eye height = %.1f mV\n", eye.eye_height_mv);
    printf("  Q-factor = %.1f, BER ~ %.2e\n",
           eye.q_factor, si_ber_from_qfactor(eye.q_factor));

    printf("\n=== Done ===\n");
    return 0;
}
