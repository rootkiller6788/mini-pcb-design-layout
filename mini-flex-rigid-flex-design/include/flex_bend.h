/**
 * @file flex_bend.h
 * @brief Flex PCB Bend Mechanics, Stress Analysis, and Life Prediction
 *
 * This module implements the mechanical analysis of flexible circuits
 * under bending conditions. Covers static (one-time) and dynamic (repeated)
 * bending scenarios per IPC-2223 and related industry standards.
 *
 * L1 (Definitions): Minimum bend radius, dynamic vs static flex,
 *                    neutral axis, strain, copper elongation limit
 * L3 (Math Structures): Beam bending theory, multilayer composite
 *                        mechanics, strain distribution
 * L4 (Fundamental Laws): IPC-2223 minimum bend radius formula,
 *                         Hooke's law in bending, Coffin-Manson fatigue
 * L5 (Algorithms): Minimum bend radius calculation, bend cycle life
 *                   estimation, strain profile computation
 *
 * @module mini-flex-rigid-flex-design
 */

#ifndef FLEX_BEND_H
#define FLEX_BEND_H

#include "flex_material.h"
#include "flex_stackup.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * L1 — Bend Configuration Types
 * -------------------------------------------------------------------------*/

/** Bend direction relative to copper grain orientation */
typedef enum {
    FLEX_BEND_PARALLEL_GRAIN = 0,     /**< Bend axis parallel to copper rolling direction */
    FLEX_BEND_PERPENDICULAR_GRAIN,    /**< Bend axis perpendicular to rolling direction */
    FLEX_BEND_45DEG_GRAIN,            /**< Bend at 45° to grain — compromise */
    FLEX_BEND_GRAIN_COUNT
} flex_bend_grain_orientation_t;

/** Bend configuration geometry */
typedef enum {
    FLEX_BEND_SINGLE = 0,             /**< Simple single bend */
    FLEX_BEND_U_SHAPE,                /**< 180° U-bend */
    FLEX_BEND_Z_SHAPE,                /**< Two opposing bends (S-curve) */
    FLEX_BEND_SPIRAL,                 /**< Spiral wrap */
    FLEX_BEND_FOLD,                   /**< Sharp fold (0° bend radius — not recommended) */
    FLEX_BEND_COUNT
} flex_bend_config_t;

/**
 * @brief Complete bend analysis parameters
 */
typedef struct {
    flex_bend_config_t config;               /**< Bend geometry type */
    flex_bend_grain_orientation_t grain_orientation;
    double bend_radius_mm;                   /**< Centerline bend radius (mm) */
    double bend_angle_deg;                   /**< Bend angle (degrees) */
    double total_thickness_mm;               /**< Total flex thickness being bent */
    double copper_thickness_total_um;        /**< Sum of all Cu layer thicknesses */
    double copper_elongation_limit_percent;  /**< Copper elongation at break (%) */
    int num_layers;                          /**< Number of layers in bend zone */
    int is_dynamic;                          /**< 1 = dynamic flex, 0 = static */
    double expected_cycles;                  /**< Expected bend cycles (dynamic only) */
    double operating_temp_min_c;             /**< Minimum operating temperature */
    double operating_temp_max_c;             /**< Maximum operating temperature */
    double youngs_modulus_copper_mpa;        /**< Copper Young's modulus (~117000 MPa) */
} flex_bend_params_t;

/**
 * @brief Bend analysis results
 */
typedef struct {
    double min_bend_radius_mm;               /**< Minimum allowed bend radius per IPC-2223 */
    double actual_bend_radius_mm;            /**< As-designed bend radius */
    double safety_factor;                    /**< Bend radius safety factor (actual/minimum) */
    double max_copper_strain_percent;        /**< Maximum copper strain under bend */
    double strain_safety_factor;             /**< Strain safety factor */
    double neutral_axis_offset_mm;           /**< Neutral axis position from inner surface */
    double estimated_cycles_to_failure;      /**< Predicted bend cycles to failure */
    double bend_force_n_per_mm;             /**< Required bending force per mm width */
    double springback_angle_deg;             /**< Expected springback after bending */
    int is_compliant_ipc2223;               /**< 1 if design meets IPC-2223 minimum */
    int failure_mode;                        /**< 0=OK, 1=copper_crack, 2=delamination, 3=buckling */
    char failure_description[256];           /**< Human-readable analysis outcome */
} flex_bend_result_t;

