/**
 * @file flex_design_rule.h
 * @brief IPC-2223 / IPC-6013 Design Rules for Flex and Rigid-Flex PCBs
 *
 * This module encodes IPC design standards as programmatic design rule
 * checks (DRC). Each function implements a specific design rule from
 * IPC-2223 (Flex Design) or IPC-6013 (Flex Performance).
 *
 * L1 (Definitions): Annular ring, coverlay opening, stiffener clearance,
 *                    tear-drop, anchor tab, strain relief
 * L3 (Math Structures): Rule checking as predicate logic over geometry
 * L4 (Fundamental Laws): IPC-2223 design rules (mandatory requirements)
 * L6 (Canonical Problems): DRC violations detection and reporting
 *
 * @module mini-flex-rigid-flex-design
 */

#ifndef FLEX_DESIGN_RULE_H
#define FLEX_DESIGN_RULE_H

#include "flex_material.h"
#include "flex_stackup.h"
#include "flex_bend.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * L1 — Design Rule Violation Structure
 * -------------------------------------------------------------------------*/

/** IPC rule severity */
typedef enum {
    FLEX_RULE_INFO = 0,       /**< Informational — best practice recommendation */
    FLEX_RULE_WARNING,        /**< Warning — may cause reliability issues */
    FLEX_RULE_ERROR,          /**< Error — violates IPC standard */
    FLEX_RULE_CRITICAL        /**< Critical — will cause immediate failure */
} flex_rule_severity_t;

/** IPC standard reference */
typedef enum {
    FLEX_STD_IPC2223 = 0,     /**< IPC-2223 — Flex Design Standard */
    FLEX_STD_IPC6013,         /**< IPC-6013 — Flex Performance Standard */
    FLEX_STD_IPC4202,         /**< IPC-4202 — Flex Base Materials */
    FLEX_STD_IPC4203,         /**< IPC-4203 — Adhesive Coated Films */
    FLEX_STD_IPC4204,         /**< IPC-4204 — Metal-Clad Dielectrics */
    FLEX_STD_IPC4562,         /**< IPC-4562 — Metal Foil */
    FLEX_STD_IPC2152,         /**< IPC-2152 — Current Carrying Capacity */
    FLEX_STD_COUNT
} flex_rule_standard_t;

/**
 * @brief Single design rule violation.
 */
typedef struct {
    int rule_id;                       /**< Unique rule identifier */
    flex_rule_severity_t severity;     /**< Violation severity */
    flex_rule_standard_t standard;     /**< IPC standard reference */
    char rule_name[64];                /**< Short rule name */
    char description[256];             /**< Human-readable description */
    double measured_value;             /**< Actual value found */
    double required_min;               /**< Minimum required value */
    double required_max;               /**< Maximum allowed value */
    char location[64];                 /**< Where the violation occurred */
} flex_drc_violation_t;

#define FLEX_MAX_VIOLATIONS 64

/**
 * @brief Complete DRC report.
 */
typedef struct {
    int total_violations;
    int error_count;
    int warning_count;
    int critical_count;
    flex_drc_violation_t violations[FLEX_MAX_VIOLATIONS];
} flex_drc_report_t;

/* ---------------------------------------------------------------------------
 * Geometry Design Rule Structures (IPC-2223)
 * -------------------------------------------------------------------------*/

/** Pad and via geometry parameters */
typedef struct {
    double pad_diameter_mm;            /**< Pad outer diameter */
    double hole_diameter_mm;           /**< Finished hole diameter */
    double annular_ring_mm;            /**< Annular ring width = (pad - hole) / 2 */
    int is_supported;                  /**< 1 = supported by stiffener, 0 = unsupported */
    int is_in_bend_zone;               /**< 1 = via in bend zone (not recommended) */
    double distance_to_bend_mm;        /**< Distance from via center to nearest bend */
    flex_section_type_t section;       /**< Rigid, flex, or transition */
} flex_via_params_t;

/** Trace geometry parameters */
typedef struct {
    double trace_width_mm;             /**< Nominal trace width */
    double trace_spacing_mm;           /**< Edge-to-edge spacing */
    double trace_to_edge_mm;           /**< Distance to board edge */
    double trace_to_stiffener_mm;      /**< Distance to nearest stiffener */
    double trace_to_bend_mm;          /**< Distance to nearest bend zone */
    int is_in_bend_zone;               /**< 1 if trace passes through bend zone */
    int has_strain_relief;             /**< 1 if strain relief features present */
    double copper_thickness_um;        /**< Copper thickness */
    flex_section_type_t section;       /**< Section type */
} flex_trace_params_t;

