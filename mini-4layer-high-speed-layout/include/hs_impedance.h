/*
 * hs_impedance.h — Characteristic & Differential Impedance Calculation
 *
 * Core Definitions (L1):
 *   - Characteristic impedance Z₀
 *   - Differential impedance Z_diff
 *   - Common-mode impedance Z_cm
 *   - Odd-mode impedance Z_odd
 *   - Even-mode impedance Z_even
 *   - Reflection coefficient Γ
 *   - VSWR
 *
 * Core Concepts (L2):
 *   - Impedance matching (source, load, termination)
 *   - Microstrip vs stripline geometry
 *   - Single-ended vs differential routing
 *   - Controlled impedance manufacturing tolerances
 *
 * Mathematical Structures (L3):
 *   - Conformal mapping for microstrip
 *   - Transmission line impedance matrix
 *   - Mode decomposition (even/odd)
 *
 * Fundamental Laws (L4):
 *   - Wheeler's microstrip impedance formula
 *   - IPC-2141 impedance equations
 *   - Hammerstad-Jensen microstrip model
 *   - Cohn's stripline equations
 *
 * References:
 *   - IPC-2141A, "Design Guide for High-Speed Controlled Impedance Circuit Boards"
 *   - Wheeler, H.A., "Transmission-line properties of parallel strips…", MTT 1965
 *   - Hammerstad & Jensen, "Accurate Models for Microstrip…", MTT-S 1980
 *   - Bogatin, "Signal and Power Integrity Simplified", Ch.10
 */

#ifndef HS_IMPEDANCE_H
#define HS_IMPEDANCE_H

#include <stddef.h>

/* ================================================================
 * L1: Core Definitions — Impedance Types and Parameters
 * ================================================================ */

typedef enum {
    HS_TERM_NONE        = 0,  /* No termination */
    HS_TERM_SERIES      = 1,  /* Series termination at source */
    HS_TERM_PARALLEL     = 2,  /* Parallel termination at load */
    HS_TERM_THEVENIN     = 3,  /* Thevenin termination (split resistors) */
    HS_TERM_AC           = 4,  /* AC termination (R+C to ground) */
    HS_TERM_DIODE        = 5   /* Diode clamp termination */
} hs_termination_type_t;

typedef enum {
    HS_TLINE_MICROSTRIP       = 0,  /* Surface microstrip (1 ref plane) */
    HS_TLINE_EMBEDDED_MICRO   = 1,  /* Embedded microstrip (coated) */
    HS_TLINE_STRIPLINE_SYM     = 2,  /* Symmetric stripline (2 ref planes, centered) */
    HS_TLINE_STRIPLINE_ASYM    = 3,  /* Asymmetric stripline (2 ref planes, offset) */
    HS_TLINE_CPW               = 4,  /* Coplanar waveguide (signal + coplanar GND) */
    HS_TLINE_CPWG              = 5   /* Grounded CPW (CPW + bottom ground) */
} hs_tline_type_t;

typedef enum {
    HS_DIFF_EDGE_COUPLED    = 0,  /* Edge-coupled differential (side-by-side) */
    HS_DIFF_BROADSIDE       = 1   /* Broadside-coupled differential (stacked) */
} hs_diff_config_t;

/**
 * Result structure for impedance calculations.
 * Bundles all impedance-related quantities for a given geometry.
 *
 * Single-ended impedance Z₀: the ratio of voltage to current for a wave
 *   propagating on a transmission line. Determines reflection amplitude.
 *
 * Differential impedance Z_diff: impedance seen by a purely differential
 *   signal (V_diff / I_diff). For loosely coupled lines, Z_diff ≈ 2×Z_odd.
 */
typedef struct {
    double z0_single;        /* Single-ended characteristic impedance (Ω) */
    double z0_diff;          /* Differential impedance (Ω) */
    double z0_odd;           /* Odd-mode impedance (Ω) */
    double z0_even;          /* Even-mode impedance (Ω) */
    double z0_common;        /* Common-mode impedance = Z_even/2 (Ω) */
    double epsilon_eff;      /* Effective dielectric constant */
    double phase_velocity;   /* Phase velocity (m/s) */
    double delay_ps_per_mm;  /* Propagation delay (ps/mm) */
    double capacitance_pf_per_m; /* Capacitance per unit length (pF/m) */
    double inductance_nh_per_m;  /* Inductance per unit length (nH/m) */
} hs_impedance_result_t;

