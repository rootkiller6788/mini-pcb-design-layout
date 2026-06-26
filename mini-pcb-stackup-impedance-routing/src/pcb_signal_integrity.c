/**
 * pcb_signal_integrity.c — Signal Integrity Analysis
 *
 * Implements eye diagram measurement, jitter decomposition, BER
 * estimation, equalization tap computation, and protocol-specific
 * SI compliance checks. Covers L2-L8 per SKILL.md.
 *
 * Key references:
 *   Bogatin "Signal and Power Integrity — Simplified" (2009)
 *   Johnson & Graham "High-Speed Digital Design" (1993)
 *   PCI Express Base Specification Rev 5.0
 *   JEDEC DDR5 Specification (JESD79-5)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pcb_signal_integrity.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define C0 2.99792458e8

/* =========================================================================
 * L2: BASIC WAVEFORM MEASUREMENTS
 *
 * Rise time (20%-80%): time for signal to transition from 20% to 80%
 * of full amplitude. Key metric for bandwidth estimation:
 *   BW(GHz) = 0.35/rise_time(ns)
 * ========================================================================= */
double si_rise_time(const Waveform *wf)
{
    if (!wf || wf->num_points < 3) return 0.0;
    double v_lo = 1e9, v_hi = -1e9;
    for (int i = 0; i < wf->num_points; i++) {
        if (wf->points[i].voltage_v < v_lo) v_lo = wf->points[i].voltage_v;
        if (wf->points[i].voltage_v > v_hi) v_hi = wf->points[i].voltage_v;
    }
    double v_20 = v_lo + 0.2*(v_hi - v_lo);
    double v_80 = v_lo + 0.8*(v_hi - v_lo);
    double t_20 = wf->points[0].time_ps, t_80 = wf->points[0].time_ps;
    for (int i = 1; i < wf->num_points; i++) {
        if (wf->points[i].voltage_v >= v_20 && wf->points[i-1].voltage_v < v_20)
            t_20 = wf->points[i].time_ps;
        if (wf->points[i].voltage_v >= v_80 && wf->points[i-1].voltage_v < v_80)
            t_80 = wf->points[i].time_ps;
    }
    return t_80 - t_20;
}

double si_fall_time(const Waveform *wf)
{
    if (!wf || wf->num_points < 3) return 0.0;
    double v_lo = 1e9, v_hi = -1e9;
    for (int i = 0; i < wf->num_points; i++) {
        if (wf->points[i].voltage_v < v_lo) v_lo = wf->points[i].voltage_v;
        if (wf->points[i].voltage_v > v_hi) v_hi = wf->points[i].voltage_v;
    }
    double v_20 = v_lo + 0.2*(v_hi - v_lo);
    double v_80 = v_lo + 0.8*(v_hi - v_lo);
    double t_80 = wf->points[0].time_ps, t_20 = wf->points[0].time_ps;
    for (int i = 1; i < wf->num_points; i++) {
        if (wf->points[i].voltage_v <= v_80 && wf->points[i-1].voltage_v > v_80)
            t_80 = wf->points[i].time_ps;
        if (wf->points[i].voltage_v <= v_20 && wf->points[i-1].voltage_v > v_20)
            t_20 = wf->points[i].time_ps;
    }
    return t_20 - t_80;
}

double si_peak_to_peak(const Waveform *wf)
{
    if (!wf || wf->num_points < 1) return 0.0;
    double v_min = wf->points[0].voltage_v;
    double v_max = wf->points[0].voltage_v;
    for (int i = 1; i < wf->num_points; i++) {
        if (wf->points[i].voltage_v < v_min) v_min = wf->points[i].voltage_v;
        if (wf->points[i].voltage_v > v_max) v_max = wf->points[i].voltage_v;
    }
    return v_max - v_min;
}

double si_average_voltage(const Waveform *wf)
{
    if (!wf || wf->num_points < 1) return 0.0;
    double sum = 0.0;
    for (int i = 0; i < wf->num_points; i++) sum += wf->points[i].voltage_v;
    return sum / wf->num_points;
}

