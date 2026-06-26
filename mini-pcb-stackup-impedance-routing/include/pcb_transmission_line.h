#ifndef PCB_TRANSMISSION_LINE_H
#define PCB_TRANSMISSION_LINE_H

#include <stddef.h>
#include <complex.h>

/* ============================================================================
 * L1: Core Definitions — Transmission line parameters
 *
 * A transmission line is characterized by its per-unit-length parameters
 * (R, L, G, C) and derived quantities Z₀, γ, v_p.
 *
 * Governing equation: Telegrapher's equations
 *   ∂v/∂x = -(R + jωL)·i
 *   ∂i/∂x = -(G + jωC)·v
 * ========================================================================= */

/* L1: Per-unit-length RLGC parameters */
typedef struct {
    double R;  /* Resistance per meter (Ω/m) */
    double L;  /* Inductance per meter (H/m)  */
    double G;  /* Conductance per meter (S/m) */
    double C;  /* Capacitance per meter (F/m)  */
} RlgcParams;

/* L1: Transmission line characteristic properties */
typedef struct {
    double z0_real;      /* Characteristic impedance Z₀ real part (Ω) */
    double z0_imag;      /* Characteristic impedance Z₀ imaginary part (Ω) */
    double alpha;        /* Attenuation constant (Np/m) */
    double beta;         /* Phase constant (rad/m) */
    double vp;           /* Phase velocity (m/s) */
    double wavelength;   /* Wavelength in medium (m) at given frequency */
    double effective_er; /* Effective dielectric constant εeff */
    double delay_per_m;  /* Propagation delay (s/m) i.e., 1/vp */
} TransmissionLine;

/* L1: Transmission line geometry parameters (cross-section) */
typedef struct {
    double trace_width_m;       /* Conductor width w (m) */
    double trace_thickness_m;   /* Conductor thickness t (m) */
    double dielectric_height_m; /* Substrate height h (m) - from trace to reference plane */
    double dielectric_er;       /* Relative permittivity εr */
    double loss_tangent;        /* Dielectric loss tangent tanδ */
    double conductor_sigma;     /* Conductor conductivity σ (S/m) */
    int    has_solder_mask;     /* Boolean: solder mask present */
    double solder_mask_height_m;/* Solder mask thickness (m) */
    double solder_mask_er;      /* Solder mask εr */
} TraceGeometry;

/* ===================================================================
 * L1: Microstrip, Stripline, CPW geometry types
 * =================================================================== */

/* L1: Microstrip geometry (signal on top, reference plane below) */
typedef struct {
    TraceGeometry trace;
    double substrate_thickness_m; /* Total substrate thickness */
} MicrostripGeometry;

/* L1: Stripline geometry (signal between two reference planes) */
typedef struct {
    TraceGeometry trace;
    double upper_height_m; /* h1: distance to upper plane (m) */
    double lower_height_m; /* h2: distance to lower plane (m) */
} StriplineGeometry;

/* L1: Coplanar waveguide geometry (signal + ground on same layer) */
typedef struct {
    TraceGeometry trace;
    double gap_m;          /* Gap between signal and coplanar ground (m) */
    int    has_back_plane; /* Boolean: backed (GCPW) or unbacked */
} CpwGeometry;

/* L1: Edge-coupled differential pair geometry */
typedef struct {
    TraceGeometry trace;
    double spacing_m;      /* Edge-to-edge spacing between traces (m) */
} DiffPairEdgeGeometry;

/* L1: Broadside-coupled differential pair geometry (stacked in adjacent layers) */
typedef struct {
    TraceGeometry trace;
    double layer_separation_m; /* Vertical separation between traces (m) */
} DiffPairBroadsideGeometry;

/* ===================================================================
 * L2: Core Concepts — Transmission line synthesis
 * =================================================================== */

/* L2: Compute RLGC from trace geometry at given frequency */
RlgcParams        tl_compute_rlgc(const TraceGeometry *geo, double freq_hz);

/* L2: Compute transmission line parameters from RLGC at given frequency */
TransmissionLine  tl_from_rlgc(const RlgcParams *rlgc, double freq_hz);

/* L2: Compute transmission line directly from trace geometry */
TransmissionLine  tl_from_geometry(const TraceGeometry *geo, double freq_hz);

/* ===================================================================
 * L3: Mathematical Structures — Telegrapher's equations solutions
 * =================================================================== */

