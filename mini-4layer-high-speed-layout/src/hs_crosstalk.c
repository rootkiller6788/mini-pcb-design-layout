/*
 * hs_crosstalk.c — Crosstalk Analysis Implementation
 *
 * Implements near-end and far-end crosstalk calculation for
 * parallel PCB traces, including multi-aggressor analysis.
 *
 * Knowledge coverage:
 *   L1: NEXT, FEXT, mutual C/L definitions
 *   L2: Guard traces, 3W rule, diff pair immunity
 *   L3: Coupled transmission line equations
 *   L4: NEXT saturation theorem, FEXT proportionality
 *   L5: Coupling parameter extraction algorithms
 *   L6: Multi-aggressor crosstalk budget analysis
 */

#include "hs_crosstalk.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define C0      299792458.0
#define EPS0    8.854187817e-12
#define MU0     1.2566370614e-6

/* ================================================================
 * L3: Mutual capacitance — analytical edge-coupled microstrip model
 *
 * Uses the semi-empirical formula from Paul:
 *   C_m ≈ ε₀εr × (0.5 + 0.35×(w/h)^(-1.5)) × (w/s) × exp(-2.9×s/h)
 *
 * This captures the physics: mutual C increases with wider traces
 * and decreases exponentially with spacing.
 *
 * Reference: Paul, "Introduction to EMC", §9.2.2
 * Complexity: O(1)
 * ================================================================ */
double hs_mutual_capacitance(const hs_coupled_pair_t *pair)
{
    if (!pair) return 0.0;
    if (pair->edge_spacing_m <= 0.0 || pair->height_to_plane_m <= 0.0 ||
        pair->trace_width_m <= 0.0) return 0.0;

    double w = pair->trace_width_m;
    double s = pair->edge_spacing_m;
    double h = pair->height_to_plane_m;
    double er = pair->er_eff;

    double s_over_h = s / h;
    double w_over_h = w / h;

    /* Paul's formula for mutual capacitance per unit length */
    double factor = pow(w_over_h, -1.5);
    double cm = EPS0 * er * (0.5 + 0.35 * factor) *
                (w / s) * exp(-2.9 * s_over_h);

    /* Guard: mutual C should not exceed self C */
    double c_self = hs_self_capacitance(pair->z0_ohm, pair->er_eff);
    if (cm > 0.5 * c_self) cm = 0.5 * c_self;

    return cm;
}

/* ================================================================
 * L3: Mutual inductance — two parallel traces above ground plane
 *
 * Using partial inductance with image current method:
 *   L_m ≈ (μ₀/(2π)) × ln(1 + (2h/s)²)
 *
 * The ground plane image currents partially cancel the mutual
 * inductance between the traces. The closer the traces are to
 * each other relative to their height, the stronger the coupling.
 *
 * Reference: Paul, "Inductance: Loop and Partial", 2010, §4.5
 * Complexity: O(1)
 * ================================================================ */
double hs_mutual_inductance(const hs_coupled_pair_t *pair)
{
    if (!pair) return 0.0;
    if (pair->edge_spacing_m <= 0.0 || pair->height_to_plane_m <= 0.0) return 0.0;

    double s = pair->edge_spacing_m + pair->trace_width_m; /* center-to-center */
    double h = pair->height_to_plane_m;

    double lm = (MU0 / (2.0 * M_PI)) * log(1.0 + pow(2.0 * h / s, 2.0));

    /* Guard: mutual L should not exceed self L */
    double l_self = hs_self_inductance(pair->z0_ohm, pair->er_eff);
    if (lm > 0.5 * l_self) lm = 0.5 * l_self;

    return lm;
}

