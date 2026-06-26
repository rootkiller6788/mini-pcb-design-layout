/*
 * hs_pdn.h — Power Distribution Network Design for 4-Layer PCBs
 *
 * Core Definitions (L1):
 *   - Power Distribution Network (PDN)
 *   - Target impedance Z_target
 *   - Decoupling capacitor (decap)
 *   - ESR (Equivalent Series Resistance)
 *   - ESL (Equivalent Series Inductance)
 *   - Plane resonance
 *   - Simultaneous Switching Noise (SSN)
 *   - DC IR drop
 *
 * Core Concepts (L2):
 *   - PDN impedance profile vs frequency
 *   - VRM (Voltage Regulator Module) bandwidth
 *   - Decoupling hierarchy: VRM → bulk → ceramic → on-package
 *   - Plane pair capacitance
 *   - Loop inductance minimization
 *   - Anti-resonance peaks
 *
 * Mathematical Structures (L3):
 *   - RLC resonator model for planes
 *   - Impedance matrix for plane pair
 *   - FFT-based SSN current spectrum
 *   - Eigenmode analysis for plane resonances
 *   - Optimization of decoupling network (cost/performance)
 *
 * Fundamental Laws (L4):
 *   - Target impedance: Z_target = ΔV / I_transient
 *   - Plane capacitance: C_plane = ε₀εr × A / d
 *   - Decap impedance: Z(f) = ESR + jωESL + 1/(jωC)
 *   - Parallel resonance: anti-resonance at f where |Z_decap| = |Z_plane|
 *
 * References:
 *   - Smith, Anderson, et al., "Power Distribution System Design...", 1999
 *   - Novak, "Power Distribution Network Design Methodologies", 2008
 *   - Bogatin, "Signal and Power Integrity Simplified", Ch.9
 *   - Hall & Heck, "Advanced Signal Integrity", Ch.7
 */

#ifndef HS_PDN_H
#define HS_PDN_H

#include <stddef.h>
#include <stdint.h>

/* ================================================================
 * L1: Core Definitions — PDN Components
 * ================================================================ */

/** Decoupling capacitor type */
typedef enum {
    HS_CAP_CERAMIC_X5R  = 0,  /* X5R MLCC: good temp stability, moderate cost */
    HS_CAP_CERAMIC_X7R  = 1,  /* X7R MLCC: wide temp range, common for decoupling */
    HS_CAP_CERAMIC_NP0  = 2,  /* NP0/C0G: ultra-stable, low capacitance only */
    HS_CAP_TANTALUM     = 3,  /* Tantalum: high C, moderate ESR, bulk */
    HS_CAP_ALUM_ELEC    = 4,  /* Aluminum electrolytic: very high C, high ESR, VRM bulk */
    HS_CAP_POLYMER      = 5,  /* Polymer: low ESR, high C, good for mid-frequency */
    HS_CAP_REVERSE_GEOM = 6,  /* Reverse-geometry MLCC: ultra-low ESL */
    HS_CAP_3TERM        = 7   /* 3-terminal / feedthrough: lowest ESL */
} hs_capacitor_type_t;

/**
 * Single decoupling capacitor model.
 *
 * The RLC model is valid up to the first series resonance (SRF).
 * Above SRF, the ESL dominates and the capacitor behaves inductively.
 *
 * For MLCC in 0402 package at typical via fanout:
 *   C = 100 nF, ESR ≈ 10 mΩ, ESL ≈ 0.5 nH
 *   SRF = 1/(2π√(LC)) ≈ 22.5 MHz
 */
typedef struct {
    hs_capacitor_type_t type;
    double capacitance_f;         /* Nominal capacitance (F) */
    double esr_ohm;               /* Equivalent Series Resistance (Ω) */
    double esl_h;                 /* Equivalent Series Inductance (H) */
    double leakage_current_a;     /* DC leakage current (A) */
    double rated_voltage_v;       /* Maximum working voltage (V) */
    double temperature_coeff;     /* ΔC/C per °C */
    char   package_code[8];       /* e.g., "0402", "0603", "0805" */
} hs_decap_t;

/**
 * Voltage Regulator Module (VRM) model.
 *
 * VRM regulates input voltage to a stable output, but only up to
 * its control-loop bandwidth (~10 kHz-1 MHz depending on switching f).
 */
