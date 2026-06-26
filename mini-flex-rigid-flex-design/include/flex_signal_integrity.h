/**
 * @file flex_signal_integrity.h
 * @brief Signal Integrity Analysis for Flexible and Rigid-Flex PCBs
 *
 * Flex circuits present unique signal integrity challenges: thin dielectrics,
 * varying dielectric constants, absence of solid reference planes in bend zones,
 * and proximity effects. This module implements transmission line analysis
 * specifically adapted for flex geometries.
 *
 * L1 (Definitions): Characteristic impedance, differential impedance,
 *                    insertion loss, return loss, crosstalk
 * L2 (Core Concepts): Microstrip on flex, stripline on rigid-flex,
 *                     impedance control, reference plane discontinuity
 * L3 (Math Structures): Maxwell's equations simplified to TL theory,
 *                        telegrapher's equations, S-parameters
 * L4 (Fundamental Laws): Transmission line theory, skin effect,
 *                         dielectric loss model (Djordjevic-Sarkar)
 * L5 (Algorithms): Impedance calculation, loss budgeting,
 *                   crosstalk estimation, via impedance modeling
 *
 * @module mini-flex-rigid-flex-design
 */

#ifndef FLEX_SIGNAL_INTEGRITY_H
#define FLEX_SIGNAL_INTEGRITY_H

#include "flex_material.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * L1 — Transmission Line Structures for Flex
 * -------------------------------------------------------------------------*/

/** Transmission line types supported in flex/rigid-flex */
typedef enum {
    FLEX_TL_MICROSTRIP_SURFACE = 0,  /**< Surface microstrip (coverlay coated) */
    FLEX_TL_MICROSTRIP_EMBEDDED,     /**< Embedded microstrip (adhesive + coverlay on top) */
    FLEX_TL_STRIPLINE_SYMMETRIC,     /**< Symmetric stripline (rigid section) */
    FLEX_TL_STRIPLINE_ASYMMETRIC,    /**< Asymmetric stripline */
    FLEX_TL_DIFF_MICROSTRIP,         /**< Edge-coupled differential microstrip */
    FLEX_TL_DIFF_STRIPLINE,          /**< Edge-coupled differential stripline */
    FLEX_TL_COPLANAR_WAVEGUIDE,      /**< Coplanar waveguide on flex */
    FLEX_TL_COUNT
} flex_tl_type_t;

/**
 * @brief Complete transmission line geometry parameters.
 */
typedef struct {
    flex_tl_type_t tl_type;
    double trace_width_um;            /**< Trace width (μm) */
    double trace_thickness_um;        /**< Trace thickness (μm) — copper weight */
    double dielectric_thickness_um;   /**< Substrate thickness to reference plane (μm) */
    double dielectric_constant;       /**< Relative permittivity εr */
    double loss_tangent;              /**< Dielectric loss tangent */
    double trace_spacing_um;          /**< Edge-to-edge spacing for diff pair (μm) */
    double coverlay_thickness_um;     /**< Coverlay thickness above trace (μm) */
    double coverlay_dk;              /**< Coverlay dielectric constant */
    double solder_mask_thickness_um;  /**< Soldermask thickness (rigid section) */
    double solder_mask_dk;           /**< Soldermask dielectric constant */
    double upper_dielectric_um;       /**< Upper dielectric thickness (asym stripline) */
    double lower_dielectric_um;       /**< Lower dielectric thickness (asym stripline) */
    double copper_roughness_um;       /**< RMS copper surface roughness (μm) */
    double frequency_hz;              /**< Frequency of interest (Hz) */
    int has_coverlay;                 /**< 1 if coverlay present over trace */
    int has_solder_mask;              /**< 1 if soldermask present (rigid section) */
} flex_tl_params_t;

/**
 * @brief Transmission line analysis results.
 */
