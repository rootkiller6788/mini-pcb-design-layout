/*
 * hs_transmission.h — Transmission Line Modeling and S-Parameters
 *
 * Core Definitions (L1):
 *   - S-parameters: S11, S21, S12, S22
 *   - Propagation constant γ = α + jβ
 *   - Phase constant β, attenuation constant α (dB/m, Np/m)
 *   - Insertion loss, return loss
 *   - Time-domain reflectometry (TDR)
 *   - Eye diagram, unit interval (UI)
 *
 * Core Concepts (L2):
 *   - Lossless vs lossy transmission line
 *   - RLGC per-unit-length parameters
 *   - Impedance profile along line
 *   - Signal rise time degradation
 *   - Intersymbol interference (ISI)
 *
 * Mathematical Structures (L3):
 *   - Telegrapher's equations: dV/dz = -(R+jωL)I, dI/dz = -(G+jωC)V
 *   - ABCD (chain) matrix for cascaded segments
 *   - S-parameter ↔ ABCD matrix conversion
 *   - Fourier analysis of digital signals
 *   - Convolution of pulse with channel impulse response
 *
 * Fundamental Laws (L4):
 *   - Telegrapher's equations → wave equation
 *   - Characteristic impedance: Z₀ = √((R+jωL)/(G+jωC))
 *   - Propagation constant: γ = √((R+jωL)(G+jωC))
 *   - S-parameter reciprocity: S12 = S21 (for passive, reciprocal network)
 *   - S-parameter losslessness: |S11|²+|S21|² = 1 (lossless)
 *
 * References:
 *   - Pozar, "Microwave Engineering", 4th Ed., Ch.2-4
 *   - Eisenstadt & Eo, "S-Parameter-Based PCB Interconnect Characterization", 1992
 *   - Johnson & Graham, "High-Speed Digital Design", Ch.7
 *   - Hall & Heck, "Advanced Signal Integrity for High-Speed Digital Designs", Ch.4-5
 */

#ifndef HS_TRANSMISSION_H
#define HS_TRANSMISSION_H

#include <stddef.h>
#include <stdint.h>

/* ================================================================
 * L1: Core Definitions — Transmission Line Parameters
 * ================================================================ */

/**
 * RLGC per-unit-length parameters.
 * These four quantities fully characterize a uniform transmission line.
 *
 * R: Series resistance per unit length (Ω/m) — conductor loss
 * L: Series inductance per unit length (H/m) — magnetic field storage
 * G: Shunt conductance per unit length (S/m) — dielectric loss
 * C: Shunt capacitance per unit length (F/m) — electric field storage
 *
 * For a lossless line: R=0, G=0
 * For a distortionless line: R/L = G/C (Heaviside condition)
 */
typedef struct {
    double resistance;     /* R: Ω/m */
    double inductance;     /* L: H/m */
    double conductance;    /* G: S/m */
    double capacitance;    /* C: F/m */
} hs_rlgc_params_t;

/**
 * Complex propagation constant.
 * γ = α + jβ
 *
 * α: Attenuation constant (Np/m or converted to dB/m: α_dB = 8.686 × α_Np)
 * β: Phase constant (rad/m) — determines wavelength λ = 2π/β
 */
typedef struct {
    double alpha_np;       /* Attenuation constant in Np/m */
    double beta_rad;       /* Phase constant in rad/m */
} hs_propagation_constant_t;

/**
 * S-parameter matrix for a 2-port network.
 *
 * [ b1 ]   [ S11  S12 ] [ a1 ]
 * [ b2 ] = [ S21  S22 ] [ a2 ]
 *
 * where a_i = incident wave, b_i = reflected wave at port i.
 *
 * |S11|² = power reflected from port 1 (return loss)
 * |S21|² = power transmitted from port 1 to port 2 (insertion loss)
 * S11 = input reflection coefficient with port 2 terminated in Z₀
 * S21 = forward transmission coefficient
 * S12 = reverse transmission coefficient
 * S22 = output reflection coefficient with port 1 terminated in Z₀
 */
typedef struct {
    double s11_real, s11_imag;
    double s21_real, s21_imag;
    double s12_real, s12_imag;
    double s22_real, s22_imag;
} hs_sparams_t;

/**
 * ABCD (chain / transmission) matrix for a 2-port network.
 *
 * [ V1 ]   [ A  B ] [ V2  ]
 * [ I1 ] = [ C  D ] [ -I2 ]
 *
 * For cascaded networks: ABCD_total = ABCD₁ × ABCD₂ × ... × ABCD_N
 *
 * For a uniform TL of length ℓ:
 *   A = cosh(γℓ),  B = Z₀ × sinh(γℓ)
 *   C = sinh(γℓ)/Z₀, D = cosh(γℓ)
 */