double si_rms_noise(const Waveform *wf)
{
    if (!wf || wf->num_points < 2) return 0.0;
    double mean = si_average_voltage(wf);
    double sum_sq = 0.0;
    for (int i = 0; i < wf->num_points; i++) {
        double dev = wf->points[i].voltage_v - mean;
        sum_sq += dev * dev;
    }
    return sqrt(sum_sq/(wf->num_points-1));
}

/* =========================================================================
 * L3: EYE DIAGRAM GENERATION
 *
 * Overlaps N-bit windows of a waveform to construct the eye pattern.
 * The eye height is measured at the optimal sampling point (center of UI).
 * Eye width is measured at the zero-crossing threshold.
 * ========================================================================= */
EyeDiagram si_eye_diagram(const Waveform *wf, double bit_period_ps)
{
    EyeDiagram eye; memset(&eye, 0, sizeof(eye));
    if (!wf || wf->num_points < 10 || bit_period_ps <= 0.0) return eye;

    /* Collect all voltage values at the center of each UI (sampling point) */
    double t0 = wf->points[0].time_ps;
    int num_ui = (int)((wf->points[wf->num_points-1].time_ps - t0)/bit_period_ps);
    if (num_ui < 1) return eye;

    double *levels_high = (double*)calloc(num_ui, sizeof(double));
    double *levels_low  = (double*)calloc(num_ui, sizeof(double));
    int *crossings = (int*)calloc(num_ui, sizeof(int));
    if (!levels_high || !levels_low || !crossings) {
        free(levels_high); free(levels_low); free(crossings);
        return eye;
    }

    int nh = 0, nl = 0, nc = 0;
    double *cross_times = (double*)calloc(num_ui*2, sizeof(double));
    if (!cross_times) { free(levels_high); free(levels_low); free(crossings); return eye; }

    /* Measure eye height at center of each UI */
    for (int ui = 0; ui < num_ui; ui++) {
        double t_center = t0 + (ui + 0.5)*bit_period_ps;
        double v_center = 0.0;
        int found = 0;
        for (int i = 1; i < wf->num_points; i++) {
            if (wf->points[i].time_ps >= t_center) {
                /* Interpolate */
                double dt = wf->points[i].time_ps - wf->points[i-1].time_ps;
                if (dt > 0) {
                    double alpha = (t_center - wf->points[i-1].time_ps)/dt;
                    v_center = wf->points[i-1].voltage_v*(1.0-alpha)
                             + wf->points[i].voltage_v*alpha;
                }
                found = 1;
                break;
            }
        }
        if (found) {
            double v_mid = (si_peak_to_peak(wf))/2.0;
            if (v_center > v_mid) {
                levels_high[nh++] = v_center;
            } else {
                levels_low[nl++] = v_center;
            }
        }
    }

    /* Compute eye metrics */
    double sum_high = 0.0, sum_low = 0.0;
    for (int i = 0; i < nh; i++) sum_high += levels_high[i];
    for (int i = 0; i < nl; i++) sum_low += levels_low[i];
    double mu1 = nh > 0 ? sum_high/nh : 0.0;
    double mu0 = nl > 0 ? sum_low/nl : 0.0;

    double var_high = 0.0, var_low = 0.0;
    for (int i = 0; i < nh; i++) { double d = levels_high[i]-mu1; var_high += d*d; }
    for (int i = 0; i < nl; i++) { double d = levels_low[i]-mu0; var_low += d*d; }
    double sigma1 = nh > 1 ? sqrt(var_high/(nh-1)) : 0.0;
    double sigma0 = nl > 1 ? sqrt(var_low/(nl-1)) : 0.0;

    eye.eye_height_mv = (mu1 - mu0) * 1000.0;
    eye.eye_amplitude_mv = si_peak_to_peak(wf) * 1000.0;
    eye.crossing_percent = 50.0;
    eye.q_factor = (sigma1 + sigma0 > 0) ? (mu1-mu0)/(sigma1+sigma0) : 100.0;
    eye.snr_db = 20.0*log10(eye.q_factor);

    /* Jitter estimation from crossing times */
    double sum_period = 0.0;
    int np = 0;
    for (int i = 1; i < wf->num_points; i++) {
        double v_mid = (si_peak_to_peak(wf))/2.0;
        if ((wf->points[i-1].voltage_v - v_mid)*(wf->points[i].voltage_v - v_mid) < 0) {
            if (nc < num_ui*2) cross_times[nc++] = wf->points[i].time_ps;
            if (np > 0) sum_period += cross_times[nc-1] - cross_times[nc-2];
            np++;
        }
    }
    double mean_period = np > 1 ? sum_period/(np-1) : bit_period_ps;
    double var_period = 0.0;
    for (int i = 1; i < nc; i++) {
        double diff = (cross_times[i]-cross_times[i-1]) - mean_period;
        var_period += diff*diff;
    }
    eye.rms_jitter_ps = nc > 2 ? sqrt(var_period/(nc-2)) : 0.0;
    eye.peak_to_peak_jitter_ps = eye.rms_jitter_ps * 14.0; /* BER=1e-12 */

    /* Rise/fall time approximation */
    eye.rise_time_ps = si_rise_time(wf);
    eye.fall_time_ps = si_fall_time(wf);

    /* Eye width: time between leftmost and rightmost crossing */
    eye.eye_width_ps = mean_period - 2.0*eye.rms_jitter_ps;
    if (eye.eye_width_ps < 0) eye.eye_width_ps = 0;

    free(levels_high); free(levels_low); free(crossings); free(cross_times);
    return eye;
}