typedef struct {
    double characteristic_impedance;      /**< Z0 in Ω */
    double differential_impedance;        /**< Zdiff in Ω (for diff pairs) */
    double even_mode_impedance;           /**< Zeven in Ω */
    double odd_mode_impedance;            /**< Zodd in Ω */
    double effective_dk;                  /**< Effective dielectric constant εeff */
    double propagation_delay_ps_per_mm;   /**< Tpd in ps/mm */
    double phase_velocity_m_per_s;        /**< vp in m/s */
    double conductor_loss_db_per_mm;      /**< αc in dB/mm */
    double dielectric_loss_db_per_mm;     /**< αd in dB/mm */
    double total_loss_db_per_mm;          /**< α_total in dB/mm */
    double crosstalk_coefficient;         /**< NEXT coupling coefficient */
    double wavelength_mm;                 /**< λ at frequency of interest */
    double skin_depth_um;                 /**< δ (skin depth) in μm */
    double copper_roughness_factor;       /**< Kr — Hammerstad roughness correction */
    double via_impedance_ohm;             /**< Estimated via impedance */
} flex_tl_result_t;

/* ---------------------------------------------------------------------------
 * L5 — Characteristic Impedance Calculation (IPC-2141 / Wheeler)
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate surface microstrip impedance using the Wheeler model.
 *
 * Z0 = (60 / √εeff) * ln(8h/W + W/(4h))   for W/h ≤ 1
 * Z0 = (120π / √εeff) / (W/h + 1.393 + 0.667*ln(W/h + 1.444))  for W/h > 1
 *
 * This is the standard Wheeler formula widely used in PCB design.
 *
 * @param trace_width_um Trace width (μm)
 * @param dielectric_thickness_um Dielectric thickness to reference (μm)
 * @param dk Relative permittivity of substrate
 * @param trace_thickness_um Trace thickness (μm), typically 18 or 35
 * @return Characteristic impedance Z0 in Ω
 *
 * Reference: Wheeler, "Transmission-Line Properties of a Strip...", IEEE MTT, 1965
 * Complexity: O(1)
 */
double flex_microstrip_z0_wheeler(double trace_width_um,
                                   double dielectric_thickness_um,
                                   double dk,
                                   double trace_thickness_um);

/**
 * @brief Calculate embedded microstrip impedance (with coverlay).
 *
 * Similar to surface microstrip but with an additional dielectric layer
 * (coverlay) above the trace. Effective dielectric constant is a
 * weighted average of substrate and coverlay.
 *
 * @param trace_width_um Trace width
 * @param substrate_thickness_um Distance to reference plane
 * @param substrate_dk Substrate εr
 * @param coverlay_thickness_um Coverlay thickness
 * @param coverlay_dk Coverlay εr
 * @param trace_thickness_um Trace thickness
 * @return Characteristic impedance Z0 in Ω
 *
 * Complexity: O(1)
 */
double flex_microstrip_embedded_z0(double trace_width_um,
                                    double substrate_thickness_um,
                                    double substrate_dk,
                                    double coverlay_thickness_um,
                                    double coverlay_dk,
                                    double trace_thickness_um);

/**
 * @brief Calculate symmetric stripline impedance.
 *
 * Z0 = (60 / √εr) * ln(4b / (0.67πW * (0.8 + t/W)))
 *
 * where b = total distance between reference planes.
 *
 * @param trace_width_um Trace width
 * @param dielectric_thickness_um Total dielectric thickness between planes
 * @param dk Dielectric constant
 * @param trace_thickness_um Trace thickness
 * @return Characteristic impedance Z0 in Ω
 *
 * Reference: Cohn, "Shielded Coupled-Strip Transmission Line", IRE MTT, 1955
 * Complexity: O(1)
 */
double flex_stripline_z0(double trace_width_um,
                          double dielectric_thickness_um,
                          double dk,
                          double trace_thickness_um);

/**
 * @brief Calculate asymmetric stripline impedance.
 *
 * For stripline with unequal distances to upper and lower reference planes.
 *
 * @param trace_width_um Trace width
 * @param h1_um Distance to upper reference plane
 * @param h2_um Distance to lower reference plane
 * @param dk Dielectric constant
 * @param trace_thickness_um Trace thickness
 * @return Z0 in Ω
 *
 * Complexity: O(1)
 */
double flex_stripline_asymmetric_z0(double trace_width_um,
                                     double h1_um, double h2_um,
                                     double dk,
                                     double trace_thickness_um);

