/**
 * @file    dfm_thermal.h
 * @brief   PCB Thermal Management for DFM - L1 Defs, L2 Concepts, L5 Algorithms
 *
 * @details Thermal analysis for PCB manufacturability: copper balance,
 *          thermal relief design, heat dissipation, via thermal
 *          management, and warpage prevention.
 *
 * Knowledge Mapping:
 *   L1 - Definitions:
 *     - Thermal conductivity of PCB materials
 *     - Thermal resistance (junction, board, ambient)
 *     - Copper balance concept
 *   L2 - Core Concepts:
 *     - Heat transfer mechanisms (conduction, convection, radiation)
 *     - Thermal vias design
 *     - Board warpage from asymmetric copper
 *   L5 - Algorithms:
 *     - Thermal via optimization
 *     - Copper balance analysis
 *     - Temperature rise estimation
 *
 * Reference: IPC-2152 (Standard for Determining Current-Carrying Capacity)
 *            IPC-2221 Annex A (Thermal Management)
 */

#ifndef DFM_THERMAL_H
#define DFM_THERMAL_H

#include "dfm_core.h"
#include <stddef.h>
#include <stdbool.h>

/* ---- Thermal Material Properties ---- */

typedef struct {
    double thermal_conductivity_k;   /* W/(m*K) */
    double specific_heat_cp;         /* J/(kg*K) */
    double density_rho;              /* kg/m^3 */
    double cte_ppm;                  /* CTE in ppm/K */
    double youngs_modulus_gpa;       /* Young's modulus */
} thermal_material_t;

thermal_material_t get_fr4_thermal_properties(void);
thermal_material_t get_copper_thermal_properties(void);

/* ---- Thermal Resistance Network ---- */

typedef struct {
    double r_junction_board;    /* Junction-to-board (K/W) */
    double r_board_ambient;     /* Board-to-ambient (K/W) */
    double r_junction_ambient;  /* Total junction-to-ambient (K/W) */
    double max_junction_temp_c; /* Maximum junction temperature */
    double ambient_temp_c;      /* Ambient temperature */
    double power_dissipation_w; /* Power dissipation */
    double junction_temp_c;      /* Calculated junction temperature */
    bool   within_limits;       /* Junction temp < max */
} thermal_resistance_t;

/**
 * Compute junction temperature from thermal resistance network.
 *
 * T_junction = T_ambient + P * R_ja
 *
 * where R_ja = R_jb + R_ba (series thermal resistances)
 *
 * @param power_w              Power dissipation (W)
 * @param r_jb                 Junction-to-board resistance (K/W)
 * @param r_ba                 Board-to-ambient resistance (K/W)
 * @param ambient_temp_c       Ambient temperature (C)
 * @param max_junction_c       Maximum allowed junction temp (C)
 * @return                     Thermal resistance analysis
 */
thermal_resistance_t compute_junction_temp(double power_w,
                                            double r_jb, double r_ba,
                                            double ambient_temp_c,
                                            double max_junction_c);

/* ---- Copper Balance Analysis ---- */

typedef struct {
    double copper_area_top_mm2;
    double copper_area_bot_mm2;
    double copper_area_inner1_mm2;
    double copper_area_inner2_mm2;
    double board_area_mm2;
    double thickness_mm;
    double fill_top_pct;
    double fill_bot_pct;
    double fill_inner1_pct;
    double fill_inner2_pct;
    double asymmetry_index;      /* Max fill diff between layers (0=perfect) */
    bool   is_balanced;          /* Asymmetry < 10% */
} copper_balance_t;

/**
 * Analyze copper distribution across layers for balance.
 *
 * Asymmetric copper distribution causes board warpage during
 * lamination due to differential CTE between copper and laminate.
 *
 * Asymmetry index = max(|fill_i - fill_j|) for all layer pairs i,j
 * Balanced if asymmetry < 10% (IPC guideline)
 *
 * @param copper_areas_mm2  Array of copper areas per layer
 * @param num_layers        Number of layers
 * @param board_area_mm2    Total board area
 * @return                  Copper balance analysis
 */
copper_balance_t analyze_copper_balance(const double *copper_areas_mm2,
                                         int num_layers,
                                         double board_area_mm2);