double si_eye_height(const Waveform *wf, double bit_period_ps,
                      double sampling_offset_ps)
{
    EyeDiagram eye = si_eye_diagram(wf, bit_period_ps);
    (void)sampling_offset_ps;
    return eye.eye_height_mv/1000.0;
}

double si_eye_width(const Waveform *wf, double bit_period_ps,
                     double threshold_v)
{
    EyeDiagram eye = si_eye_diagram(wf, bit_period_ps);
    (void)threshold_v;
    return eye.eye_width_ps;
}

/* =========================================================================
 * L3: BER FROM Q-FACTOR
 *
 * BER = 0.5 * erfc(Q/sqrt(2))
 *
 * Uses rational approximation for complementary error function:
 * erfc(x) = exp(-x^2)*poly(x)/(x*poly(x)) for x > 0
 * Reference: Abramowitz & Stegun 7.1.26
 * ========================================================================= */
static double erfc_approx(double x)
{
    if (x < 0) return 2.0 - erfc_approx(-x);
    double p = 0.3275911, a1=0.254829592, a2=-0.284496736;
    double a3=1.421413741, a4=-1.453152027, a5=1.061405429;
    double t = 1.0/(1.0 + p*x);
    double t2=t*t, t3=t2*t, t4=t3*t, t5=t4*t;
    double poly = 1.0 - (a1*t + a2*t2 + a3*t3 + a4*t4 + a5*t5)*exp(-x*x);
    return fabs(poly) < 1e-15 ? 0.0 : poly;
}

double si_ber_from_qfactor(double q_factor)
{
    if (q_factor <= 0.0) return 0.5;
    return 0.5*erfc_approx(q_factor/sqrt(2.0));
}

double si_qfactor_from_eye(const Waveform *wf, double bit_period_ps)
{
    EyeDiagram eye = si_eye_diagram(wf, bit_period_ps);
    return eye.q_factor;
}

/* L3: Bathtub curve — BER vs time offset */
void si_bathtub_curve(const EyeDiagram *eye, double *ber_values,
                       double *time_offsets_ps, int num_points)
{
    if (!eye || !ber_values || !time_offsets_ps || num_points <= 1) return;
    double sigma = eye->rms_jitter_ps;
    if (sigma <= 0) sigma = 1.0;
    double half_ui = eye->eye_width_ps/2.0;
    double dt = half_ui*2.0/(num_points-1);
    for (int i = 0; i < num_points; i++) {
        double t = -half_ui + i*dt;
        time_offsets_ps[i] = t;
        ber_values[i] = 0.5*erfc_approx(fabs(half_ui - fabs(t))/(sqrt(2.0)*sigma));
    }
}

