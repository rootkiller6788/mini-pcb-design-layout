#ifndef PCB_SIGNAL_INTEGRITY_H
#define PCB_SIGNAL_INTEGRITY_H

#include "pcb_transmission_line.h"
#include <stddef.h>

/* ============================================================================
 * L1: Core Definitions — Signal Integrity Metrics
 *
 * Signal Integrity (SI) is the study of how electrical signals degrade
 * as they propagate through interconnects. Key degradation mechanisms:
 * reflection, attenuation, crosstalk, dispersion, and power supply noise.
 *
 * SI metrics quantify: timing (jitter, skew), amplitude (eye height, noise
 * margin), and spectral (S-parameters, TDR) signal quality.
 * ========================================================================= */

/* L1: Time-domain waveform point */
typedef struct {
    double time_ps;
    double voltage_v;
} WaveformPoint;

/* L1: Complete time-domain waveform */
typedef struct {
    WaveformPoint *points;
    int            num_points;
    double         bit_period_ps;
    double         sample_interval_ps;
} Waveform;

/* L1: Eye diagram measurement results */
typedef struct {
    double eye_height_mv;      /* Vertical eye opening (mV) */
    double eye_width_ps;       /* Horizontal eye opening at zero crossing (ps) */
    double eye_amplitude_mv;   /* Peak-to-peak amplitude (mV) */
    double crossing_percent;   /* Zero-crossing level as % of amplitude */
    double rms_jitter_ps;      /* RMS jitter */
    double peak_to_peak_jitter_ps; /* Pk-pk jitter */
    double rise_time_ps;       /* 20%-80% rise time */
    double fall_time_ps;       /* 80%-20% fall time */
    double q_factor;           /* (μ₁-μ₀)/(σ₁+σ₀) — signal quality */
    double snr_db;             /* Signal-to-noise ratio estimate */
} EyeDiagram;

/* L1: S-parameter at a single frequency */
typedef struct {
    double freq_ghz;
    double s11_mag_db, s11_phase_deg;
    double s21_mag_db, s21_phase_deg;
    double s12_mag_db, s12_phase_deg;
    double s22_mag_db, s22_phase_deg;
} SParameter2Port;

/* L1: Jitter decomposition results */
typedef struct {
    double tj_ps;              /* Total jitter */
    double dj_ps;              /* Deterministic jitter */
    double rj_rms_ps;          /* Random jitter (RMS) */
    double pj_ps;              /* Periodic jitter */
    double dcd_ps;             /* Duty cycle distortion */
    double isi_ps;             /* Inter-symbol interference jitter */
    double buj_ps;             /* Bounded uncorrelated jitter */
} JitterDecomposition;

/* ===================================================================
 * L2: Core Concepts — Basic SI measurements
 * =================================================================== */

/* L2: Compute rise time from waveform (20%-80% transition) */
double si_rise_time(const Waveform *wf);

/* L2: Compute fall time from waveform (80%-20% transition) */
double si_fall_time(const Waveform *wf);

/* L2: Compute peak-to-peak voltage */
double si_peak_to_peak(const Waveform *wf);

/* L2: Compute average voltage */
double si_average_voltage(const Waveform *wf);

/* L2: Compute RMS noise (standard deviation) */
double si_rms_noise(const Waveform *wf);

/* ===================================================================
 * L3: Mathematical Structures — Eye diagram analysis
 * =================================================================== */

/* L3: Generate eye diagram from PRBS waveform
 *     Overlaps N-bit windows to construct eye pattern.
 *     Input: time-domain waveform of a PRBS pattern
 *     Output: eye diagram metrics */
EyeDiagram si_eye_diagram(const Waveform *wf, double bit_period_ps);

/* L3: Compute eye height at optimal sampling point */
double si_eye_height(const Waveform *wf, double bit_period_ps,
                      double sampling_offset_ps);

/* L3: Compute eye width at threshold crossing */
double si_eye_width(const Waveform *wf, double bit_period_ps,
                     double threshold_v);

/* L3: Compute BER from eye Q-factor
 *     BER = 0.5 · erfc(Q / √2)  for Gaussian noise
 *     Uses rational approximation for erfc */
double si_ber_from_qfactor(double q_factor);

/* L3: Compute Q-factor from eye diagram
 *     Q = (μ₁ - μ₀) / (σ₁ + σ₀) */
double si_qfactor_from_eye(const Waveform *wf, double bit_period_ps);

/* L3: Bathtub curve — BER vs sampling time offset
 *     Computes BER at each sampling offset to build the bathtub curve.
 *     BER = 0.5·erfc(|t - t_ideal| / (√2 · σ_jitter)) */
void si_bathtub_curve(const EyeDiagram *eye, double *ber_values,
                       double *time_offsets_ps, int num_points);

/* ===================================================================
 * L4: Fundamental Laws — Jitter and noise fundamentals
 * =================================================================== */

/* L4: Compute total jitter from eye diagram
 *     TJ = DJ + n·RJ (for given BER, typically n=14 for BER=1e-12) */
double si_total_jitter(const EyeDiagram *eye, double target_ber);

/* L4: Compute random jitter from multiple period measurements
 *     RJ = σ(periods) using N-period analysis
 *     Assumes Gaussian distribution for random component */
double si_random_jitter(const double *periods_ps, int num_periods);

/* L4: Compute deterministic jitter from data pattern
 *     DJ = max(period deviation from ideal) using repeating pattern */
double si_deterministic_jitter(const double *periods_ps, int num_periods,
                                double ideal_period_ps);

/* L4: ISI jitter estimation from single-bit response
 *     ISI_jitter = Σ |SBR(t = n·T)| for n ≠ 0
 *     where SBR is the single-bit response of the channel */
