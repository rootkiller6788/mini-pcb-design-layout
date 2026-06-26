/**
 * pcb_thermal_via.h — Thermal Via Design and Optimization
 *
 * L5: Algorithms for thermal via analysis, design, and optimization.
 * L6: Canonical problems — via array sizing, pitch optimization,
 *     filled vs. unfilled trade-offs.
 *
 * Courses: Berkeley EE105 (thermal design), TU Munich High-Frequency Eng.,
 *          ETH Zurich 227-0455 EM, Georgia Tech ECE 6350
 * Reference: Li, "Thermal Via Design for PCB Thermal Management", IEEE Trans (2015)
 *            IPC-4761 "Design Guide for Protection of Printed Board Via Structures"
 *            IPC-2221 "Generic Standard on Printed Board Design"
 *            Guenin, "The Thermal Conductivity of Thermal Vias", Electronics Cooling (2006)
 */

#ifndef PCB_THERMAL_VIA_H
#define PCB_THERMAL_VIA_H

#include "pcb_thermal_defs.h"
#include "pcb_thermal_analysis.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L5: Single Via Thermal Analysis
 * ================================================================== */

/** Calculate the thermal resistance of a single plated via.
 *  L5: A thermal via consists of a copper barrel connecting layers.
 *
 *  R_via = L / (k_Cu * A_cross)
 *  A_cross = pi * (d_outer^2 - d_inner^2) / 4
 *          = pi * (d_drill^2 - (d_drill - 2*t_plate)^2) / 4
 *          = pi * t_plate * (d_drill - t_plate)
 *
 *  For filled vias: A_cross = pi * d_drill^2 / 4 (if fill_k = k_Cu)
 *
 *  Typical via R_theta values (L=1.6mm, d=0.3mm, 25um plating):
 *  ≈ 60-80 °C/W per via (unfilled)
 *  ≈ 15-20 °C/W per via (copper-filled)
 *
 *  @param via_geom      Via geometry
 *  @param via_length_mm Length of via barrel (mm)
 *  @param k_cu          Copper conductivity (W/m-K), typically 385
 *  @return              Thermal resistance of single via (C/W)
 *  Complexity: O(1) */
double thermal_via_single_resistance(const thermal_via_geometry_t *via_geom,
                                      double via_length_mm, double k_cu);

/** Calculate the copper cross-sectional area of a single via.
 *  L5: The copper barrel is an annular ring.
 *
 *  A = pi * t * (d - t)  for unfilled (annular)
 *  A = pi * d^2 / 4       for filled with copper/solder
 *
 *  This area directly determines the via thermal resistance. */
double thermal_via_cross_section_area(const thermal_via_geometry_t *via_geom);

/** Calculate the total thermal resistance of a via array.
 *  L5: Multiple vias in parallel, with array efficiency factor.
 *
 *  R_array = R_single / (N * eta)
 *
 *  The efficiency eta < 1 accounts for:
 *  - Thermal interaction (heat flow constriction as vias get closer)
 *  - Non-uniform heat flux across the array
 *  - Edge effects in finite arrays
 *
 *  For well-designed arrays (pitch > 2*diameter):
 *  eta ≈ 0.85-0.95 for N < 25
 *  eta ≈ 0.7-0.85 for N = 25-100
 *  eta ≈ 0.6-0.75 for N > 100
 *
 *  @param via_geom      Via geometry
 *  @param via_length_mm Via barrel length (mm)
 *  @param k_cu          Copper conductivity (W/m-K)
 *  @return              Total array thermal resistance (C/W)
 *  Complexity: O(1) */
double thermal_via_array_resistance(thermal_via_geometry_t *via_geom,
                                     double via_length_mm, double k_cu);

/** Calculate the thermal via array efficiency factor.
 *  L5: Accounts for mutual thermal coupling between adjacent vias.
 *
 *  eta = 1 / (1 + alpha * (N_vias / A_array) * d_via^2)
 *  where alpha ≈ 0.1-0.3 experimentally for typical PCB vias.
 *
 *  This is a simplified model based on heat conduction interference
 *  between adjacent cylindrical heat paths in a finite medium. */
double thermal_via_efficiency(int num_vias, double pitch_mm,
                               double drill_diameter_mm,
                               double via_length_mm);

/* ==================================================================
 * L5: Via Array Optimization
 * ================================================================== */

/** Determine the minimum number of thermal vias needed for a target resistance.
 *  L5/L6: Given a heat source, PCB stack, and target thermal resistance,
 *  calculate how many vias are required.
 *
 *  Algorithm: Iterative search starting from N=1, computing array resistance.
 *  The search stops when R_array <= R_target or N exceeds max_vias.
 *
 *  @param power_w       Component power dissipation (W)
 *  @param max_delta_t   Maximum allowed temperature rise (K or C)
 *  @param via_length_mm Via barrel length (mm)
 *  @param k_cu          Copper conductivity (W/m-K)
 *  @param drill_mm      Via drill diameter (mm)
 *  @param plate_mm      Plating thickness (mm)
 *  @param pitch_mm      Via pitch (mm)
 *  @param max_vias      Maximum vias allowed (space constraint)
 *  @param num_vias_out  Output: required number of vias
 *  @param r_achieved    Output: achieved thermal resistance (C/W)
 *  @return              THERMAL_OK or error code
 *  Complexity: O(N) where N is the number of vias needed */