/* ================================================================
 * L2: Self capacitance and inductance
 *
 * Derived from characteristic impedance and effective dielectric
 * constant of the isolated trace.
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_self_capacitance(double z0, double er_eff)
{
    if (z0 <= 0.0 || er_eff <= 0.0) return 0.0;
    return sqrt(er_eff) / (C0 * z0);
}

double hs_self_inductance(double z0, double er_eff)
{
    if (z0 <= 0.0 || er_eff <= 0.0) return 0.0;
    return z0 * sqrt(er_eff) / C0;
}

/* ================================================================
 * L4: Coupling analysis — compute K_b and K_f
 *
 * Backward crosstalk coefficient:
 *   K_b = (1/4) × (L_m/L_self + C_m/C_self)
 *
 * Forward crosstalk coefficient:
 *   K_f = (1/2) × (C_m/C_self - L_m/L_self)
 *
 * For homogeneous medium (stripline): C_m/C = L_m/L → K_f = 0
 * For microstrip: C_m/C > L_m/L → K_f < 0 (typical)
 *
 * Physical interpretation:
 *   K_b: amplitude of reverse-traveling crosstalk pulse (saturates)
 *   K_f: amplitude factor for forward-traveling pulse (grows with length)
 *
 * Reference: Feller et al., 1977; Paul §10.3
 * Complexity: O(1)
 * ================================================================ */
void hs_coupling_analyze(const hs_coupled_pair_t *pair,
                          hs_coupling_params_t *params)
{
    if (!pair || !params) return;
    memset(params, 0, sizeof(*params));

    double c_m = hs_mutual_capacitance(pair);
    double l_m = hs_mutual_inductance(pair);
    double c_self = hs_self_capacitance(pair->z0_ohm, pair->er_eff);
    double l_self = hs_self_inductance(pair->z0_ohm, pair->er_eff);

    if (c_self <= 0.0 || l_self <= 0.0) return;

    params->mutual_capacitance_pf_per_m = c_m * 1e12;
    params->mutual_inductance_nh_per_m  = l_m * 1e9;
    params->capacitive_coupling_coeff   = c_m / c_self;
    params->inductive_coupling_coeff    = l_m / l_self;
    params->backward_crosstalk_coeff    = 0.25 * (l_m / l_self + c_m / c_self);
    params->forward_crosstalk_coeff     = 0.5 * (c_m / c_self - l_m / l_self);
}

/* ================================================================
 * L4: NEXT calculation
 *
 * NEXT is the reverse-traveling crosstalk wave.
 *
 * For short lines (t_rise >> T_prop):
 *   NEXT_peak = K_b × V_step × (2 × T_prop / t_rise)
 *
 * For long lines (t_rise < 2 × T_prop, saturated):
 *   NEXT_max = K_b × V_step
 *
 * The key insight (L4 theorem): NEXT saturates at K_b × V_amplitude
 * for any line longer than the critical length. This is because
 * only the first t_rise/(2×t_pd) of the line contributes to NEXT;
 * beyond that, the backward waves from farther points arrive after
 * the rising edge.
 *
 * Reference: Paul, Eq. 10.33
 * Complexity: O(1)
 * ================================================================ */
void hs_next_calculate(const hs_coupled_pair_t *pair,
                        const hs_coupling_params_t *params,
                        hs_crosstalk_result_t *result)
{
    if (!pair || !params || !result) return;
    memset(result, 0, sizeof(*result));

    double kb = params->backward_crosstalk_coeff;
    double v_amp = pair->aggressor_amplitude_v;

    /* Propagation delay over coupling length */
    double t_pd = sqrt(pair->er_eff) / C0; /* seconds per meter */
    double t_prop = pair->coupling_length_m * t_pd; /* one-way propagation time */
    double t_rise = pair->aggressor_rise_time_s;

    double next_peak;

    if (t_rise > 2.0 * t_prop) {
        /* Short line: NEXT is truncated by rise time */
        next_peak = kb * v_amp * (2.0 * t_prop / t_rise);
    } else {
        /* Long line: NEXT saturates */
        next_peak = kb * v_amp;
    }

    result->next_peak_mv = next_peak * 1000.0;
    result->next_saturated_mv = kb * v_amp * 1000.0;

    double coupling_ratio = pair->coupling_length_m /
                            (t_rise / (2.0 * t_pd));
    result->coupling_length_ratio = coupling_ratio;

    /* NEXT in dB */
    if (v_amp > 0.0 && next_peak > 0.0) {
        result->next_db = 20.0 * log10(next_peak / v_amp);
    } else {
        result->next_db = -200.0; /* negligible */
    }
}