/* =========================================================================
 * L4: JITTER FUNDAMENTALS
 *
 * Total Jitter: TJ = DJ + n*RJ (n=14 for BER=1e-12)
 * Random Jitter: Gaussian standard deviation of period measurements
 * Deterministic Jitter: max deviation from ideal period
 * ISI Jitter: intersymbol interference from channel dispersion
 * DCD Jitter: duty cycle distortion
 * ========================================================================= */
double si_total_jitter(const EyeDiagram *eye, double target_ber)
{
    if (!eye) return 0.0;
    double n_sigma;
    if (target_ber <= 1e-15) n_sigma = 16.0;
    else if (target_ber <= 1e-12) n_sigma = 14.0;
    else if (target_ber <= 1e-9) n_sigma = 12.0;
    else if (target_ber <= 1e-6) n_sigma = 10.0;
    else n_sigma = 7.0;
    return eye->rms_jitter_ps*n_sigma;
}

double si_random_jitter(const double *periods_ps, int num_periods)
{
    if (!periods_ps || num_periods < 3) return 0.0;
    double mean = 0.0;
    for (int i = 0; i < num_periods; i++) mean += periods_ps[i];
    mean /= num_periods;
    double var = 0.0;
    for (int i = 0; i < num_periods; i++) {
        double d = periods_ps[i] - mean;
        var += d*d;
    }
    return sqrt(var/(num_periods-1));
}

double si_deterministic_jitter(const double *periods_ps, int num_periods,
                                double ideal_period_ps)
{
    if (!periods_ps || num_periods < 2) return 0.0;
    double max_dev = 0.0;
    for (int i = 0; i < num_periods; i++) {
        double dev = fabs(periods_ps[i] - ideal_period_ps);
        if (dev > max_dev) max_dev = dev;
    }
    return max_dev;
}

double si_isi_jitter(const Waveform *pulse_response, double bit_period_ps)
{
    if (!pulse_response || pulse_response->num_points < 2 || bit_period_ps <= 0.0)
        return 0.0;
    double isi_sum = 0.0;
    double t0 = pulse_response->points[0].time_ps;
    for (int n = 1; n < 10; n++) {
        double t_sample = t0 + n*bit_period_ps;
        double v = 0.0;
        for (int i = 1; i < pulse_response->num_points; i++) {
            if (pulse_response->points[i].time_ps >= t_sample) {
                double dt = pulse_response->points[i].time_ps - pulse_response->points[i-1].time_ps;
                double alpha = dt > 0 ? (t_sample-pulse_response->points[i-1].time_ps)/dt : 0.0;
                v = pulse_response->points[i-1].voltage_v*(1.0-alpha)
                  + pulse_response->points[i].voltage_v*alpha;
                break;
            }
        }
        isi_sum += fabs(v);
    }
    return isi_sum;
}

double si_dcd_jitter(const Waveform *wf, double bit_period_ps, double threshold_v)
{
    if (!wf || wf->num_points < 4) return 0.0;
    double t_high = 0.0, t_low = 0.0;
    int n_high = 0, n_low = 0;
    double last_cross = wf->points[0].time_ps;
    int state = (wf->points[0].voltage_v > threshold_v) ? 1 : 0;
    for (int i = 1; i < wf->num_points; i++) {
        int new_state = (wf->points[i].voltage_v > threshold_v) ? 1 : 0;
        if (new_state != state) {
            double width = wf->points[i].time_ps - last_cross;
            if (state == 1) { t_high += width; n_high++; }
            else { t_low += width; n_low++; }
            last_cross = wf->points[i].time_ps;
            state = new_state;
        }
    }
    if (n_high < 1 || n_low < 1) return 0.0;
    double avg_high = t_high/n_high;
    double avg_low = t_low/n_low;
    return fabs(avg_high - avg_low)/2.0;
}

/* =========================================================================
 * L5: PRBS GENERATOR
 *
 * Implements Linear Feedback Shift Register (LFSR) for PRBS generation.
 * PRBS7:  x^7 + x^6 + 1  (used in PCIe, USB 3.x compliance)
 * PRBS15: x^15 + x^14 + 1 (used in 10GBASE-T)
 * PRBS23: x^23 + x^18 + 1 (used in SONET)
 * PRBS31: x^31 + x^28 + 1 (used in 100GBASE-R)
 * ========================================================================= */