/**
 * Geometry parameters for impedance calculation.
 *
 * All dimensions in meters for internal consistency.
 */
typedef struct {
    double trace_width;          /* w: Trace width (m) */
    double trace_thickness;      /* t: Copper thickness (m) */
    double dielectric_height;    /* h: Dielectric thickness to reference plane (m) */
    double dielectric_height2;   /* h2: Upper dielectric for stripline/asymmetric (m) */
    double dielectric_constant;  /* εr: Relative permittivity */
    double spacing;              /* s: Edge-to-edge spacing for differential (m) */
    double coating_thickness;    /* For embedded microstrip (m, 0 = no coating) */
    double coating_er;           /* εr of solder mask / coating */
} hs_impedance_geometry_t;

/* ================================================================
 * L4-L5: Impedance Calculation Functions (IPC-2141 + Hammerstad)
 *
 * Each function implements a specific transmission line geometry
 * with its corresponding analytical formula from the literature.
 * ================================================================ */

/**
 * Surface microstrip characteristic impedance (IPC-2141).
 *
 * Formula:
 *   For w/h ≤ 1:
 *     Z₀ = (87 / √(εr + 1.41)) × ln(5.98 × h / (0.8w + t))
 *   For w/h > 1:
 *     Z₀ = (120π / √εeff) / (w/h + 1.393 + 0.667×ln(w/h+1.444))
 *
 * This is the most widely used formula in PCB industry.
 * Accuracy: typically ±5% for 0.1 < w/h < 3.0 and εr < 15
 *
 * Reference: IPC-2141A, §4.2.2.1
 *
 * @param geo: Geometry parameters
 * @param result: Output impedance result (filled on success)
 * @return 0 on success, -1 on invalid geometry
 */
int hs_microstrip_impedance(const hs_impedance_geometry_t *geo,
                             hs_impedance_result_t *result);

/**
 * Surface microstrip impedance (Hammerstad-Jensen model).
 *
 * More accurate than IPC-2141, includes thickness correction.
 *
 * Formula:
 *   εeff = (εr+1)/2 + (εr-1)/2 × (1 + 10h/w)^(-ab)
 *   where a and b are empirical terms incorporating t/h effects.
 *
 *   Z₀ = (60/√εeff) × ln(F × h/w + √(1 + (2h/w)²))
 *   where F = 6 + (2π-6)×exp(-(30.666×h/w)^0.7528)
 *
 * Accuracy: ±0.2% for 0.01 ≤ w/h ≤ 100, εr ≤ 128
 *
 * Reference: E.O. Hammerstad and O. Jensen, "Accurate Models for
 *   Microstrip Computer-Aided Design", IEEE MTT-S Digest, 1980
 *
 * @param geo: Geometry parameters
 * @param result: Output impedance result
 * @return 0 on success, -1 on invalid geometry
 */
int hs_microstrip_impedance_hj(const hs_impedance_geometry_t *geo,
                                hs_impedance_result_t *result);

/**
 * Symmetric stripline characteristic impedance (Cohn's model).
 *
 * The trace is centered between two reference planes, each at
 * distance h from the trace. The total dielectric thickness is 2h + t.
 *
 * Formula (for w/(h-t) < 0.35, thin strip approximation):
 *   Z₀ = (60/√εr) × ln(4h / (π × (0.8w + t)))
 *
 * General formula (Cohn, 1955):
 *   Z₀ = (60/√εr) × ln(4h / (π × d))
 *   where d = w for thin strip; more generally:
 *
 *   Z₀ = (η₀/(4√εr)) × K(k)/K'(k)
 *   where K(k) is the complete elliptic integral of the first kind.
 *
 * Reference: S.B. Cohn, "Problems in Strip Transmission Lines",
 *   IRE Trans. MTT, 1955
 *
 * @param geo: Geometry (uses dielectric_height as half-separation)
 * @param result: Output impedance result
 * @return 0 on success, -1 on invalid geometry
 */
