/*
 * hs_crosstalk.h — Crosstalk Analysis for High-Speed PCB Layout
 *
 * Core Definitions (L1):
 *   - Near-end crosstalk (NEXT)
 *   - Far-end crosstalk (FEXT)
 *   - Mutual capacitance C_m
 *   - Mutual inductance L_m
 *   - Backward and forward crosstalk coefficients
 *   - Alien crosstalk
 *   - Power-sum crosstalk
 *
 * Core Concepts (L2):
 *   - Capacitive coupling (electric field)
 *   - Inductive coupling (magnetic field)
 *   - Guard traces and ground fill
 *   - Differential pair crosstalk immunity
 *   - 3W rule and spacing guidelines
 *   - Forward/backward wave superposition
 *
 * Mathematical Structures (L3):
 *   - Coupled transmission line equations (2N-conductor TL)
 *   - Per-unit-length L and C matrices
 *   - Modal decomposition for crosstalk
 *   - S-parameter formulation for multi-conductor lines
 *
 * Fundamental Laws (L4):
 *   - NEXT saturation: NEXT → K_b (constant) for long lines
 *   - FEXT scaling: FEXT ∝ length × frequency for homogeneous medium
 *   - For stripline (homogeneous): K_f = 0 (TEM mode cancellation)
 *   - For microstrip (inhomogeneous): K_f ≠ 0 (non-TEM)
 *
 * References:
 *   - Paul, C.R., "Introduction to Electromagnetic Compatibility", Ch.9-10
 *   - Bogatin, "Signal and Power Integrity Simplified", Ch.11
 *   - Hall & Heck, "Advanced Signal Integrity", Ch.6
 *   - Feller, Kaupp, & Digiacomo, "Crosstalk in Microstrip…", 1977
 */

#ifndef HS_CROSSTALK_H
#define HS_CROSSTALK_H

#include <stddef.h>
#include <stdint.h>

/* ================================================================
 * L1: Core Definitions — Coupling Parameters
 * ================================================================ */

/**
 * Coupled-line pair description.
 * Defines two parallel transmission lines (aggressor + victim) for
 * crosstalk analysis.
 */
typedef struct {
    double trace_width_m;          /* Width of each trace (m) */
    double trace_thickness_m;      /* Copper thickness (m) */
    double edge_spacing_m;         /* Edge-to-edge separation (m) */
    double height_to_plane_m;      /* Dielectric height to reference plane (m) */
    double er_eff;                 /* Effective dielectric constant */
    double z0_ohm;                 /* Characteristic impedance of isolated trace (Ω) */
    double coupling_length_m;      /* Parallel run length (m) */
    double aggressor_rise_time_s;  /* Aggressor signal rise time (s) */
    double aggressor_amplitude_v;  /* Aggressor step amplitude (V) */
} hs_coupled_pair_t;

/**
 * Mutual coupling parameters.
 */
typedef struct {
    double mutual_capacitance_pf_per_m;  /* C_m in pF/m */
    double mutual_inductance_nh_per_m;   /* L_m in nH/m */
    double capacitive_coupling_coeff;     /* k_c = C_m / C_self */
    double inductive_coupling_coeff;      /* k_l = L_m / L_self */
    double backward_crosstalk_coeff;      /* K_b = (k_c + k_l)/4 */
    double forward_crosstalk_coeff;       /* K_f = (k_c - k_l)/2 */
} hs_coupling_params_t;

/**
 * Crosstalk measurement results.
 */
typedef struct {
    double next_peak_mv;          /* Near-end crosstalk peak voltage (mV) */
    double fext_peak_mv;          /* Far-end crosstalk peak voltage (mV) */
    double next_saturated_mv;     /* NEXT saturated amplitude (mV) for long lines */
    double next_db;               /* NEXT in dB: 20×log10(V_next/V_aggressor) */
    double fext_db;               /* FEXT in dB */
    double crosstalk_delay_ps;    /* Time delay to crosstalk peak (ps) */
    double fext_pulse_width_ps;   /* FEXT pulse width (ps) — approx aggressor t_rise */
    double coupling_length_ratio; /* L_coupling / L_critical */
} hs_crosstalk_result_t;

/**
 * Multi-aggressor crosstalk result (power-sum).
 */
typedef struct {
    double power_sum_next_mv;     /* Power-sum NEXT amplitude (mV) */
    double power_sum_fext_mv;     /* Power-sum FEXT amplitude (mV) */
    double worst_case_next_mv;    /* Worst-case single-aggressor NEXT (mV) */
    double worst_case_fext_mv;    /* Worst-case single-aggressor FEXT (mV) */
    int    num_aggressors;        /* Number of aggressors summed */
} hs_multi_crosstalk_result_t;