/* L3: Solve Telegrapher's equations for voltage at position x
 *     V(x) = V⁺·e^(-γx) + V⁻·e^(+γx)
 *     I(x) = (V⁺·e^(-γx) - V⁻·e^(+γx)) / Z₀
 *     Output is complex: V(x) = V_re + j·V_im */
void tl_voltage_at_x(const TransmissionLine *tl, double freq_hz,
                     double v_plus_mag, double v_plus_phase,
                     double v_minus_mag, double v_minus_phase,
                     double x_m, double *v_re, double *v_im);

/* L3: Compute voltage standing wave pattern along line
 *     |V(x)| = |V⁺|·|1 + Γ_L·e^(-2γ(x-L))|  */
void tl_standing_wave(const TransmissionLine *tl, double freq_hz,
                       double zl_ohm, double z0_ohm,
                       double line_length_m, double *v_mag, int num_points,
                       double *x_positions);

/* L3: Compute input impedance at distance d from load
 *     Z_in(d) = Z₀ · (Z_L + j·Z₀·tan(βd)) / (Z₀ + j·Z_L·tan(βd)) */
double complex tl_input_impedance(const TransmissionLine *tl, double freq_hz,
                                   double complex z_load, double distance_m);

/* ===================================================================
 * L4: Fundamental Laws — Reflection, impedance, propagation
 * =================================================================== */

/* L4: Reflection coefficient
 *     Γ = (Z_L - Z₀) / (Z_L + Z₀)  — complex when Z_L is complex
 *     Returns complex: Γ_re + j·Γ_im */
double complex tl_reflection_coefficient(double complex z_load, double complex z0);

/* L4: Voltage Standing Wave Ratio from reflection coefficient
 *     VSWR = (1 + |Γ|) / (1 - |Γ|) */
double tl_vswr_from_gamma(double complex gamma);

/* L4: Return loss in dB
 *     RL_dB = -20·log₁₀(|Γ|) */
double tl_return_loss_db(double complex gamma);

/* L4: Mismatch loss in dB
 *     ML_dB = -10·log₁₀(1 - |Γ|²) — power not delivered due to mismatch */
double tl_mismatch_loss_db(double complex gamma);

/* L4: Propagation constant from geometry and frequency
 *     γ = α + jβ = √((R + jωL)(G + jωC)) */
double complex tl_propagation_constant(const RlgcParams *rlgc, double freq_hz);

/* L4: Characteristic impedance from RLGC
 *     Z₀ = √((R + jωL) / (G + jωC)) */
double complex tl_characteristic_impedance(const RlgcParams *rlgc, double freq_hz);

/* ===================================================================
 * L5: Algorithms — Transmission line analysis and optimization
 * =================================================================== */

/* L5: Time-Domain Reflectometry (TDR) simulation
 *     Simulates a step/pulse propagating down a lossy transmission line.
 *     Uses finite-difference approximation of Telegrapher's equations in time.
 *     Returns the reflected waveform at the source. */
int tl_tdr_simulate(const TransmissionLine *tl, double line_length_m,
                     double source_impedance, double load_impedance,
                     double time_step_ps, double total_time_ns,
                     double *reflected_wave, int num_samples);

/* L5: Optimal termination analysis
 *     Finds termination resistances for series/parallel/Thevenin/AC termination */
typedef enum { TERM_NONE, TERM_SERIES, TERM_PARALLEL, TERM_THEVENIN,
               TERM_AC, TERM_DIODE } TerminationType;

typedef struct {
    TerminationType type;
    double r1_ohm;     /* Series or pull-up resistor */
    double r2_ohm;     /* Pull-down (Thevenin) or cap-discharge */
    double c_ac_pf;    /* AC termination capacitor */
    int    is_valid;
} Termination;

Termination tl_optimal_termination(const TransmissionLine *tl, double z_driver,
                                    double z_receiver);

/* L5: Eye diagram mask test (simple rectangular mask)
 *     Checks if eye opening meets minimum height/width requirements */
typedef struct {
    double min_eye_height_v;
    double min_eye_width_ps;
    double bit_period_ps;
} EyeMask;

int tl_eye_mask_test(const double *eye_waveform, int num_samples,
                      double bit_period_ps, const EyeMask *mask);

/* ===================================================================
 * L6: Canonical Problems — Classic transmission line scenarios
 * =================================================================== */