typedef struct {
    double output_voltage_v;      /* Regulated output voltage (V) */
    double max_current_a;         /* Maximum output current (A) */
    double bandwidth_hz;          /* Control-loop bandwidth (Hz) */
    double output_impedance_ohm;  /* DC output impedance (Ω) */
    double efficiency;            /* Efficiency at rated load [0, 1] */
} hs_vrm_t;

/**
 * Plane pair geometry for PDN analysis.
 */
typedef struct {
    double plane_width_m;         /* Width of overlapping area (m) */
    double plane_height_m;        /* Height of overlapping area (m) */
    double separation_m;          /* Dielectric thickness between planes (m) */
    double copper_thickness_m;    /* Thickness of each plane (m) */
    double dielectric_er;         /* Relative permittivity */
    double dielectric_tan_d;      /* Loss tangent */
    double copper_conductivity;   /* Conductivity of plane copper (S/m) */
} hs_plane_pair_t;

/**
 * PDN impedance profile point.
 */
typedef struct {
    double frequency_hz;
    double impedance_ohm;
    double phase_deg;
} hs_pdn_impedance_point_t;

/**
 * PDN analysis result summary.
 */
typedef struct {
    double target_impedance_ohm;         /* Target impedance (Ω) */
    double dc_voltage_drop_v;            /* DC IR drop (V) */
    double dc_voltage_drop_percent;      /* DC IR drop as % of nominal */
    double max_impedance_ohm;            /* Maximum impedance in target range */
    double max_impedance_freq_hz;        /* Frequency of max impedance (Hz) */
    double plane_capacitance_nf;         /* Inter-plane capacitance (nF) */
    double first_plane_resonance_hz;     /* First plane cavity resonance (Hz) */
    double ssn_voltage_mv;               /* Estimated SSN amplitude (mV) */
    int    num_decaps_required;          /* Decaps needed to meet Z_target */
    int    is_compliant;                 /* 1 if PDN meets Z_target, 0 otherwise */
} hs_pdn_result_t;

/**
 * Decoupling network specification.
 */
typedef struct {
    hs_vrm_t      vrm;
    hs_decap_t   *bulk_caps;             /* Bulk decoupling capacitors array */
    int           num_bulk;
    hs_decap_t   *ceramic_caps;          /* Ceramic decoupling capacitors array */
    int           num_ceramic;
    hs_plane_pair_t plane;
    hs_decap_t   *on_board_caps;         /* High-frequency on-board decaps */
    int           num_onboard;
} hs_pdn_network_t;

/* ================================================================
 * L2-L5: PDN Analysis Functions
 * ================================================================ */

/**
 * Compute the target impedance for a PDN.
 *
 * Z_target = ΔV_allowable × V_nominal / I_max
 * or more precisely:
 * Z_target = (V_nominal × ripple_tolerance) / (I_transient × factor)
 *
 * For a 1.0V rail with 5% ripple, 2A transient:
 *   Z_target = 0.05 / 2.0 = 0.025 Ω = 25 mΩ
 *   With 50% margin: Z_target = 12.5 mΩ
 *
 * Reference: Smith et al., "Power Distribution System Design...", 1999
 *
 * @param nominal_voltage_v: Nominal rail voltage (V)
 * @param ripple_tolerance: Allowed ripple as fraction (e.g., 0.05 = 5%)
 * @param transient_current_a: Worst-case load step (A)
 * @param margin_factor: Safety margin (> 1.0 adds margin)
 * @return Target impedance in Ω
 */
double hs_pdn_target_impedance(double nominal_voltage_v,
                                double ripple_tolerance,
                                double transient_current_a,
                                double margin_factor);

/**
 * Compute the self-resonant frequency (SRF) of a decoupling capacitor.
 *
 * SRF = 1 / (2π × √(ESL × C))
 *
 * Below SRF: capacitor is capacitive (Z ∝ 1/f)
 * At SRF:     impedance = ESR (minimum)
 * Above SRF:  capacitor is inductive (Z ∝ f)
 *
 * @param decap: Decoupling capacitor
 * @return Self-resonant frequency in Hz
 */
double hs_decap_srf(const hs_decap_t *decap);

