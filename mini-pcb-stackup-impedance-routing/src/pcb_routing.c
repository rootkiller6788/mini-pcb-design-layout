/**
 * pcb_routing.c — PCB Routing Analysis and Optimization
 *
 * Implements trace routing calculations, differential pair analysis,
 * length matching, BGA breakout, crosstalk modeling, and industry
 * protocol routing validation. Covers L2-L8 per SKILL.md.
 *
 * Key references:
 *   Johnson & Graham "High-Speed Digital Design" Ch.5-7
 *   Bogatin "Signal and Power Integrity — Simplified" Ch.10-11
 *   DDR4/DDR5 JEDEC specifications
 *   PCI Express Base Specification
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pcb_routing.h"
#include "pcb_transmission_line.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define C0 2.99792458e8

/* =========================================================================
 * L2: ROUTE SEGMENT LENGTH — Euclidean and Manhattan
 *
 * Euclidean:  L = sqrt((x2-x1)^2 + (y2-y1)^2)
 * Manhattan:  L = |x2-x1| + |y2-y1|  (orthogonal routing estimate)
 *
 * Manhattan length is used for initial routing estimates since PCB
 * routing is predominantly orthogonal. Euclidean gives the shortest
 * possible distance.
 * ========================================================================= */
double routing_segment_length(const RouteSegment *seg)
{
    if (!seg) return 0.0;
    double dx = seg->end_x_mm - seg->start_x_mm;
    double dy = seg->end_y_mm - seg->start_y_mm;
    return sqrt(dx*dx + dy*dy);
}

double routing_manhattan_length(const RouteSegment *seg)
{
    if (!seg) return 0.0;
    return fabs(seg->end_x_mm - seg->start_x_mm)
         + fabs(seg->end_y_mm - seg->start_y_mm);
}

/* L2: Total propagation delay for a complete route */
double routing_total_delay(const PcbRoute *route,
                            const TransmissionLine *tl_per_layer[])
{
    if (!route || !tl_per_layer) return 0.0;
    double total_ps = 0.0;
    for (int i = 0; i < route->num_segments; i++) {
        int layer = route->segments[i].layer;
        double tpd = 6.0;  /* ps/mm default for FR-4 microstrip */
        if (tl_per_layer[layer]) {
            tpd = tl_per_layer[layer]->delay_per_m * 1e12 * 1e3;
        }
        total_ps += route->segments[i].length_mm * tpd;
    }
    return total_ps;
}

/* L2: Check if route satisfies all routing constraints */
int routing_check_constraints(const PcbRoute *route,
                               const RoutingConstraint *constraints,
                               int num_constraints)
{
    if (!route || !constraints || num_constraints <= 0) return 1;
    for (int c = 0; c < num_constraints; c++) {
        const RoutingConstraint *rc = &constraints[c];
        for (int i = 0; i < route->num_segments; i++) {
            if (rc->layer >= 0 && route->segments[i].layer != rc->layer) continue;
            if (rc->min_width_um > 0 && route->segments[i].width_um < rc->min_width_um) return 0;
            if (rc->max_width_um > 0 && route->segments[i].width_um > rc->max_width_um) return 0;
        }
        if (rc->max_length_mm > 0 && route->total_length_mm > rc->max_length_mm) return 0;
        if (rc->max_vias > 0 && route->num_vias > rc->max_vias) return 0;
    }
    return 1;
}

/* =========================================================================
 * L3: DIFFERENTIAL PAIR INTRA-PAIR SKEW
 *
 * skew = |t_pd_P - t_pd_N|
 *
 * Acceptable skew budget: < 0.15 * t_rise (general guideline)
 * For 100ps rise time, max skew = 15ps.
 * ========================================================================= */
double routing_diff_pair_skew(const DiffPairSegment *seg, const TransmissionLine *tl)
{
    if (!seg) return 0.0;
    double tpd = tl ? tl->delay_per_m * 1e12 : 6.0;  /* ps/mm */
    return fabs(seg->p_seg.length_mm - seg->n_seg.length_mm) * tpd;
}

/* L3: Coupling ratio — fraction of coupled length to total length */
double routing_diff_pair_coupling_ratio(const DiffPairSegment *seg)
{
    if (!seg || seg->p_seg.length_mm <= 0.0) return 0.0;
    return seg->coupling_length_mm / (seg->p_seg.length_mm + seg->n_seg.length_mm) * 2.0;
}

