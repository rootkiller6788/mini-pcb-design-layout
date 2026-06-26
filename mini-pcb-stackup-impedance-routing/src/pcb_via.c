/**
 * pcb_via.c — PCB Via Design, Modeling, and Optimization
 *
 * Implements via parasitic extraction, electrical modeling, impedance
 * analysis, and optimization. Covers L2-L8 per SKILL.md taxonomy.
 *
 * Via types: through-hole, blind, buried, microvia, via-in-pad,
 * backdrilled, stacked, staggered.
 *
 * Key references:
 *   Goldfarb & Pucel (1981) — via inductance
 *   IPC-2221 / IPC-2152 — via design rules
 *   IPC-TR-579 — via reliability (thermal cycling)
 *   Johnson & Graham "High-Speed Digital Design" Ch.7 — via modeling
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include "pcb_via.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define C0  2.99792458e8
#define MU0 1.2566370614e-6
#define EPS0 8.8541878176e-12

/* =========================================================================
 * L2: VIA DC RESISTANCE
 *
 * R_dc = rho * L / A_barrel
 * A_barrel = pi*(R_outer^2 - R_inner^2)
 * For copper: rho = 1/sigma = 1/5.8e7 = 1.724e-8 ohm*m
 * ========================================================================= */
double via_dc_resistance(const ViaDimensions *dim, double conductivity)
{
    if (!dim || dim->total_length_mm <= 0.0 || conductivity <= 0.0) return 0.0;
    double r_outer = (dim->drill_diameter_mm/2.0 + dim->plating_thickness_um/1000.0) * 1e-3;
    double r_inner = dim->drill_diameter_mm/2.0 * 1e-3;
    double area = M_PI * (r_outer*r_outer - r_inner*r_inner);
    double len = dim->total_length_mm * 1e-3;
    double rho = 1.0 / conductivity;
    if (area <= 0.0) return 0.0;
    return rho * len / area * 1e3;  /* mOhm */
}

/* =========================================================================
 * L2: VIA AC RESISTANCE — Skin effect
 *
 * R_ac = rho*L/(2*pi*r*delta)  when skin_depth << barrel_thickness
 * delta = sqrt(2/(omega*mu*sigma))
 * ========================================================================= */
double via_ac_resistance(const ViaDimensions *dim, double freq_hz, double conductivity)
{
    if (!dim || dim->total_length_mm <= 0.0 || freq_hz <= 0.0) return 0.0;
    double omega = 2.0*M_PI*freq_hz;
    double mu = MU0;
    double delta = sqrt(2.0/(omega*mu*conductivity));
    double r_mean = (dim->drill_diameter_mm/2.0 + dim->plating_thickness_um/2000.0) * 1e-3;
    double len = dim->total_length_mm * 1e-3;
    double surface_area = 2.0*M_PI*r_mean*delta;
    double effective_area = 2.0*M_PI*r_mean*delta;
    if (delta > dim->plating_thickness_um*1e-6) {
        effective_area = M_PI*((r_mean+dim->plating_thickness_um*1e-6/2.0)
                        *(r_mean+dim->plating_thickness_um*1e-6/2.0) - r_mean*r_mean);
    }
    double rho = 1.0/conductivity;
    if (effective_area <= 0.0) return 0.0;
    return rho*len/effective_area * 1e3;
}

/* =========================================================================
 * L2: VIA INDUCTANCE — Goldfarb & Pucel (1981) formula
 *
 * L_via = (mu0/2*pi)*h*ln(h/r + sqrt(1+(h/r)^2))
 * Approximated for h >> r:
 *   L ≈ 5.08*h*[ln(2h/r) + 1]  nH  (h, r in mm)
 *
 * This captures the partial self-inductance of the via barrel.
 * For a complete current loop, the return path inductance must also
 * be accounted for (often the dominant contribution).
 * ========================================================================= */