/* ---------------------------------------------------------------------------
 * L4 — IPC-2223 Minimum Bend Radius (Fundamental Law)
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate the IPC-2223 minimum bend radius for a flex circuit.
 *
 * IPC-2223 §5.2.4: For single-sided flex:
 *   R_min = (t / ε_max) * (1 - n_layers)
 *
 * Simplified engineering formula widely used in industry:
 *   R_min = k * t  where k depends on layer count
 *
 * For 1-layer: k ≈ 6   (R_min ≈ 6 × total_thickness)
 * For 2-layer: k ≈ 12  (R_min ≈ 12 × total_thickness)
 * For multilayers > 2: k ≈ 20-30 depending on construction
 *
 * @param thickness_mm Total flex thickness in mm
 * @param num_copper_layers Number of copper layers in the bend zone
 * @param copper_el_limit_percent Allowable copper elongation (%)
 * @return Minimum allowable bend radius in mm
 *
 * Reference: IPC-2223C "Sectional Design Standard for Flexible/Rigid-Flexible
 *            Printed Boards", 2018
 * Complexity: O(1)
 */
double flex_min_bend_radius_ipc2223(double thickness_mm,
                                     int num_copper_layers,
                                     double copper_el_limit_percent);

/**
 * @brief Calculate minimum bend radius using first-principles beam theory.
 *
 * R_min = (E_total * y_max) / σ_allowable
 *
 * where E_total = effective modulus of composite, y_max = distance from
 * neutral axis to outermost copper surface, σ_allowable = allowable stress.
 *
 * This is the more accurate physics-based method compared to the
 * simple IPC factor method.
 *
 * @param params Full bend parameters
 * @return Minimum bend radius from beam theory (mm)
 *
 * Reference: Timoshenko & Goodier, "Theory of Elasticity"
 * Complexity: O(n_layers)
 */
double flex_min_bend_radius_beam_theory(const flex_bend_params_t *params);

/* ---------------------------------------------------------------------------
 * L5 — Strain and Stress Analysis
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate the maximum copper strain during bending.
 *
 * ε_max = (y_max) / R
 *
 * where y_max = distance from neutral axis to outermost fiber of the
 * copper layer farthest from the neutral axis.
 *
 * For multilayer flex, the copper layer experiences the most strain.
 * Copper RA has allowable strain ~16% before crack initiation;
 * Copper ED has ~5-8%.
 *
 * @param bend_radius_mm Centerline bend radius (mm)
 * @param neutral_axis_offset_mm Distance from inner bend surface to neutral axis
 * @param outer_copper_distance_mm Distance from inner surface to outer copper
 * @return Maximum strain as percentage (%)
 *
 * Complexity: O(1)
 */
double flex_copper_strain_percent(double bend_radius_mm,
                                   double neutral_axis_offset_mm,
                                   double outer_copper_distance_mm);

/**
 * @brief Calculate strain distribution through the thickness of the flex.
 *
 * ε(y) = (y - y_neutral) / R
 *
 * Returns strain at each of n_points equally spaced through thickness.
 *
 * @param bend_radius_mm Centerline bend radius
 * @param neutral_offset_mm Neutral axis position from inner surface
 * @param thickness_mm Total flex thickness
 * @param strain_profile [out] Array of n_points strain values (%)
 * @param n_points Number of sample points through thickness
 *
 * Complexity: O(n_points)
 */
void flex_strain_profile(double bend_radius_mm,
                          double neutral_offset_mm,
                          double thickness_mm,
                          double *strain_profile,
                          int n_points);

/**
 * @brief Calculate the stress at the copper-dielectric interface during bending.
 *
 * Interfacial shear stress is the primary driver of delamination in
 * dynamic flex applications. τ = (V * Q) / (I * b)
 *
 * @param bend_radius_mm Bend radius
 * @param copper_thickness_um Copper thickness
 * @param dielectric_thickness_um Dielectric thickness
 * @param copper_modulus_mpa Copper Young's modulus
 * @param dielectric_modulus_mpa Dielectric Young's modulus
 * @return Interfacial shear stress in MPa
 *
 * Complexity: O(1)
 * Reference: Suhir, "Interfacial Stresses in Bimetal Thermostats", JAM, 1986
 */
double flex_interfacial_shear_stress(double bend_radius_mm,
                                      double copper_thickness_um,
                                      double dielectric_thickness_um,
                                      double copper_modulus_mpa,
                                      double dielectric_modulus_mpa);

