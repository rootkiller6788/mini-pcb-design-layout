#ifndef PCB_IMPEDANCE_H
#define PCB_IMPEDANCE_H

#include "pcb_transmission_line.h"
#include "pcb_material.h"

/* ============================================================================
 * L1-L4: PCB Impedance Calculation — Fundamental definitions and laws
 *
 * PCB impedance control is based on solving Maxwell's equations in the
 * quasi-static limit for planar transmission line cross-sections.
 * The characteristic impedance Z₀ = √(L/C) depends only on geometry and εr.
 *
 * Key equation sets (moving from simple to precise):
 *   Wheeler  (1965) — microstrip, empirical fit
 *   IPC-2141A (2004) — standard industry reference
 *   Hammerstad-Jensen (1980) — most accurate closed-form for microstrip
 *   Cohn (1954) — stripline
 * ========================================================================= */

/* ===================================================================
 * L1: Core Definitions — Impedance types
 * =================================================================== */

/* L1: Single-ended impedance result */
typedef struct {
    double z0_ohm;          /* Characteristic impedance (Ω) */
    double er_eff;          /* Effective dielectric constant */
    double w_over_h;        /* Width-to-height ratio */
    double delay_ps_per_mm; /* Propagation delay (ps/mm) */
    double capacitance_pf_per_mm; /* Per-unit-length capacitance */
    double inductance_nh_per_mm;  /* Per-unit-length inductance */
    const char *formula_used; /* Which formula was applied */
} ImpedanceResult;

/* L1: Differential impedance result */
typedef struct {
    double z_diff_ohm;      /* Differential impedance (Ω) */
    double z_odd_ohm;       /* Odd-mode impedance (Ω) */
    double z_even_ohm;      /* Even-mode impedance (Ω) */
    double z_common_ohm;    /* Common-mode impedance = z_even/2 */
    double coupling_coeff;  /* k = (z_even - z_odd) / (z_even + z_odd) */
    double er_eff_diff;     /* Effective εr for differential mode */
    double er_eff_common;   /* Effective εr for common mode */
} DiffImpedanceResult;

/* ===================================================================
 * L2: Core Concepts — Microstrip, Stripline, CPW impedance
 * =================================================================== */

/* L2: Microstrip impedance using IPC-2141A formula
 *     Z₀ = (87 / √(εr + 1.41)) · ln(5.98·h / (0.8·w + t))  [w/h < 1]
 *     Z₀ = (120π / √εeff) / (w/h + 1.393 + 0.667·ln(w/h + 1.444))  [w/h ≥ 1]
 *     Section 4.3.1 of IPC-2141A */
ImpedanceResult impedance_microstrip_ipc(const MicrostripGeometry *geo);

/* L2: Microstrip impedance using Hammerstad-Jensen more accurate formula
 *     Accounts for dispersion and finite conductor thickness.
 *     Jensen (1976) + Hammerstad (1980) refinements.
 *     Generally ±1% accuracy for 0.1 < w/h < 20 */
ImpedanceResult impedance_microstrip_hammerstad(const MicrostripGeometry *geo);

/* L2: Microstrip impedance using Wheeler's original formula
 *     Wheeler, H.A. "Transmission-Line Properties of a Strip on a Dielectric
 *     Sheet on a Plane," IEEE Trans. MTT, 1977 */
ImpedanceResult impedance_microstrip_wheeler(const MicrostripGeometry *geo);

/* L2: Stripline impedance — symmetric (h1 = h2 = h)
 *     Z₀ = (60 / √εr) · ln(4·h / (0.67·π·w·(0.8 + t/w)))
 *     Cohn, S.B. "Characteristic Impedance of the Shielded-Strip
 *     Transmission Line," IRE Trans. MTT, 1954 */
ImpedanceResult impedance_stripline_symmetric(const StriplineGeometry *geo);

/* L2: Stripline impedance — asymmetric (h1 ≠ h2)
 *     Uses velocity ratio method from IPC-2141A §4.3.2 */