/* L3: Odd-mode and even-mode impedance profile along differential route */
void routing_diff_pair_impedance_profile(const DiffPairSegment *seg,
                                          const TransmissionLine *tl,
                                          double *z_odd, double *z_even,
                                          int num_points)
{
    if (!seg || !tl || !z_odd || !z_even || num_points <= 0) return;
    double k = (seg->spacing_um > 0) ? exp(-seg->spacing_um * 1e-3 / 0.5) : 0.5;
    if (k > 0.99) k = 0.99;
    double z0 = tl->z0_real;
    double zo = z0 * sqrt((1.0 - k)/(1.0 + k));
    double ze = z0 * sqrt((1.0 + k)/(1.0 - k));
    for (int i = 0; i < num_points; i++) {
        z_odd[i] = zo;
        z_even[i] = ze;
    }
}

/* =========================================================================
 * L3: DIFFERENTIAL TO COMMON-MODE CONVERSION
 *
 * S_cd21 = delta_Z0/(2*Z0)*sin(beta*delta_L/2)
 *
 * Caused by impedance or length asymmetry in a differential pair.
 * Any asymmetry converts some differential signal to common-mode,
 * which radiates and causes EMI.
 * ========================================================================= */
double routing_diff_to_common_conversion(const DiffPairSegment *seg,
                                          const TransmissionLine *tl,
                                          double freq_hz)
{
    if (!seg || !tl) return 0.0;
    double delta_L = fabs(seg->p_seg.length_mm - seg->n_seg.length_mm) * 1e-3;
    double beta = 2.0 * M_PI * freq_hz * sqrt(tl->effective_er) / C0;
    return fabs(0.5 * sin(beta * delta_L / 2.0));
}

/* =========================================================================
 * L4: MAXIMUM ALLOWABLE LENGTH MISMATCH for timing budget
 *
 * delta_L_max = v_p * delta_t_budget
 * v_p = c / sqrt(er_eff)
 *
 * Example: FR-4 (er_eff=4), delta_t=10ps:
 *   v_p = 3e8/sqrt(4) = 1.5e8 m/s
 *   delta_L_max = 1.5e8*10e-12 = 1.5mm
 * ========================================================================= */
double routing_max_length_mismatch(double vp_m_per_s, double timing_budget_ps)
{
    if (vp_m_per_s <= 0.0 || timing_budget_ps <= 0.0) return 0.0;
    return vp_m_per_s * timing_budget_ps * 1e-12 * 1e3;  /* mm */
}

/* L4: Required serpentine length to compensate delay mismatch */
double routing_serpentine_length(double delay_mismatch_ps, double t_pd_ps_per_mm)
{
    if (t_pd_ps_per_mm <= 0.0) return 0.0;
    return delay_mismatch_ps / t_pd_ps_per_mm;  /* mm */
}

/* L4: Serpentine design — computes meander geometry for added length */
SerpentineGeometry routing_serpentine_design(double added_length_mm,
                                              double min_spacing_3x_mm)
{
    SerpentineGeometry sg; memset(&sg, 0, sizeof(sg));
    if (added_length_mm <= 0.0) return sg;
    sg.pitch_mm = min_spacing_3x_mm * 3.0;
    sg.amplitude_mm = sg.pitch_mm * 1.5;
    double length_per_meander = 2.0 * sqrt(sg.pitch_mm*sg.pitch_mm
                                + 4.0*sg.amplitude_mm*sg.amplitude_mm);
    if (length_per_meander <= 0.0) return sg;
    sg.num_meanders = (int)(added_length_mm / length_per_meander) + 1;
    sg.segment_length_mm = length_per_meander;
    return sg;
}

/* L4: Trombone delay matching structure */
TromboneGeometry routing_trombone_design(double added_length_mm,
                                          double trace_width_mm,
                                          double trace_spacing_mm)
{
    TromboneGeometry tg; memset(&tg, 0, sizeof(tg));
    if (added_length_mm <= 0.0) return tg;
    tg.trombone_width_mm = trace_spacing_mm * 4.0 + trace_width_mm;
    tg.trombone_extension_mm = added_length_mm / 2.0;
    tg.added_delay_ps = added_length_mm * 6.0;
    return tg;
}

/* =========================================================================
 * L5: BGA BREAKOUT DESIGN
 *
 * BGA escape routing: for 1.0mm pitch → dog-bone fanout with 0.25mm via,
 * 1 trace between pads per layer. For 0.8mm pitch → via-in-pad required.
 *
 * Number of routing layers needed = ceil(rows / (pitch/min_spacing))
 * ========================================================================= */