typedef struct {
    double a_real, a_imag;
    double b_real, b_imag;
    double c_real, c_imag;
    double d_real, d_imag;
} hs_abcd_matrix_t;

/**
 * Single frequency point for frequency-domain analysis.
 */
typedef struct {
    double frequency_hz;
    hs_sparams_t s;
} hs_freq_point_t;

/**
 * Time-domain pulse / waveform sample.
 */
typedef struct {
    double time_ps;
    double voltage;
} hs_waveform_sample_t;

/**
 * Eye diagram measurement results.
 */
typedef struct {
    double eye_height_v;       /* Vertical eye opening (V) */
    double eye_width_ui;       /* Horizontal eye opening (Unit Intervals) */
    double eye_width_ps;       /* Horizontal eye opening (ps) */
    double jitter_rms_ps;      /* RMS jitter (ps) */
    double jitter_pp_ps;       /* Peak-to-peak jitter (ps) */
    double rise_time_20_80_ps; /* 20%-80% rise time (ps) */
    double fall_time_20_80_ps; /* 20%-80% fall time (ps) */
    double bit_rate_gbps;      /* Bit rate (Gbps) */
    double ber_estimate;       /* Estimated BER from eye opening */
    int    is_open;            /* 1 if eye is open, 0 if closed */
} hs_eye_diagram_t;

/**
 * TDR (Time-Domain Reflectometry) impedance profile point.
 */
typedef struct {
    double distance_m;    /* Distance from TDR source (m) */
    double impedance_ohm; /* Measured impedance at this distance (Ω) */
} hs_tdr_point_t;

/* ================================================================
 * L2-L5: Transmission Line Analysis Functions
 * ================================================================ */

/**
 * Compute RLGC per-unit-length parameters for a microstrip transmission line.
 *
 * Capacitance:
 *   C = εeff / (c₀ × Z₀)  [F/m] where c₀ = speed of light
 *
 * Inductance (lossless, low-frequency):
 *   L = Z₀² × C = Z₀ × √εeff / c₀  [H/m]
 *
 * DC Resistance:
 *   R_dc = ρ / (w × t)  [Ω/m], ρ = 1.72e-8 for Cu
 *
 * AC Resistance (skin effect, f > 0):
 *   R_ac(f) = R_dc × √(f/f_δ) where δ(f_δ) = t/2
 *
 * Dielectric conductance:
 *   G = 2πf × C × tanδ  [S/m], where tanδ is the loss tangent
 *
 * Reference: Pozar Ch.3, Hall & Heck Ch.4
 *
 * @param z0: Characteristic impedance (Ω)
 * @param er_eff: Effective dielectric constant
 * @param tan_delta: Loss tangent of substrate
 * @param width_m: Trace width (m)
 * @param thickness_m: Copper thickness (m)
 * @param frequency_hz: Frequency (Hz), 0 for DC
 * @return RLGC parameters
 */
hs_rlgc_params_t hs_microstrip_rlgc(double z0, double er_eff, double tan_delta,
                                     double width_m, double thickness_m,
                                     double frequency_hz);

/**
 * Compute the complex propagation constant from RLGC parameters.
 *
 * γ = √((R + jωL)(G + jωC))
 *
 * Separating real and imaginary parts:
 *   α = √((RG - ω²LC + √((R²+ω²L²)(G²+ω²C²))) / 2)   (Np/m)
 *   β = √((ω²LC - RG + √((R²+ω²L²)(G²+ω²C²))) / 2)   (rad/m)
 *
 * For low-loss case (R << ωL, G << ωC):
 *   α ≈ R/(2Z₀) + GZ₀/2    (Np/m)
 *   β ≈ ω√(LC)              (rad/m)
 *
 * Reference: Pozar, Eq. 2.9a-2.9b
 *
 * @param rlgc: Per-unit-length parameters
 * @param frequency_hz: Frequency in Hz
 * @return Propagation constant
 */
hs_propagation_constant_t hs_propagation_constant(const hs_rlgc_params_t *rlgc,
                                                    double frequency_hz);

/**
 * Compute the characteristic impedance from RLGC parameters.
 *
 * Z₀ = √((R + jωL) / (G + jωC))  [Ω]
 *
 * For lossless (R=0, G=0): Z₀ = √(L/C)
 *
 * @param rlgc: Per-unit-length parameters
 * @param frequency_hz: Frequency in Hz (> 0)
 * @return Characteristic impedance magnitude (Ω)
 */
double hs_characteristic_impedance_rlgc(const hs_rlgc_params_t *rlgc,
                                         double frequency_hz);

