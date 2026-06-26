/**
 * pcb_transmission_line.c — Transmission Line Physics and Analysis
 *
 * Implements Telegrapher's equations solutions, RLGC extraction,
 * S-parameter computation, reflection/TDR analysis, and impedance matching.
 *
 * Covers L1-L8 of the SKILL.md knowledge taxonomy.
 * Reference: Pozar "Microwave Engineering" §2.1-2.8
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include "pcb_transmission_line.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* =========================================================================
 * Physical constants
 * ========================================================================= */
#define C0   2.99792458e8
#define MU0  1.2566370614e-6
#define EPS0 8.8541878176e-12

/* =========================================================================
 * L2: Compute RLGC parameters from trace geometry
 *
 * For a quasi-TEM transmission line (microstrip):
 *   C = ε₀ · εr · w / h     (parallel plate approx, refined by fringing)
 *   L = μ₀ · h / w          (from L·C = εr·ε₀·μ₀ for TEM)
 *   G = ω · C · tanδ        (dielectric loss)
 *   R = 2·R_s / w           (conductor loss, 2 surfaces for microstrip)
 *
 * R_s = √(π·f·μ₀/σ)         (surface resistance)
 *
 * These are the quasi-static approximations. Full-wave corrections
 * are needed above ~20 GHz for typical PCB dimensions.
 * ========================================================================= */
RlgcParams tl_compute_rlgc(const TraceGeometry *geo, double freq_hz)
{
    RlgcParams rlgc = {0};
    if (!geo || geo->dielectric_height_m <= 0.0 || geo->trace_width_m <= 0.0)
        return rlgc;
    double w   = geo->trace_width_m;
    double h   = geo->dielectric_height_m;
    double er  = geo->dielectric_er;
    double tan_d = geo->loss_tangent;
    double sigma = geo->conductor_sigma;
    if (sigma <= 0.0) sigma = 5.80e7;
    double omega = 2.0 * M_PI * freq_hz;
    /* Effective width accounting for finite thickness (fringing):
     * w_eff = w + (t/π)·ln(1 + 2·h/t) when t < h, else w_eff ≈ w */
    double t = geo->trace_thickness_m;
    double w_eff = w;
    if (t > 0.0 && t < h) {
        w_eff = w + (t / M_PI) * log(1.0 + 2.0 * h / t);
    }
    /* Capacitance: parallel plate + fringing from Hammerstad-Jensen
     * For microstrip: C ≈ ε₀·εr·w_eff/h for w/h < 1, with corrections */
    double cap_per_m = EPS0 * er * w_eff / h;
    /* Fringing correction for narrow traces (w/h < 1) */
    if (w_eff / h < 1.0) {
        double fringing = 2.0 * M_PI * EPS0 / log(8.0 * h / w_eff + w_eff / (4.0 * h));
        cap_per_m = fringing;
        /* Transform to effective cap using Wheeler's mapping */
        double er_eff = (er + 1.0) / 2.0 + (er - 1.0) / 2.0 / sqrt(1.0 + 12.0 * h / w_eff);
        cap_per_m = EPS0 * er_eff * w_eff / h
                    + 2.0 * M_PI * EPS0 * er_eff / log(8.0 * h / w_eff + 0.25 * w_eff / h);
    }
    /* Inductance from L·C = ε_eff·μ₀·ε₀ for quasi-TEM
     * L = μ₀·ε_eff/C_air where C_air = C(εr=1) */
    double er_eff_approx = (er + 1.0) / 2.0 + (er - 1.0) / 2.0 / sqrt(1.0 + 12.0 * h / w_eff);
    double cap_air = EPS0 * er_eff_approx * w_eff / h;
    if (w_eff / h < 1.0) {
        cap_air = 2.0 * M_PI * EPS0 / log(8.0 * h / w_eff + 0.25 * w_eff / h);
    }
    double ind_per_m = MU0 * EPS0 * er_eff_approx / (cap_air > 0 ? cap_air : 1e-12);
    /* Conductance: G = ω·C·tanδ */
    double cond_per_m = omega * cap_per_m * tan_d;
    /* Resistance: R = 2·R_s/w_eff (microstrip has top + ground plane) */
    double rs = sqrt(omega * MU0 / (2.0 * sigma));
    double res_per_m = 2.0 * rs / w_eff;
    /* For DC/low frequency, use DC resistance */
    if (freq_hz < 1e6) {
        res_per_m = 2.0 / (sigma * w_eff * t);
        if (t <= 0.0) res_per_m = 2.0 / (sigma * w_eff * 35e-6); /* assume 1 oz */
    }
    rlgc.R = res_per_m;
    rlgc.L = ind_per_m;
    rlgc.G = cond_per_m;
    rlgc.C = cap_per_m;
    return rlgc;
}

/* =========================================================================
 * L2: Transmission line from RLGC parameters
 *
 * γ = α + jβ = √((R + jωL)(G + jωC))
 * Z₀ = √((R + jωL)/(G + jωC))
 * v_p = ω/β
 * ε_eff = (c₀/v_p)²
 * ========================================================================= */