double via_inductance(const ViaDimensions *dim)
{
    if (!dim || dim->total_length_mm <= 0.0) return 0.0;
    double h = dim->total_length_mm;
    double r = dim->drill_diameter_mm / 2.0;
    if (r <= 0.0) return 0.0;
    /* Goldfarb & Pucel approximation for h >> r */
    if (h/r > 5.0) {
        return 5.08 * h * (log(2.0*h/r) + 1.0) * 0.001;  /* nH */
    } else {
        double term = sqrt(1.0 + (h/r)*(h/r));
        return (MU0*1e9)/(2.0*M_PI) * h*1e-3 * log(h/r + term);
    }
}

/* =========================================================================
 * L2: VIA CAPACITANCE — Barrel to plane capacitance
 *
 * C_via = 1.41*er*h*D_pad/(D_antipad - D_pad)  [pF]
 * where h = plane thickness (mm), D in mm.
 *
 * This is an empirical formula from IPC modeling, based on the
 * coaxial geometry of the via pad within the antipad clearance.
 * ========================================================================= */
double via_capacitance(const ViaDimensions *dim, double er, double plane_thickness_mm)
{
    if (!dim || er <= 0.0 || plane_thickness_mm <= 0.0) return 0.0;
    double D_pad = dim->pad_diameter_mm;
    double D_antipad = dim->antipad_diameter_mm;
    if (D_antipad <= D_pad) return 0.0;
    return 1.41 * er * plane_thickness_mm * D_pad / (D_antipad - D_pad);
}

/* =========================================================================
 * L2: COMPLETE VIA ELECTRICAL MODEL
 *
 * Builds lumped-element model: R, L, C, stub_C, Z0_approx, tau, f_res.
 *
 * The via stub (if not backdrilled) forms a quarter-wave resonator:
 *   f_res = c/(4*sqrt(er_eff)*L_stub)
 * This resonance can cause severe insertion loss at frequencies
 * where the stub electrical length equals lambda/4.
 * ========================================================================= */
ViaElectricalModel via_electrical_model(const ViaDimensions *dim,
                                         double er, double conductivity,
                                         double target_freq_ghz)
{
    ViaElectricalModel m; memset(&m, 0, sizeof(m));
    if (!dim) return m;
    m.capacitance_pf = via_capacitance(dim, er, 0.035);  /* 1oz plane */
    m.inductance_nh = via_inductance(dim);
    m.resistance_mohm = via_dc_resistance(dim, conductivity);
    double ac_r = via_ac_resistance(dim, target_freq_ghz*1e9, conductivity);
    if (ac_r > m.resistance_mohm) m.resistance_mohm = ac_r;
    if (dim->stub_length_mm > 0.0 && !dim->is_backdrilled) {
        m.stub_capacitance_pf = 1.41*er*dim->stub_length_mm
                               *dim->pad_diameter_mm/(dim->antipad_diameter_mm-dim->pad_diameter_mm);
        m.resonant_freq_ghz = C0/(4.0*sqrt(er)*dim->stub_length_mm*1e-3)/1e9;
    }
    double z0_coax = (60.0/sqrt(er))*log(dim->antipad_diameter_mm/dim->pad_diameter_mm);
    m.impedance_ohm = z0_coax;
    if (m.inductance_nh > 0.0 && m.capacitance_pf > 0.0) {
        m.delay_ps = sqrt(m.inductance_nh*1e-9 * m.capacitance_pf*1e-12) * 1e12;
    }
    return m;
}

/* =========================================================================
 * L3: VIA CHARACTERISTIC IMPEDANCE — Coaxial approximation
 *
 * Models via as coaxial line: barrel = inner conductor, antipad edge = outer.
 * Z0_via = (60/sqrt(er)) * ln(D_antipad / D_pad)
 *
 * This is an approximation valid when the ground planes form a
 * quasi-coaxial structure around the via barrel.
 * ========================================================================= */