/**
 * Compute decoupling capacitor impedance at a given frequency.
 *
 * Z(f) = ESR + j(ω × ESL - 1/(ω × C))
 * |Z(f)| = √(ESR² + (ωL - 1/(ωC))²)
 *
 * @param decap: Decoupling capacitor
 * @param frequency_hz: Frequency (Hz)
 * @return Impedance magnitude in Ω
 */
double hs_decap_impedance(const hs_decap_t *decap, double frequency_hz);

/**
 * Compute inter-plane capacitance from plane pair geometry.
 *
 * C_plane = ε₀ × εr × A / d  [F]
 *
 * For a 100×100 mm board with FR-4 (εr=4.2), d=0.25 mm (10 mil):
 *   C_plane ≈ 8.85e-12 × 4.2 × 0.01 / 2.5e-4 ≈ 1.49 nF
 *
 * This is the high-frequency capacitance available directly
 * between the power and ground planes.
 *
 * @param plane: Plane pair geometry
 * @return Inter-plane capacitance in F
 */
double hs_plane_pair_capacitance(const hs_plane_pair_t *plane);

/**
 * Compute plane spreading inductance.
 *
 * L_spread ≈ μ₀ × d / 2π × ln(w_power / w_contact)  [H]
 *
 * where d is plane separation, w_power is the effective width
 * of the power plane, w_contact is the contact region width.
 *
 * Reference: Novak §3.4; Bogatin Ch.9
 *
 * @param plane: Plane pair geometry
 * @param contact_width_m: Width of VRM contact region (m)
 * @return Spreading inductance in H
 */
double hs_plane_spreading_inductance(const hs_plane_pair_t *plane,
                                      double contact_width_m);

/**
 * Estimate the first plane cavity resonance frequency.
 *
 * For a rectangular plane pair with perfect magnetic wall boundaries:
 *   f_mn = (c₀/(2√εr)) × √((m/a)² + (n/b)²)
 *
 * Lowest mode (m=1, n=0 or m=0, n=1 depending on aspect ratio):
 *   f_10 = c₀/(2a√εr)
 *
 * For a 100×100 mm board with εr=4.2:
 *   f_10 = 3e8/(2×0.1×√4.2) ≈ 732 MHz
 *
 * Reference: Novak §4.3; Lei et al., "Power Distribution Noise
 *   Suppression...", IEEE Trans. ADVP, 2006
 *
 * @param plane: Plane pair geometry
 * @return Lowest cavity resonance frequency in Hz
 */
double hs_plane_resonance_frequency(const hs_plane_pair_t *plane);

/**
 * Compute the DC IR drop across a plane.
 *
 * R_dc = ρ × L / (w × t)  — for a rectangular section
 *
 * V_drop = I_max × R_dc  [V]
 *
 * For a 1 oz Cu plane (t=35 μm), 100 mm long, 50 mm wide,
 * carrying 5A:
 *   R_dc ≈ 1.72e-8 × 0.1 / (0.05 × 3.5e-5) ≈ 0.98 mΩ
 *   V_drop ≈ 4.9 mV (negligible)
 *
 * But for narrow necks in split planes, V_drop can be significant.
 *
 * @param plane: Plane pair geometry
 * @param current_a: Total DC current through plane (A)
 * @param plane_type: 0=power plane, 1=ground plane
 * @return IR drop in V
 */
double hs_plane_ir_drop(const hs_plane_pair_t *plane, double current_a,
                         int plane_type);

/**
 * Compute plane impedance at a given frequency.
 *
 * Includes both capacitive (C_plane) and inductive (L_spread)
 * effects, plus plane losses.
 *
 * Z_plane(f) = √(R_plane² + (ω×L_spread - 1/(ω×C_plane))²)
 *
 * @param plane: Plane pair geometry
 * @param frequency_hz: Frequency (Hz)
 * @param contact_width_m: VRM contact width for L_spread (m)
 * @return Plane impedance magnitude in Ω
 */
double hs_plane_impedance(const hs_plane_pair_t *plane,
                           double frequency_hz, double contact_width_m);

/**
 * Compute the combined PDN impedance at a frequency, including
 * VRM, inter-plane capacitance, and decoupling capacitors.
 *
 * Z_total(f) = 1 / (1/Z_vrm(f) + 1/Z_plane(f) + Σ(1/Z_decap_i(f)))
 *
 * All impedances combine in parallel.
 *
 * @param network: PDN network specification
 * @param frequency_hz: Frequency (Hz)
 * @param contact_width_m: VRM contact width (m)
 * @return Total PDN impedance in Ω
 */
