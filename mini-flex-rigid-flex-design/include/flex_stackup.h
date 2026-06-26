/**
 * @file flex_stackup.h
 * @brief Flex/Rigid-Flex PCB Layer Stackup Design Data Structures
 *
 * This module defines the data structures and algorithms for designing
 * multi-layer flex and rigid-flex printed circuit board stackups.
 *
 * L1 (Definitions): Layer types, stackup geometry, material assignment
 * L2 (Core Concepts): Symmetric vs asymmetric stackup, bend zone isolation,
 *                     shielding layers, reference plane design
 * L3 (Math Structures): Thickness accumulation, symmetry verification,
 *                       impedance budgeting
 * L4 (Fundamental Laws): IPC-2223 layer stack rules, neutral axis theorem
 *
 * @module mini-flex-rigid-flex-design
 */

#ifndef FLEX_STACKUP_H
#define FLEX_STACKUP_H

#include "flex_material.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * L1 — Layer Definition Structures
 * -------------------------------------------------------------------------*/

/**
 * @brief A single layer in the flex/rigid-flex stackup.
 *
 * Each layer has a material, thickness, copper weight, and
 * geometric constraints that depend on what section of the board it belongs to.
 */
typedef struct {
    int layer_index;                   /**< 1-based layer number (L1=top) */
    flex_layer_type_t layer_type;      /**< Signal, plane, mixed, or adhesive */
    flex_dielectric_type_t dielectric; /**< Dielectric material */
    flex_copper_type_t copper;         /**< Copper foil type */
    flex_adhesive_type_t adhesive;     /**< Adhesive system */
    double copper_thickness_um;        /**< Copper thickness (μm) */
    double dielectric_thickness_um;    /**< Dielectric core thickness (μm) */
    double adhesive_thickness_um;      /**< Adhesive thickness (μm), 0 for adhesiveless */
    double finished_thickness_um;      /**< Total layer thickness including plating */
    flex_section_type_t section;       /**< Which board section this layer belongs to */
    int is_coverlay_present;           /**< 1 if coverlay on this layer */
    flex_cover_type_t cover_type;      /**< Coverlay/covercoat type */
    double cover_thickness_um;         /**< Coverlay thickness */
    int is_shield_layer;               /**< 1 if EMI shielding layer (Ag paste or sputtered) */
    double shield_thickness_um;        /**< Shield coating thickness */
    char layer_name[32];               /**< Human-readable name e.g., "L1_SIG_TOP" */
} flex_layer_t;

/* ---------------------------------------------------------------------------
 * L1 — Complete Stackup Data Structure
 * -------------------------------------------------------------------------*/

/**
 * @brief Flex zone geometry: defines a region where bending occurs.
 */
typedef struct {
    double start_x_mm;                 /**< X coordinate of bend zone start */
    double start_y_mm;                 /**< Y coordinate of bend zone start */
    double end_x_mm;                   /**< X coordinate of bend zone end */
    double end_y_mm;                   /**< Y coordinate of bend zone end */
    double bend_angle_deg;             /**< Bend angle in degrees */
    double bend_radius_mm;             /**< Bend radius (centerline) */
    int is_dynamic_flex;               /**< 1 = dynamic (repeated bending), 0 = static */
    double expected_cycles;            /**< Expected bend cycles over product life */
    int layer_count_in_zone;           /**< Number of layers in this bend zone */
    int layer_indices[8];              /**< Which layers continue through bend */
} flex_bend_zone_t;

/**
 * @brief Rigid-flex transition zone geometry.
 */
typedef struct {
    double location_x_mm;              /**< X coordinate of transition boundary */
    double location_y_mm;              /**< Y coordinate of transition boundary */
    double transition_length_mm;       /**< Length of transition zone (typ. 1-3 mm) */
    double anchor_tab_length_mm;       /**< Anchor tab extension */
    int has_tear_stop;                 /**< 1 if tear-stop feature present */
    double tear_stop_diameter_mm;      /**< Tear-stop via/hole diameter */
    int rigid_layer_count;             /**< Number of layers in rigid section */
    int flex_layer_count;              /**< Number of layers in flex section */
    int rigid_layer_indices[16];       /**< Layer indices in rigid section */
    int flex_layer_indices[8];         /**< Layer indices in flex section */
} flex_transition_zone_t;

/**
 * @brief Complete rigid-flex PCB stackup definition.
 *
 * A rigid-flex design has at least one rigid section, at least one flex
 * section, and transition zones between them. The overall stackup is the
 * union of all layers, with each layer assigned to specific sections.
 */