/* ================================================================
 * L4: FEXT calculation
 *
 * FEXT is the forward-traveling crosstalk wave.
 *
 * For a step input:
 *   FEXT(t) ≈ -K_f × V_step × (length / (t_rise × v_p)) × pulse_shape
 *
 * The FEXT pulse width ≈ aggressor rise time.
 * FEXT amplitude is proportional to coupling length.
 *
 * For homogeneous medium (stripline): K_f = 0 → FEXT = 0
 * This is a fundamental result from TEM mode symmetry.
 *
 * For microstrip: K_f < 0 → FEXT is a narrow negative pulse.
 *
 * Reference: Paul, Eq. 10.47-10.49
 * Complexity: O(1)
 * ================================================================ */
void hs_fext_calculate(const hs_coupled_pair_t *pair,
                        const hs_coupling_params_t *params,
                        hs_crosstalk_result_t *result)
{
    if (!pair || !params || !result) return;
    memset(result, 0, sizeof(*result));

    double kf = params->forward_crosstalk_coeff;
    double v_amp = pair->aggressor_amplitude_v;
    double v_p = C0 / sqrt(pair->er_eff);
    double L = pair->coupling_length_m;
    double t_rise = pair->aggressor_rise_time_s;

    double fext_peak;
    if (t_rise > 0.0 && v_p > 0.0) {
        /* FEXT peak = -K_f × V_step × L / (t_rise × v_p) */
        fext_peak = -kf * v_amp * L / (t_rise * v_p);
    } else {
        fext_peak = 0.0;
    }

    result->fext_peak_mv = fext_peak * 1000.0;
    result->fext_pulse_width_ps = t_rise * 1e12;
    result->crosstalk_delay_ps = L / v_p * 1e12;

    if (v_amp > 0.0 && fabs(fext_peak) > 0.0) {
        result->fext_db = 20.0 * log10(fabs(fext_peak) / v_amp);
    } else {
        result->fext_db = -200.0;
    }
}

/* ================================================================
 * L6: Complete pair crosstalk analysis
 *
 * Convenience function combining coupling analysis, NEXT, and FEXT
 * into a single call.
 *
 * Complexity: O(1)
 * ================================================================ */
void hs_crosstalk_analyze_pair(const hs_coupled_pair_t *pair,
                                hs_crosstalk_result_t *result)
{
    if (!pair || !result) return;
    memset(result, 0, sizeof(*result));

    hs_coupling_params_t params;
    hs_coupling_analyze(pair, &params);

    hs_crosstalk_result_t next_res, fext_res;
    hs_next_calculate(pair, &params, &next_res);
    hs_fext_calculate(pair, &params, &fext_res);

    /* Merge results */
    result->next_peak_mv = next_res.next_peak_mv;
    result->next_saturated_mv = next_res.next_saturated_mv;
    result->next_db = next_res.next_db;
    result->fext_peak_mv = fext_res.fext_peak_mv;
    result->fext_db = fext_res.fext_db;
    result->fext_pulse_width_ps = fext_res.fext_pulse_width_ps;
    result->crosstalk_delay_ps = fext_res.crosstalk_delay_ps;
    result->coupling_length_ratio = next_res.coupling_length_ratio;
}

/* ================================================================
 * L5: Power-sum crosstalk
 *
 * For N independent aggressors:
 *   Power-sum NEXT = √(Σ NEXT_i²)   (voltage RMS)
 *   PS_NEXT_dB = 20 log₁₀(√(Σ 10^(NEXT_i/10)))
 *
 * For worst-case coherent (all aggressors switch simultaneously
 * with same polarity):
 *   PS_NEXT_dB = 20 log₁₀(Σ 10^(NEXT_i/20))
 *
 * The difference between coherent and RMS sum can be significant
 * (>6 dB for 4 aggressors), so coherent sum is used for worst-case
 * analysis.
 *
 * Reference: IEEE 802.3an, Clause 55.7.3.3
 * Complexity: O(N)
 * ================================================================ */