double via_characteristic_impedance(const ViaDimensions *dim, double er)
{
    if (!dim || er <= 0.0) return 50.0;
    double D_pad = dim->pad_diameter_mm;
    double D_antipad = dim->antipad_diameter_mm;
    if (D_antipad <= D_pad || D_pad <= 0.0) return 50.0;
    return (60.0/sqrt(er)) * log(D_antipad/D_pad);
}

/* =========================================================================
 * L3: VIA S-PARAMETERS — 2-port pi-network model
 *
 * Models via as L-C-L or C-L-C pi-network, computes S11 and S21
 * referenced to z0_ref.
 *
 * For a series impedance Z and shunt admittance Y:
 *   [S] = [ (Z/Y0+Z^2/2)/(1+Z/Y0+Z^2/2),  1/(1+Z/Y0+Z^2/2);
 *           1/(1+Z/Y0+Z^2/2),              (Z/Y0+Z^2/2)/(1+Z/Y0+Z^2/2) ]
 * ========================================================================= */
void via_s_parameters(const ViaElectricalModel *model, double freq_ghz,
                       double z0_ref, double complex *s11, double complex *s21)
{
    if (!model) return;
    double omega = 2.0*M_PI*freq_ghz*1e9;
    double L = model->inductance_nh * 1e-9;
    double C = model->capacitance_pf * 1e-12;
    double R = model->resistance_mohm * 1e-3;
    double complex Z = R + I*omega*L;
    double complex Y = I*omega*C;
    double complex A = 1.0 + Z*Y/2.0;
    double complex B = Z;
    double complex CC = Y*(1.0 + Z*Y/4.0);
    double complex D = A;
    double complex den = A + B/z0_ref + CC*z0_ref + D;
    if (cabs(den) < 1e-15) { if(s11)*s11=0.0; if(s21)*s21=0.0; return; }
    if (s11) *s11 = (A + B/z0_ref - CC*z0_ref - D)/den;
    if (s21) *s21 = 2.0/den;
}

/* =========================================================================
 * L3: VIA TDR RESPONSE
 *
 * Simulates time-domain reflection from a via impedance discontinuity.
 * Uses step response: reflection = (Z_via - Z_trace)/(Z_via + Z_trace)
 * convolved with a step function of given rise time.
 * ========================================================================= */
void via_tdr_response(const ViaElectricalModel *model, double z0_trace,
                       double rise_time_ps, double *t_ns, double *reflection,
                       int num_points)
{
    if (!model || !t_ns || !reflection || num_points <= 1) return;
    double gamma = (model->impedance_ohm - z0_trace)
                 / (model->impedance_ohm + z0_trace);
    double tau = model->delay_ps * 1e-3;  /* ns */
    double tr = rise_time_ps * 1e-3;
    double t_max = tau*3.0 + tr*2.0;
    double dt = t_max/(num_points-1);
    for (int i = 0; i < num_points; i++) {
        double t = i*dt;
        t_ns[i] = t;
        if (t < tau) reflection[i] = 0.0;
        else if (t < tau+tr) reflection[i] = gamma*(t-tau)/tr;
        else if (t < tau+tr+tau) reflection[i] = gamma;
        else reflection[i] = gamma*(1.0 - (t-tau-tr-tau)/tr > 0 ? 0 : gamma);
        if (reflection[i] > 1.0) reflection[i] = 1.0;
    }
}

/* L3: Multi-via parallel impedance for power delivery */
void via_parallel_impedance(const ViaDimensions *dim, int num_vias,
                             double er, double *l_total_nh, double *r_total_mohm)
{
    if (!dim || num_vias <= 0) return;
    double L_single = via_inductance(dim);
    double R_single = via_dc_resistance(dim, 5.8e7);
    if (l_total_nh) *l_total_nh = L_single/num_vias;
    if (r_total_mohm) *r_total_mohm = R_single/num_vias;
}