/**
 * Compute the ABCD matrix for a uniform transmission line segment.
 *
 * For a TL of length ℓ with characteristic impedance Z₀ and
 * propagation constant γ:
 *
 *   A = cosh(γℓ)
 *   B = Z₀ × sinh(γℓ)
 *   C = sinh(γℓ) / Z₀
 *   D = cosh(γℓ)
 *
 * This is the fundamental building block for cascaded TL analysis.
 *
 * @param length_m: Segment length in m
 * @param z0: Characteristic impedance (Ω)
 * @param gamma: Propagation constant
 * @return ABCD matrix
 */
hs_abcd_matrix_t hs_tline_abcd(double length_m, double z0,
                                const hs_propagation_constant_t *gamma);

/**
 * Cascade (multiply) two ABCD matrices.
 *
 * ABCD_total = ABCD₁ × ABCD₂
 *
 * This allows analysis of cascaded TL segments, vias, connectors, etc.
 * The chain matrix representation is multiplicative for cascaded blocks.
 *
 * @param m1: First ABCD matrix (closest to source)
 * @param m2: Second ABCD matrix (closest to load)
 * @return Product m1 × m2
 */
hs_abcd_matrix_t hs_abcd_cascade(const hs_abcd_matrix_t *m1,
                                   const hs_abcd_matrix_t *m2);

/**
 * Convert ABCD matrix to S-parameters (normalized to Z₀).
 *
 * S11 = (A + B/Z₀ - CZ₀ - D) / (A + B/Z₀ + CZ₀ + D)
 * S21 = 2 / (A + B/Z₀ + CZ₀ + D)
 * S12 = 2(AD - BC) / (A + B/Z₀ + CZ₀ + D)
 * S22 = (-A + B/Z₀ - CZ₀ + D) / (A + B/Z₀ + CZ₀ + D)
 *
 * For a reciprocal network (AD-BC = 1): S12 = S21
 *
 * Reference: Pozar, Eq. 4.63-4.66
 *
 * @param abcd: ABCD matrix
 * @param z0: Reference impedance (Ω)
 * @return S-parameter matrix
 */
hs_sparams_t hs_abcd_to_sparams(const hs_abcd_matrix_t *abcd, double z0);

/**
 * Compute insertion loss (|S21| in dB) from S-parameters.
 *
 * IL = -20 × log₁₀(|S21|)  [dB]
 *
 * Larger positive IL means more loss. IL=0 means perfect transmission.
 *
 * @param s: S-parameters
 * @return Insertion loss in dB (≥ 0)
 */
double hs_insertion_loss_db(const hs_sparams_t *s);

/**
 * Compute return loss (|S11| in dB) from S-parameters.
 *
 * RL = -20 × log₁₀(|S11|)  [dB]
 *
 * @param s: S-parameters
 * @return Return loss in dB (≥ 0)
 */
double hs_return_loss_db_sparam(const hs_sparams_t *s);

/**
 * Compute the per-unit-length attenuation from RLGC at a given frequency.
 *
 * α_dB = 8.686 × Re(γ)  [dB/m]
 *
 * Separate into conductor and dielectric loss components:
 *   α_c ≈ R/(2Z₀) × 8.686  [dB/m]  (conductor)
 *   α_d ≈ GZ₀/2 × 8.686    [dB/m]  (dielectric)
 *
 * @param rlgc: RLGC parameters
 * @param frequency_hz: Frequency
 * @param alpha_conductor_db: Output conductor loss (dB/m)
 * @param alpha_dielectric_db: Output dielectric loss (dB/m)
 * @return Total attenuation in dB/m
 */
double hs_attenuation_components(const hs_rlgc_params_t *rlgc,
                                  double frequency_hz,
                                  double *alpha_conductor_db,
                                  double *alpha_dielectric_db);

/**
 * Compute the bandwidth of a digital signal from its rise time.
 *
 * BW ≈ 0.35 / t_rise    (for Gaussian step response)
 * BW ≈ 0.5 / t_rise     (for exponential step)
 *
 * This is the frequency at which the channel must have acceptable
 * transmission characteristics to preserve signal integrity.
 *
 * Reference: Johnson & Graham, Eq. 1.4; Bogatin, Ch.2
 *
 * @param rise_time_20_80_s: 20%-80% rise time in seconds (> 0)
 * @param model: 0=Gaussian (0.35), 1=Exponential (0.5)
 * @return Bandwidth in Hz
 */
double hs_bandwidth_from_rise_time(double rise_time_20_80_s, int model);