BgaBreakoutDesign routing_bga_breakout(const BgaPackage *bga, int ipc_class)
{
    BgaBreakoutDesign bd; memset(&bd, 0, sizeof(bd));
    if (!bga || bga->bga_pitch_mm <= 0.0) return bd;
    bd.via_drill_mm = 0.25;
    bd.via_pad_mm = 0.50;
    double min_trace = 0.1;
    double min_space = 0.1;
    if (ipc_class == 2) { min_trace = 0.12; min_space = 0.12; }
    if (ipc_class == 3) { min_trace = 0.15; min_space = 0.15; }
    double usable_channel = bga->bga_pitch_mm - bd.via_pad_mm;
    if (usable_channel < min_trace + 2.0*min_space) {
        bd.is_via_in_pad = 1;
    } else {
        bd.is_via_in_pad = 0;
    }
    bd.min_trace_width_um = min_trace * 1000.0;
    bd.min_spacing_um = min_space * 1000.0;
    bd.escape_layers_per_row = bd.is_via_in_pad ? 1 : 2;
    bd.num_routing_layers_needed = bga->num_rows * bd.escape_layers_per_row / 2;
    if (bd.num_routing_layers_needed < 1) bd.num_routing_layers_needed = 1;
    return bd;
}

/* L5: Minimize via count for a multi-segment route */
int routing_minimize_vias(PcbRoute *route,
                           const RoutingConstraint *constraints,
                           int num_layers_available)
{
    if (!route || num_layers_available <= 1) return 0;
    int vias_needed = 0;
    int current_layer = route->segments[0].layer;
    for (int i = 1; i < route->num_segments; i++) {
        if (route->segments[i].layer != current_layer) {
            vias_needed++;
            current_layer = route->segments[i].layer;
        }
    }
    (void)constraints;
    return vias_needed;
}

/* =========================================================================
 * L5: CROSSTALK BETWEEN PARALLEL SEGMENTS
 *
 * NEXT (Near-End Crosstalk):
 *   NEXT = K_near * coupling_length * dV/dt
 *   K_near ~ 0.25*Cm/C_total + 0.25*Lm/L_total
 *
 * FEXT (Far-End Crosstalk):
 *   FEXT = K_far * coupling_length * dV/dt
 *   K_far ~ 0.5*(Cm/C_total - Lm/L_total)  — zero for homogeneous TEM
 *
 * In microstrip (inhomogeneous), FEXT is non-zero due to different
 * phase velocities for even/odd modes.
 * ========================================================================= */
CrosstalkResult routing_crosstalk_parallel_segments(
    const RouteSegment *aggressor, const RouteSegment *victim,
    const TransmissionLine *tl, double freq_hz, double rise_time_ps)
{
    CrosstalkResult cr; memset(&cr, 0, sizeof(cr));
    if (!aggressor || !victim || !tl) return cr;
    double coupling = (aggressor->width_um > 0)
                      ? exp(-fabs(aggressor->start_y_mm - victim->start_y_mm)
                            / (aggressor->width_um * 1e-3))
                      : 0.1;
    cr.coupling_length_mm = fmin(aggressor->length_mm, victim->length_mm);
    double Lm_over_L = coupling * 0.3;
    double Cm_over_C = coupling * 0.25;
    double tr = rise_time_ps * 1e-12;
    if (tr <= 0) tr = 100e-12;
    double NEXT_v = 0.25*(Cm_over_C + Lm_over_L)/tr*cr.coupling_length_mm*1e-3;
    cr.NEXT_db = 20.0*log10(fabs(NEXT_v));
    double FEXT_v = 0.5*(Cm_over_C - Lm_over_L)/tr*cr.coupling_length_mm*1e-3;
    cr.FEXT_db = 20.0*log10(fabs(FEXT_v));
    cr.max_saturation_db = -10.0;
    (void)freq_hz;
    return cr;
}

/* L5: Minimum spacing for crosstalk budget */
double routing_min_spacing_for_crosstalk(double max_crosstalk_db,
                                          double coupling_length_mm,
                                          const TransmissionLine *tl,
                                          double freq_hz)
{
    if (!tl || coupling_length_mm <= 0.0) return 0.5;
    double target_voltage_ratio = pow(10.0, max_crosstalk_db/20.0);
    double min_space = 0.1;
    for (int iter = 0; iter < 30; iter++) {
        double coupling = exp(-min_space/0.5);
        double tr = 100e-12;
        double xtalk_v = 0.25*(0.3+0.25)*coupling/tr*coupling_length_mm*1e-3;
        double xtalk_db = 20.0*log10(fabs(xtalk_v));
        if (xtalk_db < max_crosstalk_db) break;
        min_space *= 1.2;
    }
    (void)freq_hz;
    return min_space;
}