double si_isi_jitter(const Waveform *pulse_response, double bit_period_ps);

/* L4: Duty cycle distortion jitter
 *     DCD = |t_high - t_low| / 2  in ps */
double si_dcd_jitter(const Waveform *wf, double bit_period_ps,
                      double threshold_v);

/* ===================================================================
 * L5: Algorithms — SI analysis methods
 * =================================================================== */

/* L5: Generate PRBS (Pseudo-Random Bit Sequence) data pattern
 *     PRBS7 (x⁷+x⁶+1), PRBS15 (x¹⁵+x¹⁴+1), PRBS23, PRBS31 */
typedef enum { PRBS7, PRBS9, PRBS11, PRBS15, PRBS23, PRBS31 } PrbsType;

int si_prbs_generate(PrbsType type, int *bit_sequence, int num_bits);

/* L5: Simulate channel pulse response
 *     Convolves a rectangular pulse with channel impulse response.
 *     Channel modeled as low-pass filter with given -3dB bandwidth. */
void si_channel_pulse_response(double bit_period_ps, double bandwidth_ghz,
                                double *response, int num_samples,
                                double sample_interval_ps);

/* L5: Compute channel step response from S-parameters
 *     Inverse Fourier Transform of H(ω) * (1/jω)
 *     Uses simple IFFT-based approach. */
void si_step_response(const SParameter2Port *s_params, int num_freqs,
                       double *step_response, double *time_ps,
                       int num_time_points);

/* L5: Decision Feedback Equalization (DFE) tap computation
 *     Computes DFE tap coefficients to cancel post-cursor ISI.
 *     Uses zero-forcing criterion on pulse response samples. */
int si_dfe_taps(const double *pulse_response, int num_taps,
                 double *tap_coefficients);

/* L5: Feed-Forward Equalizer (FFE) tap computation
 *     3-tap FFE (pre, main, post) using zero-forcing */
void si_ffe_3tap(const double *pulse_response, double *pre, double *main,
                  double *post);

/* L5: Compute channel capacity from frequency response
 *     Shannon capacity for given SNR profile across frequency bins.
 *     C = Σ Δf · log₂(1 + SNR(f)) */
double si_channel_capacity(const double *freq_ghz, const double *snr_db,
                            int num_bins);

/* ===================================================================
 * L6: Canonical Problems — Standard SI scenarios
 * =================================================================== */

/* L6: ISI analysis for NRZ signaling
 *     Computes worst-case eye closure due to ISI for a given channel
 *     impulse response length (in UI). */
typedef struct {
    double eye_height_loss_pct;
    double eye_width_loss_pct;
    double worst_case_isi_mv;
    int    requires_equalization; /* 1 if eye closure > 50% */
} NrzIsiAnalysis;

NrzIsiAnalysis si_nrz_isi_analysis(const Waveform *pulse_response,
                                    double bit_period_ps);

/* L6: PAM4 eye analysis (three eyes: upper, middle, lower)
 *     PAM4 has 3 eyes vs NRZ's 1. Reports the worst case. */
typedef struct {
    double upper_eye_height_mv;
    double middle_eye_height_mv;
    double lower_eye_height_mv;
    double worst_eye_height_mv;
    double eye_width_ps;
    double level_separation_mismatch_pct; /* Ratio mismatch between eyes */
} Pam4EyeAnalysis;

Pam4EyeAnalysis si_pam4_eye_analysis(const Waveform *wf, double symbol_period_ps);

/* L6: Reflection-induced eye closure
 *     Estimates eye closure from impedance discontinuities along a link.
 *     Γ_total = Γ₁ + Γ₂·e^(-2γl) + ... */
double si_reflection_eye_closure(double *z_ohm, int num_segments,
                                  double *lengths_mm, double bit_period_ps,
                                  double er_eff);

/* ===================================================================
 * L7: Applications — Protocol-specific SI validation
 * =================================================================== */

/* L7: PCIe compliance: eye height/width at BER=1e-12 */
int si_pcie_eye_compliance(const EyeDiagram *eye, int generation);

/* L7: USB 3.2 compliance: eye mask test at 10 Gbps */
int si_usb3_eye_compliance(const EyeDiagram *eye, int gen);

/* L7: DDR5 DQ eye validation (Vref-based, not differential) */
int si_ddr5_eye_compliance(const EyeDiagram *eye, double vref_v,
                            double vddq_v);

/* ===================================================================
 * L8: Advanced — Statistical and system-level SI
 * =================================================================== */

/* L8: Statistical eye analysis (peak distortion analysis)
 *     Computes worst-case eye from single-bit response without
 *     needing long PRBS simulation. Uses superposition. */
EyeDiagram si_statistical_eye(const Waveform *single_bit_response,
                               double bit_period_ps);

/* L8: Power Supply Induced Jitter (PSIJ) estimation
 *     PSIJ ≈ K_vco · V_noise_rms / (2π · f_noise)
 *     where K_vco is the supply sensitivity (ps/mV) of the TX PLL. */
double si_psij(double v_noise_rms_mv, double noise_freq_mhz,
                double vco_sensitivity_ps_per_mv);

/* L8: Simultaneous Switching Noise (SSN) analysis
 *     N switching outputs → I_di/dt → L_eff · dI/dt ground bounce
 *     ΔV_gnd = N · L_eff · ΔI / Δt */
double si_ssn_ground_bounce(int num_switching, double di_dt_a_per_ns,
                             double effective_inductance_nh);

/* L8: Crosstalk-induced jitter (CIJ) estimation */
double si_crosstalk_jitter(double crosstalk_amplitude_mv,
                            double signal_amplitude_mv,
                            double bit_period_ps, double rise_time_ps);

#endif /* PCB_SIGNAL_INTEGRITY_H */
