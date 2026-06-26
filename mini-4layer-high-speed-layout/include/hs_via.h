/*
 * hs_via.h — Via Modeling and Design for High-Speed PCBs
 *
 * Core Definitions (L1):
 *   - Through-hole via, blind via, buried via, microvia
 *   - Via barrel, pad, antipad
 *   - Via inductance, via capacitance
 *   - Via stub, back-drilling
 *   - Via impedance
 *
 * Core Concepts (L2):
 *   - Via transition as impedance discontinuity
 *   - Via stub resonance
 *   - Return path disruption
 *   - Differential via design
 *   - Ground stitching vias
 *   - Via density tradeoffs
 *
 * Mathematical Structures (L3):
 *   - Partial inductance of cylindrical conductor
 *   - Coaxial capacitance model for via barrel
 *   - Lumped-element π/T-network equivalent
 *   - S-parameter extraction for via transition
 *
 * Fundamental Laws (L4):
 *   - Via inductance: L_via ≈ (μ₀h/(2π)) × ln(2h/r)  (partial)
 *   - Via capacitance: C_via ∝ pad area / antipad clearance
 *   - Stub resonance: f_res = c/(4h_stub × √εr)  (quarter-wave)
 *   - Via impedance: Z_via ≈ √(L_via/C_via)
 *
 * References:
 *   - Johnson & Graham, "High-Speed Digital Design", Ch.7
 *   - Hall & Heck, "Advanced Signal Integrity", Ch.6
 *   - LaMeres, "High-Speed Digital System Design", Ch.9
 *   - Bogatin, "Signal and Power Integrity Simplified", Ch.12
 */

#ifndef HS_VIA_H
#define HS_VIA_H

#include <stddef.h>

/* ================================================================
 * L1: Core Definitions — Via Types and Parameters
 * ================================================================ */

typedef enum {
    HS_VIA_THROUGH_HOLE = 0,  /* Plated through-hole (all layers) */
    HS_VIA_BLIND        = 1,  /* Blind via (surface to inner layer) */
    HS_VIA_BURIED       = 2,  /* Buried via (inner to inner layer only) */
    HS_VIA_MICROVIA     = 3,  /* Microvia (laser-drilled, ≤ 0.15mm diam) */
    HS_VIA_STACKED_MICRO = 4 /* Stacked microvia */
} hs_via_type_t;

/**
 * Via geometry parameters.
 *
 * Key dimensions for a plated through-hole via:
 *
 *           ┌─────────┐  ← pad diameter (d_pad)
 *           │  ┌───┐  │
 *           │  │   │  │  ← barrel hole diameter (d_hole)
 *           │  │   │  │
 *           │  └───┘  │
 *           └─────────┘
 *           ←──d_anti──→  (antipad on non-connected planes)
 *
 * Barrel wall thickness = (d_pad - d_hole)/2 + plating
 */
typedef struct {
    hs_via_type_t type;
    double hole_diameter_m;       /* Finished hole diameter (m) */
    double pad_diameter_m;        /* Pad diameter on signal layers (m) */
    double antipad_diameter_m;    /* Antipad (clearance) diameter on planes (m) */
    double barrel_length_m;       /* Total barrel length (board thickness, m) */
    double plating_thickness_m;   /* Copper plating thickness in barrel (m) */
    double stub_length_m;         /* Unused via stub length (0 if back-drilled) */
    double pitch_m;               /* Center-to-center spacing for diff pair (m) */
    int    layer_start;           /* Starting layer (1=top) */
    int    layer_end;             /* Ending layer (4=bottom) */
} hs_via_geometry_t;

/**
 * Via electrical model (lumped-element equivalent).
 *
 * For frequencies where via length < λ/10, the via can be modeled
 * as a lumped π-network:
 *
 *    Port1 ──┬── L_via ──┬── Port2
 *            C1          C2
 *            │           │
 *           GND         GND
 *
 * L_via: Series inductance of barrel
 * C1, C2: Pad capacitance to reference planes at each end
 */
typedef struct {
    double inductance_ph;         /* Via series inductance (pH) */
    double capacitance_pad_ff[2]; /* Pad capacitance at each end (fF) */
    double resistance_mohm;       /* DC barrel resistance (mΩ) */
    double impedance_ohm;         /* Approximate Z_via = √(L/C) (Ω) */
    double resonant_freq_ghz;     /* Stub quarter-wave resonance (GHz) */
    double bandwidth_ghz;         /* 3dB bandwidth of via transition (GHz) */
} hs_via_model_t;

