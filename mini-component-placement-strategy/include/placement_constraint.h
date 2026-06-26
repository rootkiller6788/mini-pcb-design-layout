/**
 * @file placement_constraint.h
 * @brief Placement constraints and design rule checking (DRC)
 *
 * Implements all physical, electrical, thermal, and manufacturing
 * constraints that govern legal component placement on a PCB.
 *
 * Knowledge Mapping:
 *   L2 (Core Concepts): Spacing rules, keep-out zones, thermal relief,
 *                       signal integrity constraints, manufacturability
 *   L4 (Fundamental Laws): IPC-2221/7351 design rules, thermal resistance
 *                         network, signal integrity placement laws
 *   L6 (Canonical Problems): Analog/digital separation, mixed-signal
 *                            partition, high-current loop minimization
 *
 * Course Alignment:
 *   - Berkeley EE105: Analog IC layout constraints
 *   - Illinois ECE 451: EM compatibility constraints
 *   - Michigan EECS 411: Microwave/RF PCB design rules
 *   - TU Munich: High-frequency engineering layout rules
 *
 * References:
 *   - IPC-2221: Generic Standard on Printed Board Design
 *   - IPC-7351: Generic Requirements for SMD Design and Land Pattern
 *   - IPC-A-610: Acceptability of Electronic Assemblies
 *   - Paul, C.R., "Introduction to Electromagnetic Compatibility", 2006
 */

#ifndef PLACEMENT_CONSTRAINT_H
#define PLACEMENT_CONSTRAINT_H

#include "placement_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * L1 Definitions: Constraint Types
 * ============================================================================ */

/** Constraint type categories */
typedef enum {
    CONSTRAINT_SPACING       = 0,   /* Minimum spacing between components */
    CONSTRAINT_KEEPOUT       = 1,   /* Keep-out zones (mechanical, EMI) */
    CONSTRAINT_THERMAL       = 2,   /* Thermal constraints */
    CONSTRAINT_HIGH_SPEED    = 3,   /* High-speed signal constraints */
    CONSTRAINT_POWER_INTEGRITY = 4, /* Power delivery constraints */
    CONSTRAINT_MECHANICAL    = 5,   /* Mechanical fit constraints */
    CONSTRAINT_MANUFACTURING = 6,   /* DFM constraints */
    CONSTRAINT_HEIGHT        = 7,   /* Component height restrictions */
    CONSTRAINT_TYPE_COUNT    = 8
} ConstraintType;

/** Violation severity */
typedef enum {
    VIOLATION_NONE    = 0,
    VIOLATION_WARNING = 1,
    VIOLATION_ERROR   = 2,
    VIOLATION_FATAL   = 3
} ViolationSeverity;

/** Detailed violation record */
typedef struct {
    ConstraintType    type;
    ViolationSeverity severity;
    uint32_t          comp_id_a;       /* First involved component */
    uint32_t          comp_id_b;       /* Second involved component (0 if N/A) */
    char              description[256];
    double            measured_value;  /* Actual value */
    double            limit_value;     /* Allowed limit */
    double            margin;          /* limit - measured (negative = violation) */
} Violation;

/** Constraint checking result */
typedef struct {
    uint32_t   violation_count;
    Violation* violations;          /* Dynamically allocated */
    bool       all_clear;           /* True if no violations */
    double     worst_margin;        /* Most negative margin */
} ConstraintResult;

/** Keep-out zone definition */
typedef struct {
    Rect2D   region;              /* Keep-out rectangle in board coordinates */
    char     reason[128];         /* Reason for keep-out */
    bool     restrict_top;        /* True if top-side placement blocked */
    bool     restrict_bottom;     /* True if bottom-side placement blocked */
} KeepOutZone;

/** Spacing matrix entry: minimum spacing between component categories */
typedef struct {
    ComponentCategory cat_a;
    ComponentCategory cat_b;
    double            min_spacing_mm;  /* Edge-to-edge minimum distance */
} SpacingRule;

/* ============================================================================
 * L1 Definitions: IPC Standard Spacing Classes
 * ============================================================================ */

/**
 * IPC density levels (IPC-2221 §5.2)
 *
 * Level A: General design (most relaxed, highest yield)
 * Level B: Moderate design (standard)
 * Level C: High-density design (tightest, lowest yield)
 */
typedef enum {
    IPC_LEVEL_A = 0,  /* General (low density, high reliability) */
    IPC_LEVEL_B = 1,  /* Moderate (standard production) */
    IPC_LEVEL_C = 2   /* High (high density, requires advanced mfg) */
} IPCDensityLevel;