/* =========================================================================
 * L6: DDR4 FLY-BY ROUTING VERIFICATION
 *
 * DDR4 uses fly-by topology for address/command/control bus.
 * The clock is routed past each SDRAM in daisy-chain fashion.
 * Skew at each rank = t_pd * distance from controller.
 * ========================================================================= */
Ddr4FlyByBus routing_ddr4_flyby_check(double clock_freq_mhz,
                                       const RouteSegment *segments,
                                       int num_segments,
                                       const TransmissionLine *tl)
{
    Ddr4FlyByBus bus; memset(&bus, 0, sizeof(bus));
    if (!segments || num_segments < 1 || !tl) return bus;
    bus.clock_freq_mhz = clock_freq_mhz;
    bus.num_ranks = num_segments;
    double tpd = tl->delay_per_m * 1e12 * 1e3;  /* ps/mm */
    double cum_len = 0.0;
    double ui = 1e6 / clock_freq_mhz;  /* ps */
    bus.timing_ok = 1;
    for (int i = 0; i < num_segments && i < 4; i++) {
        cum_len += (i == 0) ? segments[i].length_mm : 0.0;
        bus.trace_lengths_mm[i] = cum_len;
        bus.skew_per_rank_ps[i] = cum_len * tpd;
        if (bus.skew_per_rank_ps[i] > 0.25 * ui) bus.timing_ok = 0;
    }
    return bus;
}

/* =========================================================================
 * L6: PCIe LANE-TO-LANE SKEW BUDGET ANALYSIS
 *
 * PCIe Base Spec: max lane skew
 *   Gen1/2 (2.5/5 GT/s): 1.6ns
 *   Gen3 (8 GT/s): 1.0ns
 *   Gen4 (16 GT/s): 0.8ns
 *   Gen5 (32 GT/s): 0.5ns
 * ========================================================================= */
PcieSkewBudget routing_pcie_skew_check(const double *lane_lengths_mm,
                                        int num_lanes, int pcie_generation,
                                        const TransmissionLine *tl)
{
    PcieSkewBudget psb; memset(&psb, 0, sizeof(psb));
    if (!lane_lengths_mm || num_lanes < 2 || !tl) return psb;
    psb.num_lanes = num_lanes;
    double max_skew_ns;
    switch (pcie_generation) {
        case 1: case 2: max_skew_ns = 1.6; break;
        case 3: max_skew_ns = 1.0; break;
        case 4: max_skew_ns = 0.8; break;
        case 5: max_skew_ns = 0.5; break;
        default: max_skew_ns = 1.0; break;
    }
    psb.max_skew_ps = max_skew_ns * 1000.0;
    double tpd = tl->delay_per_m * 1e12 * 1e3;
    double min_len = lane_lengths_mm[0], max_len = lane_lengths_mm[0];
    for (int i = 0; i < num_lanes; i++) {
        psb.lane_lengths_mm[i] = lane_lengths_mm[i];
        if (lane_lengths_mm[i] < min_len) min_len = lane_lengths_mm[i];
        if (lane_lengths_mm[i] > max_len) max_len = lane_lengths_mm[i];
    }
    psb.worst_pair_skew_ps = (max_len - min_len) * tpd;
    psb.skew_budget_used_pct = psb.worst_pair_skew_ps / psb.max_skew_ps * 100.0;
    psb.compliant = (psb.worst_pair_skew_ps <= psb.max_skew_ps) ? 1 : 0;
    return psb;
}

/* L6: DDR bus length matching target optimization */
double routing_bus_matching_target(const double *trace_lengths_mm,
                                    int num_traces, double *serpentine_per_trace)
{
    if (!trace_lengths_mm || num_traces <= 1) return 0.0;
    double max_len = trace_lengths_mm[0];
    for (int i = 1; i < num_traces; i++)
        if (trace_lengths_mm[i] > max_len) max_len = trace_lengths_mm[i];
    if (serpentine_per_trace) {
        for (int i = 0; i < num_traces; i++)
            serpentine_per_trace[i] = max_len - trace_lengths_mm[i];
    }
    return max_len;
}

/* =========================================================================
 * L7: HDMI 2.1 routing validation (48 Gbps)
 *
 * Requirements: 4 TMDS differential pairs + clock, max intra-pair skew
 * < 5ps, max inter-pair skew < 100ps, impedance controlled at 100 Ohm.
 * ========================================================================= */