int thermal_via_calculate_count(double power_w, double max_delta_t,
                                 double via_length_mm, double k_cu,
                                 double drill_mm, double plate_mm,
                                 double pitch_mm, int max_vias,
                                 int *num_vias_out, double *r_achieved);

/** Optimize via array geometry for a given footprint area.
 *  L5/L6: Given a fixed rectangular area for the via array,
 *  find the optimal drill diameter, pitch, and count that minimizes
 *  thermal resistance.
 *
 *  Constraints:
 *  - Minimum drill diameter: typically 0.2mm (mechanical drill limit)
 *  - Minimum pitch: 2.5 * drill_diameter (to prevent drill breakage)
 *  - Maximum aspect ratio: via_length / drill_diameter <= 10 (plating limit)
 *  - The array must fit within the specified footprint
 *
 *  Optimization: Exhaustive search over feasible (diameter, rows, cols).
 *
 *  @param footprint_width_mm   Available width for via array (mm)
 *  @param footprint_length_mm  Available length for via array (mm)
 *  @param via_length_mm        Via barrel length (mm)
 *  @param k_cu                 Copper conductivity (W/m-K)
 *  @param plate_thickness_mm   Plating thickness (mm)
 *  @param optimized            Output: optimized via geometry
 *  @return                     THERMAL_OK or error code
 *  Complexity: O(K^2 * R*C) where K = diameter candidates, R*C = grid dimensions */
int thermal_via_optimize_array(double footprint_width_mm,
                                double footprint_length_mm,
                                double via_length_mm, double k_cu,
                                double plate_thickness_mm,
                                thermal_via_geometry_t *optimized);

/** Estimate the effective thermal conductivity of a via field region.
 *  L5: When modeling a board with many vias, the via field can be represented
 *  as an anisotropic material with effective conductivity.
 *
 *  k_effective_z = (k_Cu * A_vias_total + k_dielectric * A_remaining) / A_total
 *  This is the rule of mixtures (parallel model) for Z-direction conductivity.
 *
 *  For typical via arrays: k_eff_z can be 10-50x higher than bare FR4. */
double thermal_via_effective_kz(const thermal_via_geometry_t *via_geom,
                                 double k_cu, double k_dielectric);

/** Calculate the thermal resistance improvement from adding thermal vias.
 *  L5: Improvement factor = R_without_vias / R_with_vias
 *
 *  R_without_vias is dominated by the FR4 dielectric (k ≈ 0.3 W/m-K)
 *  R_with_vias is primarily through the copper barrels (k_Cu = 385 W/m-K)
 *  Typical improvement: 50-200x for well-designed via arrays. */
double thermal_via_improvement_factor(const thermal_via_geometry_t *via_geom,
                                       double via_length_mm,
                                       double area_mm2,
                                       double k_cu, double k_dielectric);

/* ==================================================================
 * L5: Filled vs Unfilled Via Analysis
 * ================================================================== */

/** Analyze the cost-benefit of filling thermal vias with conductive material.
 *  L5: Filled vias improve thermal conductivity but add manufacturing cost.
 *
 *  Unfilled via A_cross = pi * t_plate * (d - t_plate) ≈ pi * d * t_plate
 *  Filled via A_cross = pi * d^2 / 4  (full area if fill_k ≈ k_Cu)
 *
 *  Improvement = (pi*d^2/4) / (pi*d*t_plate) = d / (4*t_plate)
 *  For d=0.3mm, t_plate=0.025mm: improvement ≈ 3x
 *  For d=0.5mm, t_plate=0.025mm: improvement ≈ 5x
 *
 *  But manufacturing: filled vias cost 2-5x more and require planarization.
 *
 *  @param drill_diameter_mm Via drill diameter (mm)
 *  @param plating_mm        Plating thickness (mm)
 *  @param fill_k            Fill material conductivity (W/m-K)
 *  @param r_unfilled_out    Output: single via Rθ, unfilled (C/W)
 *  @param r_filled_out      Output: single via Rθ, filled (C/W)
 *  @param improvement       Output: ratio r_unfilled / r_filled
 *  Complexity: O(1) */
void thermal_via_fill_analysis(double drill_diameter_mm, double plating_mm,
                                double fill_k,
                                double *r_unfilled_out, double *r_filled_out,
                                double *improvement);

#ifdef __cplusplus
}
#endif

#endif /* PCB_THERMAL_VIA_H */