/* ---- Trace Current Capacity ---- */

/**
 * Compute maximum current capacity of a PCB trace.
 *
 * IPC-2152 generic formula:
 *   I_max = k * dT^b1 * A^b2
 *
 * where:
 *   I_max = maximum current (A)
 *   dT    = temperature rise above ambient (C)
 *   A     = trace cross-sectional area (sq mils)
 *   k, b1, b2 = empirical constants
 *
 * For external traces: k=0.048, b1=0.44, b2=0.725
 * For internal traces: k=0.024, b1=0.44, b2=0.725
 *
 * @param trace_width_um       Trace width (um)
 * @param copper_thickness_um  Copper thickness (um)
 * @param temp_rise_c          Allowed temperature rise (C)
 * @param is_external          External or internal layer
 * @return                     Maximum current (A)
 */
double compute_trace_current_capacity(double trace_width_um,
                                       double copper_thickness_um,
                                       double temp_rise_c,
                                       bool is_external);

/**
 * Compute required trace width for a given current.
 *
 * Inverse of IPC-2152 formula.
 *
 * @param current_a            Required current (A)
 * @param copper_thickness_um  Copper thickness (um)
 * @param temp_rise_c          Allowed temperature rise (C)
 * @param is_external          External or internal layer
 * @return                     Required trace width (um)
 */
double compute_required_trace_width(double current_a,
                                     double copper_thickness_um,
                                     double temp_rise_c,
                                     bool is_external);

/* ---- Thermal Via Optimization ---- */

typedef struct {
    int    num_vias;
    double via_diameter_mm;
    double via_pitch_mm;
    double total_thermal_resistance_kw;
    double copper_area_mm2;
    double board_thickness_mm;
} thermal_via_array_t;

/**
 * Design thermal via array for given heat dissipation.
 *
 * Thermal resistance of a single filled via:
 *   R_via = L / (k_cu * pi * (d/2)^2)
 *
 * where L = board thickness, k_cu = 385 W/(m*K)
 *
 * Parallel vias reduce resistance: R_total = R_via / N
 *
 * @param power_w             Power to dissipate (W)
 * @param max_temp_rise_c     Maximum allowed temperature rise (C)
 * @param board_thickness_mm  Board thickness
 * @param via_diameter_mm     Via diameter
 * @param max_vias            Maximum vias allowed
 * @return                    Thermal via array design
 */
thermal_via_array_t design_thermal_vias(double power_w,
                                         double max_temp_rise_c,
                                         double board_thickness_mm,
                                         double via_diameter_mm,
                                         int max_vias);

/* ---- Board Warpage Estimation ---- */

/**
 * Estimate board warpage from asymmetric copper distribution.
 *
 * Uses Timoshenko's bimetal strip model adapted for PCB:
 *   curvature = 6*(alpha_cu-alpha_fr4)*(Tcure-Tamb)*(1+m)^2 /
 *               (h*(3*(1+m)^2+(1+mn)*(m^2+1/(mn))))
 *
 * where m = t_cu / t_fr4
 *       n = E_cu / E_fr4
 *
 * @param balance  Copper balance analysis result
 * @return         Estimated warpage in mm over 100mm span
 */
double estimate_warpage(const copper_balance_t *balance);

/* ---- Heat Spreading ---- */

/**
 * Compute heat spreading resistance in a PCB plane.
 *
 * For a copper plane of thickness t:
 *   R_spread = 1 / (pi * k * t) * ln(r2/r1)
 *
 * where r1 = heat source radius, r2 = spreading radius
 *
 * @param source_radius_mm       Heat source radius
 * @param spreading_radius_mm    Effective spreading radius
 * @param copper_thickness_um    Plane copper thickness
 * @return                       Spreading resistance (K/W)
 */
double compute_spreading_resistance(double source_radius_mm,
                                     double spreading_radius_mm,
                                     double copper_thickness_um);

/* Effective thermal conductivity of a multilayer PCB */
double compute_effective_thermal_conductivity(double fr4_thickness_mm,
                                               double total_cu_thickness_mm,
                                               bool in_plane);

#endif /* DFM_THERMAL_H */