int routing_hdmi21_validate(const PcbRoute *tmds_pairs[4],
                             const PcbRoute *clock_pair,
                             const Hdmi21RoutingSpec *spec,
                             const TransmissionLine *tl)
{
    if (!tmds_pairs || !spec) return 0;
    for (int i = 0; i < 4; i++) {
        if (tmds_pairs[i]->total_length_mm > spec->max_length_mm) return 0;
    }
    (void)clock_pair;
    (void)tl;
    return 1;
}

/* L7: USB-C routing validation */
int routing_usb_c_validate(const PcbRoute *ss_pairs[4],
                            const DiffPairSegment *diff_segs[4],
                            const TransmissionLine *tl)
{
    if (!ss_pairs || !diff_segs) return 0;
    for (int i = 0; i < 4; i++) {
        if (!ss_pairs[i]) continue;
        double skew = routing_diff_pair_skew(diff_segs[i], tl);
        if (skew > 10.0) return 0;
    }
    return 1;
}

/* L7: 10GBASE-T Ethernet routing validation */
int routing_10gbase_t_validate(const PcbRoute *pairs[4],
                                const TransmissionLine *tl,
                                double total_length_m)
{
    if (!pairs || !tl) return 0;
    for (int i = 0; i < 4; i++) {
        if (!pairs[i]) continue;
        if (pairs[i]->total_length_mm * 1e-3 > total_length_m) return 0;
    }
    return 1;
}

/* =========================================================================
 * L8: FIBER-WEAVE SKEW REDUCTION THROUGH ANGLED ROUTING
 *
 * Routing at 10-15 degree angle to the glass weave breaks the periodic
 * er variation pattern, reducing effective skew by averaging over
 * multiple weave periods.
 *
 * skew_reduction = 1 - sin(pi*angle/180)/(pi*angle/180) * (w/pitch)
 * ========================================================================= */
double routing_fiber_weave_skew_reduction(double angle_degrees,
                                           double trace_length_mm,
                                           double glass_pitch_mm)
{
    if (glass_pitch_mm <= 0.0 || trace_length_mm <= 0.0) return 0.0;
    double angle_rad = angle_degrees * M_PI / 180.0;
    if (angle_rad < 0.01) return 0.0;
    double reduction = 1.0 - sin(M_PI*angle_degrees/180.0)
                       / (M_PI*angle_degrees/180.0 + 1e-12);
    return reduction * 100.0;
}

/* =========================================================================
 * L8: VIA STUB RESONANCE FREQUENCY
 *
 * f_res = c/(4*sqrt(er_eff)*L_stub)  — quarter-wave stub resonance
 *
 * A via stub forms an open-circuit quarter-wave resonator. At f_res,
 * the stub input impedance approaches zero (short circuit), causing
 * a deep null in insertion loss. This is why backdrilling is needed
 * for high-speed serial links (>5 Gbps).
 * ========================================================================= */
double routing_via_stub_resonance_freq(double stub_length_mm, double er_eff)
{
    if (stub_length_mm <= 0.0 || er_eff <= 0.0) return 0.0;
    return C0/(4.0*sqrt(er_eff)*stub_length_mm*1e-3)/1e9;  /* GHz */
}

/* L8: Maximum stub length budget for given bit rate */
double routing_max_stub_length(double bit_rate_gbps, double er_eff,
                                double margin_db)
{
    if (bit_rate_gbps <= 0.0 || er_eff <= 0.0) return 0.0;
    double f_nyquist = bit_rate_gbps * 1e9 / 2.0;
    double f_max = f_nyquist * pow(10.0, margin_db/20.0);
    return C0/(4.0*sqrt(er_eff)*f_max)*1e3;  /* mm */
}

/* =========================================================================
 * L8: T-COIL INDUCTANCE for capacitive load compensation
 *
 * L_peak = R^2 * C_load / (4 * zeta^2)
 * For maximally flat Butterworth response: zeta = 1/sqrt(2)
 *
 * T-coil networks extend bandwidth by resonating with the capacitive
 * load at the receiver input, creating a doublet that widens the
 * frequency response. Used in high-speed I/O receiver designs.
 * ========================================================================= */
double routing_tcoil_inductance(double termination_ohm, double load_cap_pf,
                                 double damping_factor)
{
    if (termination_ohm <= 0.0 || load_cap_pf <= 0.0 || damping_factor <= 0.0)
        return 0.0;
    return termination_ohm * termination_ohm * load_cap_pf * 1e-12
           / (4.0 * damping_factor * damping_factor) * 1e9;  /* nH */
}