typedef struct {
    char design_name[64];              /**< Design identifier */
    char ipc_class[8];                 /**< IPC-6013 class: "1", "2", "3" */
    int total_layer_count;             /**< Total unique layers across all sections */
    int rigid_section_count;           /**< Number of rigid sections */
    int flex_section_count;            /**< Number of flex sections (bend zones) */
    int transition_zone_count;         /**< Number of rigid-flex transitions */

    flex_layer_t layers[FLEX_MAX_LAYERS];          /**< All layers */
    flex_bend_zone_t bend_zones[FLEX_MAX_BEND_ZONES];    /**< Bend zone definitions */
    flex_transition_zone_t transitions[FLEX_MAX_BEND_ZONES]; /**< Transition zones */
    flex_stiffener_spec_t stiffeners[FLEX_MAX_STIFFENERS];  /**< Stiffener specs */

    double total_thickness_rigid_mm;   /**< Overall rigid section thickness */
    double total_thickness_flex_mm;    /**< Overall flex section thickness */
    double copper_total_thickness_um;  /**< Sum of all copper thicknesses */

    int is_symmetric;                  /**< 1 if stackup is symmetric about mid-plane */
    int imbalance_warning;             /**< 1 if asymmetry exceeds IPC tolerance */
} flex_stackup_t;

/* ---------------------------------------------------------------------------
 * L2 — Stackup Design Operations
 * -------------------------------------------------------------------------*/

/**
 * @brief Initialize an empty stackup structure.
 *
 * @param design_name Identifier for this design
 * @return Zero-initialized stackup
 *
 * Complexity: O(1)
 */
flex_stackup_t flex_stackup_init(const char *design_name);

/**
 * @brief Add a signal layer to the stackup.
 *
 * @param stackup Stackup being built
 * @param layer_name Name for this layer
 * @param copper_thickness_um Copper thickness in μm
 * @param copper_type RA or ED
 * @param dielectric_type Dielectric material
 * @param dielectric_thickness_um Core dielectric thickness
 * @return Layer index (1-based), or -1 on failure (stackup full)
 *
 * Complexity: O(1)
 */
int flex_stackup_add_signal_layer(flex_stackup_t *stackup,
                                   const char *layer_name,
                                   double copper_thickness_um,
                                   flex_copper_type_t copper_type,
                                   flex_dielectric_type_t dielectric_type,
                                   double dielectric_thickness_um);

/**
 * @brief Add a reference plane layer to the stackup.
 *
 * @param stackup Stackup being built
 * @param layer_name Name for this plane
 * @param copper_thickness_um Copper thickness
 * @param copper_type Copper type
 * @return Layer index, or -1 on failure
 *
 * Complexity: O(1)
 */
int flex_stackup_add_plane_layer(flex_stackup_t *stackup,
                                  const char *layer_name,
                                  double copper_thickness_um,
                                  flex_copper_type_t copper_type);

/**
 * @brief Set a layer as belonging to the rigid section only.
 *
 * This removes the layer from flex zones — typical for outer rigid
 * layers that should not bend.
 *
 * @param stackup Stackup to modify
 * @param layer_index 1-based layer index
 * @return 0 on success, -1 on invalid index
 */
int flex_stackup_set_rigid_only(flex_stackup_t *stackup, int layer_index);

/**
 * @brief Set a layer as continuing through flex sections.
 *
 * @param stackup Stackup to modify
 * @param layer_index 1-based layer index
 * @return 0 on success, -1 on invalid index
 */
int flex_stackup_set_flex_through(flex_stackup_t *stackup, int layer_index);

/**
 * @brief Add a bend zone definition to the stackup.
 *
 * @param stackup Stackup being built
 * @param start_x X start of bend zone
 * @param start_y Y start of bend zone
 * @param end_x X end of bend zone
 * @param end_y Y end of bend zone
 * @param bend_radius_mm Centerline bend radius
 * @param bend_angle_deg Bend angle
 * @param dynamic 1 = dynamic flex, 0 = static (one-time bend)
 * @return Bend zone index, or -1 on failure
 */
int flex_stackup_add_bend_zone(flex_stackup_t *stackup,
                                double start_x, double start_y,
                                double end_x, double end_y,
                                double bend_radius_mm, double bend_angle_deg,
                                int dynamic);

/**
 * @brief Add a stiffener to the stackup.
 *
 * @param stackup Stackup being built
 * @param type Stiffener material type
 * @param thickness_mm Stiffener thickness
 * @param bonded 1 = bonded, 0 = mechanical attach
 * @return Stiffener index, or -1 on failure
 */
int flex_stackup_add_stiffener(flex_stackup_t *stackup,
                                flex_stiffener_type_t type,
                                double thickness_mm,
                                int bonded);

/* ---------------------------------------------------------------------------
 * L3 — Mathematical Operations on Stackup
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate the total thickness of the flex section.
 *
 * Sums layer thicknesses for layers marked as flex-through.
 *
 * @param stackup The stackup to analyze
 * @return Total flex section thickness in mm
 *
 * Complexity: O(n) where n = total_layer_count
 */