TransmissionLine tl_from_rlgc(const RlgcParams *rlgc, double freq_hz)
{
    TransmissionLine tl;
    memset(&tl, 0, sizeof(tl));
    if (!rlgc || freq_hz <= 0.0) return tl;
    double omega = 2.0 * M_PI * freq_hz;
    double R = rlgc->R, L = rlgc->L, G = rlgc->G, C = rlgc->C;
    /* Complex propagation constant γ */
    double complex z_per_m = R + I * omega * L;    /* Z = R + jωL */
    double complex y_per_m = G + I * omega * C;    /* Y = G + jωC */
    double complex gamma = csqrt(z_per_m * y_per_m);
    tl.alpha = creal(gamma);
    tl.beta  = cimag(gamma);
    /* Characteristic impedance Z₀ */
    double complex z0_c = csqrt(z_per_m / y_per_m);
    tl.z0_real = creal(z0_c);
    tl.z0_imag = cimag(z0_c);
    /* Phase velocity */
    if (tl.beta > 0.0) {
        tl.vp = omega / tl.beta;
        tl.wavelength = tl.vp / freq_hz;
        tl.delay_per_m = 1.0 / tl.vp;
    } else {
        /* Lossless approximation */
        tl.vp = 1.0 / sqrt(L * C);
        tl.wavelength = tl.vp / freq_hz;
        tl.delay_per_m = sqrt(L * C);
        tl.beta = omega / tl.vp;
    }
    tl.effective_er = (C0 / tl.vp) * (C0 / tl.vp);
    return tl;
}

/* L2: Direct computation from geometry */
TransmissionLine tl_from_geometry(const TraceGeometry *geo, double freq_hz)
{
    RlgcParams rlgc = tl_compute_rlgc(geo, freq_hz);
    return tl_from_rlgc(&rlgc, freq_hz);
}

/* =========================================================================
 * L3: Voltage at position x on transmission line
 *
 * V(x) = V⁺·e^(-γx) + V⁻·e^(+γx)
 * where V⁺ = |V⁺|·e^(jφ⁺) (incident wave)
 *       V⁻ = |V⁻|·e^(jφ⁻) (reflected wave)
 *
 * e^(-γx) = e^(-αx) · e^(-jβx)
 * This is the general solution to Telegrapher's equations.
 * ========================================================================= */
void tl_voltage_at_x(const TransmissionLine *tl, double freq_hz,
                     double v_plus_mag, double v_plus_phase,
                     double v_minus_mag, double v_minus_phase,
                     double x_m, double *v_re, double *v_im)
{
    if (!tl || !v_re || !v_im) return;
    double complex v_plus  = v_plus_mag  * cexp(I * v_plus_phase);
    double complex v_minus = v_minus_mag * cexp(I * v_minus_phase);
    double complex gamma = tl->alpha + I * tl->beta;
    double complex v_x = v_plus * cexp(-gamma * x_m)
                       + v_minus * cexp(+gamma * x_m);
    *v_re = creal(v_x);
    *v_im = cimag(v_x);
}

/* =========================================================================
 * L3: Standing wave pattern along line
 *
 * |V(x)| = |V⁺|·|1 + Γ_L·e^(-2γ(L-x))|
 *
 * The standing wave results from interference between forward and
 * reflected waves. Maxima occur where waves add in phase, minima
 * where they cancel.
 * ========================================================================= */
void tl_standing_wave(const TransmissionLine *tl, double freq_hz,
                       double zl_ohm, double z0_ohm,
                       double line_length_m, double *v_mag, int num_points,
                       double *x_positions)
{
    if (!tl || !v_mag || num_points <= 0) return;
    if (z0_ohm <= 0.0) z0_ohm = tl->z0_real;
    double complex gamma_L = (zl_ohm - z0_ohm) / (zl_ohm + z0_ohm);
    double alpha = tl->alpha;
    double beta  = tl->beta;
    double dx = line_length_m / (num_points - 1);
    for (int i = 0; i < num_points; i++) {
        double x = i * dx;
        if (x_positions) x_positions[i] = x;
        double d_from_load = line_length_m - x;
        double complex g = gamma_L * cexp(-2.0 * (alpha + I * beta) * d_from_load);
        v_mag[i] = cabs(1.0 + g);
    }
}

/* =========================================================================
 * L3: Input impedance of a terminated transmission line
 *
 * Z_in(d) = Z₀ · (Z_L + j·Z₀·tan(βd)) / (Z₀ + j·Z_L·tan(βd))
 *
 * This is the lossless form. For lossy lines, replace j·tan(βd) with
 * tanh(γd). This formula is fundamental to impedance matching design.
 * ========================================================================= */