/* ============================================================================
 * Constraint API: Spacing Rules
 * ============================================================================ */

/**
 * Get IPC-7351 recommended minimum spacing between two package types.
 *
 * Based on IPC-7351B tables for courtyard spacing.
 *
 * @param pkg_a  First package type
 * @param pkg_b  Second package type
 * @param level  IPC density level
 * @return       Minimum spacing in mm (edge-to-edge)
 */
double placement_constraint_get_ipc_spacing(PackageType pkg_a, PackageType pkg_b,
                                            IPCDensityLevel level);

/**
 * Check minimum spacing between two components.
 *
 * Computes edge-to-edge distance accounting for both components' rotations,
 * and compares against the IPC-recommended minimum spacing.
 *
 * @param comp_a   First component
 * @param comp_b   Second component
 * @param level    IPC density level
 * @param violation Output violation record if spacing violated (can be NULL)
 * @return         True if spacing is sufficient
 */
bool placement_constraint_check_spacing(const Component* comp_a,
                                        const Component* comp_b,
                                        IPCDensityLevel level,
                                        Violation* violation);

/**
 * Check minimum spacing for all component pairs in a placement.
 *
 * Complexity: O(C^2) where C = number of components.
 *
 * @param result  Placement result to check
 * @param level   IPC density level
 * @return        Constraint checking result (caller must free violations array)
 */
ConstraintResult placement_constraint_check_all_spacing(
    const PlacementResult* result, IPCDensityLevel level);

/* ============================================================================
 * Constraint API: Board Boundary
 * ============================================================================ */

/**
 * Check if a component is fully within the board outline.
 *
 * @param comp   Component to check (in board coordinates)
 * @param board  Board definition
 * @return       True if component is fully within board boundaries
 */
bool placement_constraint_check_board_boundary(const Component* comp,
                                                const Board* board);

/* ============================================================================
 * Constraint API: Keep-Out Zones
 * ============================================================================ */

/**
 * Check if a component encroaches on any keep-out zone.
 *
 * @param comp          Component to check
 * @param zones         Array of keep-out zones
 * @param zone_count    Number of zones
 * @return              True if component is clear of all zones
 */
bool placement_constraint_check_keepout(const Component* comp,
                                        const KeepOutZone* zones,
                                        uint32_t zone_count);

/* ============================================================================
 * Constraint API: Thermal Constraints
 * ============================================================================ */

/**
 * Check if component placement allows sufficient heat dissipation.
 *
 * Verifies that the estimated junction temperature of each component,
 * accounting for neighbor heating effects, remains below T_j_max.
 *
 * Thermal model: T_j = T_amb + P * θ_JA + sum_neighbors(P_neighbor * θ_neighbor)
 * where θ_neighbor depends on distance between components.
 *
 * Reference: Erickson & Maksimovic, "Fundamentals of Power Electronics", 2001, Ch. 19.
 *
 * @param result     Placement result
 * @param ambient_C  Ambient temperature in Celsius
 * @param violation  Output violation if thermal limit exceeded (can be NULL)
 * @return           True if all components within thermal limits
 */
bool placement_constraint_check_thermal(const PlacementResult* result,
                                        double ambient_C,
                                        Violation* violation);

/**
 * Compute the thermal coupling coefficient between two components.
 *
 * θ_coupling = k / (2π * d)  for point sources on an infinite plane,
 * scaled by board thermal conductivity.
 *
 * Reference: Carslaw & Jaeger, "Conduction of Heat in Solids", 1959.
 *
 * @param comp_a   First component
 * @param comp_b   Second component
 * @param board_thickness_mm  Board thickness
 * @return         Temperature rise at comp_b per watt dissipated at comp_a (°C/W)
 */
double placement_constraint_thermal_coupling(const Component* comp_a,
                                              const Component* comp_b,
                                              double board_thickness_mm);

/* ============================================================================
 * Constraint API: Signal Integrity
 * ============================================================================ */

/**
 * Check if a critical net's total Manhattan distance exceeds the maximum
 * allowed trace length for signal integrity.
 *
 * For high-speed signals, trace length is limited by:
 *   L_max = t_rise / (2 * t_pd)   (critical length rule)
 * where t_rise = signal rise time, t_pd = propagation delay per unit length.
 *
 * Reference: Johnson & Graham, "High-Speed Digital Design", 1993, Ch. 1.
 *
 * @param result        Placement result
 * @param net_id        Net to check
 * @param max_length_mm Maximum allowed trace length
 * @param violation     Output violation (can be NULL)
 * @return              True if net length is within limits
 */
