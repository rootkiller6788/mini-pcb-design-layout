#ifndef PCB_VIA_H
#define PCB_VIA_H

#include "pcb_transmission_line.h"
#include "pcb_material.h"

/* ============================================================================
 * L1: Core Definitions — PCB Via Structures and Modeling
 *
 * A via is a vertical interconnect that transitions a signal between layers.
 * It consists of a plated barrel, pads, and antipads / clearance holes
 * in reference planes. Vias introduce parasitic L and C that cause
 * impedance discontinuities and stub resonances.
 *
 * Via types:
 *   Through-hole — drills through entire board (most common, cheapest)
 *   Blind — connects outer layer to inner layer (back-drill alternative)
 *   Buried — connects inner layers only (not visible from surface)
 *   Microvia — laser-drilled, ≤150µm diameter, max 1-2 layer span
 *   Via-in-pad — placed directly in SMD pad (filled + capped)
 * ========================================================================= */

/* L1: Via type enumeration */
typedef enum {
    VIA_THROUGH_HOLE = 0,
    VIA_BLIND        = 1,
    VIA_BURIED       = 2,
    VIA_MICROVIA     = 3,
    VIA_IN_PAD       = 4,
    VIA_BACKDRILLED  = 5,
    VIA_STACKED      = 6,  /* Stacked microvias */
    VIA_STAGGERED    = 7   /* Staggered microvias */
} ViaType;

/* L1: Via physical dimensions */
typedef struct {
    double drill_diameter_mm;
    double finished_hole_diameter_mm;
    double pad_diameter_mm;
    double antipad_diameter_mm;   /* Clearance in reference planes */
    double plating_thickness_um;  /* Barrel wall thickness */
    int    start_layer;
    int    end_layer;
    double total_length_mm;       /* Length from start to end layer */
    double stub_length_mm;        /* Unused stub length (0 for blind/buried) */
    int    is_backdrilled;
} ViaDimensions;

/* L1: Via electrical model (lumped-element) */
typedef struct {
    double capacitance_pf;     /* Barrel-to-plane capacitance */
    double inductance_nh;      /* Barrel self-inductance */
    double resistance_mohm;    /* Barrel DC resistance */
    double stub_capacitance_pf; /* Stub capacitance (if not backdrilled) */
    double impedance_ohm;      /* Characteristic impedance approximation */
    double delay_ps;            /* Propagation delay through via */
    double resonant_freq_ghz;   /* Stub quarter-wave resonance */
} ViaElectricalModel;

/* L1: Via array for power delivery / ground stitching */
typedef struct {
    double pitch_mm;
    int    num_rows;
    int    num_cols;
    double total_inductance_nh;
    double total_capacitance_pf;
    double effective_impedance_ohm;
    double via_perimeter_mm;    /* Perimeter covered by via array */
} ViaArray;

/* ===================================================================
 * L2: Core Concepts — Via parasitic extraction
 * =================================================================== */

/* L2: Compute via barrel DC resistance
 *     R = ρ·L / A  where A = π·(R_outer² - R_inner²)
 *     ρ_copper = 1.72e-8 Ω·m */
double via_dc_resistance(const ViaDimensions *dim, double conductivity);

/* L2: Compute via barrel AC resistance (skin effect)
 *     R_ac ≈ ρ·L / (2π·r·δ)  where δ = skin depth
 *     Valid when skin depth << barrel thickness */
double via_ac_resistance(const ViaDimensions *dim, double freq_hz,
                          double conductivity);

/* L2: Compute via self-inductance (through-hole)
 *     L_via ≈ (μ₀/2π) · h · ln(h/r + √(1 + (h/r)²))
 *     Approximated for h >> r as: L ≈ 5.08·h [ln(2h/r) + 1] nH
 *     where h in mm, r in mm. Goldfarb & Pucel (1981) formula */
double via_inductance(const ViaDimensions *dim);

/* L2: Compute via barrel-to-plane capacitance
 *     C_via ≈ 1.41 · εr · h · D_pad / (D_antipad - D_pad)  [pF]
 *     where h = plane thickness (mm), D in mm */
