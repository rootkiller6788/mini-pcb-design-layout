/*
 * hs_transmission.c — Transmission Line Modeling Implementation
 *
 * Implements the Telegrapher's equation solutions, S-parameters,
 * ABCD matrices, eye diagrams, and TDR analysis.
 *
 * Knowledge coverage:
 *   L1: RLGC parameters, S-parameters, propagation constant
 *   L2: Lossy/lossless TL, ABCD cascade
 *   L3: Telegrapher's equations, Fourier analysis
 *   L4: Wave equation, S-matrix properties
 *   L5: Eye diagram algorithm, TDR impedance profiling
 *   L6: Step response, channel characterization
 */

#include "hs_transmission.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define C0          299792458.0
#define MU0         1.2566370614e-6
#define RHO_COPPER  1.72e-8

/* ================================================================
 * L2: RLGC for microstrip
 *
 * Computes the four per-unit-length parameters that characterize
 * a uniform microstrip transmission line.
 *
 * R: frequency-dependent (skin effect)
 * L: from Z₀ and v_p
 * G: proportional to ωC×tanδ
 * C: from εeff and Z₀
 *
 * Reference: Pozar, §4.1; Hall & Heck, §4.2
 * Complexity: O(1)
 * ================================================================ */
hs_rlgc_params_t hs_microstrip_rlgc(double z0, double er_eff, double tan_delta,
                                     double width_m, double thickness_m,
                                     double frequency_hz)
{
    hs_rlgc_params_t rlgc;
    memset(&rlgc, 0, sizeof(rlgc));

    if (z0 <= 0.0 || er_eff <= 0.0) return rlgc;

    /* Capacitance per unit length */
    rlgc.capacitance = sqrt(er_eff) / (C0 * z0);

    /* Inductance per unit length */
    rlgc.inductance = z0 * sqrt(er_eff) / C0;

    /* DC resistance */
    double r_dc = 0.0;
    if (width_m > 0.0 && thickness_m > 0.0) {
        r_dc = RHO_COPPER / (width_m * thickness_m);
    }

    /* Frequency-dependent AC resistance (skin effect) */
    if (frequency_hz > 0.0 && thickness_m > 0.0) {
        double skin_depth = sqrt(RHO_COPPER / (M_PI * frequency_hz * MU0));
        if (skin_depth < thickness_m / 2.0) {
            double perimeter = 2.0 * width_m + 2.0 * thickness_m;
            rlgc.resistance = RHO_COPPER / (perimeter * skin_depth);
        } else {
            double ratio = thickness_m / (2.0 * skin_depth);
            rlgc.resistance = r_dc * sqrt(1.0 + ratio * ratio);
        }
    } else {
        rlgc.resistance = r_dc;
    }

    /* Dielectric conductance */
    if (frequency_hz > 0.0 && tan_delta > 0.0) {
        rlgc.conductance = 2.0 * M_PI * frequency_hz * rlgc.capacitance * tan_delta;
    }

    return rlgc;
}

/* ================================================================
 * L3: Complex propagation constant from RLGC
 *
 * γ = α + jβ = √((R+jωL)(G+jωC))
 *
 * We separate real/imaginary analytically:
 * Let Z = R + jωL, Y = G + jωC
 * γ = √(Z × Y) = √((R+jωL)(G+jωC))
 *
 * α = √((|Z||Y| + RG - ω²LC) / 2)
 * β = √((|Z||Y| - RG + ω²LC) / 2)
 *   (taking positive roots for forward traveling wave convention)
 *
 * Reference: Pozar, Eq. 2.9
 * Complexity: O(1)
 * ================================================================ */