int hs_stripline_impedance(const hs_impedance_geometry_t *geo,
                            hs_impedance_result_t *result);

/**
 * Asymmetric stripline impedance.
 *
 * Trace is offset between two reference planes at distances h1 and h2.
 * Effective height: h_eff = 2 × h1 × h2 / (h1 + h2)
 *
 * @param geo: Geometry (dielectric_height = h1, dielectric_height2 = h2)
 * @param result: Output impedance result
 * @return 0 on success
 */
int hs_stripline_asymmetric_impedance(const hs_impedance_geometry_t *geo,
                                       hs_impedance_result_t *result);

/**
 * Edge-coupled differential microstrip impedance.
 *
 * Two identical microstrip traces with edge-to-edge spacing s.
 * Uses odd-mode and even-mode decomposition.
 *
 * Odd-mode (differential signal):
 *   Z_odd is computed from the single-ended impedance modified by
 *   the coupling factor. For closely-spaced traces, Z_odd < Z₀_single.
 *
 * Even-mode (common signal):
 *   Z_even > Z₀_single due to reduced fringe fields.
 *
 * Differential impedance: Z_diff = 2 × Z_odd × (1 - k²)^(-1/2)
 * where k is the coupling coefficient.
 *
 * Simplified IPC-2141 formula:
 *   Z_diff ≈ 2 × Z₀ × (1 - 0.48 × exp(-0.96 × s/h))
 *   where Z₀ is the single-ended impedance of one trace in isolation.
 *
 * Reference: IPC-2141A, §4.2.4; Bogatin Ch.10
 *
 * @param geo: Geometry (must set spacing > 0 for differential)
 * @param result: Output impedance result
 * @return 0 on success
 */
int hs_diff_microstrip_impedance(const hs_impedance_geometry_t *geo,
                                  hs_impedance_result_t *result);

/**
 * Edge-coupled differential stripline impedance.
 *
 * For symmetric stripline, the differential impedance is:
 *   Z_diff = 2 × Z_odd
 * where Z_odd is calculated with modified geometry accounting for coupling.
 *
 * Reference: IPC-2141A, §4.2.4.2; Cohn 1955
 *
 * @param geo: Geometry
 * @param result: Output impedance result
 * @return 0 on success
 */
int hs_diff_stripline_impedance(const hs_impedance_geometry_t *geo,
                                 hs_impedance_result_t *result);

/**
 * Coplanar waveguide (CPW) impedance.
 *
 * Signal trace with coplanar ground planes on either side,
 * without a bottom ground plane.
 *
 * Formula:
 *   k = w / (w + 2s)
 *   k' = √(1 - k²)
 *   Z₀ = (30π/√εeff) × K(k')/K(k)
 *   εeff = 1 + (εr-1)/2 × K(k')K(k₁)/K(k)K(k₁')
 *
 * Reference: Simons, "Coplanar Waveguide Circuits", 2001
 *
 * @param geo: Geometry (trace_width=w, spacing=gap to side grounds)
 * @param result: Output impedance result
 * @return 0 on success
 */
int hs_cpw_impedance(const hs_impedance_geometry_t *geo,
                      hs_impedance_result_t *result);

/**
 * Compute the reflection coefficient at an impedance discontinuity.
 *
 * Fundamental Law (L4):
 *   Γ = (Z_L - Z₀) / (Z_L + Z₀)
 *
 * where Z_L is the load impedance, Z₀ is the characteristic impedance.
 * |Γ|² is the fraction of incident power reflected.
 *
 * Properties:
 *   Γ = 0  → perfect match (Z_L = Z₀)
 *   Γ = -1 → short circuit (Z_L = 0)
 *   Γ = +1 → open circuit (Z_L = ∞)
 *
 * @param z_load: Load impedance (Ω)
 * @param z0: Characteristic impedance (Ω, > 0)
 * @return Reflection coefficient (complex magnitude ≤ 1)
 */
double hs_reflection_coefficient(double z_load, double z0);