double hs_pdn_total_impedance(const hs_pdn_network_t *network,
                               double frequency_hz, double contact_width_m);

/**
 * Analyze PDN impedance profile across a frequency range.
 *
 * Sweeps from DC to max_freq_hz and computes impedance at each point.
 * Identifies anti-resonance peaks and the maximum impedance.
 *
 * Algorithm: O(N_freqs × N_decaps) frequency sweep
 *
 * @param network: PDN network
 * @param f_min_hz: Start frequency
 * @param f_max_hz: End frequency
 * @param n_points: Number of frequency points
 * @param profile: Output impedance profile (caller-allocated, n_points)
 * @param result: Output PDN analysis summary
 */
void hs_pdn_analyze(const hs_pdn_network_t *network,
                     double f_min_hz, double f_max_hz, int n_points,
                     hs_pdn_impedance_point_t *profile,
                     hs_pdn_result_t *result);

/**
 * Estimate Simultaneous Switching Noise (SSN) amplitude.
 *
 * SSN is caused by many outputs switching simultaneously, drawing
 * transient current through the PDN impedance.
 *
 * Simplified model:
 *   V_ssn ≈ N × (di/dt) × L_eff
 *
 * where N = number of simultaneous switching outputs,
 * di/dt = instantaneous current slope per driver,
 * L_eff = effective loop inductance from die to decap.
 *
 * More refined (frequency domain):
 *   V_ssn_peak ≈ Z_target × I_transient × √(N)  (for N uncorrelated)
 *   V_ssn_peak ≈ Z_target × I_transient × N      (for N coherent)
 *
 * Reference: Senthinathan & Prince, "Simultaneous Switching Noise
 *   of CMOS Devices and Systems", 1994.
 *
 * @param num_drivers: Number of simultaneously switching drivers
 * @param current_per_driver_a: Switching current per driver (A)
 * @param rise_time_s: Output rise time (s)
 * @param effective_inductance_h: Loop inductance from die to nearest decap (H)
 * @return Estimated SSN peak-to-peak voltage (V)
 */
double hs_ssn_estimate(int num_drivers, double current_per_driver_a,
                        double rise_time_s, double effective_inductance_h);

/**
 * Select the optimal number and values of decoupling capacitors
 * to meet the target impedance across a frequency range.
 *
 * Algorithm:
 *   1. Compute plane capacitance → determines high-frequency floor
 *   2. Select ceramic decaps to cover mid-frequency (1 MHz-100 MHz)
 *   3. Select bulk caps to cover low frequency (DC-VRM bandwidth)
 *   4. Verify no anti-resonance peaks exceed Z_target
 *
 * Reference: Smith et al., 1999; Novak, 2008
 *
 * @param network: PDN network (bulk_caps and ceramic_caps arrays filled by caller, values selected by function)
 * @param max_bulk: Maximum number of bulk capacitors to allocate
 * @param max_ceramic: Maximum number of ceramic capacitors to allocate
 * @param target_z_ohm: Target impedance (Ω)
 * @param f_max_hz: Maximum frequency of interest (Hz)
 * @param num_selected_bulk: Output: number of bulk caps selected
 * @param num_selected_ceramic: Output: number of ceramic caps selected
 * @return 0 if target met, 1 if partially met, -1 if infeasible
 */
int hs_decap_select(hs_pdn_network_t *network,
                     int max_bulk, int max_ceramic,
                     double target_z_ohm, double f_max_hz,
                     int *num_selected_bulk, int *num_selected_ceramic);

/**
 * Initialize a typical decoupling capacitor with standard values.
 *
 * @param decap: Capacitor to initialize
 * @param type: Capacitor type
 * @param capacitance_f: Capacitance value (F)
 * @param package: Package code (e.g., "0402")
 */
void hs_decap_init(hs_decap_t *decap, hs_capacitor_type_t type,
                    double capacitance_f, const char *package);

/**
 * Initialize a typical VRM for common voltage rails.
 *
 * @param vrm: VRM to initialize
 * @param output_voltage_v: Output voltage
 * @param max_current_a: Maximum current
 */
void hs_vrm_init_typical(hs_vrm_t *vrm, double output_voltage_v,
                          double max_current_a);

#endif /* HS_PDN_H */