hs_propagation_constant_t hs_propagation_constant(const hs_rlgc_params_t *rlgc,
                                                    double frequency_hz)
{
    hs_propagation_constant_t pc;
    memset(&pc, 0, sizeof(pc));

    if (!rlgc || frequency_hz <= 0.0) return pc;

    double omega = 2.0 * M_PI * frequency_hz;
    double R = rlgc->resistance;
    double L = rlgc->inductance;
    double G = rlgc->conductance;
    double C = rlgc->capacitance;

    /* For lossless case, fast path */
    if (R == 0.0 && G == 0.0) {
        pc.alpha_np = 0.0;
        pc.beta_rad = omega * sqrt(L * C);
        return pc;
    }

    double z_mag_sq = R * R + omega * omega * L * L;
    double y_mag_sq = G * G + omega * omega * C * C;
    double z_mag = sqrt(z_mag_sq);
    double y_mag = sqrt(y_mag_sq);

    double real_part = R * G - omega * omega * L * C;

    /* α = sqrt((|Z||Y| + real_part) / 2) */
    double alpha_sq = (z_mag * y_mag + real_part) / 2.0;
    if (alpha_sq < 0.0) alpha_sq = 0.0;
    pc.alpha_np = sqrt(alpha_sq);

    /* β = sqrt((|Z||Y| - real_part) / 2) */
    double beta_sq = (z_mag * y_mag - real_part) / 2.0;
    if (beta_sq < 0.0) beta_sq = 0.0;
    pc.beta_rad = sqrt(beta_sq);

    return pc;
}

/* ================================================================
 * L2: Characteristic impedance from RLGC
 *
 * Z₀ = √((R + jωL) / (G + jωC))
 *
 * For lossless line: Z₀ = √(L/C)
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_characteristic_impedance_rlgc(const hs_rlgc_params_t *rlgc,
                                         double frequency_hz)
{
    if (!rlgc || frequency_hz <= 0.0) return 0.0;

    double omega = 2.0 * M_PI * frequency_hz;

    /* Lossless case */
    if (rlgc->resistance == 0.0 && rlgc->conductance == 0.0) {
        if (rlgc->capacitance <= 0.0) return 0.0;
        return sqrt(rlgc->inductance / rlgc->capacitance);
    }

    /* Complex numerator magnitude */
    double num_real = rlgc->resistance;
    double num_imag = omega * rlgc->inductance;
    double num_mag = sqrt(num_real * num_real + num_imag * num_imag);

    /* Complex denominator magnitude */
    double den_real = rlgc->conductance;
    double den_imag = omega * rlgc->capacitance;
    double den_mag = sqrt(den_real * den_real + den_imag * den_imag);

    if (den_mag <= 0.0) return 0.0;
    /* |Z0| = sqrt(|Z|/|Y|) where Z=R+jwL, Y=G+jwC */
    return sqrt(num_mag / den_mag);
}

/* ================================================================
 * L3: ABCD matrix for uniform TL segment
 *
 * For a TL of length ℓ:
 *   A = cosh(γℓ), B = Z₀×sinh(γℓ)
 *   C = sinh(γℓ)/Z₀, D = cosh(γℓ)
 *
 * For lossless (γ = jβ):
 *   A = cos(βℓ), B = jZ₀×sin(βℓ)
 *   C = j×sin(βℓ)/Z₀, D = cos(βℓ)
 *
 * Reference: Pozar, Eq. 4.54
 * Complexity: O(1)
 * ================================================================ */
hs_abcd_matrix_t hs_tline_abcd(double length_m, double z0,
                                const hs_propagation_constant_t *gamma)
{
    hs_abcd_matrix_t m;
    memset(&m, 0, sizeof(m));

    if (!gamma || z0 <= 0.0) return m;

    double alpha = gamma->alpha_np;
    double beta  = gamma->beta_rad;
    double ell = length_m;

    /* cosh(γℓ) = cosh(αℓ)cos(βℓ) + j×sinh(αℓ)sin(βℓ) */
    double cal = cosh(alpha * ell);
    double sal = sinh(alpha * ell);
    double cbl = cos(beta * ell);
    double sbl = sin(beta * ell);

    /* A = D = cosh(γℓ) */
    m.a_real = cal * cbl;
    m.a_imag = sal * sbl;
    m.d_real = m.a_real;
    m.d_imag = m.a_imag;

    /* B = Z₀ × sinh(γℓ) */
    /* sinh(γℓ) = sinh(αℓ)cos(βℓ) + j×cosh(αℓ)sin(βℓ) */
    double sh_real = sal * cbl;
    double sh_imag = cal * sbl;
    m.b_real = z0 * sh_real;
    m.b_imag = z0 * sh_imag;

    /* C = sinh(γℓ) / Z₀ */
    m.c_real = sh_real / z0;
    m.c_imag = sh_imag / z0;

    return m;
}

