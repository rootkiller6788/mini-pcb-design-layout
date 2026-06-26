/**
 * pcb_thermal_design.h — PCB Thermal Design Methods and Optimization
 *
 * L5: Design algorithms — copper pour sizing, PCB stack thermal
 *     optimization, component placement for thermal balance.
 * L6: Canonical problems — LDO copper pour design, MOSFET heat sinking,
 *     parallel device thermal runaway prevention.
 *
 * Courses: Berkeley EE105 Analog, TU Munich High-Frequency Eng.,
 *          Michigan EECS 411 Microwave, Georgia Tech ECE 6350
 * Reference: IPC-2152 "Standard for Determining Current-Carrying Capacity"
 *            Erickson & Maksimovic, "Fundamentals of Power Electronics" (2001), Ch. 4
 *            Lee, "PCB Thermal Design Guide", Texas Instruments (2014)
 */

#ifndef PCB_THERMAL_DESIGN_H
#define PCB_THERMAL_DESIGN_H

#include "pcb_thermal_defs.h"
#include "pcb_thermal_analysis.h"
#include "pcb_thermal_via.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L5: Copper Pour Sizing for Component Cooling
 * ================================================================== */

/** Calculate minimum copper pour area needed for a component.
 *  L5/L6: Given component power, ambient temperature, and maximum
 *  allowed junction temperature, determine the copper area required.
 *
 *  Algorithm:
 *  1. Compute allowed temperature rise: dT_max = Tj_max - Ta - P*Rjc
 *  2. Required R_ambient = dT_max / P
 *  3. R_ambient = R_spread(pour) || R_conv(pour, h)
 *  4. For natural convection: solve for area iteratively
 *
 *  Empirical model for FR4 1oz copper pour, natural convection:
 *    R_board-ambient(A) ≈ 1 / (h * A * eta)
 *    where eta ≈ 0.7 for practical copper pours (accounts for non-isothermal surface)
 *    h ≈ 10 W/m^2-K for typical natural convection
 *
 *  @param power_w       Component power dissipation (W)
 *  @param ta_c          Ambient temperature (C)
 *  @param tj_max_c      Maximum junction temperature (C)
 *  @param r_jc          Junction-to-case resistance (C/W)
 *  @param board_k       Board effective thermal conductivity (W/m-K)
 *  @param board_t_mm    Board thickness (mm)
 *  @param h_conv        Convection coefficient (W/m^2-K)
 *  @param copper_oz     Copper weight on the pour layer
 *  @param area_mm2_out  Output: required copper pour area (mm^2)
 *  @param r_pour_out    Output: achieved thermal resistance (C/W)
 *  @return              THERMAL_OK or error code
 *  Complexity: O(log(A_max/A_min)) — binary search */
int copper_pour_sizing(double power_w, double ta_c, double tj_max_c,
                        double r_jc, double board_k, double board_t_mm,
                        double h_conv, copper_weight_t copper_oz,
                        double *area_mm2_out, double *r_pour_out);

/** Calculate junction temperature for a component with a copper pour.
 *  L5/L6: Complete thermal analysis of a component with copper pour cooling.
 *
 *  Resistance network: Tj → [Rjc] → Tc → [Rspread] → Tpour → [Rconv] → Ta
 *  The total: Rja = Rjc + Rspread(pour) + Rconv(pour)
 *
 *  @param source     Heat source (with power, Rjc, footprint)
 *  @param pour       Copper pour geometry
 *  @param ta_c       Ambient temperature (C)
 *  @param h_conv     Convection coefficient (W/m^2-K)
 *  @param tj_out     Output: junction temperature (C)
 *  @return           THERMAL_OK or error code
 *  Complexity: O(1) */
int copper_pour_thermal_analysis(const heat_source_t *source,
                                  const copper_pour_geom_t *pour,
                                  double ta_c, double h_conv,
                                  double *tj_out);

/** Optimal copper pour dimensions for a given area and source size.
 *  L5: For a fixed pour area, the optimal shape minimizes spreading
 *  resistance. The ideal shape is circular around the source, but
 *  manufacturing favors rectangular with aspect ratio near 1.
 *
 *  The spreading resistance in a rectangular pour is minimized when
 *  width ≈ length (square shape), as this minimizes the maximum
 *  distance from source edge to pour edge.
 *
 *  @param area_mm2        Target pour area (mm^2)
 *  @param source_w_mm     Source width (mm)
 *  @param source_h_mm     Source height (mm)
 *  @param pour_w_out      Output: optimal pour width (mm)
 *  @param pour_h_out      Output: optimal pour height (mm)
 *  Complexity: O(1) */
void copper_pour_optimal_dimensions(double area_mm2,
                                     double source_w_mm, double source_h_mm,
                                     double *pour_w_out, double *pour_h_out);