/* ---------------------------------------------------------------------------
 * Effective Dielectric Constant
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate effective dielectric constant for microstrip.
 *
 * εeff = (εr + 1)/2 + (εr - 1)/2 * 1/√(1 + 12h/W)
 *
 * This accounts for the fact that microstrip fields exist partially
 * in air and partially in the dielectric.
 *
 * @param dk Substrate dielectric constant
 * @param trace_width_um Trace width
 * @param dielectric_thickness_um Substrate thickness
 * @return Effective dielectric constant εeff
 *
 * Complexity: O(1)
 * Reference: Hammerstad & Jensen, IEEE MTT-S, 1980
 */
double flex_effective_dk_microstrip(double dk,
                                     double trace_width_um,
                                     double dielectric_thickness_um);

/* ---------------------------------------------------------------------------
 * Differential Pair Impedance
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate edge-coupled differential microstrip impedance.
 *
 * Zdiff = 2 * Z0_odd = 2 * Z0 * √((1 - k) / (1 + k))
 *
 * where k = coupling coefficient determined by spacing/height ratio.
 *
 * @param trace_width_um Single trace width
 * @param trace_spacing_um Edge-to-edge spacing
 * @param dielectric_thickness_um Height above reference plane
 * @param dk Dielectric constant
 * @param trace_thickness_um Trace thickness
 * @return Differential impedance Zdiff in Ω
 *
 * Complexity: O(1)
 */
double flex_diff_microstrip_z0(double trace_width_um,
                                double trace_spacing_um,
                                double dielectric_thickness_um,
                                double dk,
                                double trace_thickness_um);

/**
 * @brief Calculate edge-coupled differential stripline impedance.
 *
 * @param trace_width_um Single trace width
 * @param trace_spacing_um Edge-to-edge spacing
 * @param dielectric_thickness_um Total height between planes
 * @param dk Dielectric constant
 * @param trace_thickness_um Trace thickness
 * @return Differential impedance in Ω
 */
double flex_diff_stripline_z0(double trace_width_um,
                               double trace_spacing_um,
                               double dielectric_thickness_um,
                               double dk,
                               double trace_thickness_um);

/* ---------------------------------------------------------------------------
 * L5 — Loss Calculations
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate conductor loss (skin effect) for a microstrip trace.
 *
 * αc = R_s / (Z0 * W_eff)   (simplified low-loss approximation)
 *
 * where R_s = √(π * f * μ0 * ρ) = surface resistance,
 * and ρ = resistivity of copper corrected for roughness.
 *
 * @param frequency_hz Frequency (Hz)
 * @param trace_width_um Trace width (μm)
 * @param trace_thickness_um Trace thickness (μm)
 * @param z0_ohm Characteristic impedance (Ω)
 * @param roughness_um RMS copper roughness (μm)
 * @return Conductor loss αc in dB/mm
 *
 * Complexity: O(1)
 */
double flex_conductor_loss_db_per_mm(double frequency_hz,
                                      double trace_width_um,
                                      double trace_thickness_um,
                                      double z0_ohm,
                                      double roughness_um);

/**
 * @brief Calculate dielectric loss.
 *
 * αd = (π * f * √εeff * tanδ) / (c0)  (in nepers/m)
 * αd_dB = 8.686 * αd_np   (convert to dB/m)
 *
 * @param frequency_hz Frequency (Hz)
 * @param effective_dk Effective dielectric constant
 * @param loss_tangent Dielectric loss tangent
 * @return Dielectric loss in dB/mm
 *
 * Complexity: O(1)
 */
double flex_dielectric_loss_db_per_mm(double frequency_hz,
                                       double effective_dk,
                                       double loss_tangent);

/**
 * @brief Calculate the Hammerstad copper roughness correction factor.
 *
 * Kr = 1 + (2/π) * arctan(1.4 * (Rrms / δ)^2)
 *
 * where Rrms = RMS surface roughness, δ = skin depth.
 * Smooth copper: Kr ≈ 1.0. Standard ED: Kr ≈ 1.2-1.5 at 10 GHz.
 *
 * @param rms_roughness_um RMS surface roughness
 * @param skin_depth_um Skin depth at frequency of interest
 * @return Roughness correction factor (≥ 1.0)
 *
 * Reference: Hammerstad & Bekkadal, "Microstrip Handbook", ELAB, 1975
 * Complexity: O(1)
 */