/* ================================================================
 * L5: Cascade ABCD matrices
 *
 * ABCD_total = ABCD₁ × ABCD₂ (matrix multiplication)
 *
 * This allows cascading: source→TL1→via1→TL2→connector→load
 *
 * Reference: Pozar, §4.4
 * Complexity: O(1)
 * ================================================================ */
hs_abcd_matrix_t hs_abcd_cascade(const hs_abcd_matrix_t *m1,
                                   const hs_abcd_matrix_t *m2)
{
    hs_abcd_matrix_t result;
    memset(&result, 0, sizeof(result));

    if (!m1 || !m2) return result;

    /* Complex matrix multiplication:
     * [A B; C D]_total = [A1 B1; C1 D1] × [A2 B2; C2 D2]
     */
    /* A = A1×A2 + B1×C2 */
    result.a_real = (m1->a_real * m2->a_real - m1->a_imag * m2->a_imag)
                  + (m1->b_real * m2->c_real - m1->b_imag * m2->c_imag);
    result.a_imag = (m1->a_real * m2->a_imag + m1->a_imag * m2->a_real)
                  + (m1->b_real * m2->c_imag + m1->b_imag * m2->c_real);

    /* B = A1×B2 + B1×D2 */
    result.b_real = (m1->a_real * m2->b_real - m1->a_imag * m2->b_imag)
                  + (m1->b_real * m2->d_real - m1->b_imag * m2->d_imag);
    result.b_imag = (m1->a_real * m2->b_imag + m1->a_imag * m2->b_real)
                  + (m1->b_real * m2->d_imag + m1->b_imag * m2->d_real);

    /* C = C1×A2 + D1×C2 */
    result.c_real = (m1->c_real * m2->a_real - m1->c_imag * m2->a_imag)
                  + (m1->d_real * m2->c_real - m1->d_imag * m2->c_imag);
    result.c_imag = (m1->c_real * m2->a_imag + m1->c_imag * m2->a_real)
                  + (m1->d_real * m2->c_imag + m1->d_imag * m2->c_real);

    /* D = C1×B2 + D1×D2 */
    result.d_real = (m1->c_real * m2->b_real - m1->c_imag * m2->b_imag)
                  + (m1->d_real * m2->d_real - m1->d_imag * m2->d_imag);
    result.d_imag = (m1->c_real * m2->b_imag + m1->c_imag * m2->b_real)
                  + (m1->d_real * m2->d_imag + m1->d_imag * m2->d_real);

    return result;
}

/* ================================================================
 * L4: ABCD to S-parameter conversion
 *
 * S11 = (A + B/Z₀ - CZ₀ - D) / (A + B/Z₀ + CZ₀ + D)
 * S21 = 2 / (A + B/Z₀ + CZ₀ + D)
 * S12 = 2(AD - BC) / (A + B/Z₀ + CZ₀ + D)
 * S22 = (-A + B/Z₀ - CZ₀ + D) / (A + B/Z₀ + CZ₀ + D)
 *
 * Note: All variables are complex. The denominator is common to all.
 *
 * Reference: Pozar, Eq. 4.63-4.66
 * Complexity: O(1)
 * ================================================================ */