bool placement_constraint_check_trace_length(const PlacementResult* result,
                                              uint32_t net_id,
                                              double max_length_mm,
                                              Violation* violation);

/**
 * Check differential pair length matching constraint.
 *
 * The two traces of a differential pair must be matched within:
 *   ΔL_max = t_rise * v / (10 * ε_r)  (for <5° phase mismatch)
 *
 * Reference: Bogatin, "Signal and Power Integrity — Simplified", 2009, Ch. 11.
 *
 * @param result         Placement result
 * @param net_id_p       Positive net of differential pair
 * @param net_id_n       Negative net of differential pair
 * @param max_delta_mm   Maximum allowed length difference
 * @param violation      Output violation (can be NULL)
 * @return               True if length matching constraint is satisfied
 */
bool placement_constraint_check_diff_pair(const PlacementResult* result,
                                           uint32_t net_id_p,
                                           uint32_t net_id_n,
                                           double max_delta_mm,
                                           Violation* violation);

/* ============================================================================
 * Constraint API: Manufacturing (DFM)
 * ============================================================================ */

/**
 * Check component orientation for wave soldering compatibility.
 *
 * SMD components on the bottom side must be oriented so that
 * both pads enter the solder wave simultaneously to prevent
 * tombstoning. For SOIC packages, orientation should be
 * perpendicular to wave direction.
 *
 * Reference: IPC-610, "Acceptability of Electronic Assemblies", §8.3.
 *
 * @param comp       Component to check
 * @param wave_dir   Wave direction angle (0 = left-to-right)
 * @param violation  Output violation (can be NULL)
 * @return           True if orientation is compatible
 */
bool placement_constraint_check_wave_solder(const Component* comp,
                                             double wave_dir,
                                             Violation* violation);

/**
 * Check that component does not shadow smaller components during reflow.
 *
 * Larger/taller components up-wave from smaller components can create
 * solder shadowing. Rule: height ratio > 3:1 and spacing < 5mm = violation.
 *
 * @param comp_a      Up-wave component (first to see solder)
 * @param comp_b      Down-wave component
 * @param wave_dir    Wave direction angle
 * @param violation   Output violation (can be NULL)
 * @return            True if no shadowing risk
 */
bool placement_constraint_check_shadowing(const Component* comp_a,
                                           const Component* comp_b,
                                           double wave_dir,
                                           Violation* violation);

/* ============================================================================
 * Constraint API: Mechanical
 * ============================================================================ */

/**
 * Check if component height violates any z-height restrictions.
 *
 * @param comp          Component to check
 * @param max_height_mm Maximum allowed height above board
 * @return              True if component fits within height envelope
 */
bool placement_constraint_check_height(const Component* comp,
                                        double max_height_mm);

/**
 * Check that connectors are placed with adequate clearance for mating.
 *
 * @param comp              Connector component
 * @param clearance_x_mm    Required clearance in X direction beyond body
 * @param clearance_y_mm    Required clearance in Y direction beyond body
 * @param board             Board definition
 * @return                  True if adequate clearance exists
 */
bool placement_constraint_check_connector_clearance(const Component* comp,
                                                     double clearance_x_mm,
                                                     double clearance_y_mm,
                                                     const Board* board);

/* ============================================================================
 * Constraint API: Comprehensive Checking
 * ============================================================================ */

/**
 * Free the violations array in a ConstraintResult.
 *
 * @param result  Result to free
 */
void placement_constraint_result_free(ConstraintResult* result);

/**
 * Run comprehensive constraint checking on a placement.
 *
 * Checks: spacing, board boundary, thermal, signal integrity,
 * manufacturing, and mechanical constraints.
 *
 * @param result        Placement to check
 * @param ipc_level     IPC density level for spacing rules
 * @param keepout_zones Keep-out zones array
 * @param zone_count    Number of keep-out zones
 * @param ambient_C     Ambient temperature for thermal check
 * @return              Constraint checking result
 */
ConstraintResult placement_constraint_check_all(const PlacementResult* result,
                                                 IPCDensityLevel ipc_level,
                                                 const KeepOutZone* keepout_zones,
                                                 uint32_t zone_count,
                                                 double ambient_C);

#ifdef __cplusplus
}
#endif

#endif /* PLACEMENT_CONSTRAINT_H */