/** Coverlay geometry parameters */
typedef struct {
    double coverlay_opening_mm;        /**< Coverlay opening diameter/dimension */
    double pad_diameter_mm;            /**< Pad diameter the coverlay exposes */
    double coverlay_to_pad_mm;         /**< Distance from coverlay edge to pad edge */
    double coverlay_to_edge_mm;        /**< Distance from coverlay edge to board edge */
    double coverlay_thickness_um;      /**< Coverlay thickness */
    flex_cover_type_t cover_type;      /**< Coverlay type */
} flex_coverlay_params_t;

/** Stiffener geometry parameters */
typedef struct {
    double stiffener_thickness_mm;     /**< Stiffener thickness */
    double stiffener_to_edge_mm;       /**< Distance to board edge */
    double stiffener_to_bend_mm;       /**< Distance to bend zone */
    double stiffener_to_via_mm;        /**< Distance to nearest via */
    double adhesive_squeeze_out_mm;    /**< Expected adhesive squeeze-out */
    flex_stiffener_type_t type;        /**< Stiffener material */
} flex_stiffener_params_t;

/* ---------------------------------------------------------------------------
 * L4 — IPC-2223 Design Rule Checks (Fundamental Laws)
 * -------------------------------------------------------------------------*/

/**
 * @brief Check minimum bend radius compliance per IPC-2223 §5.2.4.
 *
 * @param actual_radius_mm As-designed bend radius
 * @param total_thickness_mm Total flex thickness
 * @param num_layers Number of layers in bend zone
 * @param violation [out] Violation record if rule fails
 * @return 1 if compliant, 0 if violation
 */
int flex_drc_bend_radius(double actual_radius_mm,
                          double total_thickness_mm,
                          int num_layers,
                          flex_drc_violation_t *violation);

/**
 * @brief Check annular ring requirement per IPC-2223 §9.1.
 *
 * For flex: minimum annular ring = 0.15 mm (supported) or 0.25 mm
 * (unsupported). Rigid sections follow standard IPC-2221.
 *
 * @param params Via parameters
 * @param violation [out] Violation record
 * @return 1 if compliant, 0 if violation
 */
int flex_drc_annular_ring(const flex_via_params_t *params,
                           flex_drc_violation_t *violation);

/**
 * @brief Check that no vias are placed in bend zones (IPC-2223 §5.2.6).
 *
 * Vias in bend zones concentrate stress and are a primary failure mode.
 * Minimum distance from via to bend = 2× bend radius.
 *
 * @param params Via parameters
 * @param bend_radius_mm Nearest bend radius
 * @param violation [out] Violation record
 * @return 1 if compliant, 0 if violation
 */
int flex_drc_via_in_bend_zone(const flex_via_params_t *params,
                               double bend_radius_mm,
                               flex_drc_violation_t *violation);

/**
 * @brief Check trace width in bend zones (IPC-2223 §5.3).
 *
 * Traces in bend zones should be narrower and have rounded corners
 * to minimize stress concentration. Also verifies strain relief.
 *
 * @param params Trace parameters
 * @param violation [out] Violation record
 * @return 1 if compliant, 0 if violation
 */
int flex_drc_trace_in_bend_zone(const flex_trace_params_t *params,
                                 flex_drc_violation_t *violation);

/**
 * @brief Check minimum trace-to-edge clearance (IPC-2223 §5.4).
 *
 * Outer edge clearance: ≥ 0.5 mm for flex, ≥ 0.25 mm for rigid.
 * This prevents copper exposure and delamination during cutting.
 *
 * @param trace_to_edge_mm Distance from trace to nearest board edge
 * @param section Rigid or flex section
 * @param violation [out] Violation record
 * @return 1 if compliant, 0 if violation
 */
int flex_drc_trace_to_edge(double trace_to_edge_mm,
                            flex_section_type_t section,
                            flex_drc_violation_t *violation);

/**
 * @brief Check coverlay opening to pad clearance (IPC-2223 §6.2).
 *
 * Coverlay opening must be larger than the pad to ensure reliable
 * soldering. Minimum clearance depends on coverlay type.
 *
 * @param params Coverlay parameters
 * @param violation [out] Violation record
 * @return 1 if compliant, 0 if violation
 */
int flex_drc_coverlay_clearance(const flex_coverlay_params_t *params,
                                 flex_drc_violation_t *violation);

/**
 * @brief Check stiffener placement rules (IPC-2223 §7.1).
 *
 * Stiffeners must maintain clearance from bend zones and board edges.
 * Minimum adhesive squeeze-out allowance must be considered.
 *
 * @param params Stiffener parameters
 * @param violation [out] Violation record
 * @return 1 if compliant, 0 if violation
 */