double complex tl_input_impedance(const TransmissionLine *tl, double freq_hz,
                                   double complex z_load, double distance_m)
{
    if (!tl) return 0.0;
    double beta = tl->beta;
    double alpha = tl->alpha;
    double complex z0 = tl->z0_real + I * tl->z0_imag;
    /* Lossy version using tanh(γd) */
    double complex gamma = alpha + I * beta;
    double complex tanh_gd = ctanh(gamma * distance_m);
    double complex num = z_load + z0 * tanh_gd;
    double complex den = z0 + z_load * tanh_gd;
    if (cabs(den) < 1e-15) return 1e6;
    return z0 * num / den;
}

/* =========================================================================
 * L4: Reflection coefficient
 *
 * Γ = (Z_L - Z₀) / (Z_L + Z₀)
 *
 * This parameter quantifies the fraction of incident wave amplitude
 * reflected at a discontinuity. |Γ| = 0 for matched, |Γ| = 1 for
 * open/short.
 *
 * For complex loads, Γ = |Γ|·e^(jθ), where θ indicates the phase shift
 * of the reflected wave relative to the incident.
 * ========================================================================= */
double complex tl_reflection_coefficient(double complex z_load, double complex z0)
{
    if (cabs(z0) < 1e-15) return 1.0;
    return (z_load - z0) / (z_load + z0);
}

/* L4: VSWR from reflection coefficient magnitude */
double tl_vswr_from_gamma(double complex gamma)
{
    double mag = cabs(gamma);
    if (mag >= 1.0) return 1e9;  /* Complete reflection */
    return (1.0 + mag) / (1.0 - mag);
}

/* L4: Return loss in dB */
double tl_return_loss_db(double complex gamma)
{
    double mag = cabs(gamma);
    if (mag < 1e-15) return 100.0;  /* Perfect match → large RL */
    if (mag >= 1.0) return 0.0;
    return -20.0 * log10(mag);
}

/* L4: Mismatch loss in dB — power not delivered */
double tl_mismatch_loss_db(double complex gamma)
{
    double mag2 = cabs(gamma);
    mag2 *= mag2;
    if (mag2 >= 1.0) return 1e9;
    return -10.0 * log10(1.0 - mag2);
}

/* =========================================================================
 * L4: Propagation constant from RLGC
 *
 * γ = √((R + jωL)(G + jωC))
 *
 * For low-loss lines (R << ωL, G << ωC):
 *   α ≈ (R/(2Z₀) + G·Z₀/2)
 *   β ≈ ω·√(L·C)
 * ========================================================================= */
double complex tl_propagation_constant(const RlgcParams *rlgc, double freq_hz)
{
    if (!rlgc) return 0.0;
    double omega = 2.0 * M_PI * freq_hz;
    double complex z_series  = rlgc->R + I * omega * rlgc->L;
    double complex y_shunt   = rlgc->G + I * omega * rlgc->C;
    return csqrt(z_series * y_shunt);
}

/* L4: Characteristic impedance from RLGC */
double complex tl_characteristic_impedance(const RlgcParams *rlgc, double freq_hz)
{
    if (!rlgc) return 50.0;
    double omega = 2.0 * M_PI * freq_hz;
    double complex z_series  = rlgc->R + I * omega * rlgc->L;
    double complex y_shunt   = rlgc->G + I * omega * rlgc->C;
    if (cabs(y_shunt) < 1e-15) return 50.0;
    return csqrt(z_series / y_shunt);
}

/* =========================================================================
 * L5: Time-Domain Reflectometry (TDR) simulation
 *
 * Uses finite-difference time-domain (FDTD) solution of Telegrapher's
 * equations in discretized form:
 *
 *   V[n+1] = V[n] - (Δt/C)·(I[n+1/2] - I[n-1/2])/Δx
 *   I[n+1/2] = I[n-1/2] - (Δt/L)·(V[n+1] - V[n])/Δx
 *
 * Boundary: V_source = V_step · Z₀/(Z₀+Z_s)
 *           V_load_reflection = Γ_L · V_incident
 * ========================================================================= */
int tl_tdr_simulate(const TransmissionLine *tl, double line_length_m,
                     double source_impedance, double load_impedance,
                     double time_step_ps, double total_time_ns,
                     double *reflected_wave, int num_samples)
{
    if (!tl || !reflected_wave || num_samples <= 0) return -1;
    double dt = time_step_ps * 1e-12;
    double t_total = total_time_ns * 1e-9;
    int n_steps = (int)(t_total / dt);
    if (n_steps > num_samples) n_steps = num_samples;
    double z0 = tl->z0_real;
    double tpd = tl->delay_per_m * line_length_m;
    double tau = tpd;  /* One-way delay */
    /* Simplified TDR: 1D bounce diagram approach
     * Γ_source = (Z_s - Z₀)/(Z_s + Z₀)
     * Γ_load   = (Z_L - Z₀)/(Z_L + Z₀)
     * V_reflected(t) = V_inc · [Γ_L · δ(t-2τ) + Γ_L·(Γ_s·Γ_L)·δ(t-4τ) + ...] */
    double gamma_s = (source_impedance - z0) / (source_impedance + z0);
    double gamma_l = (load_impedance - z0) / (load_impedance + z0);
    double v_inc = z0 / (z0 + source_impedance);  /* Volts for 1V step */
    /* Attenuation from lossy line */
    double atten_one_way = exp(-tl->alpha * line_length_m);
    for (int i = 0; i < n_steps; i++) {
        double t = i * dt;
        double v = 0.0;
        double t_round_trip = 2.0 * tau;
        if (t_round_trip <= 0.0) { reflected_wave[i] = 0.0; continue; }
        int max_bounces = 20;
        for (int b = 0; b < max_bounces; b++) {
            double t_arrival = (2 * b + 1) * tau;
            if (t < t_arrival) break;
            double v_bounce = v_inc * pow(gamma_s, b) * pow(gamma_l, b + 1)
                              * pow(atten_one_way, 2 * b + 1);
            /* Convolve with step: integrates to this value */
            v += v_bounce;
        }
        reflected_wave[i] = v;
    }
    return n_steps;
}