/* ================================================================
 * L2-L5: Crosstalk Analysis Functions
 * ================================================================ */

/**
 * Compute mutual capacitance per unit length between two parallel microstrip traces.
 *
 * Using the analytical formula for edge-coupled microstrips:
 *
 *   C_m ≈ ε₀ × εr × (0.5 + 0.35 × (w/h)^(-1.5)) × (w/s) × exp(-2.9×s/h)  [F/m]
 *
 * This is valid for s/h > 0.2 and w/h in [0.5, 3.0].
 *
 * Reference: Paul, "Introduction to EMC", §9.2.2; Weeks, "Calculation of
 *   Coefficients of Capacitance...", IEEE Trans. MTT, 1970.
 *
 * @param pair: Coupled pair geometry
 * @return Mutual capacitance in F/m
 */
double hs_mutual_capacitance(const hs_coupled_pair_t *pair);

/**
 * Compute mutual inductance per unit length between two parallel traces.
 *
 * For two traces at height h above a ground plane:
 *
 *   L_m ≈ (μ₀/(2π)) × ln(1 + (2h/s)²)  [H/m]
 *
 * This is the partial mutual inductance with a ground-plane return path.
 * The image current in the ground plane modifies the inductance.
 *
 * Reference: Paul, "Inductance: Loop and Partial", 2010, §4.5;
 *   Grover, "Inductance Calculations", 1946.
 *
 * @param pair: Coupled pair geometry
 * @return Mutual inductance in H/m
 */
double hs_mutual_inductance(const hs_coupled_pair_t *pair);

/**
 * Compute the self-capacitance per unit length of a microstrip trace.
 *
 * C_self = εeff / (c₀ × Z₀)  [F/m]
 *
 * @param z0: Characteristic impedance (Ω)
 * @param er_eff: Effective dielectric constant
 * @return Self-capacitance in F/m
 */
double hs_self_capacitance(double z0, double er_eff);

/**
 * Compute the self-inductance per unit length of a microstrip trace.
 *
 * L_self = Z₀ × √εeff / c₀  [H/m]
 * or equivalently: L_self = Z₀² × C_self
 *
 * @param z0: Characteristic impedance (Ω)
 * @param er_eff: Effective dielectric constant
 * @return Self-inductance in H/m
 */
double hs_self_inductance(double z0, double er_eff);

/**
 * Compute the full coupling parameters (C_m, L_m, K_b, K_f) for a pair.
 *
 * This is the main entry point for crosstalk characterization.
 *
 * Backward crosstalk coefficient K_b:
 *   K_b = (1/4) × (L_m/L_self + C_m/C_self)   (dimensionless)
 *
 * Forward crosstalk coefficient K_f:
 *   K_f = (1/2) × (C_m/C_self - L_m/L_self)   (dimensionless)
 *
 * For homogeneous medium (stripline): C_m/C_self = L_m/L_self → K_f = 0
 * For inhomogeneous (microstrip): K_f < 0 typically
 *
 * Reference: Feller et al., 1977; Paul §10.3
 *
 * @param pair: Coupled pair geometry
 * @param params: Output coupling parameters
 */
void hs_coupling_analyze(const hs_coupled_pair_t *pair,
                          hs_coupling_params_t *params);

/**
 * Compute near-end crosstalk (NEXT) for a given pair.
 *
 * Theory:
 *   For short lines (t_rise >> 2×t_pd×L):
 *     NEXT(t) ≈ K_b × V_aggressor × (t / t_rise)  [ramping up]
 *     NEXT_peak ≈ K_b × V_aggressor × (2×t_pd×L / t_rise)
 *
 *   For long lines (t_rise < 2×t_pd×L, saturated):
 *     NEXT_max ≈ K_b × V_aggressor  [constant, independent of length]
 *
 *   NEXT in dB:
 *     NEXT_dB = 20 × log10(K_b)
 *
 * Key insight (L4): NEXT saturates at K_b × V_amplitude for lines
 *   longer than the critical length. This is a fundamental limit.
 *
 * Reference: Paul §10.2.2, Eq. 10.33
 *
 * @param pair: Coupled pair geometry
 * @param params: Pre-computed coupling parameters
 * @param result: Output crosstalk result
 */
void hs_next_calculate(const hs_coupled_pair_t *pair,
                        const hs_coupling_params_t *params,
                        hs_crosstalk_result_t *result);

/**
 * Compute far-end crosstalk (FEXT) for a given pair.
 *
 * Theory:
 *   FEXT(t) ≈ K_f × V_aggressor × (L × t_pd / t_rise) × time-derivative
 *
 *   FEXT_peak ≈ K_f × V_aggressor × (L / (t_rise × v_p))
 *
 *   FEXT pulse is a differentiated version of aggressor edge,
 *   with amplitude proportional to L and inversely proportional to t_rise.
 *
 * For stripline (homogeneous): K_f = 0, FEXT ≈ 0 (TEM symmetry)
 * For microstrip: K_f < 0, FEXT is a negative-going narrow pulse
 *
 * Reference: Paul §10.3, Eq. 10.47-10.49
 *
 * @param pair: Coupled pair geometry
 * @param params: Pre-computed coupling parameters
 * @param result: Output crosstalk result
 */