double flex_hammerstad_roughness_factor(double rms_roughness_um,
                                         double skin_depth_um);

/**
 * @brief Calculate skin depth in copper at a given frequency.
 *
 * δ = √(ρ / (π * f * μ0))
 *
 * where ρ = 1.72e-8 Ω·m for copper, μ0 = 4π × 10^-7 H/m.
 *
 * @param frequency_hz Frequency in Hz
 * @return Skin depth in μm
 *
 * Complexity: O(1)
 */
double flex_skin_depth_um(double frequency_hz);

/* ---------------------------------------------------------------------------
 * L5 — Crosstalk Estimation
 * -------------------------------------------------------------------------*/

/**
 * @brief Estimate near-end crosstalk (NEXT) coefficient for parallel microstrips.
 *
 * NEXT ≈ (1/4) * (Cm/C + Lm/L)
 *
 * Simplified: K_NEXT ≈ 1 / (1 + (s/h)^2)
 *
 * where s = trace spacing, h = height above reference plane.
 *
 * @param trace_spacing_um Edge-to-edge spacing
 * @param dielectric_thickness_um Height above reference plane
 * @return NEXT coupling coefficient (dimensionless, < 1)
 *
 * Complexity: O(1)
 */
double flex_next_coefficient(double trace_spacing_um,
                              double dielectric_thickness_um);

/**
 * @brief Estimate far-end crosstalk (FEXT) coefficient.
 *
 * FEXT per unit length in dB:
 *   FEXT_dB ≈ -20 * log10(1 + (s/h)^2) * (length / λ)
 *
 * @param trace_spacing_um Edge-to-edge spacing
 * @param dielectric_thickness_um Height above reference
 * @param parallel_length_mm Length over which traces run parallel
 * @param wavelength_mm Signal wavelength
 * @return FEXT in dB at the far end
 */
double flex_fext_db(double trace_spacing_um,
                     double dielectric_thickness_um,
                     double parallel_length_mm,
                     double wavelength_mm);

/* ---------------------------------------------------------------------------
 * L5 — Time Domain Parameters
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate propagation delay from effective dielectric constant.
 *
 * Tpd = √εeff / c0  (in ps/mm)
 *
 * where c0 = 299.792458 mm/ns ≈ 3.3356 ps/mm in vacuum.
 *
 * @param effective_dk Effective dielectric constant
 * @return Propagation delay in ps/mm
 *
 * Complexity: O(1)
 */
double flex_propagation_delay_ps_per_mm(double effective_dk);

/**
 * @brief Compute signal wavelength in the transmission line.
 *
 * λ = c0 / (f * √εeff)
 *
 * @param frequency_hz Signal frequency
 * @param effective_dk Effective dielectric constant
 * @return Wavelength in mm
 */
double flex_wavelength_mm(double frequency_hz, double effective_dk);

/**
 * @brief Calculate the critical length above which transmission line
 *        effects must be considered.
 *
 * L_critical = tr / (2 * Tpd)   (rule of thumb)
 *
 * where tr = rise time of the signal.
 * A trace is "electrically long" if length > λ/10 or > tr/(2*Tpd).
 *
 * @param rise_time_ps Signal rise time in picoseconds
 * @param tpd_ps_per_mm Propagation delay in ps/mm
 * @return Critical length in mm
 *
 * Complexity: O(1)
 */
double flex_critical_length_mm(double rise_time_ps, double tpd_ps_per_mm);

/**
 * @brief Run a complete transmission line analysis for a flex trace.
 *
 * This is the main entry point: computes all impedance, loss, delay,
 * and crosstalk parameters in one call.
 *
 * @param params TL geometry and material parameters
 * @param result [out] Complete analysis results
 * @return 0 on success, -1 on invalid input
 *
 * Complexity: O(1) per parameter, O(1) total
 */
int flex_tl_analyze(const flex_tl_params_t *params,
                     flex_tl_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* FLEX_SIGNAL_INTEGRITY_H */