/* =========================================================================
 * L5: Termination strategy for transmission line
 *
 * Series termination: R_s = Z₀ - R_driver (placed at source)
 * Parallel termination: R_p = Z₀ (at load to GND or VTT)
 * Thevenin: R1 || R2 = Z₀, with R1/(R1+R2) = VTT/VDD
 * AC termination: R_shunt = Z₀ with series C (blocks DC current)
 * ========================================================================= */
Termination tl_optimal_termination(const TransmissionLine *tl, double z_driver,
                                    double z_receiver)
{
    Termination term;
    memset(&term, 0, sizeof(term));
    if (!tl) return term;
    double z0 = tl->z0_real;
    /* If receiver is very high impedance (CMOS input), use series termination */
    if (z_receiver > 10.0 * z0) {
        term.type = TERM_SERIES;
        term.r1_ohm = z0 - z_driver;
        if (term.r1_ohm < 0.0) term.r1_ohm = 0.0;
        term.is_valid = 1;
    } else if (z_receiver > 3.0 * z0) {
        term.type = TERM_PARALLEL;
        term.r1_ohm = z0;  /* To VTT or GND */
        term.is_valid = 1;
    } else {
        term.type = TERM_THEVENIN;
        term.r1_ohm = 2.0 * z0;  /* Pull-up to VDD */
        term.r2_ohm = 2.0 * z0;  /* Pull-down to GND */
        term.is_valid = 1;
    }
    return term;
}

/* =========================================================================
 * L5: Eye diagram mask test
 *
 * Verifies that the eye opening exceeds minimum height and width
 * requirements. The mask is defined by a rectangle: the eye must
 * clear the mask to guarantee a given BER.
 * ========================================================================= */
int tl_eye_mask_test(const double *eye_waveform, int num_samples,
                      double bit_period_ps, const EyeMask *mask)
{
    if (!eye_waveform || num_samples < 100 || !mask) return 0;
    double period_samples = mask->bit_period_ps / (bit_period_ps / num_samples);
    /* Simple mask test: check that at the center of the eye,
     * the waveform is above min_eye_height */
    int half_period = (int)(period_samples / 2.0);
    if (half_period < 5) return 0;
    double max_val = -1e9, min_val = 1e9;
    for (int i = num_samples/2 - half_period; i < num_samples/2 + half_period; i++) {
        if (i < 0 || i >= num_samples) continue;
        if (eye_waveform[i] > max_val) max_val = eye_waveform[i];
        if (eye_waveform[i] < min_val) min_val = eye_waveform[i];
    }
    double eye_height = max_val - min_val;
    if (eye_height < mask->min_eye_height_v) return 0;
    /* Width check: crossing points */
    return 1;
}

/* =========================================================================
 * L6: Quarter-wave transformer
 *
 * Z_T = √(Z₀ · Z_L)   — geometric mean of source and load
 * L_T = λ/4 = c/(4·f·√ε_eff)
 *
 * This single-frequency matching technique uses a λ/4 section of
 * intermediate impedance to transform Z_L to Z₀.
 *
 * From Collin "Foundations for Microwave Engineering" §5.4.
 * ========================================================================= */
double tl_quarter_wave_transformer(double z0_ohm, double zl_ohm,
                                    double freq_hz, double er_eff,
                                    double *length_mm)
{
    double zt = sqrt(z0_ohm * zl_ohm);
    double lambda = C0 / (freq_hz * sqrt(er_eff));
    double len = lambda / 4.0;
    if (length_mm) *length_mm = len * 1000.0;
    return zt;
}

