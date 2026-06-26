/**
 * @file flex_rigid_transition.h
 * @brief Rigid-to-Flex Transition Zone Design and Analysis
 *
 * The transition from rigid PCB (FR-4) to flexible circuit (polyimide)
 * is the most mechanically and electrically critical region in rigid-flex
 * design. This module implements the analysis and design rules for
 * robust transition zones.
 *
 * L1 (Definitions): Anchor tab, tear-stop, transition zone, bookbinder,
 *                    no-flow prepreg, selective flex
 * L2 (Core Concepts): Stress concentration at transition, CTE gradient,
 *                     impedance continuity across transition
 * L4 (Fundamental Laws): IPC-2223 §8 transition design rules,
 *                         fracture mechanics at material interfaces
 * L5 (Algorithms): Transition zone stress analysis, anchor tab sizing,
 *                   bookbinder construction optimization
 *
 * @module mini-flex-rigid-flex-design
 */

#ifndef FLEX_RIGID_TRANSITION_H
#define FLEX_RIGID_TRANSITION_H

#include "flex_material.h"
#include "flex_stackup.h"
#include "flex_bend.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * L1 — Transition Zone Structures
 * -------------------------------------------------------------------------*/

/** Transition zone construction type */
typedef enum {
    FLEX_TRANS_SELECTIVE_FLEX = 0,  /**< Selective removal of rigid layers */
    FLEX_TRANS_BOOKBINDER,          /**< Bookbinder construction (flex embedded in rigid) */
    FLEX_TRANS_CAPPED,              /**< Capped flex (rigid caps on flex ends) */
    FLEX_TRANS_SYMMETRIC,           /**< Symmetric construction — balanced CTE */
    FLEX_TRANS_ASYMMETRIC,          /**< Asymmetric — requires careful analysis */
    FLEX_TRANS_COUNT
} flex_transition_type_t;

/**
 * @brief Transition zone design parameters.
 */
typedef struct {
    flex_transition_type_t trans_type;
    double transition_length_mm;       /**< Length of transition zone */
    double rigid_thickness_mm;         /**< Thickness of rigid section at transition */
    double flex_thickness_mm;          /**< Thickness of flex section at transition */
    double rigid_cte_xy_ppm;           /**< Rigid section CTE in X-Y (ppm/°C) */
    double flex_cte_xy_ppm;            /**< Flex section CTE in X-Y (ppm/°C) */
    double rigid_modulus_mpa;          /**< Rigid section effective modulus (MPa) */
    double flex_modulus_mpa;           /**< Flex section effective modulus (MPa) */

    /* Anchor tab parameters */
    int has_anchor_tab;                /**< 1 if anchor tabs present */
    double anchor_tab_length_mm;       /**< Anchor tab extension length */
    double anchor_tab_width_mm;        /**< Anchor tab width */
    int anchor_tab_count;              /**< Number of anchor tabs */

    /* Tear-stop parameters */
    int has_tear_stop;                 /**< 1 if tear-stop features present */
    double tear_stop_diameter_mm;      /**< Tear-stop hole/via diameter */
    double tear_stop_spacing_mm;       /**< Spacing between tear-stops */

    /* Plating parameters */
    double plating_thickness_um;       /**< Copper plating thickness at transition */
    int has_plated_slots;              /**< 1 if plated slots at transition */

    /* Impedance continuity */
    double rigid_z0_ohm;               /**< Target impedance in rigid section */
    double flex_z0_ohm;                /**< Target impedance in flex section */
    int has_impedance_taper;           /**< 1 if impedance tapering is used */

    /* Temperature loading */
    double temp_range_min_c;           /**< Minimum operating temperature */
    double temp_range_max_c;           /**< Maximum operating temperature */
    double lamination_temp_c;          /**< Lamination/cure temperature */
    int expected_thermal_cycles;       /**< Expected thermal cycles over life */
} flex_transition_params_t;

/**
 * @brief Transition zone analysis results.
 */
typedef struct {
    double max_shear_stress_mpa;       /**< Maximum interfacial shear stress */
    double max_peel_stress_mpa;        /**< Maximum peel/normal stress */
    double stress_concentration_factor;/**< SCF at transition corner */
    double min_transition_length_mm;   /**< Required transition length */
    double anchor_tab_strength_n;      /**< Anchor tab pull-out strength */
    double impedance_discontinuity_ohm;/**< Z0 difference across transition */
    double reflection_coefficient;     /**< Γ at transition (SI impact) */
    double estimated_thermal_life;     /**< Predicted thermal cycles to failure */
    int transition_rating;             /**< 0-5 rating (5 = robust) */
    char recommendation[256];          /**< Design improvement recommendation */
} flex_transition_result_t;