void hs_fext_calculate(const hs_coupled_pair_t *pair,
                        const hs_coupling_params_t *params,
                        hs_crosstalk_result_t *result);

/**
 * Complete crosstalk analysis for a coupled pair (NEXT + FEXT).
 *
 * @param pair: Coupled pair geometry
 * @param result: Output with both NEXT and FEXT
 */
void hs_crosstalk_analyze_pair(const hs_coupled_pair_t *pair,
                                hs_crosstalk_result_t *result);

/**
 * Compute power-sum crosstalk from multiple aggressors.
 *
 * Power-sum NEXT = 20 × log10( Σ 10^(NEXT_i/20) )
 * (Voltage sum, for phase-aligned worst case)
 *
 * For multiple random-phase aggressors:
 *   Power-sum NEXT = 20 × log10( √(Σ 10^(NEXT_i/10)) )  (RMS sum)
 *
 * Reference: TIA/EIA-568-B.2, Annex E; IEEE 802.3 Clause 55.7
 *
 * @param next_values_db: Array of NEXT in dB for each aggressor
 * @param n_aggressors: Number of aggressors
 * @param coherent: 1 for worst-case coherent sum, 0 for RMS
 * @return Power-sum NEXT in dB
 */
double hs_power_sum_crosstalk_db(const double *next_values_db,
                                  int n_aggressors, int coherent);

/**
 * Compute the 3W rule spacing for a given trace width.
 *
 * 3W Rule (L2): center-to-center spacing ≥ 3× trace width.
 * For a typical 5-mil trace: spacing ≥ 15 mils between centers,
 * edge-to-edge spacing ≥ 10 mils (for w=5).
 *
 * This rule limits NEXT to approximately -25 to -30 dB for FR-4.
 *
 * @param trace_width_m: Trace width in m
 * @return Recommended edge-to-edge spacing (m)
 */
double hs_three_w_rule_spacing(double trace_width_m);

/**
 * Compute the NEXT based on spacing ratio (s/h) — approximate formula.
 *
 * NEXT_dB ≈ NEXT_ref - log10(s/h) × NEXT_slope
 *
 * This is a simplified design-estimation formula.
 * Typical: NEXT_ref ≈ -10 dB, NEXT_slope ≈ 15-20 dB/decade of s/h.
 *
 * @param spacing_m: Edge-to-edge spacing (m)
 * @param height_m: Height above reference plane (m)
 * @param er: Dielectric constant
 * @return Estimated NEXT in dB
 */
double hs_next_estimate(double spacing_m, double height_m, double er);

/**
 * Calculate guard trace effectiveness.
 *
 * A guard trace placed between aggressor and victim, grounded at
 * regular intervals, reduces crosstalk by 6-12 dB.
 *
 * Effectiveness depends on:
 *   - Guard trace width (wider = better)
 *   - Via spacing (closer = better, ideal < 1/10 × λ)
 *   - Guard-to-trace spacing (closer = better)
 *
 * Simple model: NEXT_reduction_dB ≈ 6 + 20 × log10(1 + s_guard/s_pair)
 *
 * @param guard_width_m: Guard trace width (m)
 * @param guard_to_victim_spacing_m: Space from guard to victim (m)
 * @param original_spacing_m: Original agg-to-victim spacing (m)
 * @return Expected NEXT reduction in dB (> 0 means improvement)
 */
double hs_guard_trace_reduction_db(double guard_width_m,
                                    double guard_to_victim_spacing_m,
                                    double original_spacing_m);

/**
 * Compute differential pair crosstalk immunity, comparing to single-ended.
 *
 * Differential signaling provides 20-30 dB additional crosstalk
 * immunity compared to single-ended signaling at the same spacing.
 *
 * CMRR (Common-Mode Rejection Ratio) contribution:
 *   For perfectly balanced differential pair, NEXT_CM couples equally,
 *   and the differential receiver rejects it:
 *
 *   NEXT_diff_dB = NEXT_se_dB - CMRR_dB - 3 dB
 *
 * @param se_next_db: Single-ended NEXT in dB (negative, e.g., -25 dB)
 * @param cmrr_db: Receiver common-mode rejection ratio in dB
 * @return Differential NEXT in dB (more negative)
 */
double hs_diff_crosstalk_immunity(double se_next_db, double cmrr_db);

#endif /* HS_CROSSTALK_H */