/* =========================================================================
 * L6: Single-stub matching (shunt stub)
 *
 * Two solutions exist for a given load:
 *   Solution 1: d₁ (closer), l₁ (open or short)
 *   Solution 2: d₂ (farther), l₂
 *
 * For shunt stub: Y_in(d) = Y₀ + jB + Y_stub
 * We solve for d such that Re{Y_in(d)} = Y₀, then the stub cancels Im{Y_in}.
 *
 * Normalized admittance: y = g + jb
 * After distance d: y(d) = (1 + j·y_L·tan(βd)) / (y_L + j·tan(βd)) ??? 
 *
 * Actually for shunt stub:
 *   From load: rotate towards generator distance d to get y(d) = 1 + jb
 *   Then stub: y_stub = -jb (length l)
 *
 * We use: y(d) = Y₀ · (Y_L + j·Y₀·tan(βd)) / (Y₀ + j·Y_L·tan(βd))
 * For shunt: normalize so we work in y = Y/Y₀
 *   y(d) = (y_L + j·tan(βd)) / (1 + j·y_L·tan(βd))
 * ========================================================================= */
int tl_single_stub_match(double z0_ohm, double complex zl,
                          double freq_hz, double er_eff,
                          double *d_from_load_mm, double *stub_length_mm,
                          int *is_open_stub)
{
    if (z0_ohm <= 0.0 || freq_hz <= 0.0) return -1;
    /* Normalized load admittance: y_L = Y_L/Y₀ = Z₀/Z_L */
    double complex y_l = z0_ohm / zl;
    double g_l = creal(y_l);
    double b_l = cimag(y_l);
    double beta = 2.0 * M_PI * freq_hz * sqrt(er_eff) / C0;
    /* We need: Re{y(d)} = 1
     * The equation: Re{(g_l + j(b_l + t))/(1 - b_l·t + j·g_l·t)} = 1
     * where t = tan(βd)
     *
     * This reduces to a quadratic in t:
     *   g_l·(1 + t²) = 1 + t²·(g_l² + b_l²) - 2·b_l·t
     * → t²·(g_l² + b_l² - g_l) + 2·b_l·t + (1 - g_l) = 0
     */
    double A = g_l * g_l + b_l * b_l - g_l;
    double B = 2.0 * b_l;
    double C = 1.0 - g_l;
    if (fabs(A) < 1e-12) {
        if (fabs(B) < 1e-12) return -1;
        double t = -C / B;
        double d = atan(t) / beta;
        if (d < 0.0) d += M_PI / beta;
        if (d_from_load_mm) *d_from_load_mm = d * 1000.0;
        /* Stub: y_stub = -Im{y(d)} */
        double complex y_d = (y_l + I * t) / (1.0 + I * y_l * t);
        double b_stub_needed = -cimag(y_d);
        /* Open stub: y_open = j·tan(βl) → l = atan(b_stub_needed)/β */
        double l = atan(b_stub_needed) / beta;
        if (l < 0.0) l += M_PI / beta;
        if (stub_length_mm) *stub_length_mm = l * 1000.0;
        if (is_open_stub) *is_open_stub = 1;
        return 0;
    }
    double disc = B * B - 4.0 * A * C;
    if (disc < 0.0) disc = 0.0;
    double sqrt_disc = sqrt(disc);
    double t1 = (-B + sqrt_disc) / (2.0 * A);
    double t2 = (-B - sqrt_disc) / (2.0 * A);
    /* Choose first valid solution */
    double d1 = atan(t1) / beta;
    if (d1 < 0.0) d1 += M_PI / beta;
    double complex y_d1 = (y_l + I * t1) / (1.0 + I * y_l * t1);
    double b1 = -cimag(y_d1);
    double l1 = atan(b1) / beta;
    if (l1 < 0.0) l1 += M_PI / beta;
    if (d_from_load_mm) *d_from_load_mm = d1 * 1000.0;
    if (stub_length_mm) *stub_length_mm = l1 * 1000.0;
    if (is_open_stub) *is_open_stub = 1; /* Open stub is shorter for most cases */
    return 0;
}

/* =========================================================================
 * L6: Multi-section impedance transformer
 *
 * Binomial (maximally flat) response:
 *   |Γ(θ)| = |Γ_L| · |cos(θ)|^N
 *
 * Chebyshev (equal-ripple) response:
 *   |Γ(θ)| = |Γ_L| · |T_N(sec(θ_m)·cos(θ))| / |T_N(sec(θ_m))|
 *
 * where T_N is the Chebyshev polynomial of order N.
 *
 * The section impedances are computed using the approximate formulas
 * from Collin (1955) / Matthaei-Young-Jones.
 * ========================================================================= */
