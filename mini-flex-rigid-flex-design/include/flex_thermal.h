/**
 * @file flex_thermal.h
 * @brief Thermal Analysis for Flexible and Rigid-Flex PCBs
 *
 * Flex circuits operate in thermally challenging environments (tight spaces,
 * dynamic bending generates heat, CTE mismatches create stress).
 * This module provides thermal analysis specific to flex designs.
 *
 * L1 (Definitions): CTE, Tg, thermal conductivity, thermal resistance, theta-JA
 * L3 (Math Structures): Heat equation (1D steady-state), Fourier's law,
 *                        composite thermal conductivity
 * L4 (Fundamental Laws): Fourier's law of heat conduction,
 *                         Newton's law of cooling
 * L5 (Algorithms): Trace ampacity calculation, thermal via optimization,
 *                   CTE mismatch stress, junction temperature estimation
 *
 * @module mini-flex-rigid-flex-design
 */

#ifndef FLEX_THERMAL_H
#define FLEX_THERMAL_H

#include "flex_material.h"
#include "flex_stackup.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * L1 — Thermal Analysis Data Structures
 * -------------------------------------------------------------------------*/

/** Thermal analysis configuration */
typedef struct {
    double ambient_temp_c;            /**< Ambient temperature (°C) */
    double max_operating_temp_c;      /**< Maximum allowed operating temp (°C) */
    double copper_trace_width_um;     /**< Trace width for ampacity calculation */
    double copper_trace_thickness_um; /**< Trace thickness */
    double trace_length_mm;           /**< Trace length */
    int layer_position;               /**< Layer number in stackup (1=outer layer) */
    int is_outer_layer;               /**< 1 = outer layer (better cooling) */
    double airflow_m_per_s;           /**< Airflow velocity (m/s), 0 for natural convection */
    double board_thickness_mm;        /**< Total board thickness */
    double board_width_mm;            /**< Board width for convection area */
    double board_length_mm;           /**< Board length */
} flex_thermal_config_t;

/** Thermal analysis results */
typedef struct {
    double max_trace_current_a;       /**< Maximum safe current (A) per IPC-2152 */
    double trace_temperature_rise_c;  /**< Expected temperature rise above ambient */
    double trace_resistance_ohm;      /**< DC trace resistance (Ω) */
    double power_dissipation_w;       /**< Power dissipated in trace (W) */
    double thermal_resistance_board;  /**< Board thermal resistance θ_BA (°C/W) */
    double junction_temp_est_c;       /**< Estimated junction temperature (if component) */
    double cte_mismatch_stress_mpa;   /**< Thermal-mechanical stress from CTE mismatch */
    double thermal_time_constant_s;   /**< Thermal time constant (seconds) */
} flex_thermal_result_t;

/* ---------------------------------------------------------------------------
 * L4 — Fourier's Law & Conduction
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate 1D steady-state heat conduction through a flex stackup.
 *
 * q = -k * A * dT/dx  (Fourier's law)
 *
 * For multilayer: thermal resistance in series
 *   Rth_total = Σ (t_i / (k_i * A))
 *
 * @param layers Array of (thickness_mm, thermal_conductivity_w_mk) pairs
 * @param num_layers Number of layers
 * @param area_mm2 Cross-sectional area perpendicular to heat flow
 * @param delta_t_c Temperature difference across the stack (°C)
 * @return Heat flow in Watts
 *
 * Complexity: O(n_layers)
 */
double flex_thermal_conduction_1d(const double *thicknesses_mm,
                                   const double *conductivities_w_mk,
                                   int num_layers,
                                   double area_mm2,
                                   double delta_t_c);

/**
 * @brief Calculate effective in-plane thermal conductivity of a multilayer flex.
 *
 * k_effective_in_plane = Σ (k_i * t_i) / Σ t_i   (parallel rule)
 *
 * @param conductivities Array of k_i values (W/m·K)
 * @param thicknesses Array of t_i values (mm)
 * @param num_layers Number of layers
 * @return Effective in-plane thermal conductivity (W/m·K)
 */