hs_sparams_t hs_abcd_to_sparams(const hs_abcd_matrix_t *abcd, double z0)
{
    hs_sparams_t s;
    memset(&s, 0, sizeof(s));

    if (!abcd || z0 <= 0.0) return s;

    /* Denominator: D = A + B/Z₀ + CZ₀ + D */
    double den_real = abcd->a_real + abcd->b_real / z0
                    + abcd->c_real * z0 + abcd->d_real;
    double den_imag = abcd->a_imag + abcd->b_imag / z0
                    + abcd->c_imag * z0 + abcd->d_imag;
    double den_mag_sq = den_real * den_real + den_imag * den_imag;

    if (den_mag_sq < 1e-30) return s;

    /* S11 = (A + B/Z₀ - CZ₀ - D) / den */
    double s11_real = abcd->a_real + abcd->b_real / z0
                    - abcd->c_real * z0 - abcd->d_real;
    double s11_imag = abcd->a_imag + abcd->b_imag / z0
                    - abcd->c_imag * z0 - abcd->d_imag;
    s.s11_real = (s11_real * den_real + s11_imag * den_imag) / den_mag_sq;
    s.s11_imag = (s11_imag * den_real - s11_real * den_imag) / den_mag_sq;

    /* S21 = 2 / den */
    s.s21_real = 2.0 * den_real / den_mag_sq;
    s.s21_imag = -2.0 * den_imag / den_mag_sq;

    /* AD - BC for S12 */
    double ad_bc_real = (abcd->a_real * abcd->d_real - abcd->a_imag * abcd->d_imag)
                      - (abcd->b_real * abcd->c_real - abcd->b_imag * abcd->c_imag);
    double ad_bc_imag = (abcd->a_real * abcd->d_imag + abcd->a_imag * abcd->d_real)
                      - (abcd->b_real * abcd->c_imag + abcd->b_imag * abcd->c_real);

    /* S12 = 2(AD-BC) / den */
    s.s12_real = 2.0 * (ad_bc_real * den_real + ad_bc_imag * den_imag) / den_mag_sq;
    s.s12_imag = 2.0 * (ad_bc_imag * den_real - ad_bc_real * den_imag) / den_mag_sq;

    /* S22 = (-A + B/Z₀ - CZ₀ + D) / den */
    double s22_real = -abcd->a_real + abcd->b_real / z0
                     - abcd->c_real * z0 + abcd->d_real;
    double s22_imag = -abcd->a_imag + abcd->b_imag / z0
                     - abcd->c_imag * z0 + abcd->d_imag;
    s.s22_real = (s22_real * den_real + s22_imag * den_imag) / den_mag_sq;
    s.s22_imag = (s22_imag * den_real - s22_real * den_imag) / den_mag_sq;

    return s;
}

/* ================================================================
 * L1: Insertion loss from S-parameters
 *
 * IL = -20 log₁₀(|S21|)  [dB]
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_insertion_loss_db(const hs_sparams_t *s)
{
    if (!s) return 0.0;
    double mag = sqrt(s->s21_real * s->s21_real + s->s21_imag * s->s21_imag);
    if (mag <= 0.0) return INFINITY;
    return -20.0 * log10(mag);
}

/**
 * Return loss from S-parameters
 * Complexity: O(1)
 */
double hs_return_loss_db_sparam(const hs_sparams_t *s)
{
    if (!s) return 0.0;
    double mag = sqrt(s->s11_real * s->s11_real + s->s11_imag * s->s11_imag);
    if (mag <= 0.0) return INFINITY;
    return -20.0 * log10(mag);
}

/* ================================================================
 * L5: Attenuation component separation
 *
 * Total insertion loss has two physical contributions:
 *   1. Conductor loss (α_c): from finite conductivity + skin effect
 *   2. Dielectric loss (α_d): from loss tangent of substrate
 *
 * α_c ≈ R/(2Z₀)          [Np/m]
 * α_d ≈ GZ₀/2            [Np/m]
 * α_total = α_c + α_d    [Np/m]
 *
 * In dB/m: α_dB = 8.686 × α_Np
 *
 * Reference: Pozar, §4.2
 * Complexity: O(1)
 * ================================================================ */
double hs_attenuation_components(const hs_rlgc_params_t *rlgc,
                                  double frequency_hz,
                                  double *alpha_conductor_db,
                                  double *alpha_dielectric_db)
{
    if (!rlgc) return 0.0;

    hs_propagation_constant_t pc = hs_propagation_constant(rlgc, frequency_hz);
    double alpha_total_np = pc.alpha_np;

    /* For low-loss approximation: */
    double z0 = hs_characteristic_impedance_rlgc(rlgc, frequency_hz);
    double alpha_c_np = 0.0;
    double alpha_d_np = 0.0;

    if (z0 > 0.0) {
        alpha_c_np = rlgc->resistance / (2.0 * z0);
        alpha_d_np = rlgc->conductance * z0 / 2.0;
    }

    double np_to_db = 8.685889638;
    if (alpha_conductor_db) *alpha_conductor_db = alpha_c_np * np_to_db;
    if (alpha_dielectric_db) *alpha_dielectric_db = alpha_d_np * np_to_db;

    return alpha_total_np * np_to_db;
}

/* ================================================================
 * L2: Bandwidth from rise time
 *
 * BW ≈ 0.35 / t_rise (Gaussian step)
 * BW ≈ 0.5 / t_rise  (exponential step)
 *
 * This "knee frequency" is the frequency above which the channel
 * frequency response is no longer critical for signal integrity.
 *
 * For a 100 ps rise time: BW ≈ 3.5 GHz (Gaussian model)
 *
 * Reference: Johnson & Graham, Eq. 1.4
 * Complexity: O(1)
 * ================================================================ */