/**
 * @brief Calculate the bending moment required to achieve the specified radius.
 *
 * M = E * I / R  (pure bending of beam, small deflection)
 *
 * @param flexural_rigidity_n_mm2 Flexural rigidity D (from stackup analysis)
 * @param bend_radius_mm Desired bend radius
 * @return Bending moment per unit width (N·mm/mm width)
 *
 * Complexity: O(1)
 */
double flex_bending_moment(double flexural_rigidity_n_mm2,
                            double bend_radius_mm);

/* ---------------------------------------------------------------------------
 * L5 — Dynamic Flex Life Prediction
 * -------------------------------------------------------------------------*/

/**
 * @brief Estimate the bend cycle life using a modified Coffin-Manson approach.
 *
 * N_f = (ε_f / ε_applied)^c
 *
 * where ε_f = fatigue ductility coefficient (≈ 0.16 for RA copper),
 * ε_applied = cyclic strain amplitude,
 * c = fatigue ductility exponent (≈ -0.5 to -0.7 for copper).
 *
 * @param cyclic_strain_percent Cyclic strain amplitude (%)
 * @param copper_type Copper type (affects fatigue ductility)
 * @return Estimated cycles to failure (N_f)
 *
 * Complexity: O(1)
 * Reference: Engelmaier, "Fatigue of Electronic Materials", 1982
 */
double flex_cycles_to_failure_coffin_manson(double cyclic_strain_percent,
                                              flex_copper_type_t copper_type);

/**
 * @brief Estimate dynamic flex life using IPC-TM-650 empirical model.
 *
 * N_f = A * (R / t)^B
 *
 * where A and B are empirically determined constants per IPC-TM-650 2.4.3.
 * This is the simpler industry-standard method.
 *
 * @param bend_radius_mm Bend radius
 * @param total_thickness_mm Total flex thickness
 * @param copper_type Copper type
 * @return Estimated cycles to failure
 *
 * Reference: IPC-TM-650 Method 2.4.3 "Flexural Fatigue, Flexible Printed Wiring"
 */
double flex_cycles_ipc_tm650(double bend_radius_mm,
                              double total_thickness_mm,
                              flex_copper_type_t copper_type);

/**
 * @brief Derate bend life for temperature using the Arrhenius model.
 *
 * Life_derated = Life_25C * exp((Ea/k) * (1/T_op - 1/298))
 *
 * @param cycles_at_25c Estimated cycles at 25°C
 * @param operating_temp_c Operating temperature (°C)
 * @param activation_energy_ev Activation energy (eV), ~0.8 for Cu fatigue
 * @return Temperature-derated cycle life
 *
 * Complexity: O(1)
 */
double flex_cycles_temperature_derate(double cycles_at_25c,
                                       double operating_temp_c,
                                       double activation_energy_ev);

/**
 * @brief Run a complete bend analysis on a flex design.
 *
 * Combines all bend mechanics calculations into one comprehensive analysis.
 * This is the main entry point for bend evaluation.
 *
 * @param params Input bend parameters
 * @param result [out] Complete bend analysis results
 * @return 0 on success, -1 on invalid input
 *
 * Complexity: O(n_layers) due to beam theory calculation
 */
int flex_bend_analyze(const flex_bend_params_t *params,
                       flex_bend_result_t *result);

/**
 * @brief Calculate springback after bending.
 *
 * Springback angle Δθ = θ * (Mp * R) / (E * I)
 *
 * where Mp = plastic moment, R = bend radius, EI = flexural rigidity.
 * Simplified for elastic springback: Δθ = θ * (3*σy * R) / (E * t)
 *
 * @param bend_angle_deg Nominal bend angle
 * @param bend_radius_mm Bend radius
 * @param thickness_mm Material thickness
 * @param yield_strength_mpa Yield strength of the composite
 * @param elastic_modulus_mpa Effective elastic modulus
 * @return Springback angle in degrees
 *
 * Complexity: O(1)
 */
double flex_springback_angle(double bend_angle_deg,
                              double bend_radius_mm,
                              double thickness_mm,
                              double yield_strength_mpa,
                              double elastic_modulus_mpa);

/**
 * @brief Determine the optimal copper grain orientation for a given bend.
 *
 * For RA copper: bending perpendicular to grain direction reduces
 * stress by ~15% and extends fatigue life by ~2x.
 *
 * @param bend_direction_deg Direction of bend relative to panel (degrees)
 * @return Recommended grain orientation for the copper foil
 */
flex_bend_grain_orientation_t flex_optimal_grain_orientation(
    double bend_direction_deg);

#ifdef __cplusplus
}
#endif

#endif /* FLEX_BEND_H */