double via_capacitance(const ViaDimensions *dim, double er,
                        double plane_thickness_mm);

/* L2: Build complete via electrical model */
ViaElectricalModel via_electrical_model(const ViaDimensions *dim,
                                         double er, double conductivity,
                                         double target_freq_ghz);

/* ===================================================================
 * L3: Mathematical Structures — Via impedance and S-parameters
 * =================================================================== */

/* L3: Via characteristic impedance (coaxial approximation)
 *     Z₀_via ≈ (60/√εr) · ln(D_antipad / d_pad)
 *     Models via as coaxial transmission line with barrel as center
 *     conductor and antipad edge as outer conductor. */
double via_characteristic_impedance(const ViaDimensions *dim, double er);

/* L3: Via S-parameter computation (2-port)
 *     Models via as a pi-network of L-C-L or C-L-C elements.
 *     Computes S11, S21 at given frequency. */
void via_s_parameters(const ViaElectricalModel *model, double freq_ghz,
                       double z0_ref, double complex *s11, double complex *s21);

/* L3: Via TDR response simulation
 *     Simulates TDR reflection from a via in a transmission line.
 *     Z_via discontinuity → Γ → reflected voltage step. */
void via_tdr_response(const ViaElectricalModel *model, double z0_trace,
                       double rise_time_ps, double *t_ns, double *reflection,
                       int num_points);

/* L3: Multi-via impedance for parallel vias (power delivery)
 *     For N identical parallel vias: L_eff = L_single / N */
void via_parallel_impedance(const ViaDimensions *dim, int num_vias,
                             double er, double *l_total_nh,
                             double *r_total_mohm);

/* ===================================================================
 * L4: Fundamental Laws — Via design constraints
 * =================================================================== */

/* L4: Minimum via pad and drill sizes for given IPC class
 *     IPC-2221: annular ring = (pad - drill)/2 ≥ min_annular_ring
 *     Class 1: 0.05mm, Class 2: 0.1mm, Class 3: 0.15mm */
int via_min_dimensions(int ipc_class, double *min_drill_mm,
                        double *min_pad_mm, double *min_antipad_mm);

/* L4: Aspect ratio limit for plating reliability
 *     AR = board_thickness / drill_diameter
 *     Standard: AR ≤ 8, Advanced: AR ≤ 12, Microvia: AR ≤ 1 */
int via_aspect_ratio_check(double board_thickness_mm, double drill_mm,
                            double max_aspect_ratio);

/* L4: Via reliability — thermal fatigue life estimation
 *     Coffin-Manson model: N_f = C · (Δε_plastic)^(-n)
 *     for plated through-hole vias under thermal cycling */
double via_thermal_cycle_life(double delta_t_c, double via_diameter_mm,
                               double board_thickness_mm,
                               double cte_z_ppm_per_c);

/* L4: Via current carrying capacity
 *     Based on barrel cross-sectional area and allowed temperature rise.
 *     I_max = k · ΔT^β · A_cross_section^γ   [IPC-2152 adapted for via] */
double via_current_capacity(const ViaDimensions *dim, double temp_rise_c);

/* ===================================================================
 * L5: Algorithms — Via optimization and placement
 * =================================================================== */

/* L5: Optimal antipad size for impedance matching
 *     Finds D_antipad such that Z_via ≈ Z_trace
 *     Uses bisection on via_characteristic_impedance equation. */
double via_optimal_antipad(const ViaDimensions *dim, double er,
                            double target_z0_ohm);

/* L5: Via stub resonance analysis
 *     Stub forms a quarter-wave resonator at f_res = c/(4·√εr·L_stub)
 *     Computes stub length for given max frequency. */
double via_max_stub_for_frequency(double max_freq_ghz, double er_eff);

/* L5: Backdrill depth calculation
 *     Stub length = total_via_length - backdrill_depth - signal_layer_depth
 *     Finds required backdrill depth to keep stub below threshold. */
double via_backdrill_depth(const ViaDimensions *dim, double signal_layer_depth_mm,
                            double max_stub_mm);