double flex_stackup_flex_thickness(const flex_stackup_t *stackup);

/**
 * @brief Calculate total copper thickness in flex section.
 *
 * Important for minimum bend radius calculation (IPC-2223: R_min ∝ total Cu).
 *
 * @param stackup The stackup to analyze
 * @return Total copper thickness in μm in flex layers
 */
double flex_stackup_flex_copper_total(const flex_stackup_t *stackup);

/**
 * @brief Calculate the total number of layers in the flex section.
 *
 * @param stackup The stackup
 * @return Count of layers marked as flex-through
 */
int flex_stackup_flex_layer_count(const flex_stackup_t *stackup);

/**
 * @brief Find the neutral bending axis of the flex section.
 *
 * The neutral axis is the plane where strain = 0 during bending.
 * y_neutral = Σ(E_i * t_i * y_i) / Σ(E_i * t_i)
 *
 * where E_i = Young's modulus of layer i, t_i = thickness,
 * y_i = distance from reference plane to center of layer i.
 *
 * @param stackup The stackup
 * @param neutral_offset_mm [out] Distance from bottom surface to neutral axis
 * @return 0 on success, -1 if no flex layers
 *
 * Reference: Timoshenko, "Strength of Materials", Ch.6
 * Complexity: O(n), n = flex layer count
 */
int flex_stackup_neutral_axis(const flex_stackup_t *stackup,
                               double *neutral_offset_mm);

/**
 * @brief Calculate the bending stiffness (flexural rigidity) of the flex section.
 *
 * D = Σ E_i * I_i = Σ E_i * (b * t_i^3 / 12 + b * t_i * d_i^2)
 *
 * where d_i = distance from neutral axis to center of layer i
 *
 * @param stackup The stackup
 * @return Flexural rigidity in N·mm² (per unit width b=1mm)
 */
double flex_stackup_flexural_rigidity(const flex_stackup_t *stackup);

/**
 * @brief Verify stackup symmetry about the mid-plane.
 *
 * Asymmetric stackups cause warpage during thermal cycling.
 * IPC-2223 recommends ≤ 10% asymmetry tolerance.
 *
 * Checks both material and thickness symmetry.
 *
 * @param stackup The stackup to analyze
 * @return 1 if symmetric, 0 if asymmetric
 */
int flex_stackup_verify_symmetry(flex_stackup_t *stackup);

/**
 * @brief Calculate the asymmetry metric (0 = perfect symmetry, 1 = max asymmetry).
 *
 * Uses the weighted thickness-moment method:
 * A = Σ|t_i * (z_i - z_mid)| / Σ(t_i * |z_i - z_mid|)
 *
 * @param stackup The stackup
 * @return Asymmetry metric in [0, 1]
 */
double flex_stackup_asymmetry_metric(const flex_stackup_t *stackup);

/**
 * @brief Estimate warpage from stackup asymmetry and temperature change.
 *
 * w = (ΔCTE * ΔT * L²) / (8 * h)   (simplified bi-material strip model)
 *
 * @param stackup The stackup
 * @param delta_temperature Temperature change from lamination (°C)
 * @param board_length_mm Board length in the direction of interest (mm)
 * @return Estimated warpage in mm
 *
 * Reference: Timoshenko, "Analysis of Bi-Metal Thermostats", JOSA, 1925
 */
double flex_stackup_warpage_estimate(const flex_stackup_t *stackup,
                                      double delta_temperature,
                                      double board_length_mm);

/**
 * @brief Validate the stackup against IPC-2223 construction rules.
 *
 * Checks: layer count limits, thickness limits, coverlay rules,
 * adhesive thickness limits, copper weight per layer, etc.
 *
 * Returns the number of violations (0 = fully compliant).
 *
 * @param stackup The stackup to validate
 * @return Number of IPC-2223 rule violations
 */
int flex_stackup_validate_ipc2223(const flex_stackup_t *stackup);

/**
 * @brief Get a human-readable description of the stackup.
 *
 * @param stackup The stackup
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return Number of characters written (excluding null terminator)
 */
int flex_stackup_describe(const flex_stackup_t *stackup,
                           char *buffer, size_t buffer_size);

/**
 * @brief Compute the estimated manufacturing cost index.
 *
 * Cost model based on: layer count, material type, adhesiveless premium,
 * stiffener count, and bend zone complexity. Normalized to a 2-layer
 * simple flex = 1.0.
 *
 * @param stackup The stackup
 * @return Cost index (relative to baseline 2-layer flex)
 */
double flex_stackup_cost_index(const flex_stackup_t *stackup);

#ifdef __cplusplus
}
#endif

#endif /* FLEX_STACKUP_H */
