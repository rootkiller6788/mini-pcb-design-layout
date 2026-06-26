/**
 * pcb_thermal_material.h — PCB Material Properties Database
 *
 * L1: Comprehensive material property database for PCB thermal analysis.
 *     Provides reliable values for all common PCB and cooling materials
 *     used in thermal calculations.
 *
 * Courses: Berkeley EE105 Analog, TU Munich High-Frequency Eng.,
 *          ETH Zurich 227-0455 EM
 * Reference: IPC-4101 "Specification for Base Materials for Rigid and Multilayer Boards"
 *            Cengel, "Heat and Mass Transfer" (2014), Appendix A
 *            Isola, Rogers, and Panasonic material datasheets
 *            MatWeb Material Property Database
 */

#ifndef PCB_THERMAL_MATERIAL_H
#define PCB_THERMAL_MATERIAL_H

#include "pcb_thermal_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L1: Material Database — 22 canonical materials
 * ================================================================== */

/** Get the full material property record for a given PCB material type.
 *  L1: Returns a pointer to a const material_property_t with all
 *  thermal, mechanical, and electrical properties.
 *
 *  Materials covered:
 *  - PCB substrates: FR4, Polyimide, Rogers 4350B, PTFE, BT-Epoxy, CEM-3
 *  - Metal cores: Aluminum (5052, 6061), Copper
 *  - Ceramics: Alumina (96%, 99.6%), Aluminum Nitride (AlN), Beryllia (BeO)
 *  - IMS: Bergquist HPL, Laird Tgard
 *  - Copper: pure copper, copper alloys
 *  - Air: for gap/clearance modeling
 *
 *  @param type   Material type enum
 *  @return       Material property record, or NULL for invalid type
 *  Complexity: O(1) */
const material_property_t *pcb_thermal_material_get(pcb_material_type_t type);

/** Get the material property for a specific named material.
 *  L1: String lookup for materials not in the standard enum.
 *  Supports names like "FR4", "Al2O3", "AlN", "Copper", "Rogers 4350B",
 *  "Aluminum 6061", "SAC305" (solder), "Thermal Grease", etc.
 *
 *  @param name   Material name string (case-sensitive)
 *  @return       Material property record, or NULL if not found
 *  Complexity: O(N_materials) linear search */
const material_property_t *pcb_thermal_material_by_name(const char *name);

/** Get the thermal conductivity for a material in XY plane.
 *  L1: Convenience accessor. Returns k_xy in W/m-K. */
double pcb_thermal_material_k_xy(pcb_material_type_t type);

/** Get the through-plane thermal conductivity for a material.
 *  L1: Convenience accessor. Returns k_z in W/m-K.
 *  For isotropic materials, k_z = k_xy. For laminated materials
 *  (FR4, Rogers), k_z is typically 2-3x lower than k_xy due to
 *  the glass weave orientation. */
double pcb_thermal_material_k_z(pcb_material_type_t type);

/** Get the thermal diffusivity for a material.
 *  L1: alpha = k / (rho * cp) in m^2/s.
 *  Diffusivity determines how quickly temperature changes propagate.
 *  Higher diffusivity = faster thermal response.
 *
 *  Copper: alpha ~ 1.17e-4 m^2/s (very fast)
 *  FR4:    alpha ~ 1.4e-7 m^2/s (slow)
 *  Air:    alpha ~ 2.2e-5 m^2/s (fast but low heat capacity) */
double pcb_thermal_material_diffusivity(pcb_material_type_t type);

/** Get density for a material in kg/m^3. */
double pcb_thermal_material_density(pcb_material_type_t type);

/** Get specific heat capacity for a material in J/kg-K. */
double pcb_thermal_material_spec_heat(pcb_material_type_t type);

/** Get glass transition temperature for a polymer material.
 *  L1: Tg is the temperature at which the polymer transitions from
 *  rigid (glassy) to rubbery state. The CTE increases dramatically
 *  above Tg (typically 3-5x), causing reliability issues.
 *
 *  FR4 typically has Tg of 130-140 C (standard) or 170-180 C (high-Tg). */
double pcb_thermal_material_tg(pcb_material_type_t type);

/** Get maximum continuous operating temperature.
 *  L1: The maximum temperature at which the material maintains
 *  its specified properties over the product lifetime.
 *  Exceeding this causes accelerated aging, delamination risk. */
double pcb_thermal_material_max_temp(pcb_material_type_t type);

/* ==================================================================
 * L2: Material Selection and Comparison
 * ================================================================== */

/** Compare two materials for thermal performance in a given application.
 *  L2: Computes a figure of merit (FOM) based on the use case.
 *
 *  For heat spreading (in-plane):
 *    FOM = k_xy / cost
 *
 *  For through-thickness conduction:
 *    FOM = k_z / cost
 *
 *  For thermal vias compatibility:
 *    FOM = min(CTE_match_score, max_temp_score, k_z_score)
 *
 *  @param type1    First material
 *  @param type2    Second material
 *  @param use_case 0=spreading, 1=through_thickness, 2=via_compatibility
 *  @return         Positive if type1 is better, negative if type2 is better
 *  Complexity: O(1) */