/* ---------------------------------------------------------------------------
 * L4 — Transition Zone Stress Analysis (Fundamental Laws)
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate the interfacial shear stress at the rigid-flex boundary
 *        due to CTE mismatch.
 *
 * τ_max = (Δα * ΔT * E) / (2 * sinh(β*L/2) / (β*L))
 *
 * where β = √(G_a / (E*t*h_a)), the shear lag parameter.
 * This is the fundamental equation governing transition zone reliability.
 *
 * @param delta_cte_ppm CTE difference between rigid and flex materials
 * @param delta_temp_c Temperature change from stress-free state
 * @param effective_modulus_mpa Effective in-plane modulus
 * @param transition_length_mm Transition zone length
 * @param adhesive_shear_modulus_mpa Bonding adhesive shear modulus
 * @param layer_thickness_mm Layer thickness
 * @param adhesive_thickness_mm Adhesive bond line thickness
 * @return Maximum interfacial shear stress in MPa
 *
 * Reference: Suhir, "Stresses in Bi-Metal Thermostats", ASME JAM, 1986
 *            IPC-2223 §8.3 Transition Zone Design
 * Complexity: O(1)
 */
double flex_transition_shear_stress(double delta_cte_ppm,
                                     double delta_temp_c,
                                     double effective_modulus_mpa,
                                     double transition_length_mm,
                                     double adhesive_shear_modulus_mpa,
                                     double layer_thickness_mm,
                                     double adhesive_thickness_mm);

/**
 * @brief Calculate the peel (normal) stress at the rigid-flex interface.
 *
 * Peel stress σ_z peaks near the free edge and is the primary cause
 * of delamination at the transition boundary.
 *
 * σ_peel(x) = σ_0 * exp(-λ*x) * cos(λ*x)
 *
 * where λ depends on material properties and geometry.
 *
 * @param shear_stress_mpa Interfacial shear stress
 * @param transition_length_mm Transition zone length
 * @param layer_thickness_mm Layer thickness
 * @return Maximum peel stress in MPa
 *
 * Complexity: O(1)
 */
double flex_transition_peel_stress(double shear_stress_mpa,
                                    double transition_length_mm,
                                    double layer_thickness_mm);

/**
 * @brief Calculate the stress concentration factor at the rigid-flex
 *        transition corner.
 *
 * K_t = σ_max / σ_nominal
 *
 * For a sharp corner: K_t ≈ 2-5 (depending on geometry)
 * With radiused transition: K_t ≈ 1.5-2
 *
 * @param corner_radius_mm Radius at the transition corner
 * @param rigid_thickness_mm Rigid section thickness
 * @param flex_thickness_mm Flex section thickness
 * @return Stress concentration factor K_t (dimensionless, ≥ 1)
 *
 * Reference: Peterson's Stress Concentration Factors, 4th Ed.
 * Complexity: O(1)
 */
double flex_transition_stress_concentration(double corner_radius_mm,
                                              double rigid_thickness_mm,
                                              double flex_thickness_mm);

/* ---------------------------------------------------------------------------
 * L5 — Anchor Tab Design
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate required anchor tab length for a given transition.
 *
 * L_anchor = F_pull / (τ_adhesive * w_tab * n_tabs)
 *
 * where F_pull is the total CTE mismatch force.
 *
 * @param delta_cte_ppm CTE mismatch (ppm/°C)
 * @param delta_temp_c Temperature change (°C)
 * @param rigid_modulus_mpa Rigid section modulus
 * @param rigid_area_mm2 Rigid section cross-sectional area
 * @param tab_width_mm Width of each anchor tab
 * @param tab_count Number of anchor tabs
 * @param adhesive_shear_strength_mpa Adhesive shear strength
 * @return Required anchor tab length in mm
 *
 * Complexity: O(1)
 */
double flex_anchor_tab_length(double delta_cte_ppm,
                               double delta_temp_c,
                               double rigid_modulus_mpa,
                               double rigid_area_mm2,
                               double tab_width_mm,
                               int tab_count,
                               double adhesive_shear_strength_mpa);

/**
 * @brief Calculate anchor tab pull-out strength.
 *
 * F_pullout = τ_bond * w_tab * L_tab * n_tabs
 *
 * @param tab_length_mm Anchor tab length
 * @param tab_width_mm Anchor tab width
 * @param tab_count Number of tabs
 * @param bond_shear_strength_mpa Adhesive shear strength
 * @return Pull-out force in Newtons
 */
double flex_anchor_tab_strength(double tab_length_mm,
                                 double tab_width_mm,
                                 int tab_count,
                                 double bond_shear_strength_mpa);