double hs_power_sum_crosstalk_db(const double *next_values_db,
                                  int n_aggressors, int coherent)
{
    if (!next_values_db || n_aggressors <= 0) return -200.0;

    double sum = 0.0;

    if (coherent) {
        /* Voltage (linear) sum: Σ 10^(NEXT_dB/20) */
        for (int i = 0; i < n_aggressors; i++) {
            double v = pow(10.0, next_values_db[i] / 20.0);
            sum += v;
        }
    } else {
        /* Power sum: Σ 10^(NEXT_dB/10) */
        for (int i = 0; i < n_aggressors; i++) {
            double p = pow(10.0, next_values_db[i] / 10.0);
            sum += p;
        }
        /* Convert power sum to voltage for dB */
        sum = sqrt(sum);
    }

    if (sum <= 0.0) return -200.0;
    return 20.0 * log10(sum);
}

/* ================================================================
 * L2: 3W rule spacing
 *
 * Center-to-center ≥ 3 × trace width
 * Edge-to-edge ≥ 2 × trace width
 *
 * This rule provides approximately -25 to -30 dB NEXT for typical
 * FR-4 boards at nominal geometries.
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_three_w_rule_spacing(double trace_width_m)
{
    if (trace_width_m <= 0.0) return 0.0;
    return 2.0 * trace_width_m; /* edge-to-edge = center(3w) - 2*(w/2) = 2w */
}

/* ================================================================
 * L5: NEXT estimate from spacing ratio
 *
 * Empirical formula for quick estimation without full analysis.
 *
 * NEXT_dB ≈ NEXT_0 - 20 × log10(1 + s/h) × slope_factor
 *
 * This captures the insight that NEXT improves (becomes more
 * negative) as s/h increases.
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_next_estimate(double spacing_m, double height_m, double er)
{
    (void)er; /* reserved for er-dependent refinement */
    if (height_m <= 0.0) return 0.0;

    double s_over_h = spacing_m / height_m;
    if (s_over_h < 0.1) s_over_h = 0.1;

    /* Base coupling for s/h = 1 is approximately 0.04 to 0.08 */
    double kb_base = 0.06 * exp(-2.0 * (s_over_h - 1.0));

    if (kb_base <= 0.0) return -120.0;
    return 20.0 * log10(kb_base);
}

/* ================================================================
 * L5: Guard trace effectiveness
 *
 * A grounded guard trace placed between aggressor and victim
 * intercepts electric field lines and modifies the return
 * current path, reducing crosstalk.
 *
 * NEXT_reduction ≈ 6 + 20×log10(1 + s_guard_wider / s_pair)
 *
 * Guard trace width, grounding via spacing, and guard-to-trace
 * spacing all affect effectiveness. For maximum effectiveness:
 *   - Via spacing < λ/10 at maximum frequency
 *   - Guard width ≥ aggressor trace width
 *   - Guard must be grounded at both ends
 *
 * Reference: Bogatin, §11.10
 * Complexity: O(1)
 * ================================================================ */
double hs_guard_trace_reduction_db(double guard_width_m,
                                    double guard_to_victim_spacing_m,
                                    double original_spacing_m)
{
    (void)guard_to_victim_spacing_m; /* reserved for future proximity model */
    if (original_spacing_m <= 0.0) return 0.0;

    /* Guard trace halves the effective coupling path length */
    double base_reduction = 6.0; /* dB from halving coupling */

    /* Additional benefit from wider guard */
    double width_benefit = 20.0 * log10(1.0 + guard_width_m / original_spacing_m);

    double total = base_reduction + width_benefit;
    return (total > 0.0) ? total : 0.0;
}

/* ================================================================
 * L6: Differential crosstalk immunity
 *
 * Differential receivers reject common-mode coupled crosstalk.
 *
 * NEXT_diff = NEXT_se - CMRR
 *
 * where CMRR is the receiver's common-mode rejection ratio in dB.
 *
 * Typical CMRR for high-speed differential receivers: 20-40 dB.
 * This means 100 mV of single-ended crosstalk becomes < 1-10 mV
 * of differential noise.
 *
 * This is why differential signaling is used for all high-speed
 * interfaces: PCIe, USB, HDMI, Ethernet, DDR (strobe).
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_diff_crosstalk_immunity(double se_next_db, double cmrr_db)
{
    /* Differential NEXT = single-ended NEXT - CMRR */
    return se_next_db - cmrr_db;
}