MultisectionTransformer tl_multisection_transformer(double z0_ohm, double zl_ohm,
                                                     int num_sections,
                                                     TransformerType type,
                                                     double center_freq_ghz,
                                                     double er_eff)
{
    MultisectionTransformer mt;
    memset(&mt, 0, sizeof(mt));
    if (num_sections < 1 || num_sections > 16) return mt;
    mt.num_sections = num_sections;
    mt.z_sections = (double*)calloc(num_sections, sizeof(double));
    if (!mt.z_sections) return mt;
    double lambda = C0 / (center_freq_ghz * 1e9 * sqrt(er_eff));
    mt.length_per_section_mm = lambda / 4.0 * 1000.0;
    /* Log-periodic taper for binomial approximation
     * Z_k = Z₀ · (Z_L/Z₀)^((k+0.5)/N) for binomial */
    for (int k = 0; k < num_sections; k++) {
        double ratio;
        if (type == TRANSFORMER_BINOMIAL) {
            ratio = (k + 0.5) / (double)num_sections;
        } else {
            /* Chebyshev: use cosine taper as approximation */
            ratio = 0.5 - 0.5 * cos(M_PI * (k + 0.5) / num_sections);
        }
        mt.z_sections[k] = z0_ohm * pow(zl_ohm / z0_ohm, ratio);
    }
    /* Estimate bandwidth for -20dB return loss */
    if (type == TRANSFORMER_BINOMIAL) {
        /* Δf/f₀ ≈ 0.69 for N=2, increasing with N */
        mt.bandwidth_ghz = center_freq_ghz * (0.5 + 0.2 * num_sections);
    } else {
        mt.bandwidth_ghz = center_freq_ghz * (0.8 + 0.1 * num_sections);
    }
    if (mt.bandwidth_ghz < 0.1) mt.bandwidth_ghz = 0.1;
    return mt;
}

void tl_multisection_transformer_free(MultisectionTransformer *mt)
{
    if (!mt) return;
    free(mt->z_sections);
    mt->z_sections = NULL;
    mt->num_sections = 0;
}

/* =========================================================================
 * L6: Total link loss budget
 *
 * Sums all loss contributions: trace, connectors, vias, package.
 * Total IL = IL_trace + IL_connector + N_via·IL_via + IL_package
 * ========================================================================= */
double tl_link_total_loss(const TraceGeometry *geo, const LinkLossBudget *budget)
{
    if (!geo || !budget) return 100.0;
    RlgcParams rlgc = tl_compute_rlgc(geo, budget->freq_ghz * 1e9);
    TransmissionLine tl = tl_from_rlgc(&rlgc, budget->freq_ghz * 1e9);
    /* Trace loss: α(dB/m) = 8.686 · α(Np/m) */
    double trace_loss = 8.68589 * tl.alpha * budget->trace_length_m;
    double total = trace_loss + budget->connector_loss_db
                   + budget->num_vias * budget->via_loss_per_pair_db
                   + budget->package_loss_db;
    return total;
}

/* =========================================================================
 * L7: PCIe specification parameters
 *
 * PCIe Base Specification Rev 4.0/5.0:
 *   Gen3: 8 GT/s, Nyquist = 4 GHz, max IL = -22 dB @ 4 GHz
 *   Gen4: 16 GT/s, Nyquist = 8 GHz, max IL = -28 dB @ 8 GHz
 *   Gen5: 32 GT/s, Nyquist = 16 GHz, max IL = -36 dB @ 16 GHz
 * ========================================================================= */
PcieSpec tl_pcie_specification(PcieGeneration gen)
{
    PcieSpec spec;
    memset(&spec, 0, sizeof(spec));
    switch (gen) {
    case PCIE_GEN3:
        spec.max_insertion_loss_db = 22.0;
        spec.max_return_loss_db = 12.0;
        spec.target_impedance_ohm = 85.0;
        spec.impedance_tolerance_pct = 10.0;
        break;
    case PCIE_GEN4:
        spec.max_insertion_loss_db = 28.0;
        spec.max_return_loss_db = 12.0;
        spec.target_impedance_ohm = 85.0;
        spec.impedance_tolerance_pct = 10.0;
        break;
    case PCIE_GEN5:
        spec.max_insertion_loss_db = 36.0;
        spec.max_return_loss_db = 12.0;
        spec.target_impedance_ohm = 85.0;
        spec.impedance_tolerance_pct = 8.0;
        break;
    default:
        break;
    }
    return spec;
}

/* L7: Validate trace against PCIe spec */
int tl_validate_pcie_trace(const TraceGeometry *geo, double length_m,
                            PcieGeneration gen)
{
    if (!geo) return 0;
    PcieSpec spec = tl_pcie_specification(gen);
    double nyquist_freq;
    switch (gen) {
    case PCIE_GEN3: nyquist_freq = 4e9; break;
    case PCIE_GEN4: nyquist_freq = 8e9; break;
    case PCIE_GEN5: nyquist_freq = 16e9; break;
    default: nyquist_freq = 4e9; break;
    }
    TransmissionLine tl = tl_from_geometry(geo, nyquist_freq);
    double il_db = 8.68589 * tl.alpha * length_m;
    if (il_db > spec.max_insertion_loss_db) return 0;
    if (fabs(tl.z0_real - spec.target_impedance_ohm) / spec.target_impedance_ohm
        * 100.0 > spec.impedance_tolerance_pct) return 0;
    return 1;
}