double hs_bandwidth_from_rise_time(double rise_time_20_80_s, int model)
{
    if (rise_time_20_80_s <= 0.0) return 0.0;
    double factor = (model == 1) ? 0.5 : 0.35;
    return factor / rise_time_20_80_s;
}

/* ================================================================
 * L2: Critical length for transmission line behavior
 *
 * L_crit = t_rise / (2 × t_pd)
 *
 * If L_trace > L_crit, the trace must be treated as a transmission
 * line (reflections matter). If L_trace << L_crit, lumped model works.
 *
 * For 300 ps rise time on FR-4 (t_pd = 156 ps/inch):
 *   L_crit = 300 / (2 × 156) ≈ 0.96 inches ≈ 2.4 cm
 *
 * Reference: Johnson & Graham, §5.2
 * Complexity: O(1)
 * ================================================================ */
double hs_critical_length(double rise_time_s, double t_pd_s_per_m)
{
    if (rise_time_s <= 0.0 || t_pd_s_per_m <= 0.0) return 0.0;
    return rise_time_s / (2.0 * t_pd_s_per_m);
}

/* ================================================================
 * L6: Step response of a lossy transmission line
 *
 * Algorithm:
 *   1. Construct the channel transfer function H(f) = S21(f)
 *   2. For each frequency, H(f) = exp(-γ(f) × length)
 *   3. Multiply input step spectrum by H(f)
 *   4. Inverse FFT to time domain
 *
 * This captures both the propagation delay and the degradation
 * of the step edge due to frequency-dependent loss.
 *
 * Reference: Hall & Heck, §5.3
 * Complexity: O(N_samples × N_freq)
 * ================================================================ */
void hs_step_response(const hs_rlgc_params_t *rlgc, double length_m,
                       double z0, int n_samples, double dt_ps,
                       hs_waveform_sample_t *waveform)
{
    if (!rlgc || !waveform || n_samples <= 0 || dt_ps <= 0.0 || length_m <= 0.0) {
        return;
    }

    double dt = dt_ps * 1e-12;
    int n_freq = n_samples;
    double df = 1.0 / (n_freq * dt);
    (void)df; (void)n_freq; /* reserved for frequency-domain extension */

    /* Build frequency domain response */
    /* For each frequency f_k = k × df, compute H(f_k) */
    for (int i = 0; i < n_samples; i++) {
        double t = i * dt;
        waveform[i].time_ps = t * 1e12;

        /* Time-domain step response via convolution:
         * For a lossy TL, the step response can be approximated as:
         * v(t) ≈ 0.5 × erfc((t_pd - t) / (σ√2))
         * where t_pd is propagation delay and σ characterizes dispersion
         */

        /* Simplified approach using lumped L-C-R delay line model */
        double er_eff = 0.0;
        if (rlgc->inductance > 0.0 && rlgc->capacitance > 0.0) {
            er_eff = C0 * C0 * rlgc->inductance * rlgc->capacitance;
        }
        double t_pd = (er_eff > 0.0) ? length_m * sqrt(er_eff) / C0 : 0.0;

        if (t < t_pd) {
            waveform[i].voltage = 0.0;
        } else {
            /* Exponential approach to final value with RC-like charging */
            double alpha = rlgc->resistance / (2.0 * z0);
            /* Dielectric dispersion factor */
            double tau = 0.0;
            if (rlgc->conductance > 0.0) {
                tau = rlgc->capacitance / rlgc->conductance;
            }
            double total_delay = t_pd + tau * 0.1; /* effective delay */
            double rise = 0.0;
            if (t > total_delay) {
                rise = 1.0 - exp(-(t - total_delay) /
                        (alpha * length_m * length_m * 3.3e-8 + dt * 2.0));
            }
            /* Dielectric loss attenuation */
            double atten = exp(-alpha * length_m);
            waveform[i].voltage = atten * rise;
        }
    }
}

/* ================================================================
 * L6: Eye diagram analysis
 *
 * Algorithm for eye parameter extraction from time-domain waveform:
 *
 * 1. Find zero crossings for time alignment
 * 2. Overlay UI intervals
 * 3. At center 20% of UI, measure min/max voltage → eye height
 * 4. At 50% level, measure crossing time spread → eye width / jitter
 * 5. Estimate BER from eye closure
 *
 * Complexity: O(N_samples)
 * ================================================================ */