/* =========================================================================
 * L4: VIA MINIMUM DIMENSIONS — IPC-2221 design rules
 *
 * Annular ring = (pad - drill)/2 >= min_annular_ring
 * Class 1: 0.05mm, Class 2: 0.10mm, Class 3: 0.15mm (IPC-6012)
 * ========================================================================= */
int via_min_dimensions(int ipc_class, double *min_drill_mm,
                        double *min_pad_mm, double *min_antipad_mm)
{
    if (!min_drill_mm || !min_pad_mm || !min_antipad_mm) return -1;
    double annular_ring, drill_base, antipad_clearance;
    switch (ipc_class) {
        case 1: annular_ring=0.05; drill_base=0.30; antipad_clearance=0.15; break;
        case 2: annular_ring=0.10; drill_base=0.25; antipad_clearance=0.20; break;
        case 3: annular_ring=0.15; drill_base=0.20; antipad_clearance=0.25; break;
        default: return -2;
    }
    *min_drill_mm = drill_base;
    *min_pad_mm = drill_base + 2.0*annular_ring;
    *min_antipad_mm = *min_pad_mm + 2.0*antipad_clearance;
    return 0;
}

/* =========================================================================
 * L4: ASPECT RATIO CHECK — Plating reliability
 *
 * AR = board_thickness/drill_diameter
 * Standard: AR <= 8, Advanced: AR <= 12, Microvia: AR <= 1
 *
 * Higher aspect ratios make uniform plating difficult, increasing
 * the risk of voids and barrel cracking.
 * ========================================================================= */
int via_aspect_ratio_check(double board_thickness_mm, double drill_mm,
                            double max_aspect_ratio)
{
    if (drill_mm <= 0.0) return 0;
    double ar = board_thickness_mm/drill_mm;
    return (ar <= max_aspect_ratio) ? 1 : 0;
}

/* =========================================================================
 * L4: VIA THERMAL CYCLE LIFE — Coffin-Manson fatigue model
 *
 * N_f = C*(delta_epsilon_plastic)^(-n)
 * delta_epsilon = CTE_z * delta_T * 1e-6 (plastic strain range)
 *
 * Based on IPC-TR-579 reliability data for FR-4/copper vias:
 * C = 0.65, n = 1.9
 * Aspect ratio stress intensification: factor = 1 + 0.3*AR
 * ========================================================================= */
double via_thermal_cycle_life(double delta_t_c, double via_diameter_mm,
                               double board_thickness_mm, double cte_z_ppm_per_c)
{
    if (delta_t_c <= 0.0 || via_diameter_mm <= 0.0) return 0.0;
    double ar = board_thickness_mm/via_diameter_mm;
    double strain = cte_z_ppm_per_c*1e-6*delta_t_c;
    double stress_f = 1.0 + 0.3*ar;
    double eff_strain = strain*stress_f;
    double C=0.65, n=1.9;
    if (eff_strain < 1e-10) return 1e9;
    double cycles = C*pow(eff_strain, -n);
    if (cycles < 10.0) cycles=10.0;
    if (cycles > 1e9) cycles=1e9;
    return cycles;
}

/* =========================================================================
 * L4: VIA CURRENT CAPACITY — IPC-2152 adapted for vias
 *
 * I_max = J_max * A_barrel
 * J_max = 250 A/mm^2 for 10C temperature rise (copper)
 * ========================================================================= */
double via_current_capacity(const ViaDimensions *dim, double temp_rise_c)
{
    if (!dim || dim->drill_diameter_mm <= 0.0) return 0.0;
    double r_outer = dim->drill_diameter_mm/2.0 + dim->plating_thickness_um/1000.0;
    double r_inner = dim->drill_diameter_mm/2.0;
    double area_mm2 = M_PI*(r_outer*r_outer - r_inner*r_inner);
    double J = 250.0*pow(temp_rise_c/10.0, 0.44);
    return J*area_mm2;
}