/* ---------------------------------------------------------------------------
 * L5 — Tear-Stop Design
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate optimal tear-stop spacing along the transition boundary.
 *
 * Tear-stops are drilled/etched holes that prevent crack propagation
 * from the transition zone into the flex circuit.
 *
 * s_optimal = √(π * K_IC² / (2 * σ²))
 *
 * where K_IC = fracture toughness of the dielectric.
 *
 * @param operational_stress_mpa Expected stress at transition
 * @param fracture_toughness_mpa_sqrtm Fracture toughness of PI (≈ 3-5 MPa√m)
 * @return Optimal tear-stop spacing in mm
 *
 * Reference: Linear Elastic Fracture Mechanics applied to flex circuits
 * Complexity: O(1)
 */
double flex_tear_stop_spacing(double operational_stress_mpa,
                               double fracture_toughness_mpa_sqrtm);

/**
 * @brief Verify tear-stop is sized to arrest a propagating crack.
 *
 * The tear-stop diameter must exceed the critical crack length:
 * d_tearstop > 2 * a_critical
 * where a_critical = (K_IC / (σ * √π))²
 *
 * @param tear_stop_diameter_mm Diameter
 * @param stress_mpa Peak stress at transition
 * @param fracture_toughness_mpa_sqrtm Material K_IC
 * @return 1 if adequate, 0 if undersized
 */
int flex_tear_stop_is_adequate(double tear_stop_diameter_mm,
                                double stress_mpa,
                                double fracture_toughness_mpa_sqrtm);

/* ---------------------------------------------------------------------------
 * L5 — Impedance Continuity Across Transition
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate impedance discontinuity between rigid and flex sections.
 *
 * ΔZ0 = |Z0_rigid - Z0_flex|
 *
 * Reflection coefficient Γ = (Z0_rigid - Z0_flex) / (Z0_rigid + Z0_flex)
 *
 * @param rigid_z0_ohm Impedance in rigid section
 * @param flex_z0_ohm Impedance in flex section
 * @param reflection_coeff [out] Reflection coefficient Γ
 * @return Impedance difference ΔZ0 in Ω
 */
double flex_transition_impedance_delta(double rigid_z0_ohm,
                                        double flex_z0_ohm,
                                        double *reflection_coeff);

/**
 * @brief Design an impedance taper to smoothly transition between
 *        rigid and flex impedances.
 *
 * A tapered transmission line reduces reflections compared to an
 * abrupt impedance change. The optimal taper follows a Klopfenstein
 * or exponential profile.
 *
 * For an exponential taper:
 *   Z0(x) = Z0_rigid * exp((x/L) * ln(Z0_flex / Z0_rigid))
 *
 * @param rigid_z0_ohm Starting impedance
 * @param flex_z0_ohm Target impedance
 * @param taper_length_mm Length of taper
 * @param num_points Number of sample points
 * @param impedance_profile [out] Array of impedance values along taper
 *
 * Complexity: O(num_points)
 */
void flex_impedance_taper_exponential(double rigid_z0_ohm,
                                       double flex_z0_ohm,
                                       double taper_length_mm,
                                       int num_points,
                                       double *impedance_profile);

/* ---------------------------------------------------------------------------
 * L5 — Complete Transition Analysis
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate thermal cycle life prediction for the transition zone.
 *
 * Uses the Engelmaier-Wild model for solder joint / adhesive fatigue:
 *   N_f = 0.5 * (Δγ / 2ε_f)^(1/c)
 *
 * where Δγ = shear strain range, ε_f = fatigue ductility, c = exponent.
 *
 * @param shear_strain_per_cycle Shear strain per thermal cycle
 * @param fatigue_ductility Material fatigue ductility
 * @param fatigue_exponent Fatigue exponent (typically -0.5 to -0.7)
 * @return Predicted cycles to failure
 */
double flex_transition_thermal_life(double shear_strain_per_cycle,
                                     double fatigue_ductility,
                                     double fatigue_exponent);

/**
 * @brief Run complete transition zone analysis.
 *
 * @param params Transition zone parameters
 * @param result [out] Analysis results
 * @return 0 on success, -1 on invalid input
 */
int flex_transition_analyze(const flex_transition_params_t *params,
                             flex_transition_result_t *result);

/**
 * @brief Rate the transition design robustness on a 0-5 scale.
 *
 * 0 = guaranteed failure, 5 = aerospace-grade robustness.
 *
 * @param result Transition analysis results
 * @return Rating 0-5
 */
int flex_transition_rating(const flex_transition_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* FLEX_RIGID_TRANSITION_H */