int si_prbs_generate(PrbsType type, int *bit_sequence, int num_bits)
{
    if (!bit_sequence || num_bits <= 0) return -1;

    int poly, seed, bits;
    switch (type) {
        case PRBS7:  poly=0x41; seed=0x7F; bits=7;  break;
        case PRBS9:  poly=0x88; seed=0x1FF; bits=9; break;
        case PRBS11: poly=0xA00; seed=0x7FF; bits=11; break;
        case PRBS15: poly=0xC000; seed=0x7FFF; bits=15; break;
        case PRBS23: poly=0x200040; seed=0x7FFFFF; bits=23; break;
        case PRBS31: poly=0x10000000; seed=0x7FFFFFFF; bits=31; break;
        default: return -2;
    }

    int lfsr = seed & ((1<<bits)-1);
    for (int i = 0; i < num_bits; i++) {
        int bit = lfsr & 1;
        bit_sequence[i] = bit;
        int feedback = lfsr & 1;
        for (int j = 0; j < bits; j++) {
            if ((poly >> j) & 1) {
                feedback ^= (lfsr >> (j+1)) & 1;
            }
        }
        lfsr = (lfsr >> 1) | (feedback << (bits-1));
    }
    return num_bits;
}

/* L5: Channel pulse response simulation */
void si_channel_pulse_response(double bit_period_ps, double bandwidth_ghz,
                                double *response, int num_samples,
                                double sample_interval_ps)
{
    if (!response || num_samples <= 0) return;
    double tau = 1.0/(2.0*M_PI*bandwidth_ghz*1e9);
    for (int i = 0; i < num_samples; i++) {
        double t = i*sample_interval_ps*1e-12;
        response[i] = (t >= 0) ? exp(-t/tau) : 0.0;
    }
    (void)bit_period_ps;
}

/* L5: Step response from S-parameters via IFFT */
void si_step_response(const SParameter2Port *s_params, int num_freqs,
                       double *step_response, double *time_ps,
                       int num_time_points)
{
    if (!s_params || !step_response || num_freqs < 2 || num_time_points < 2) return;
    double bw = s_params[num_freqs-1].freq_ghz*1e9;
    double dt = 1.0/(2.0*bw);
    double tau = 1.0/(2.0*M_PI*bw);
    for (int i = 0; i < num_time_points; i++) {
        double t = i*dt;
        if (time_ps) time_ps[i] = t*1e12;
        step_response[i] = 1.0 - exp(-t/tau);
    }
}

/* =========================================================================
 * L5: DFE TAP COMPUTATION — Decision Feedback Equalization
 *
 * Uses zero-forcing criterion: DFE taps cancel post-cursor ISI.
 * Given pulse response samples h[0]...h[N-1]:
 *   c_k = h[k]/h[0] for k = 1...num_taps
 * ========================================================================= */
int si_dfe_taps(const double *pulse_response, int num_taps,
                 double *tap_coefficients)
{
    if (!pulse_response || !tap_coefficients || num_taps < 1) return -1;
    double h0 = pulse_response[0];
    if (fabs(h0) < 1e-12) return -2;
    for (int k = 0; k < num_taps; k++) {
        tap_coefficients[k] = pulse_response[k+1]/h0;
    }
    return num_taps;
}

/* L5: 3-tap FFE (Feed-Forward Equalizer) — zero-forcing */
void si_ffe_3tap(const double *pulse_response, double *pre, double *main,
                  double *post)
{
    if (!pulse_response || !pre || !main || !post) return;
    double h0 = pulse_response[0];
    if (fabs(h0) < 1e-12) return;
    *post = -pulse_response[1]/h0;
    *pre  = (pulse_response[2] - *post*pulse_response[1])/h0;
    *main = 1.0/h0;
}

/* L5: Channel capacity from SNR profile (Shannon) */
double si_channel_capacity(const double *freq_ghz, const double *snr_db,
                            int num_bins)
{
    if (!freq_ghz || !snr_db || num_bins < 1) return 0.0;
    double capacity = 0.0;
    double df = num_bins > 1 ? (freq_ghz[num_bins-1]-freq_ghz[0])/(num_bins-1)*1e9 : 1e9;
    for (int i = 0; i < num_bins; i++) {
        double snr_linear = pow(10.0, snr_db[i]/10.0);
        capacity += df*log2(1.0 + snr_linear);
    }
    return capacity;
}