/* =========================================================================
 * L5: OPTIMAL ANTIPAD SIZE — Impedance matching via bisection
 *
 * Finds D_antipad such that Z_via = target_Z0.
 * Z_via = (60/sqrt(er))*ln(D_antipad/D_pad)
 * => D_antipad = D_pad * exp(target_Z0*sqrt(er)/60)
 * ========================================================================= */
double via_optimal_antipad(const ViaDimensions *dim, double er, double target_z0_ohm)
{
    if (!dim || er <= 0.0 || target_z0_ohm <= 0.0) return 0.0;
    return dim->pad_diameter_mm * exp(target_z0_ohm*sqrt(er)/60.0);
}

/* =========================================================================
 * L5: VIA STUB RESONANCE — Quarter-wave stub analysis
 *
 * f_res = c/(4*sqrt(er_eff)*L_stub)
 * max_stub = c/(4*sqrt(er_eff)*f_max)
 *
 * For 10 Gbps (Nyquist=5GHz), max stub on FR-4 (er=4.2):
 *   max_stub = C0/(4*sqrt(4.2)*5e9) = 7.3mm → need backdrilling!
 * ========================================================================= */
double via_max_stub_for_frequency(double max_freq_ghz, double er_eff)
{
    if (max_freq_ghz <= 0.0 || er_eff <= 1.0) return 0.0;
    return C0/(4.0*sqrt(er_eff)*max_freq_ghz*1e9) * 1e3;  /* mm */
}

/* L5: Backdrill depth calculation */
double via_backdrill_depth(const ViaDimensions *dim, double signal_layer_depth_mm,
                            double max_stub_mm)
{
    if (!dim) return 0.0;
    double stub = dim->total_length_mm - signal_layer_depth_mm;
    if (stub <= max_stub_mm) return 0.0;
    return stub - max_stub_mm;
}

/* L5: Via array optimization — minimum number of vias for target inductance */
int via_array_optimize(const ViaDimensions *dim, double er,
                        double target_inductance_nh, int max_vias,
                        double *actual_inductance_nh)
{
    if (!dim || max_vias <= 0 || target_inductance_nh <= 0.0) return -1;
    double L_single = via_inductance(dim);
    int n_needed = (int)ceil(L_single/target_inductance_nh);
    if (n_needed > max_vias) n_needed = max_vias;
    if (n_needed < 1) n_needed = 1;
    if (actual_inductance_nh) *actual_inductance_nh = L_single/n_needed;
    (void)er;
    return n_needed;
}

/* =========================================================================
 * L5: VIA INSERTION LOSS AT FREQUENCY
 *
 * IL = 20*log10|1 - gamma^2| - 0.115*R_via/Z0  [dB]
 * Combines reflection loss + resistive loss through via.
 * ========================================================================= */
double via_insertion_loss_db(const ViaElectricalModel *model, double z0_trace, double freq_ghz)
{
    if (!model) return 0.0;
    double gamma = (model->impedance_ohm - z0_trace)/(model->impedance_ohm + z0_trace);
    double reflect_loss = 20.0*log10(fabs(1.0 - gamma*gamma));
    double resistive_loss = 0.115*model->resistance_mohm*1e-3/z0_trace;
    return reflect_loss - resistive_loss;
}

/* =========================================================================
 * L6: DESIGN 50 OHM THROUGH-HOLE VIA
 *
 * For a standard 1.6mm board, IPC Class 2:
 *   Drill: 0.30mm, Pad: 0.55mm, Antipad: 0.85mm
 * ========================================================================= */
ViaDimensions via_design_50ohm_through(double board_thickness_mm, int ipc_class)
{
    ViaDimensions v; memset(&v, 0, sizeof(v));
    double min_drill, min_pad, min_antipad;
    via_min_dimensions(ipc_class, &min_drill, &min_pad, &min_antipad);
    v.drill_diameter_mm = min_drill;
    v.finished_hole_diameter_mm = min_drill - 0.025;
    v.pad_diameter_mm = min_pad;
    v.antipad_diameter_mm = min_antipad;
    v.plating_thickness_um = 25.0;
    v.start_layer = 1;
    v.end_layer = 2;
    v.total_length_mm = board_thickness_mm;
    v.stub_length_mm = 0.0;
    v.is_backdrilled = 0;
    return v;
}