ImpedanceResult impedance_stripline_asymmetric(const StriplineGeometry *geo);

/* L2: Coplanar waveguide impedance
 *     Z₀ = (30π / √εeff) · K(k₀') / K(k₀)
 *     Where K is complete elliptic integral of the first kind.
 *     Wen, C.P. "Coplanar Waveguide," IEEE Trans. MTT, 1969 */
ImpedanceResult impedance_cpw(const CpwGeometry *geo);

/* L2: Grounded coplanar waveguide (GCPW) impedance */
ImpedanceResult impedance_gcpw(const CpwGeometry *geo);

/* ===================================================================
 * L3: Mathematical Structures — Differential pair analysis
 * =================================================================== */

/* L3: Edge-coupled differential impedance
 *     Uses odd/even mode analysis from IPC-2141A §4.3.3
 *     Z_diff = 2 · Z_odd
 *     Cohn nomograph method + analytical approximation */
DiffImpedanceResult impedance_diff_pair_edge(const DiffPairEdgeGeometry *geo);

/* L3: Broadside-coupled differential impedance
 *     Traces in adjacent layers, vertically aligned */
DiffImpedanceResult impedance_diff_pair_broadside(const DiffPairBroadsideGeometry *geo);

/* L3: Compute mode conversion S-parameters for differential pair
 *     SDD11, SDD21, SCC11, SCD21 for mixed-mode S-parameter analysis */
void impedance_mode_conversion(double z_odd, double z_even,
                                double z0_diff, double length_m, double freq_hz,
                                double complex *sdd11, double complex *sdd21,
                                double complex *scc11, double complex *scd21);

/* ===================================================================
 * L4: Fundamental Laws — Inverse design (geometry from target Z₀)
 * =================================================================== */

/* L4: Compute microstrip trace width for target impedance
 *     Inverse of Hammerstad-Jensen using Brent's method root-finding.
 *     Given Z₀_target, h, εr, t → returns required w.
 *     Returns 0 on failure, w in µm on success. */
double impedance_microstrip_w_for_z0(double target_z0_ohm, double height_um,
                                      double er, double thickness_um,
                                      const char **formula_used);

/* L4: Compute stripline trace width for target impedance */
double impedance_stripline_w_for_z0(double target_z0_ohm, double height_um,
                                     double er, double thickness_um,
                                     const char **formula_used);

/* L4: Compute edge-coupled differential pair spacing for target Z_diff
 *     Given trace width w, height h, εr → required edge-to-edge spacing s */
double impedance_diff_spacing_for_z(double target_z_diff_ohm,
                                     double trace_width_um, double height_um,
                                     double er, double thickness_um);

/* L4: Compute effective εr for microstrip
 *     εeff = (εr + 1)/2 + (εr - 1)/2 · 1/√(1 + 12·h/w)  [Schneider, 1969] */
double impedance_er_effective_microstrip(double er, double w, double h);

/* L4: Compute effective εr for stripline
 *     εeff = εr (TEM mode, pure dielectric) */
double impedance_er_effective_stripline(double er);

/* ===================================================================
 * L5: Algorithms — Impedance sweep and optimization
 * =================================================================== */

/* L5: Sweep trace width and compute Z₀ vs w curve
 *     Returns arrays of w (µm) and Z₀ (Ω) for the given range.
 *     Useful for impedance feasibility analysis. */
void impedance_sweep_width(MicrostripGeometry *geo,
                            double w_start_um, double w_end_um, int num_points,
                            double *widths_um, double *z0_values,
                            const char **formulas);

/* L5: Compute impedance sensitivity to manufacturing variations
 *     ∂Z₀/∂w, ∂Z₀/∂h, ∂Z₀/∂εr, ∂Z₀/∂t
 *     Using central finite differences. */
typedef struct {
    double dZ_dw;    /* Ω/µm */
    double dZ_dh;    /* Ω/µm */
    double dZ_der;   /* Ω per unit εr */
    double dZ_dt;    /* Ω/µm */
    double worst_case_delta_Z; /* Max expected ΔZ given tolerances */
} ImpedanceSensitivity;