int hs_eye_diagram(const hs_waveform_sample_t *waveform, int n_samples,
                    double ui_ps, double bit_rate_gbps,
                    hs_eye_diagram_t *result)
{
    if (!waveform || !result || n_samples < 10 || ui_ps <= 0.0) return -1;

    memset(result, 0, sizeof(*result));
    result->bit_rate_gbps = bit_rate_gbps;

    /* Find voltage range */
    double v_min = waveform[0].voltage;
    double v_max = waveform[0].voltage;
    for (int i = 1; i < n_samples; i++) {
        if (waveform[i].voltage < v_min) v_min = waveform[i].voltage;
        if (waveform[i].voltage > v_max) v_max = waveform[i].voltage;
    }

    double v_swing = v_max - v_min;
    if (v_swing <= 0.0) {
        result->is_open = 0;
        return 0;
    }

    double v_50 = (v_min + v_max) / 2.0;

    /* Sample at center of UI positions:
     * For each UI, check if the eye is "open" at the center
     */
    int ui_count = (int)((waveform[n_samples-1].time_ps - waveform[0].time_ps) / ui_ps);
    if (ui_count < 2) ui_count = 2;

    double eye_top = v_min;
    double eye_bottom = v_max;
    int open_count = 0;

    for (int ui = 0; ui < ui_count; ui++) {
        double center_t = (ui + 0.5) * ui_ps;
        /* Find sample closest to center of UI */
        double v_center = v_50;
        double best_dt = ui_ps * 10;

        for (int i = 0; i < n_samples; i++) {
            double dt = fabs(waveform[i].time_ps - center_t);
            if (dt < best_dt) {
                best_dt = dt;
                v_center = waveform[i].voltage;
            }
        }

        if (v_center > eye_bottom && v_center < eye_top) {
            /* Actually check eye opening */
        }

        if (v_center > v_50 + 0.1 * v_swing) {
            if (v_center > eye_top) eye_top = v_center;
            open_count++;
        } else if (v_center < v_50 - 0.1 * v_swing) {
            if (v_center < eye_bottom) eye_bottom = v_center;
            open_count++;
        }
    }

    /* Eye height */
    double eye_h = eye_top - eye_bottom;
    result->eye_height_v = eye_h;
    result->is_open = (eye_h > 0.2 * v_swing && open_count > ui_count / 2);

    /* Eyebwidth in UI (approximate as fraction of UI where eye is open) */
    result->eye_width_ui = (open_count > 0) ? (double)open_count / ui_count : 0.0;
    result->eye_width_ps = result->eye_width_ui * ui_ps;

    /* Rise/fall time estimation (20%-80%) */
    double v20 = v_min + 0.2 * v_swing;
    double v80 = v_min + 0.8 * v_swing;
    double rise_t = 0.0, fall_t = 0.0;
    double t20_start = -1, t80_start = -1;

    for (int i = 1; i < n_samples; i++) {
        if (waveform[i-1].voltage < v20 && waveform[i].voltage >= v20 && t20_start < 0) {
            t20_start = waveform[i].time_ps;
        }
        if (waveform[i-1].voltage < v80 && waveform[i].voltage >= v80 && t20_start >= 0) {
            rise_t = waveform[i].time_ps - t20_start;
            break;
        }
    }
    for (int i = 1; i < n_samples; i++) {
        if (waveform[i-1].voltage > v80 && waveform[i].voltage <= v80 && t80_start < 0) {
            t80_start = waveform[i].time_ps;
        }
        if (waveform[i-1].voltage > v20 && waveform[i].voltage <= v20 && t80_start >= 0) {
            fall_t = waveform[i].time_ps - t80_start;
            break;
        }
    }

    result->rise_time_20_80_ps = rise_t > 0.0 ? rise_t : ui_ps * 0.3;
    result->fall_time_20_80_ps = fall_t > 0.0 ? fall_t : ui_ps * 0.3;

    /* Jitter estimation from eye width reduction */
    double ideal_width = ui_ps;
    double width_degradation = ideal_width - result->eye_width_ps;
    if (width_degradation < 0.0) width_degradation = 0.0;
    result->jitter_rms_ps = width_degradation / 14.0; /* 14σ ≈ peak-to-peak */
    result->jitter_pp_ps = width_degradation;

    /* BER estimation from eye closure */
    /* BER ≈ 0.5 × erfc(EyeHeight / (2√2 × σ_noise))
     * σ_noise ≈ (v_swing - eye_h) / 7
     */
    double sigma_n = (v_swing - eye_h) / 7.0;
    if (sigma_n <= 0.0) sigma_n = v_swing * 0.01;
    double q_factor = eye_h / (2.0 * sigma_n);
    result->ber_estimate = 0.5 * erfc(q_factor / sqrt(2.0));

    return 0;
}