/* ==================================================================
 * L5: PCB Stack-up Thermal Optimization
 * ================================================================== */

/** Compute effective in-plane thermal conductivity of a multi-layer PCB.
 *  L5: For N layers with copper only on conducting layers:
 *    k_eff_xy = sum_i (k_i * t_i) / sum_i (t_i)
 *  where k_i = k_Cu * cu_coverage_i + k_FR4 * (1 - cu_coverage_i)
 *
 *  Solid planes (GND, PWR) have cu_coverage ≈ 1.0
 *  Signal layers have cu_coverage ≈ 0.2-0.5
 *
 *  A 4-layer board with 2 solid planes has k_eff ≈ 15-25x bare FR4! */
double pcb_stack_effective_k_xy(const pcb_stackup_t *stackup);

/** Compute effective through-plane thermal conductivity.
 *  L5: For through-thickness (Z) direction, layers are in series:
 *    1/k_eff_z = sum_i (t_i / (t_total * k_i))
 *  or equivalently: k_eff_z = t_total / sum_i (t_i / k_i)
 *
 *  FR4 dominates Z-conductivity because it's the thickest and has
 *  the lowest k. Copper planes in parallel add negligible Z-conductivity
 *  because they're thin. Thermal vias are the only effective way
 *  to improve Z-conduction. */
double pcb_stack_effective_k_z(const pcb_stackup_t *stackup);

/** Analyze the thermal benefit of adding internal copper planes.
 *  L5: Compare the effective in-plane conductivity with and without
 *  additional solid planes. This quantifies the heat spreading benefit
 *  of converting a signal layer to a ground/power plane.
 *
 *  For a 2-layer (SIG/GND) to 4-layer (SIG/GND/PWR/SIG) upgrade:
 *  Typical k_eff improvement: 20-40% for in-plane spreading. */
double pcb_stack_plane_benefit(const pcb_stackup_t *stackup);

/** Find optimal layer count for thermal performance.
 *  L5: Thermal benefit of adding planes diminishes after ~6 layers.
 *  Cost increases quadratically. This function evaluates the
 *  cost (layers * area * cost_per_layer) vs thermal performance
 *  (k_eff_xy) trade-off.
 *
 *  @param stackup_template  Base stack-up to optimize
 *  @param max_layers        Maximum affordable layers
 *  @param cost_per_layer    Relative cost factor per layer
 *  @param optimal_layers    Output: optimal layer count
 *  @param benefit_score     Output: benefit/cost score
 *  Complexity: O(max_layers) */
void pcb_stack_optimize_layer_count(const pcb_stackup_t *stackup_template,
                                     int max_layers, double cost_per_layer,
                                     int *optimal_layers, double *benefit_score);

/** Estimate board-to-ambient thermal resistance for a given PCB area.
 *  L5: Empirical model based on board size and convection conditions.
 *
 *  For natural convection:
 *  R_ba(A) ≈ 1 / (h * A_board)
 *  where h ≈ 7-15 W/m^2-K for typical PCB sizes (50x50 to 200x200 mm)
 *
 *  The coefficient h depends on board size (smaller boards have higher h)
 *  due to boundary layer scaling. This is captured by:
 *  h = C * (dT / L_char)^(1/4) for laminar natural convection. */
double pcb_board_to_ambient_resistance(double board_area_mm2,
                                        double board_temp_c,
                                        const ambient_conditions_t *ambient);

/* ==================================================================
 * L5: Thermal Derating and Safety Margins
 * ================================================================== */

/** Compute thermal derating factor for a power device.
 *  L5/L6: Derating = (Tj_max - Tj_actual) / (Tj_max - Ta)
 *  A derating factor of 1.0 means Tj = Ta (ideal, no heating)
 *  A derating factor of 0.0 means Tj = Tj_max (at limit)
 *  A derating factor < 0 means Tj > Tj_max (overheated!)
 *
 *  Typical design guidelines:
 *  - Derating > 0.5: safe operation
 *  - Derating 0.3-0.5: marginal, consider improved cooling
 *  - Derating < 0.3: redesign required
 *
 *  Reference: MIL-HDBK-217F, "Reliability Prediction of Electronic Equipment" */
double thermal_derating_factor(double tj_actual, double tj_max, double ta_c);

/** Compute estimated lifetime impact of elevated temperature.
 *  L5/L7: Arrhenius equation for temperature-accelerated failure.
 *
 *  AF = exp((Ea/k) * (1/T_use - 1/T_stress))
 *  where: Ea = activation energy (~0.7 eV for typical IC failures)
 *         k = Boltzmann constant = 8.617333262145e-5 eV/K
 *         T in Kelvin
 *
 *  Rule of thumb: every 10°C increase in junction temperature
 *  reduces semiconductor lifetime by approximately 50%.
 *
 *  @param tj_actual_c    Actual junction temperature (C)
 *  @param tj_rated_c     Rated junction temperature for specified lifetime (C)
 *  @param rated_life_hours  Specified lifetime at rated temperature (hours)
 *  @param ea_ev          Activation energy (eV), typically 0.6-0.8 for ICs
 *  @return               Estimated lifetime at actual temperature (hours) */