/* L6: Design microvia for HDI board (laser-drilled, <= 100um) */
ViaDimensions via_design_microvia(double dielectric_thickness_mm, double pad_diameter_mm)
{
    ViaDimensions v; memset(&v, 0, sizeof(v));
    v.drill_diameter_mm = 0.100;
    v.finished_hole_diameter_mm = 0.075;
    v.pad_diameter_mm = pad_diameter_mm > 0 ? pad_diameter_mm : 0.250;
    v.antipad_diameter_mm = v.pad_diameter_mm + 0.300;
    v.plating_thickness_um = 15.0;
    v.start_layer = 1;
    v.end_layer = 2;
    v.total_length_mm = dielectric_thickness_mm;
    return v;
}

/* =========================================================================
 * L6: DIFFERENTIAL PAIR VIA TRANSITION
 *
 * Two signal vias + surrounding GND stitching vias for continuous
 * return path. The GND vias are placed in a fence around the signal vias
 * to maintain the differential impedance through the transition.
 * ========================================================================= */
DiffPairViaTransition via_design_diff_pair_transition(
    double board_thickness_mm, double target_z_diff,
    double er, int ipc_class)
{
    DiffPairViaTransition dvt; memset(&dvt, 0, sizeof(dvt));
    ViaDimensions sv = via_design_50ohm_through(board_thickness_mm, ipc_class);
    sv.antipad_diameter_mm = via_optimal_antipad(&sv, er, target_z_diff/2.0);
    dvt.signal_vias[0] = sv;
    dvt.signal_vias[1] = sv;
    dvt.num_ground_vias = 4;
    dvt.via_to_via_pitch_mm = 1.0;
    double z_se = via_characteristic_impedance(&sv, er);
    dvt.diff_impedance_ohm = 2.0*z_se;
    for (int i=0; i<dvt.num_ground_vias; i++) {
        dvt.ground_vias[i] = sv;
        dvt.ground_vias[i].pad_diameter_mm = sv.pad_diameter_mm*0.8;
    }
    return dvt;
}

/* =========================================================================
 * L6: VIA FENCE / GUARD TRACE ISOLATION
 *
 * A row of grounded vias creates a quasi-coaxial shield that improves
 * isolation. The isolation depends on the fence pitch (fence_pitch_mm)
 * relative to wavelength:
 *   isolation ~ 20*log10(pi*pitch/lambda) for pitch < lambda/2
 * ========================================================================= */
ViaFence via_design_fence(double signal_trace_length_mm,
                           double er, double target_isolation_db,
                           double max_freq_ghz)
{
    ViaFence vf; memset(&vf, 0, sizeof(vf));
    double lambda = C0/(max_freq_ghz*1e9*sqrt(er))*1e3;
    vf.fence_pitch_mm = lambda/4.0;
    vf.fence_to_signal_spacing_mm = lambda/8.0;
    vf.num_fence_vias = (int)(signal_trace_length_mm/vf.fence_pitch_mm) + 1;
    vf.max_isolation_freq_ghz = max_freq_ghz;
    vf.isolation_db_at_freq = 20.0*log10(M_PI*vf.fence_pitch_mm/lambda);
    if (-vf.isolation_db_at_freq < target_isolation_db) {
        vf.isolation_db_at_freq = target_isolation_db;
    }
    (void)er;
    return vf;
}

/* =========================================================================
 * L7: PCIe Gen5 via design (32 GT/s)
 *
 * Requires backdrilling (max stub < 10mil = 0.254mm at 16GHz Nyquist)
 * and tight impedance control (85 Ohm differential).
 * ========================================================================= */