/**
 * Differential via model.
 */
typedef struct {
    hs_via_model_t via_p;         /* Positive signal via */
    hs_via_model_t via_n;         /* Negative signal via */
    double diff_impedance_ohm;    /* Differential impedance (Ω) */
    double coupling_coeff;        /* Coupling coefficient between vias */
    double skew_ps;               /* Timing skew between P and N paths (ps) */
    double diff_bandwidth_ghz;    /* Differential 3dB bandwidth (GHz) */
} hs_diff_via_model_t;

/**
 * Via optimization constraints for cost/performance tradeoff.
 */
typedef struct {
    double max_stub_length_m;     /* Max allowed stub before back-drilling */
    double min_antipad_ratio;     /* Min antipad/pad diameter ratio */
    double target_impedance_ohm;  /* Desired via impedance */
    double max_aspect_ratio;      /* Max barrel_length / hole_diameter */
    double max_resonance_ghz;     /* Max allowed resonance below signal BW */
} hs_via_constraints_t;

/* ================================================================
 * L2-L5: Via Electrical Modeling Functions
 * ================================================================ */

/**
 * Compute via partial inductance.
 *
 * For a cylindrical conductor of length h and radius r:
 *   L_via ≈ (μ₀ × h / (2π)) × [ln(2h/r) - 1 + r/h]  [H]
 *
 * Simplified (r << h):
 *   L_via ≈ (μ₀ × h / (2π)) × ln(2h/r)  [H]
 *
 * In practical units (mils):
 *   L_via ≈ 5.08 × h × [ln(2h/r) + 1]  [nH]   (h, r in mils)
 *
 * Typical via (h=62mil, r=5mil): L ≈ 1.2 nH
 * This inductance causes ~3.8 Ω impedance at 500 MHz:
 *   Z_L = 2πfL = 2π × 5e8 × 1.2e-9 ≈ 3.8 Ω
 * At 5 GHz: Z_L ≈ 38 Ω — significant!
 *
 * Reference: Johnson & Graham, Eq. 7.19; Grover §5
 *
 * @param geo: Via geometry
 * @return Partial inductance in Henrys
 */
double hs_via_inductance(const hs_via_geometry_t *geo);

/**
 * Compute via pad capacitance to reference planes.
 *
 * The pad-to-plane capacitance for a circular pad:
 *   C_pad ≈ ε₀ × εr × π × (d_pad² - d_anti²) / (4 × h_clearance)  [F]
 *
 * For a pad of diameter d_pad and antipad clearance gap:
 *   C_pad ≈ ε₀ × εr × (pad_area - hole_area) / dielectric_thickness
 *
 * Typical values: 0.3-0.8 pF per pad
 *
 * Reference: Johnson & Graham, Eq. 7.15; LaMeres §9.3.2
 *
 * @param geo: Via geometry
 * @param pad_end: 0 for port1 pad, 1 for port2 pad
 * @return Pad capacitance in Farads
 */
double hs_via_pad_capacitance(const hs_via_geometry_t *geo, int pad_end);

/**
 * Compute via barrel DC resistance.
 *
 * R_dc = ρ × h / (π × (r_outer² - r_inner²))
 * where r_outer = d_hole/2 + plating, r_inner = d_hole/2
 *
 * For 1 mil plating (=25.4 μm Cu), 10 mil hole, 62 mil length:
 *   Barrel cross-section ≈ π × ((5+1)² - 5²) × 25.4² ≈ 1.1e-8 m²
 *   R_dc ≈ 1.72e-8 × 1.57e-3 / 1.1e-8 ≈ 2.4 mΩ
 *
 * @param geo: Via geometry
 * @return DC barrel resistance in ohms
 */
double hs_via_dc_resistance(const hs_via_geometry_t *geo);

/**
 * Build the complete lumped-element via model.
 *
 * Combines inductance, pad capacitances, and resistance into
 * a unified model. Also computes approximate impedance and
 * stub resonant frequency.
 *
 * @param geo: Via geometry
 * @param er: Dielectric constant of surrounding material
 * @return Complete via model
 */
hs_via_model_t hs_via_build_model(const hs_via_geometry_t *geo, double er);