int flex_drc_stiffener_placement(const flex_stiffener_params_t *params,
                                  flex_drc_violation_t *violation);

/**
 * @brief Check minimum trace width and spacing per IPC-2223.
 *
 * Flex typically requires wider minimum trace/space than rigid:
 *   Class 2: 100 μm (4 mil)
 *   Class 3: 125 μm (5 mil)
 *
 * @param trace_width_mm Trace width
 * @param trace_spacing_mm Trace spacing
 * @param ipc_class 2 or 3
 * @param violation [out] Violation record
 * @return 1 if compliant, 0 if violation
 */
int flex_drc_trace_width_spacing(double trace_width_mm,
                                  double trace_spacing_mm,
                                  int ipc_class,
                                  flex_drc_violation_t *violation);

/**
 * @brief Check copper thickness limits per layer in flex section.
 *
 * Maximum copper weight per layer in flex: 70 μm (2 oz).
 * Heavier copper reduces flexibility and increases minimum bend radius.
 *
 * @param copper_thickness_um Copper thickness per layer
 * @param is_in_flex 1 if layer is in flex section
 * @param violation [out] Violation record
 * @return 1 if compliant, 0 if violation
 */
int flex_drc_copper_thickness(double copper_thickness_um,
                               int is_in_flex,
                               flex_drc_violation_t *violation);

/**
 * @brief Check adhesive thickness in flex construction.
 *
 * Adhesive thickness limits per IPC-4203. Too thick = excess flow and
 * reduced flexibility. Too thin = insufficient bonding.
 *
 * @param adhesive_thickness_um Actual adhesive thickness
 * @param adhesive_type Adhesive material type
 * @param violation [out] Violation record
 * @return 1 if compliant, 0 if violation
 */
int flex_drc_adhesive_thickness(double adhesive_thickness_um,
                                 flex_adhesive_type_t adhesive_type,
                                 flex_drc_violation_t *violation);

/* ---------------------------------------------------------------------------
 * L5 — Rigid-Flex Transition Zone Rules (Critical IPC rules)
 * -------------------------------------------------------------------------*/

/**
 * @brief Check rigid-to-flex transition zone design per IPC-2223 §8.
 *
 * The transition zone is the most critical region in rigid-flex.
 * Key rules:
 * - Minimum transition length: 1.5 mm
 * - Anchor tabs required for ≥ 4 layers
 * - No vias within 1 mm of transition boundary
 * - Tear-stop features needed for ≥ 6 layers
 *
 * @param transition_length_mm Length of transition zone
 * @param rigid_layers Number of rigid layers
 * @param flex_layers Number of flex layers
 * @param has_anchor_tab 1 if anchor tabs present
 * @param has_tear_stop 1 if tear-stop present
 * @param violation [out] Violation record
 * @return 1 if compliant, 0 if violation
 */
int flex_drc_transition_zone(double transition_length_mm,
                              int rigid_layers,
                              int flex_layers,
                              int has_anchor_tab,
                              int has_tear_stop,
                              flex_drc_violation_t *violation);

/**
 * @brief Check layer count imbalance in rigid-flex sections.
 *
 * Total rigid layers should be no more than 3× the flex layers
 * to prevent excessive stress at the transition.
 *
 * @param rigid_layer_count Number of layers in rigid section
 * @param flex_layer_count Number of layers in flex section
 * @param violation [out] Violation record
 * @return 1 if compliant, 0 if violation
 */
int flex_drc_layer_imbalance(int rigid_layer_count,
                              int flex_layer_count,
                              flex_drc_violation_t *violation);

/* ---------------------------------------------------------------------------
 * L5 — Comprehensive DRC Runner
 * -------------------------------------------------------------------------*/

/**
 * @brief Initialize an empty DRC report.
 *
 * @return Zero-initialized report
 */
flex_drc_report_t flex_drc_report_init(void);

/**
 * @brief Add a violation to the DRC report.
 *
 * @param report Report to add to
 * @param violation Violation to add
 * @return 0 on success, -1 if report full
 */
int flex_drc_add_violation(flex_drc_report_t *report,
                            const flex_drc_violation_t *violation);

/**
 * @brief Run comprehensive DRC on a flex stackup and geometry.
 *
 * Performs all applicable IPC-2223 design rule checks.
 *
 * @param stackup The flex/rigid-flex stackup
 * @return DRC report with all violations found
 */
flex_drc_report_t flex_drc_run_full(const flex_stackup_t *stackup);

/**
 * @brief Print a human-readable DRC report to stdout.
 *
 * @param report The DRC report to print
 */
void flex_drc_report_print(const flex_drc_report_t *report);

#ifdef __cplusplus
}
#endif

#endif /* FLEX_DESIGN_RULE_H */