double pcb_thermal_material_compare(pcb_material_type_t type1,
                                     pcb_material_type_t type2,
                                     int use_case);

/** Check if two materials are CTE-compatible.
 *  L2: CTE mismatch between materials causes thermal stress during
 *  temperature cycling. The maximum acceptable CTE mismatch depends
 *  on the bonding method and temperature range.
 *
 *  For solder joints: CTE mismatch < 5-7 ppm/K acceptable
 *  For epoxy bonds: CTE mismatch < 10-15 ppm/K acceptable
 *  For mechanical fastening: CTE mismatch < 20 ppm/K acceptable
 *
 *  @param type1, type2    Materials to compare
 *  @param max_mismatch    Maximum acceptable CTE difference (ppm/K)
 *  @return                1 if compatible, 0 if not
 *  Complexity: O(1) */
int pcb_thermal_material_cte_compatible(pcb_material_type_t type1,
                                         pcb_material_type_t type2,
                                         double max_mismatch);

/** Get the cost index for a material (normalized, 1.0 = standard FR4).
 *  L2: Cost is a critical design consideration.
 *  - FR4: 1.0 (baseline)
 *  - High-Tg FR4: 1.3-1.5
 *  - Polyimide: 3-5
 *  - Rogers: 5-15
 *  - Aluminum core: 2-4
 *  - Ceramic (Al2O3): 5-10
 *  - AlN: 20-50
 *  - BeO: 50-100 (toxic, restricted)
 *  @return Cost index relative to standard FR4 */
double pcb_thermal_material_cost_index(pcb_material_type_t type);

/** Get the recommended material based on application requirements.
 *  L2: Recommends the best material for given thermal, mechanical,
 *  and cost constraints.
 *
 *  @param min_k_xy          Minimum in-plane conductivity (W/m-K)
 *  @param min_k_z           Minimum through-plane conductivity (W/m-K)
 *  @param min_tg_c          Minimum glass transition temp (C), 0 = no requirement
 *  @param min_max_temp_c    Minimum max operating temp (C), 0 = no requirement
 *  @param max_cte_ppm       Maximum CTE (ppm/K), 0 = no requirement
 *  @param prefer_low_cost   1 = prefer lower cost over performance
 *  @return                  Recommended material type
 *  Complexity: O(N_materials) */
pcb_material_type_t pcb_thermal_material_recommend(double min_k_xy,
                                                     double min_k_z,
                                                     double min_tg_c,
                                                     double min_max_temp_c,
                                                     double max_cte_ppm,
                                                     int prefer_low_cost);

/** Get the standard copper thickness value for a given weight.
 *  L1: Returns mm. 1 oz = 0.03479 mm per IPC standard.
 *  Used for computing cross-sectional area in lateral conduction.
 *  Complexity: O(1) */
double pcb_thermal_copper_thickness(copper_weight_t weight);

/** Get the effective thermal conductivity for a copper layer
 *  with a given coverage ratio.
 *  L2: For patterned layers, the effective conductivity is
 *  between bare dielectric (coverage=0) and solid copper (coverage=1).
 *
 *  k_eff = k_dielectric + coverage * (k_Cu - k_dielectric)
 *
 *  Typical signal layer coverage: 0.3-0.5
 *  Typical power plane coverage: 0.8-0.95
 *  Solid ground plane coverage: 1.0
 *
 *  @param weight           Copper weight
 *  @param coverage         Copper coverage ratio (0-1)
 *  @param k_dielectric     Dielectric material conductivity (W/m-K)
 *  @return                 Effective in-plane conductivity (W/m-K) */
double pcb_thermal_copper_layer_k(copper_weight_t weight,
                                    double coverage, double k_dielectric);

/** Get the recommended convection coefficient for rough estimation.
 *  L2: Quick lookup for initial design calculations.
 *
 *  Natural convection, indoor:     7-12 W/m^2-K
 *  Natural convection, enclosed:   5-8 W/m^2-K
 *  Forced convection, 1 m/s:       15-25 W/m^2-K
 *  Forced convection, 3 m/s:       30-50 W/m^2-K
 *  Forced convection, 5 m/s:       50-80 W/m^2-K
 *  Liquid cooling:                 100-10000 W/m^2-K
 *
 *  @param cooling_type   Cooling method
 *  @param velocity_ms    Airflow velocity (m/s), 0 for natural
 *  @return               Estimated convection coefficient (W/m^2-K) */
double pcb_thermal_estimate_h(cooling_type_t cooling_type, double velocity_ms);

#ifdef __cplusplus
}
#endif

#endif /* PCB_THERMAL_MATERIAL_H */