/* L5: Via array optimization for power delivery
 *     Finds minimum number of vias to achieve target inductance. */
int via_array_optimize(const ViaDimensions *dim, double er,
                        double target_inductance_nh, int max_vias,
                        double *actual_inductance_nh);

/* L5: Compute via transition loss (insertion loss through via)
 *     IL ≈ 20·log₁₀|1 - Γ²| - 0.115·R_via/Z₀
 *     Combines reflection + resistive loss. */
double via_insertion_loss_db(const ViaElectricalModel *model,
                              double z0_trace, double freq_ghz);

/* ===================================================================
 * L6: Canonical Problems — Standard via designs
 * =================================================================== */

/* L6: Design through-hole via for 50Ω microstrip transition */
ViaDimensions via_design_50ohm_through(double board_thickness_mm, int ipc_class);

/* L6: Design microvia for HDI board (laser-drilled)
 *     Typically 100µm drill, max 1 layer penetration */
ViaDimensions via_design_microvia(double dielectric_thickness_mm,
                                   double pad_diameter_mm);

/* L6: Design differential pair via transition
 *     Two signal vias + surrounding ground vias for return path continuity */
typedef struct {
    ViaDimensions signal_vias[2];
    ViaDimensions ground_vias[8];  /* Up to 8 ground stitching vias */
    int           num_ground_vias;
    double        diff_impedance_ohm;
    double        via_to_via_pitch_mm;
} DiffPairViaTransition;

DiffPairViaTransition via_design_diff_pair_transition(
    double board_thickness_mm, double target_z_diff,
    double er, int ipc_class);

/* L6: Via fence / guard trace design for isolation */
typedef struct {
    double   fence_pitch_mm;
    double   fence_to_signal_spacing_mm;
    int      num_fence_vias;
    double   isolation_db_at_freq;
    double   max_isolation_freq_ghz;
} ViaFence;

ViaFence via_design_fence(double signal_trace_length_mm,
                           double er, double target_isolation_db,
                           double max_freq_ghz);

/* ===================================================================
 * L7: Applications — Protocol-specific via requirements
 * =================================================================== */

/* L7: PCIe Gen5 via design (32 GT/s)
 *     Requires backdrilling, tight impedance control, minimal stub */
ViaDimensions via_pcie_gen5_design(double board_thickness_mm);

/* L7: DDR5 via design (up to 6400 MT/s)
 *     Address/command/control via constraints */
ViaDimensions via_ddr5_design(double board_thickness_mm);

/* L7: RF/microwave via design for frequencies up to Ka-band (40 GHz) */
ViaDimensions via_rf_ka_band_design(double board_thickness_mm, double er);

/* ===================================================================
 * L8: Advanced — Exotic via structures and high-frequency effects
 * =================================================================== */

/* L8: Coaxial via structure (signal via surrounded by GND vias)
 *     Provides continuous return path → minimal impedance discontinuity.
 *     Models as multi-conductor coaxial line. */
typedef struct {
    ViaDimensions signal_via;
    int           num_ground_vias;
    double        ring_radius_mm;      /* Radius of GND via ring */
    double        coaxial_z0_ohm;
    double        cutoff_freq_ghz;     /* TE11 mode cutoff */
} CoaxialVia;

CoaxialVia via_design_coaxial(const ViaDimensions *signal_via,
                               double er, int num_ground_vias);

/* L8: Via resonance suppression using damping resistors
 *     Series R in stub path → reduces Q of stub resonance.
 *     Computes optimal R for critical damping: R = √(L_stub / C_stub) */
double via_stub_damping_resistor(const ViaElectricalModel *model);

/* L8: Coupled via crosstalk analysis
 *     Two adjacent signal vias → mutual L and C.
 *     NEXT/FEXT estimation for via-to-via coupling. */
double via_to_via_crosstalk(const ViaElectricalModel *aggressor,
                             const ViaElectricalModel *victim,
                             double separation_mm, double freq_ghz,
                             double rise_time_ps);

#endif /* PCB_VIA_H */