/**
 * Compute the critical length for transmission line behavior.
 *
 * A trace should be treated as a transmission line when its length
 * exceeds the critical length:
 *   L_crit = t_rise / (2 × t_pd)
 *
 * where t_pd is the propagation delay per unit length.
 * Typically, L_crit ≈ t_rise/(6×t_pd) for more conservative margin.
 *
 * For 1 ns rise time on FR-4 (t_pd ≈ 6 ns/m):
 *   L_crit ≈ 1e-9 / (2 × 6e-9) ≈ 8.3 cm ≈ 3.3 inches
 *
 * @param rise_time_s: Rise time in seconds
 * @param t_pd_s_per_m: Propagation delay per meter (s/m)
 * @return Critical length in meters
 */
double hs_critical_length(double rise_time_s, double t_pd_s_per_m);

/**
 * Generate the time-domain step response of a lossy transmission line.
 *
 * Computes the response to a unit step using inverse Fourier transform
 * of the S21(f) of the line. Accounts for frequency-dependent loss
 * (skin effect + dielectric loss).
 *
 * Algorithm: Sample S21 at N frequency points, apply IFFT.
 *
 * Reference: Hall & Heck, Ch.5
 *
 * @param rlgc: RLGC parameters
 * @param length_m: Line length in m
 * @param z0: Characteristic impedance (Ω)
 * @param n_samples: Number of time samples (> 0)
 * @param dt_ps: Time step in ps (> 0)
 * @param waveform: Output waveform array (caller-allocated, n_samples)
 */
void hs_step_response(const hs_rlgc_params_t *rlgc, double length_m,
                       double z0, int n_samples, double dt_ps,
                       hs_waveform_sample_t *waveform);

/**
 * Compute the eye diagram parameters from a received bit stream.
 *
 * Algorithm:
 *   1. Overlay all unit intervals
 *   2. Find minimum and maximum voltage in the center 20% of UI (eye height)
 *   3. Find minimum and maximum crossing times at 50% level (eye width)
 *   4. Estimate BER from eye opening: BER ≈ 0.5×erfc(EyeHeight/(2√2×σ))
 *
 * Reference: Hall & Heck, Ch.9; Derickson & Muller, "Digital Communications
 *   Test and Measurement", Ch.5
 *
 * @param waveform: Received waveform samples
 * @param n_samples: Number of samples
 * @param ui_ps: Unit interval in ps
 * @param bit_rate_gbps: Bit rate in Gbps
 * @param result: Output eye diagram parameters
 * @return 0 on success
 */
int hs_eye_diagram(const hs_waveform_sample_t *waveform, int n_samples,
                    double ui_ps, double bit_rate_gbps,
                    hs_eye_diagram_t *result);

/**
 * TDR impedance profile along a transmission line.
 *
 * Given S11(f) measured at the TDR port, compute the impedance
 * as a function of distance (round-trip time conversion).
 *
 * Z(d) = Z₀ × (1 + C(d)) / (1 - C(d))
 * where C(d) ≈ IFFT(S11(f) × W(f)) is the time-domain reflection
 * coefficient, and W(f) is a window function for sidelobe suppression.
 *
 * Reference: "TDR Impedance Measurements", Agilent AN 1304-2
 *
 * @param s11_freq: Array of S11 at each frequency point
 * @param n_freqs: Number of frequency points
 * @param z0: Reference impedance (Ω)
 * @param er_eff: Effective εr for distance conversion
 * @param n_points: Number of output profile points
 * @param profile: Output TDR impedance profile
 */
void hs_tdr_impedance_profile(const hs_freq_point_t *s11_freq, int n_freqs,
                               double z0, double er_eff,
                               int n_points, hs_tdr_point_t *profile);

/**
 * Compute the wavelength on a PCB transmission line.
 *
 * λ = v_p / f = c / (f × √εeff)
 *
 * For 1 GHz on FR-4 (εeff=3.4): λ ≈ 16.3 cm ≈ 6.4 inches
 * For 10 GHz on FR-4: λ ≈ 1.63 cm ≈ 0.64 inches
 *
 * A quarter-wave stub at 2.4 GHz on FR-4 requires ≈ 17 mm.
 *
 * @param frequency_hz: Frequency in Hz
 * @param er_eff: Effective dielectric constant
 * @return Wavelength in meters
 */
double hs_wavelength(double frequency_hz, double er_eff);

/**
 * Compute the frequency where a trace behaves as a quarter-wave resonator.
 *
 * f_res = v_p / (4 × L) = c / (4 × L × √εeff)
 *
 * This is important for via stubs and open stubs on transmission lines.
 * Quarter-wave resonance causes severe S11 degradation.
 *
 * @param length_m: Length of stub/trace in m
 * @param er_eff: Effective dielectric constant
 * @return Resonant frequency in Hz
 */
double hs_quarter_wave_frequency(double length_m, double er_eff);

#endif /* HS_TRANSMISSION_H */