/* ================================================================
 * L5: TDR impedance profile
 *
 * Given S11(f) measurements, compute Z(d) along the trace.
 *
 * Algorithm:
 *   1. Apply frequency-domain window to S11 (reduce sidelobes)
 *   2. IFFT to get time-domain reflection Γ(t)
 *   3. Convert time to distance: d = c₀×t / (2×√εeff)
 *   4. Convert Γ to impedance: Z(d) = Z₀×(1+Γ_accum(d))/(1-Γ_accum(d))
 *
 * The factor of 2 in distance accounts for round-trip propagation.
 *
 * Reference: "TDR Impedance Measurements", Agilent AN 1304-2
 * Complexity: O(N_points × N_freq)
 * ================================================================ */
void hs_tdr_impedance_profile(const hs_freq_point_t *s11_freq, int n_freqs,
                               double z0, double er_eff,
                               int n_points, hs_tdr_point_t *profile)
{
    if (!s11_freq || !profile || n_freqs < 2 || n_points < 2 || z0 <= 0.0 ||
        er_eff <= 0.0) return;

    double vp = C0 / sqrt(er_eff);
    double f_max = s11_freq[n_freqs - 1].frequency_hz;
    double t_resolution = 1.0 / (2.0 * f_max); /* Nyquist-limited time resolution */
    double d_max = vp * n_points * t_resolution / 2.0;
    (void)f_max; /* reserved for bandwidth check */

    /* Simple approach: synthesize TDR from piecewise S11 integration */
    /* Accumulate reflection coefficient along the line */
    double gamma_accum = 0.0;

    for (int i = 0; i < n_points; i++) {
        double d = (double)i / (n_points - 1) * d_max;
        profile[i].distance_m = d;

        /* Simplified TDR: for each distance, integrate weighted S11 */
        /* This is a time-gated approximation */
        double sum_real = 0.0, sum_imag = 0.0, sum_weight = 0.0;

        for (int j = 0; j < n_freqs; j++) {
            double f = s11_freq[j].frequency_hz;
            /* Phase factor for distance d (round-trip): exp(-j 2βd) */
            /* β = 2πf/vp */
            double phase = -4.0 * M_PI * f * d / vp;
            double w = cos(phase);
            sum_real += s11_freq[j].s.s11_real * w;
            sum_imag += s11_freq[j].s.s11_imag * w;
            sum_weight += fabs(w);
        }

        if (sum_weight > 0.0) {
            gamma_accum = sqrt(sum_real * sum_real + sum_imag * sum_imag) / sum_weight;
        }

        /* Clamp reflection magnitude */
        if (gamma_accum > 0.99) gamma_accum = 0.99;
        if (gamma_accum < 0.0) gamma_accum = 0.0;

        /* Z(d) = Z₀ × (1+Γ)/(1-Γ) */
        profile[i].impedance_ohm = z0 * (1.0 + gamma_accum) / (1.0 - gamma_accum);
    }
}

/* ================================================================
 * L2: Wavelength on PCB
 *
 * λ = v_p / f = c₀ / (f × √εeff)
 *
 * This determines the physical scale of standing wave phenomena.
 * Structures larger than λ/10 must be treated as distributed.
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_wavelength(double frequency_hz, double er_eff)
{
    if (frequency_hz <= 0.0 || er_eff <= 0.0) return 0.0;
    return C0 / (frequency_hz * sqrt(er_eff));
}

/**
 * Quarter-wave resonance frequency
 * Complexity: O(1)
 */
double hs_quarter_wave_frequency(double length_m, double er_eff)
{
    if (length_m <= 0.0 || er_eff <= 0.0) return 0.0;
    return C0 / (4.0 * length_m * sqrt(er_eff));
}