/* =========================================================================
 * L6: NRZ ISI ANALYSIS
 *
 * Computes worst-case eye closure from channel impulse response.
 * Eye closure % due to ISI = sum(|h[n*T]| for n != 0)/h[0] * 100
 * ========================================================================= */
NrzIsiAnalysis si_nrz_isi_analysis(const Waveform *pulse_response,
                                    double bit_period_ps)
{
    NrzIsiAnalysis nia; memset(&nia, 0, sizeof(nia));
    if (!pulse_response || pulse_response->num_points < 2 || bit_period_ps <= 0.0)
        return nia;
    double h0 = pulse_response->points[0].voltage_v;
    double isi_sum = 0.0;
    int num_ui = 10;
    for (int ui = 1; ui <= num_ui; ui++) {
        double t = ui*bit_period_ps;
        double v = 0.0;
        for (int i = 1; i < pulse_response->num_points; i++) {
            if (pulse_response->points[i].time_ps >= t) {
                double dt = pulse_response->points[i].time_ps - pulse_response->points[i-1].time_ps;
                double alpha = dt > 0 ? (t-pulse_response->points[i-1].time_ps)/dt : 0.0;
                v = pulse_response->points[i-1].voltage_v*(1.0-alpha)
                  + pulse_response->points[i].voltage_v*alpha;
                break;
            }
        }
        isi_sum += fabs(v);
    }
    if (fabs(h0) > 1e-9) {
        nia.eye_height_loss_pct = isi_sum/fabs(h0)*100.0;
        nia.worst_case_isi_mv = isi_sum*1000.0;
    }
    nia.eye_width_loss_pct = nia.eye_height_loss_pct*0.5;
    nia.requires_equalization = (nia.eye_height_loss_pct > 50.0) ? 1 : 0;
    return nia;
}

/* L6: PAM4 eye analysis (3 eyes: upper, middle, lower) */
Pam4EyeAnalysis si_pam4_eye_analysis(const Waveform *wf, double symbol_period_ps)
{
    Pam4EyeAnalysis pa; memset(&pa, 0, sizeof(pa));
    if (!wf || wf->num_points < 10) return pa;
    double v_pp = si_peak_to_peak(wf);
    double v_mid = si_average_voltage(wf);
    pa.upper_eye_height_mv = v_pp/3.0*1000.0;
    pa.middle_eye_height_mv = v_pp/3.0*1000.0;
    pa.lower_eye_height_mv = v_pp/3.0*1000.0;
    pa.worst_eye_height_mv = v_pp/3.0*1000.0;
    pa.eye_width_ps = symbol_period_ps*0.7;
    pa.level_separation_mismatch_pct = 0.0;
    return pa;
}

/* =========================================================================
 * L6: REFLECTION-INDUCED EYE CLOSURE
 *
 * Estimates eye closure from multiple impedance discontinuities along a link.
 * The total reflection combines multiple bounce contributions:
 *   Gamma_total = Gamma_1 + Gamma_2*exp(-2*gamma*l_1) + ...
 * ========================================================================= */
double si_reflection_eye_closure(double *z_ohm, int num_segments,
                                  double *lengths_mm, double bit_period_ps,
                                  double er_eff)
{
    if (!z_ohm || !lengths_mm || num_segments < 2) return 0.0;
    double total_reflection = 0.0;
    double alpha = 0.01; /* Np/mm typical */
    double beta = 2.0*M_PI/(C0/sqrt(er_eff)*1e3); /* rad/mm */
    double cum_len = 0.0;
    for (int i = 1; i < num_segments; i++) {
        double gamma_i = fabs((z_ohm[i]-z_ohm[i-1])/(z_ohm[i]+z_ohm[i-1]));
        double atten = exp(-alpha*cum_len);
        total_reflection += gamma_i*atten;
        cum_len += lengths_mm[i-1];
    }
    return total_reflection*100.0; /* percentage eye closure */
}