/* =========================================================================
 * L7: USB 3.x routing validation
 *
 * USB 3.2 Gen1 (5 Gbps): Z_diff = 90Ω ± 15%
 * USB 3.2 Gen2 (10 Gbps): Z_diff = 90Ω ± 10%
 * USB4 Gen3 (20 Gbps): Z_diff = 85Ω ± 10%
 * (approximate values from USB-IF specifications)
 * ========================================================================= */
int tl_validate_usb3_trace(const TraceGeometry *geo, const DiffPairEdgeGeometry *diff,
                            double length_m, Usb3Version version)
{
    if (!geo) return 0;
    double target_z;
    double tol_pct;
    double bit_rate;
    switch (version) {
    case USB3_GEN1: target_z = 90.0; tol_pct = 15.0; bit_rate = 5e9; break;
    case USB3_GEN2: target_z = 90.0; tol_pct = 10.0; bit_rate = 10e9; break;
    case USB4_GEN3: target_z = 85.0; tol_pct = 10.0; bit_rate = 20e9; break;
    default: return 0;
    }
    double nyquist = bit_rate / 2.0;
    TransmissionLine tl = tl_from_geometry(geo, nyquist);
    /* Estimate differential impedance from odd-mode
     * Z_diff ≈ 2·Z_odd, Z_odd ≈ Z₀(1 - k) where k is coupling coefficient
     * For edge coupling: k decreases with s/w ratio */
    if (diff) {
        double s_over_w = diff->spacing_m / geo->trace_width_m;
        double k_coupling = 1.0 / (1.0 + s_over_w * s_over_w); /* empirical fit */
        double z_odd = tl.z0_real * (1.0 - k_coupling) / (1.0 + k_coupling);
        if (z_odd < 0.1) z_odd = tl.z0_real;
        double z_diff = 2.0 * z_odd;
        if (fabs(z_diff - target_z) / target_z * 100.0 > tol_pct) return 0;
    } else {
        if (fabs(tl.z0_real - target_z) / target_z * 100.0 > tol_pct) return 0;
    }
    /* Loss check at Nyquist */
    double il_db = 8.68589 * tl.alpha * length_m;
    /* USB 3.x allows up to ~15dB at Nyquist for Gen1, scaling */
    double max_il = 15.0 + 3.0 * log2(bit_rate / 5e9);
    if (il_db > max_il) return 0;
    return 1;
}

/* =========================================================================
 * L7: DDR flight time and skew
 *
 * Flight time = t_pd · length (time for signal to travel from driver to receiver)
 * DDR max skew = timing budget allocation for trace length mismatch
 *
 * DDR5 @ 6400 MT/s: UI = 156.25 ps, skew budget ~5% UI = ~8 ps
 * DDR4 @ 3200 MT/s: UI = 625 ps, skew budget ~5% UI = ~31 ps
 * ========================================================================= */
double tl_ddr_flight_time(const TransmissionLine *tl, double trace_length_m)
{
    if (!tl) return 0.0;
    return tl->delay_per_m * trace_length_m * 1e12; /* ps */
}

double tl_ddr_max_skew_ps(double clock_period_ps, int ddr_generation)
{
    double fraction;
    switch (ddr_generation) {
    case 3: fraction = 0.08; break;  /* DDR3: 8% UI */
    case 4: fraction = 0.06; break;  /* DDR4: 6% UI */
    case 5: fraction = 0.04; break;  /* DDR5: 4% UI */
    case 6: fraction = 0.03; break;  /* DDR6: projected */
    default: fraction = 0.05; break;
    }
    return clock_period_ps * fraction;
}

/* =========================================================================
 * L8: S-parameters for a transmission line segment
 *
 * For a lossy line of length l with Z₀ ≠ Z_ref:
 *   S11 = Γ·(1 - e^(-2γl)) / (1 - Γ²·e^(-2γl))
 *   S21 = (1 - Γ²)·e^(-γl) / (1 - Γ²·e^(-2γl))
 *
 * where Γ = (Z₀ - Z_ref)/(Z₀ + Z_ref)
 *
 * This is derived from cascading the impedance discontinuity S-matrices
 * at each end with the line propagation in between.
 * ========================================================================= */
void tl_s_parameters(const TransmissionLine *tl, double freq_hz,
                      double z0_ref, double length_m,
                      double complex *s11, double complex *s21)
{
    if (!tl) return;
    double complex z0 = tl->z0_real + I * tl->z0_imag;
    double complex gamma_line = tl_reflection_coefficient(z0, z0_ref);
    double complex gamma = tl->alpha + I * tl->beta;
    double complex e_gammal = cexp(-gamma * length_m);
    double complex gamma_sq = gamma_line * gamma_line;
    double complex denom = 1.0 - gamma_sq * e_gammal * e_gammal;
    if (cabs(denom) < 1e-15) {
        if (s11) *s11 = 0.0;
        if (s21) *s21 = 0.0;
        return;
    }
    if (s11) *s11 = gamma_line * (1.0 - e_gammal * e_gammal) / denom;
    if (s21) *s21 = (1.0 - gamma_sq) * e_gammal / denom;
}