/**
 * Compute VSWR from reflection coefficient.
 *
 * VSWR = (1 + |Γ|) / (1 - |Γ|)
 *
 * VSWR = 1.0 → perfect match
 * VSWR = 1.5 → |Γ| = 0.2 → 4% power reflected (typical good match)
 * VSWR = 2.0 → |Γ| = 0.333 → 11% power reflected
 * VSWR = ∞ → total reflection
 *
 * @param gamma_magnitude: Absolute value of reflection coefficient [0, 1]
 * @return VSWR [1, ∞)
 */
double hs_vswr(double gamma_magnitude);

/**
 * Compute return loss (RL) from reflection coefficient.
 *
 * RL = -20 × log₁₀(|Γ|)  [dB]
 *
 * RL > 20 dB → good match
 * RL > 10 dB → acceptable
 * RL < 6 dB  → poor match
 *
 * @param gamma_magnitude: |Γ| in [0, 1]
 * @return Return loss in dB (non-negative)
 */
double hs_return_loss_db(double gamma_magnitude);

/**
 * Compute insertion loss precursor: mismatch loss at a single interface.
 *
 * ML = -10 × log₁₀(1 - |Γ|²)  [dB]
 *
 * This is the power lost due to reflection at an impedance discontinuity,
 * not including dielectric or conductor loss.
 *
 * @param gamma_magnitude: |Γ| in [0, 1]
 * @return Mismatch loss in dB
 */
double hs_mismatch_loss_db(double gamma_magnitude);

/**
 * Compute the required trace width to achieve a target impedance.
 *
 * Uses iterative numerical refinement (Newton-Raphson on the
 * Hammerstad-Jensen microstrip model).
 *
 * Convergence is quadratic near the root. Initial guess from IPC-2141.
 *
 * Complexity: O(log(1/ε)) iterations for tolerance ε
 *
 * @param target_z0: Desired characteristic impedance (Ω)
 * @param geo: Initial geometry (trace_width is overwritten with result)
 * @param result: Filled with final impedance parameters
 * @return 0 on success, -1 if no solution exists
 */
int hs_solve_trace_width(double target_z0, hs_impedance_geometry_t *geo,
                          hs_impedance_result_t *result);

/**
 * Apply solder mask (coating) correction to effective εr.
 *
 * Solder mask (typical εr ≈ 3.5-4.0, thickness 0.5-1.5 mils)
 * increases the effective dielectric constant and decreases Z₀
 * by 0.5-2 Ω for typical microstrips.
 *
 * Correction factor:
 *   Δεeff = (εr_coat - 1) × 2t_coat/h × exp(-1.5×w/h)
 *
 * Reference: IPC-2141A, Appendix
 *
 * @param geo: Geometry with coating parameters set
 * @param base_er_eff: Effective εr without coating
 * @return Corrected effective εr
 */
double hs_solder_mask_er_correction(const hs_impedance_geometry_t *geo,
                                     double base_er_eff);

/**
 * Compute the coupling coefficient k between two adjacent traces.
 *
 * k = (Z_even - Z_odd) / (Z_even + Z_odd)
 *
 * k = 0 → uncoupled (isolated traces)
 * k ≈ 0.05 → loosely coupled (s > 3w)
 * k ≈ 0.2 → coupled (s ≈ w)
 * k ≈ 0.4 → strongly coupled (s ≈ 0.3w)
 *
 * @param z_even: Even-mode impedance (Ω)
 * @param z_odd: Odd-mode impedance (Ω)
 * @return Coupling coefficient [0, 1)
 */
double hs_coupling_coefficient(double z_even, double z_odd);

/**
 * Compute the minimum trace spacing to achieve a target coupling level.
 *
 * For edge-coupled microstrip:
 *   s_min ≈ h × ln(2/(1 - exp(-(k_target × 2.4)))) / 1.8
 *
 * This is an approximate inverse model for quick spacing estimation.
 *
 * @param target_coupling: Maximum allowed coupling coefficient
 * @param height_m: Dielectric height to reference plane
 * @return Minimum spacing in meters
 */
double hs_min_spacing_for_coupling(double target_coupling, double height_m);

#endif /* HS_IMPEDANCE_H */