double flex_thermal_conductivity_in_plane(const double *conductivities,
                                           const double *thicknesses,
                                           int num_layers);

/**
 * @brief Calculate effective through-thickness thermal conductivity.
 *
 * k_effective_thru = Σ t_i / Σ (t_i / k_i)   (series rule)
 *
 * Through-thickness conductivity is dominated by the poorest conductor
 * (typically the dielectric layers).
 *
 * @param conductivities Array of k_i values (W/m·K)
 * @param thicknesses Array of t_i values (mm)
 * @param num_layers Number of layers
 * @return Effective through-thickness thermal conductivity (W/m·K)
 */
double flex_thermal_conductivity_through(const double *conductivities,
                                          const double *thicknesses,
                                          int num_layers);

/* ---------------------------------------------------------------------------
 * L5 — Trace Ampacity (IPC-2152)
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate maximum current for a flex trace per IPC-2152.
 *
 * IPC-2152 replaced the older IPC-2221 current capacity charts with
 * physics-based models. Key insight: flex traces have lower ampacity
 * than rigid PCB traces due to thinner copper and less heat spreading.
 *
 * I = k * ΔT^b1 * A^b2  (empirical IPC-2152 curve fit)
 *
 * where A = cross-sectional area, ΔT = temperature rise.
 *
 * @param trace_width_um Trace width (μm)
 * @param trace_thickness_um Trace thickness (μm)
 * @param temp_rise_c Allowed temperature rise (°C)
 * @param is_outer_layer 1 for outer, 0 for inner
 * @return Maximum current in Amperes
 *
 * Reference: IPC-2152 "Standard for Determining Current-Carrying Capacity
 *            in Printed Board Design"
 * Complexity: O(1)
 */
double flex_trace_ampacity_ipc2152(double trace_width_um,
                                    double trace_thickness_um,
                                    double temp_rise_c,
                                    int is_outer_layer);

/**
 * @brief Calculate temperature rise for a given current in a flex trace.
 *
 * Inverse of the ampacity formula. Useful for verifying if a trace
 * will stay within thermal limits.
 *
 * ΔT = (I / (k * A^b2))^(1/b1)
 *
 * @param current_a Current flowing through trace (A)
 * @param trace_width_um Trace width (μm)
 * @param trace_thickness_um Trace thickness (μm)
 * @param is_outer_layer 1 for outer layer
 * @return Temperature rise in °C above ambient
 */
double flex_trace_temp_rise(double current_a,
                             double trace_width_um,
                             double trace_thickness_um,
                             int is_outer_layer);

/**
 * @brief Derate trace ampacity for flex-specific conditions.
 *
 * Flex traces have lower ampacity than rigid PCB due to:
 * - No thick FR-4 heat spreading
 * - Polyimide has lower thermal conductivity than FR-4 in Z-direction
 * - Tighter bend areas limit airflow
 *
 * @param base_ampacity_a Ampacity calculated for rigid PCB
 * @param flex_layer_count Number of flex layers (more layers = less cooling)
 * @param is_in_bend_zone 1 if trace is in a bend zone (restricted airflow)
 * @return Flex-derated ampacity in Amperes
 */
double flex_ampacity_derate_flex(double base_ampacity_a,
                                  int flex_layer_count,
                                  int is_in_bend_zone);

/* ---------------------------------------------------------------------------
 * L5 — Convection & Thermal Resistance
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate convective heat transfer coefficient for a flex surface.
 *
 * For natural convection (vertical surface): h ≈ 1.42 * (ΔT/L)^0.25
 * For forced convection: h ≈ 3.79 * (v^0.78 / L^0.22)
 *
 * where v = airflow velocity (m/s), L = characteristic length (m).
 *
 * @param delta_t_c Temperature difference between surface and ambient (°C)
 * @param char_length_m Characteristic length of the surface (m)
 * @param airflow_m_per_s Airflow velocity (m/s, 0 for natural)
 * @return Convective heat transfer coefficient h (W/m²·K)
 *
 * Complexity: O(1)
 */