ImpedanceSensitivity impedance_sensitivity(const MicrostripGeometry *geo,
                                            double dw_um, double dh_um,
                                            double der, double dt_um);

/* L5: Compute required trace width considering solder mask effect
 *     Solder mask increases εeff, slightly reducing Z₀.
 *     Uses perturbation method: Z₀_masked ≈ Z₀ - ΔZ_mask */
ImpedanceResult impedance_microstrip_with_mask(const MicrostripGeometry *geo,
                                                double mask_thickness_um,
                                                double mask_er);

/* ===================================================================
 * L6: Canonical Problems — Standard impedance designs
 * =================================================================== */

/* L6: Design a 50Ω microstrip on standard FR-4 stackup */
ImpedanceResult impedance_standard_50ohm_fr4(double height_um,
                                              double copper_weight_oz);

/* L6: Design a 100Ω differential pair on FR-4 */
DiffImpedanceResult impedance_standard_100ohm_diff_fr4(double height_um,
                                                        double copper_weight_oz,
                                                        double trace_width_um);

/* L6: Design 90Ω USB differential pair (typical USB 2.0/3.x requirement) */
DiffImpedanceResult impedance_usb_90ohm_diff(double height_um,
                                              double copper_weight_oz);

/* L6: Compute impedance vs frequency dispersion (up to 50 GHz)
 *     Fills arrays with Z₀(f) and εeff(f) using Getsinger dispersion model:
 *     εeff(f) = εr - (εr - εeff(0)) / (1 + G·(f/fp)²)
 *     where fp = Z₀/(2·μ₀·h) and G is a geometry-dependent factor */
void impedance_dispersion(const MicrostripGeometry *geo,
                           double f_start_ghz, double f_stop_ghz,
                           int num_points,
                           double *frequencies_ghz, double *z0_values);

/* ===================================================================
 * L7: Applications — High-speed serial design
 * =================================================================== */

/* L7: PCIe Gen5 85Ω differential pair design at 32 GT/s */
DiffImpedanceResult impedance_pcie_gen5_85ohm(double height_um);

/* L7: 100Ω Ethernet (1000BASE-T / 10GBASE-T) differential pair */
DiffImpedanceResult impedance_ethernet_100ohm_diff(double height_um,
                                                    double copper_weight_oz);

/* L7: Compute impedance profile along a non-uniform transmission line
 *     Used for connector launch, package escape, and BGA breakout regions */
typedef struct {
    double position_mm;
    double z0_ohm;
} ImpedanceProfilePoint;

int impedance_profile(const MicrostripGeometry *base_geo,
                       int num_segments,
                       const double *widths_um,   /* Width per segment */
                       const double *lengths_mm,  /* Length per segment */
                       ImpedanceProfilePoint *profile);

/* ===================================================================
 * L8: Advanced — Frequency-dependent effects
 * =================================================================== */

/* L8: Surface roughness impact on impedance
 *     The increased path length due to roughness causes a slight
 *     increase in effective L and Z₀ at high frequencies.
 *     Uses Hammerstad correction to adjust conductor loss into
 *     an effective impedance shift. */
double impedance_roughness_shift(double z0_ideal, double freq_ghz,
                                  const CopperRoughness *rough,
                                  double conductivity);

/* L8: Impedance analysis considering glass-weave skew
 *     For thin traces over woven glass laminates, the local εr varies
 *     periodically between glass-rich (~6.0) and resin-rich (~3.5) regions.
 *     Computes the Z₀ variation range. */
typedef struct {
    double z0_min;
    double z0_max;
    double z0_mean;
    double z0_variation_pct;
    double skew_ps_per_mm;    /* Worst-case intra-pair skew */
} GlassWeaveImpedance;

GlassWeaveImpedance impedance_glass_weave_effect(const MicrostripGeometry *geo,
                                                  double glass_pitch_mm,
                                                  double er_glass,
                                                  double er_resin,
                                                  double glass_volume_fraction);

#endif /* PCB_IMPEDANCE_H */