double thermal_lifetime_estimate(double tj_actual_c, double tj_rated_c,
                                  double rated_life_hours, double ea_ev);

/** Check for thermal runaway risk in parallel power devices.
 *  L5/L6: When MOSFETs are paralleled without individual ballasting,
 *  the device with the lowest Vth (highest current) gets hotter,
 *  which further reduces Vth → more current → thermal runaway.
 *
 *  Stability criterion (from positive temperature coefficient analysis):
 *    dP/dT < 1 / R_thermal  →  stable
 *    dP/dT > 1 / R_thermal  →  thermal runaway possible
 *
 *  For MOSFETs: Rds(on) increases with T (positive TC above ~100mA),
 *  so current sharing improves at high current — self-stabilizing.
 *  But in the subthreshold/saturation region, TC is negative! */
int thermal_runaway_check(const heat_source_t *devices, int num_devices,
                           double r_thermal_per_device, double ta_c);

/* ==================================================================
 * L6: Complete Cooling Solution Design
 * ================================================================== */

/** Design a complete cooling solution for a power component.
 *  L6: Integrates copper pour, thermal vias, and heat sink selection
 *  into a single design flow.
 *
 *  Steps:
 *  1. Calculate required Rja from power budget
 *  2. Try copper pour alone → if sufficient, done
 *  3. If not, add thermal vias → recalculate
 *  4. If still not, select appropriate heat sink
 *  5. Verify with safety margin (>20%)
 *
 *  @param source            Heat source specification (in/out)
 *  @param ta_c              Ambient temperature (C)
 *  @param board_area_mm2    Available board area (mm^2)
 *  @param board_k           Board effective in-plane conductivity
 *  @param board_t_mm        Board thickness
 *  @param copper_oz         Copper weight available
 *  @param allow_heatsink    1 = allow heat sink, 0 = copper/vias only
 *  @param design_margin     Safety margin factor (>1.0, typically 1.25)
 *  @param cooling_used      Output: which cooling types were employed
 *  @param final_tj          Output: final junction temperature (C)
 *  @return                  THERMAL_OK or error code
 *  Complexity: O(1) */
int design_cooling_solution(heat_source_t *source,
                             double ta_c,
                             double board_area_mm2,
                             double board_k, double board_t_mm,
                             copper_weight_t copper_oz,
                             int allow_heatsink,
                             double design_margin,
                             cooling_type_t *cooling_used,
                             double *final_tj);

/** Select an appropriate heat sink from a catalog.
 *  L6: Given required Rsa and available volume, select the best
 *  heat sink from a set of standard profiles.
 *
 *  Selection criteria:
 *  - R_sa at natural/forced convection must meet requirement
 *  - Physical dimensions must fit
 *  - Minimize cost (typically correlated with weight)
 *
 *  @param r_sa_required   Required sink-to-ambient resistance (C/W)
 *  @param max_width_mm    Maximum allowed width (mm)
 *  @param max_height_mm   Maximum allowed height (mm)
 *  @param max_length_mm   Maximum allowed length (mm)
 *  @param forced_velocity_ms  Airflow velocity (0 = natural convection)
 *  @param selected        Output: selected heat sink model
 *  @return                THERMAL_OK or error code (THERMAL_ERR_NOT_IMPL if none fit)
 *  Complexity: O(N_catalog) */
int heatsink_select_from_catalog(double r_sa_required,
                                  double max_width_mm, double max_height_mm,
                                  double max_length_mm,
                                  double forced_velocity_ms,
                                  heat_sink_model_t *selected);

/** Determine the junction temperature rise time after power-on.
 *  L6: Using the lumped thermal time constant and Foster model.
 *  Estimates how long it takes for the junction to reach 63% (1*tau),
 *  95% (3*tau), or 99% (5*tau) of its final temperature.
 *
 *  @param r_ja           Junction-to-ambient resistance (C/W)
 *  @param mass_g         Component mass (g)
 *  @param cp_jkgk        Specific heat (J/kg-K)
 *  @param t_rise_63pct   Output: time to 63% of final temperature (s)
 *  @param t_rise_95pct   Output: time to 95% of final temperature (s)
 *  @param t_rise_99pct   Output: time to 99% of final temperature (s)
 *  Complexity: O(1) */
void thermal_rise_time(double r_ja, double mass_g, double cp_jkgk,
                        double *t_rise_63pct, double *t_rise_95pct,
                        double *t_rise_99pct);

#ifdef __cplusplus
}
#endif

#endif /* PCB_THERMAL_DESIGN_H */