/**
 * Compute stub resonant frequency.
 *
 * For a quarter-wave stub of length L_stub:
 *   f_res = c / (4 × L_stub × √εr)
 *
 * The stub reflects as an open circuit, creating a transmission
 * null at quarter-wave resonance.
 *
 * For FR-4 (εr=4.0):
 *   L=100mil → f_res ≈ 14.8 GHz
 *   L=200mil → f_res ≈ 7.4 GHz
 *   L=300mil → f_res ≈ 4.9 GHz  ← problem for 10G links
 *
 * Reference: Johnson & Graham, Eq. 7.12
 *
 * @param stub_length_m: Length of unused via stub (m)
 * @param er: Effective dielectric constant around via barrel
 * @return Quarter-wave resonant frequency in Hz
 */
double hs_via_stub_resonance(double stub_length_m, double er);

/**
 * Compute the required back-drill depth to remove a via stub.
 *
 * Back-drilling removes the unused portion of a through-hole via
 * to eliminate stub resonance.
 *
 * Required depth = total barrel - (layer_end_depth) + margin
 *
 * @param geo: Via geometry
 * @param signal_bw_hz: Signal bandwidth — stub resonance should be > 3× BW
 * @return Required depth from the non-connected end (m), 0 if not needed
 */
double hs_via_backdrill_depth(const hs_via_geometry_t *geo,
                               double signal_bw_hz);

/**
 * Compute the impedance of a via from its LC model.
 *
 * Z_via ≈ √(L_via / (C1 + C2))
 *
 * For low-loss: Z_via is approximately real.
 *
 * @param model: Via model
 * @return Via characteristic impedance in Ω
 */
double hs_via_impedance(const hs_via_model_t *model);

/**
 * Compute the 3 dB bandwidth of a lumped via π-network.
 *
 * BW ≈ 1 / (π × √(L_via × (C1||C2)))
 *
 * This estimates the frequency at which the via transition
 * introduces -3 dB of insertion loss.
 *
 * @param model: Via model
 * @return 3 dB bandwidth in Hz
 */
double hs_via_bandwidth(const hs_via_model_t *model);

/**
 * Build a differential via model from two single-ended vias.
 *
 * Differential impedance: Z_diff ≈ 2 × Z_odd
 * where Z_odd is the odd-mode impedance of a single via in the pair.
 *
 * @param geo: Via geometry (uses pitch for separation)
 * @param er: Dielectric constant
 * @return Differential via model
 */
hs_diff_via_model_t hs_diff_via_model(const hs_via_geometry_t *geo, double er);

/**
 * Optimize via geometry for target impedance.
 *
 * Primarily adjusts antipad diameter to tune via impedance.
 * Larger antipad → lower capacitance → higher impedance.
 *
 * Algorithm: Binary search on antipad diameter within manufacturing limits.
 *
 * @param geo: Initial via geometry (modified in place with optimized values)
 * @param target_z: Target impedance in Ω
 * @param er: Dielectric constant
 * @return 0 on success, -1 if target is unachievable
 */
int hs_via_optimize_antipad(hs_via_geometry_t *geo, double target_z, double er);

/**
 * Calculate the number of ground stitching vias needed along a
 * differential pair routing.
 *
 * Rule of thumb: spacing ≤ λ/10 at the maximum frequency component
 * of the signal.
 *
 * Number = ceil(L_route / (λ_max/10))
 *
 * For 10 Gbps signal (BW ≈ 7 GHz on FR-4, λ ≈ 2.3 cm):
 *   Stitch spacing ≈ 2.3 mm → ~43 vias per 10 cm route
 *
 * @param route_length_m: Length of differential route (m)
 * @param max_freq_hz: Maximum frequency component (Hz)
 * @param er_eff: Effective dielectric constant
 * @return Number of stitching vias required
 */
double hs_stitching_via_count(double route_length_m, double max_freq_hz,
                               double er_eff);

/**
 * Evaluate via feasibility against manufacturing constraints.
 *
 * Checks: aspect ratio, minimum drill size, minimum annular ring,
 * pad/antipad ratios, and stub requirements.
 *
 * @param geo: Via geometry to evaluate
 * @param constraints: Manufacturing constraints
 * @return 0 if feasible, error code otherwise
 */
int hs_via_check_feasibility(const hs_via_geometry_t *geo,
                              const hs_via_constraints_t *constraints);

#endif /* HS_VIA_H */