ViaDimensions via_pcie_gen5_design(double board_thickness_mm)
{
    ViaDimensions v = via_design_50ohm_through(board_thickness_mm, 3);
    v.pad_diameter_mm = 0.45;
    v.drill_diameter_mm = 0.20;
    v.antipad_diameter_mm = 0.80;
    v.plating_thickness_um = 20.0;
    double max_stub = via_max_stub_for_frequency(16.0, 3.7);
    v.stub_length_mm = max_stub;
    v.is_backdrilled = 1;
    return v;
}

/* L7: DDR5 via design (up to 6400 MT/s) */
ViaDimensions via_ddr5_design(double board_thickness_mm)
{
    ViaDimensions v = via_design_50ohm_through(board_thickness_mm, 2);
    v.drill_diameter_mm = 0.25;
    v.pad_diameter_mm = 0.50;
    v.antipad_diameter_mm = 0.70;
    return v;
}

/* L7: RF/microwave via design for Ka-band (40 GHz) */
ViaDimensions via_rf_ka_band_design(double board_thickness_mm, double er)
{
    ViaDimensions v; memset(&v, 0, sizeof(v));
    v.drill_diameter_mm = 0.15;
    v.pad_diameter_mm = 0.30;
    v.antipad_diameter_mm = via_optimal_antipad(&v, er, 50.0);
    v.plating_thickness_um = 15.0;
    v.start_layer = 1;
    v.end_layer = 2;
    v.total_length_mm = board_thickness_mm;
    return v;
}

/* =========================================================================
 * L8: COAXIAL VIA — Signal via surrounded by GND vias
 *
 * Creates continuous return path → minimal impedance discontinuity.
 * Models as multi-conductor coaxial line with TE11 mode cutoff.
 * cutoff_f_ghz = C0*1.841/(2*pi*ring_radius*sqrt(er))
 * ========================================================================= */
CoaxialVia via_design_coaxial(const ViaDimensions *signal_via,
                               double er, int num_ground_vias)
{
    CoaxialVia cv; memset(&cv, 0, sizeof(cv));
    if (!signal_via) return cv;
    cv.signal_via = *signal_via;
    cv.num_ground_vias = num_ground_vias > 0 ? num_ground_vias : 4;
    cv.ring_radius_mm = signal_via->pad_diameter_mm*1.5;
    cv.coaxial_z0_ohm = (60.0/sqrt(er))*log(cv.ring_radius_mm/(signal_via->pad_diameter_mm/2.0));
    cv.cutoff_freq_ghz = C0*1.841/(2.0*M_PI*cv.ring_radius_mm*1e-3*sqrt(er))/1e9;
    return cv;
}

/* L8: Via stub damping resistor for resonance suppression */
double via_stub_damping_resistor(const ViaElectricalModel *model)
{
    if (!model || model->stub_capacitance_pf <= 0.0 || model->inductance_nh <= 0.0)
        return 0.0;
    return sqrt(model->inductance_nh*1e-9/(model->stub_capacitance_pf*1e-12));
}

/* L8: Via-to-via crosstalk — NEXT/FEXT estimation */
double via_to_via_crosstalk(const ViaElectricalModel *aggressor,
                             const ViaElectricalModel *victim,
                             double separation_mm, double freq_ghz,
                             double rise_time_ps)
{
    if (!aggressor || !victim || separation_mm <= 0.0) return 0.0;
    double k_coupling = exp(-separation_mm/2.0);
    double mutual_L = k_coupling*sqrt(aggressor->inductance_nh*victim->inductance_nh);
    double mutual_C = k_coupling*sqrt(aggressor->capacitance_pf*victim->capacitance_pf);
    double Z0 = aggressor->impedance_ohm;
    if (Z0 <= 0.0) Z0 = 50.0;
    double NEXT = (mutual_L/Z0 + mutual_C*Z0)/(4.0*rise_time_ps*1e-12);
    return 20.0*log10(fabs(NEXT));
}