double flex_convection_coefficient(double delta_t_c,
                                    double char_length_m,
                                    double airflow_m_per_s);

/**
 * @brief Calculate board-level thermal resistance (ambient to board).
 *
 * θ_BA = 1 / (h * A_total)  for single-surface cooling
 *
 * Simplified: also includes radiation contribution at higher temps.
 *
 * @param board_area_mm2 Total board surface area
 * @param h_convection Convection coefficient (from flex_convection_coefficient)
 * @param board_emissivity Surface emissivity (≈ 0.8 for PI, ≈ 0.9 for FR-4)
 * @return Thermal resistance θ_BA in °C/W
 */
double flex_thermal_resistance_board(double board_area_mm2,
                                      double h_convection,
                                      double board_emissivity);

/* ---------------------------------------------------------------------------
 * L5 — Thermal-Mechanical Coupling
 * -------------------------------------------------------------------------*/

/**
 * @brief Calculate thermal strain from CTE mismatch in a flex stackup.
 *
 * ε_thermal = Δα * ΔT
 *
 * where Δα = α_layer1 - α_layer2 (CTE difference),
 * ΔT = temperature change from stress-free state.
 *
 * @param cte_1 CTE of material 1 (ppm/°C)
 * @param cte_2 CTE of material 2 (ppm/°C)
 * @param delta_t_c Temperature change (°C)
 * @return Thermal strain (dimensionless)
 */
double flex_thermal_strain(double cte_1, double cte_2, double delta_t_c);

/**
 * @brief Calculate the critical temperature change that causes
 *        delamination between copper and dielectric.
 *
 * ΔT_critical = σ_peel / (E * Δα)
 *
 * where σ_peel = peel strength (N/mm²), E = effective modulus.
 *
 * @param peel_strength_n_per_mm2 Peel strength (N/mm²)
 * @param effective_modulus_mpa Effective elastic modulus
 * @param cte_diff_ppm_per_c CTE difference (ppm/°C)
 * @return Critical temperature change (°C)
 */
double flex_critical_temp_delta(double peel_strength_n_per_mm2,
                                 double effective_modulus_mpa,
                                 double cte_diff_ppm_per_c);

/**
 * @brief Calculate the thermal time constant for a flex PCB.
 *
 * τ = C_th * R_th = (ρ * c_p * V) / (h * A)
 *
 * where ρ = density, c_p = specific heat, V = volume.
 *
 * @param board_volume_mm3 Board volume
 * @param board_density_kg_m3 Board effective density
 * @param specific_heat_j_kgk Specific heat capacity
 * @param thermal_resistance_cw Thermal resistance to ambient (°C/W)
 * @return Thermal time constant in seconds
 */
double flex_thermal_time_constant(double board_volume_mm3,
                                   double board_density_kg_m3,
                                   double specific_heat_j_kgk,
                                   double thermal_resistance_cw);

/**
 * @brief Run complete thermal analysis for a flex PCB design.
 *
 * @param config Thermal analysis configuration
 * @param result [out] Thermal analysis results
 * @return 0 on success, -1 on invalid input
 */
int flex_thermal_analyze(const flex_thermal_config_t *config,
                          flex_thermal_result_t *result);

/**
 * @brief Estimate junction temperature for a component on flex.
 *
 * T_junction = T_ambient + P * (θ_JC + θ_CA)
 *
 * On flex, θ_CA is typically higher than on rigid due to lower
 * thermal mass and less copper spreading.
 *
 * @param ambient_temp_c Ambient temperature
 * @param power_dissipation_w Component power dissipation
 * @param theta_jc_cw Junction-to-case thermal resistance
 * @param theta_ca_cw Case-to-ambient thermal resistance
 * @return Estimated junction temperature in °C
 */
double flex_junction_temperature(double ambient_temp_c,
                                  double power_dissipation_w,
                                  double theta_jc_cw,
                                  double theta_ca_cw);

#ifdef __cplusplus
}
#endif

#endif /* FLEX_THERMAL_H */