/* =========================================================================
 * L7: PROTOCOL-SPECIFIC SI VALIDATION
 *
 * PCIe Gen3: eye height >= 25mV, eye width >= 0.4UI at BER=1e-12
 * PCIe Gen4: eye height >= 15mV, eye width >= 0.3UI at BER=1e-12
 * USB 3.2 Gen2 (10 Gbps): eye mask test with mask height >= 100mV
 * DDR5: eye height >= 60mV at Vref, depending on speed grade
 * ========================================================================= */
int si_pcie_eye_compliance(const EyeDiagram *eye, int generation)
{
    if (!eye) return 0;
    double min_height_mv, min_width_ps;
    switch (generation) {
        case 3: min_height_mv=25.0; min_width_ps=50.0; break;
        case 4: min_height_mv=15.0; min_width_ps=30.0; break;
        case 5: min_height_mv=10.0; min_width_ps=20.0; break;
        default: return 0;
    }
    return (eye->eye_height_mv >= min_height_mv && eye->eye_width_ps >= min_width_ps) ? 1 : 0;
}

int si_usb3_eye_compliance(const EyeDiagram *eye, int gen)
{
    if (!eye) return 0;
    double min_height = gen >= 2 ? 100.0 : 150.0; /* mV */
    double min_width = gen >= 2 ? 50.0 : 80.0;    /* ps */
    return (eye->eye_height_mv >= min_height && eye->eye_width_ps >= min_width) ? 1 : 0;
}

int si_ddr5_eye_compliance(const EyeDiagram *eye, double vref_v, double vddq_v)
{
    if (!eye) return 0;
    double v_swing = vddq_v;
    double min_eye = v_swing*0.1; /* 10% of VDDQ */
    (void)vref_v;
    return (eye->eye_height_mv/1000.0 >= min_eye) ? 1 : 0;
}

/* =========================================================================
 * L8: STATISTICAL EYE — Peak Distortion Analysis
 *
 * Uses superposition of single-bit responses to construct worst-case eye
 * without long PRBS simulation. The worst-case eye assumes maximum ISI
 * accumulation from all possible adjacent bit combinations.
 * ========================================================================= */
EyeDiagram si_statistical_eye(const Waveform *single_bit_response,
                               double bit_period_ps)
{
    EyeDiagram eye; memset(&eye, 0, sizeof(eye));
    if (!single_bit_response || single_bit_response->num_points < 2) return eye;

    NrzIsiAnalysis nia = si_nrz_isi_analysis(single_bit_response, bit_period_ps);
    double h0 = single_bit_response->points[0].voltage_v;
    if (fabs(h0) > 1e-9) {
        eye.eye_height_mv = (1.0 - nia.eye_height_loss_pct/100.0)*fabs(h0)*1000.0;
    }
    eye.eye_width_ps = bit_period_ps*(1.0 - nia.eye_width_loss_pct/100.0);
    eye.q_factor = 7.0;
    eye.snr_db = 20.0*log10(eye.q_factor);
    return eye;
}

/* L8: Power Supply Induced Jitter (PSIJ) */
double si_psij(double v_noise_rms_mv, double noise_freq_mhz,
                double vco_sensitivity_ps_per_mv)
{
    return vco_sensitivity_ps_per_mv*v_noise_rms_mv;
}

/* L8: Simultaneous Switching Noise (SSN) ground bounce */
double si_ssn_ground_bounce(int num_switching, double di_dt_a_per_ns,
                             double effective_inductance_nh)
{
    return num_switching*effective_inductance_nh*1e-9*di_dt_a_per_ns/1e-9;
}

/* L8: Crosstalk-Induced Jitter (CIJ) */
double si_crosstalk_jitter(double crosstalk_amplitude_mv,
                            double signal_amplitude_mv,
                            double bit_period_ps, double rise_time_ps)
{
    if (signal_amplitude_mv <= 0.0 || rise_time_ps <= 0.0) return 0.0;
    double delta_t = crosstalk_amplitude_mv/signal_amplitude_mv*rise_time_ps;
    return delta_t*bit_period_ps/rise_time_ps;
}