/* L6: Quarter-wave transformer design
 *     Z_T = √(Z₀ · Z_L), length = λ/4
 *     Used for single-frequency impedance matching */
double tl_quarter_wave_transformer(double z0_ohm, double zl_ohm,
                                    double freq_hz, double er_eff,
                                    double *length_mm);

/* L6: Single-stub matching design (shunt stub)
 *     Finds stub length and distance from load for conjugate match */
int tl_single_stub_match(double z0_ohm, double complex zl,
                          double freq_hz, double er_eff,
                          double *d_from_load_mm, double *stub_length_mm,
                          int *is_open_stub);

/* L6: Multi-section impedance transformer design (binomial / Chebyshev) */
typedef enum { TRANSFORMER_BINOMIAL, TRANSFORMER_CHEBYSHEV } TransformerType;

typedef struct {
    double *z_sections;    /* Array of impedance values per section */
    int     num_sections;
    double  length_per_section_mm; /* λ/4 at center frequency */
    double  bandwidth_ghz;         /* -20dB return loss bandwidth */
} MultisectionTransformer;

MultisectionTransformer tl_multisection_transformer(double z0_ohm, double zl_ohm,
                                                     int num_sections,
                                                     TransformerType type,
                                                     double center_freq_ghz,
                                                     double er_eff);

void tl_multisection_transformer_free(MultisectionTransformer *mt);

/* L6: Loss budget analysis for high-speed serial link */
typedef struct {
    double freq_ghz;
    double trace_length_m;
    double connector_loss_db;
    double via_loss_per_pair_db;
    int    num_vias;
    double package_loss_db;
} LinkLossBudget;

double tl_link_total_loss(const TraceGeometry *geo, const LinkLossBudget *budget);

/* ===================================================================
 * L7: Applications — Industry-standard interface validation
 * =================================================================== */

/* L7: Validate trace geometry for PCIe Gen3/4/5 requirements */
typedef enum { PCIE_GEN3, PCIE_GEN4, PCIE_GEN5 } PcieGeneration;

typedef struct {
    double max_insertion_loss_db;
    double max_return_loss_db;
    double target_impedance_ohm;
    double impedance_tolerance_pct;
} PcieSpec;

PcieSpec tl_pcie_specification(PcieGeneration gen);
int      tl_validate_pcie_trace(const TraceGeometry *geo, double length_m,
                                 PcieGeneration gen);

/* L7: USB 3.x trace validation */
typedef enum { USB3_GEN1, USB3_GEN2, USB4_GEN3 } Usb3Version;

int tl_validate_usb3_trace(const TraceGeometry *geo, const DiffPairEdgeGeometry *diff,
                            double length_m, Usb3Version version);

/* L7: DDR memory bus timing - flight time and skew */
double tl_ddr_flight_time(const TransmissionLine *tl, double trace_length_m);
double tl_ddr_max_skew_ps(double clock_period_ps, int ddr_generation);

/* ===================================================================
 * L8: Advanced — Frequency-dependent loss modeling
 * =================================================================== */

/* L8: Compute S-parameters for a transmission line segment
 *     S11 = Γ·(1 - e^(-2γl)) / (1 - Γ²·e^(-2γl))
 *     S21 = (1 - Γ²)·e^(-γl) / (1 - Γ²·e^(-2γl))
 *     Where Γ = (Z_L - Z₀)/(Z_L + Z₀) */
void tl_s_parameters(const TransmissionLine *tl, double freq_hz,
                      double z0_ref, double length_m,
                      double complex *s11, double complex *s21);

/* L8: Causality check — verify Kramers-Kronig consistency
 *     For a physically realizable line, α(ω) and β(ω) must satisfy
 *     the Hilbert transform relationship. */
int tl_check_causality(const TransmissionLine *tl, int num_freqs,
                        double *freqs);

/* L8: Stochastic impedance variation analysis
 *     Monte Carlo analysis of impedance given manufacturing tolerances
 *     w ± Δw, h ± Δh, εr ± Δεr */
typedef struct {
    double trace_width_tol_pct;
    double height_tol_pct;
    double er_tol_pct;
    int    num_trials;
} ImpedanceTolerance;

double tl_impedance_yield(const TraceGeometry *geo, double target_z0,
                           double tolerance_pct, const ImpedanceTolerance *tol);

#endif /* PCB_TRANSMISSION_LINE_H */