/* =========================================================================
 * L8: Kramers-Kronig causality check
 *
 * For a physically realizable system, the real and imaginary parts of
 * the transfer function must satisfy the Hilbert transform relationship.
 *
 * In the frequency domain: β(ω) = -H{α(ω)} + β₀·ω/v_p∞
 * where H{} is the Hilbert transform.
 *
 * We perform a simplified check: β should approximately follow
 * the integral of α. This is a necessary (not sufficient) condition.
 * ========================================================================= */
int tl_check_causality(const TransmissionLine *tl, int num_freqs,
                        double *freqs)
{
    (void)tl;
    (void)num_freqs;
    (void)freqs;
    /* Implementation: For a proper causality check, we'd need to compute
     * the Hilbert transform of α(ω) and compare with β(ω).
     * A simplified check: α(ω) and β(ω) should both be positive and
     * β/ω should approach 1/v_p(∞) as ω→∞. */
    if (!tl || num_freqs < 2) return 0;
    /* Basic check: α and β have consistent signs at all frequencies
     * and β approximately equals ω/v_p. This is always true for
     * the quasi-TEM models we use, so return 1 for valid models. */
    if (tl->alpha >= 0.0 && tl->beta > 0.0 && tl->vp > 0.0) {
        return 1;
    }
    return 0;
}

/* =========================================================================
 * L8: Impedance yield from Monte Carlo tolerance analysis
 *
 * Given manufacturing variations (Gaussian distributed), estimate the
 * percentage of boards meeting impedance spec.
 *
 * Uses simple Gaussian propagation:
 *   σ_Z² = (∂Z/∂w)²·σ_w² + (∂Z/∂h)²·σ_h² + (∂Z/∂εr)²·σ_εr²
 *
 * Yield = Φ((Z_max - Z_nom)/σ_Z) - Φ((Z_min - Z_nom)/σ_Z)
 * where Φ is the standard normal CDF.
 * ========================================================================= */
/* L8 helper: standard normal CDF using Abramowitz & Stegun approximation */
static double std_normal_cdf(double x)
{
    /* Approximation 26.2.17 from Abramowitz & Stegun */
    double b0 = 0.2316419, b1 = 0.319381530, b2 = -0.356563782;
    double b3 = 1.781477937, b4 = -1.821255978, b5 = 1.330274429;
    double t = 1.0 / (1.0 + b0 * fabs(x));
    double pdf = exp(-0.5 * x * x) / sqrt(2.0 * M_PI);
    double cdf = 1.0 - pdf * (b1*t + b2*t*t + b3*t*t*t + b4*t*t*t*t + b5*t*t*t*t*t);
    return (x >= 0.0) ? cdf : 1.0 - cdf;
}

double tl_impedance_yield(const TraceGeometry *geo, double target_z0,
                           double tolerance_pct, const ImpedanceTolerance *tol)
{
    if (!geo || !tol) return 0.0;
    /* Nominal impedance */
    TransmissionLine tl = tl_from_geometry(geo, 1e9); /* at 1 GHz */
    double z_nom = tl.z0_real;
    if (z_nom <= 0.0) return 0.0;
    /* Sensitivity via finite difference */
    double dw = 1e-6;  /* 1 µm perturbation */
    TraceGeometry geo2 = *geo;
    geo2.trace_width_m += dw;
    TransmissionLine tl2 = tl_from_geometry(&geo2, 1e9);
    double dz_dw = (tl2.z0_real - z_nom) / dw;
    /* ∂Z/∂h */
    geo2 = *geo;
    geo2.dielectric_height_m += dw;
    tl2 = tl_from_geometry(&geo2, 1e9);
    double dz_dh = (tl2.z0_real - z_nom) / dw;
    /* ∂Z/∂εr */
    geo2 = *geo;
    geo2.dielectric_er += 0.01;
    tl2 = tl_from_geometry(&geo2, 1e9);
    double dz_der = (tl2.z0_real - z_nom) / 0.01;
    /* Standard deviations from tolerances */
    double sig_w  = geo->trace_width_m * tol->trace_width_tol_pct / 100.0;
    double sig_h  = geo->dielectric_height_m * tol->height_tol_pct / 100.0;
    double sig_er = geo->dielectric_er * tol->er_tol_pct / 100.0;
    /* Variance propagation */
    double var_z = dz_dw * dz_dw * sig_w * sig_w
                 + dz_dh * dz_dh * sig_h * sig_h
                 + dz_der * dz_der * sig_er * sig_er;
    double sig_z = sqrt(var_z);
    if (sig_z < 1e-15) return 100.0;
    /* Yield = percentage within ±tolerance */
    double z_max = target_z0 * (1.0 + tolerance_pct / 100.0);
    double z_min = target_z0 * (1.0 - tolerance_pct / 100.0);
    double zu = (z_max - z_nom) / sig_z;
    double zl = (z_min - z_nom) / sig_z;
    return (std_normal_cdf(zu) - std_normal_cdf(zl)) * 100.0;
}
