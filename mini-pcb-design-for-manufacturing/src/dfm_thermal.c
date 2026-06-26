/**
 * @file    dfm_thermal.c
 * @brief   PCB Thermal Management for DFM - L1-L5
 *
 * @details Thermal analysis for PCB manufacturability and reliability:
 *          - Material thermal properties (FR4, copper, polyimide)
 *          - Thermal resistance network (junction-board-ambient)
 *          - Copper balance analysis (warpage prevention)
 *          - IPC-2152 trace current-carrying capacity
 *          - Thermal via array design
 *          - Board warpage estimation (Timoshenko bimetal model)
 *          - Heat spreading in copper planes
 *          - Via count optimization for heat dissipation
 *
 * Knowledge Mapping:
 *   L1 - Definitions: Thermal conductivity, thermal resistance,
 *        CTE, copper balance, junction temperature
 *   L2 - Core Concepts: Heat transfer (conduction, convection,
 *        radiation), thermal vias, board warpage mechanism
 *   L3 - Mathematical Structures: Thermal resistance networks,
 *        differential thermal expansion, spreading resistance
 *   L4 - Fundamental Laws: Fourier's law of conduction,
 *        Timoshenko bimetal strip theory, IPC-2152
 *   L5 - Algorithms: Thermal via optimization, copper balance
 *        analysis, temperature rise estimation
 *
 * Reference: IPC-2152 (Current-Carrying Capacity)
 *            IPC-2221 Annex A (Thermal Management)
 *            Timoshenko, "Analysis of Bi-Metal Thermostats" (1925)
 *            Incropera & DeWitt, "Fundamentals of Heat Transfer"
 */

#include "dfm_thermal.h"
#include <string.h>
#include <stdio.h>

/* ================================================================
   L1 - Thermal Material Properties
   ================================================================

   FR4 (Flame Retardant 4) - The standard PCB laminate:
     - Thermal conductivity: 0.3 W/(m*K) - Poor conductor
       (Copper is 385 W/(m*K), >1000x difference)
     - CTE below Tg: 14 ppm/K (X/Y), 50 ppm/K (Z)
     - CTE above Tg: 200-300 ppm/K (Z) - rapid expansion!
     - Tg: 130-170C (standard FR4), >170C (high-Tg FR4)
     - Young's modulus: 24 GPa (fiberglass-reinforced)

   Copper:
     - Thermal conductivity: 385 W/(m*K) - Excellent conductor
     - Electrical conductivity: 5.8e7 S/m (annealed)
     - CTE: 17 ppm/K (close to FR4 X/Y, reason for reliability)
     - Young's modulus: 117 GPa
     - Specific heat: 385 J/(kg*K)
     - Density: 8960 kg/m^3
   ================================================================ */

thermal_material_t get_fr4_thermal_properties(void)
{
    thermal_material_t mat;
    mat.thermal_conductivity_k = 0.30;   /* W/(m*K) */
    mat.specific_heat_cp       = 1000.0; /* J/(kg*K) */
    mat.density_rho            = 1850.0; /* kg/m^3 */
    mat.cte_ppm                = 14.0;   /* ppm/K (X/Y) */
    mat.youngs_modulus_gpa     = 24.0;   /* GPa */
    return mat;
}

thermal_material_t get_copper_thermal_properties(void)
{
    thermal_material_t mat;
    mat.thermal_conductivity_k = 385.0;  /* W/(m*K) */
    mat.specific_heat_cp       = 385.0;  /* J/(kg*K) */
    mat.density_rho            = 8960.0; /* kg/m^3 */
    mat.cte_ppm                = 17.0;   /* ppm/K */
    mat.youngs_modulus_gpa     = 117.0;  /* GPa */
    return mat;
}

/* ================================================================
   L2/L3 - Junction Temperature Calculation
   ================================================================

   The thermal resistance network models heat flow from the
   semiconductor junction to ambient:

   T_junction --[R_jb]--> T_board --[R_ba]--> T_ambient

   where:
     R_jb = Junction-to-Board thermal resistance (K/W or C/W)
     R_ba = Board-to-Ambient thermal resistance (K/W or C/W)
     R_ja = R_jb + R_ba = total junction-to-ambient

   Steady-state junction temperature:
     T_j = T_a + P * (R_jb + R_ba)

   where P = power dissipation in watts.

   This is analogous to Ohm's law:
     Voltage (T) = Current (P) * Resistance (R_thermal)

   Key thermal resistance contributors on a PCB:
   1. Die attach (inside package): 1-5 K/W
   2. Package leadframe/BGA balls: 5-20 K/W
   3. PCB copper to ambient:
      - 2-layer board: 50-80 K/W (no thermal plane)
      - 4-layer board with 1oz Cu plane: 20-40 K/W
      - 4-layer board with 2oz Cu plane: 15-30 K/W
      - 6+ layer board: 10-20 K/W

   The board-to-ambient resistance is the dominant factor.
   Adding copper planes and thermal vias is the most effective
   way to reduce this resistance.
   ================================================================ */

thermal_resistance_t compute_junction_temp(double power_w,
                                            double r_jb, double r_ba,
                                            double ambient_temp_c,
                                            double max_junction_c)
{
    thermal_resistance_t result;
    memset(&result, 0, sizeof(result));

    if (power_w < 0.0 || r_jb < 0.0 || r_ba < 0.0) {
        result.within_limits = false;
        return result;
    }

    result.r_junction_board  = r_jb;
    result.r_board_ambient   = r_ba;
    result.r_junction_ambient = r_jb + r_ba;
    result.max_junction_temp_c = max_junction_c;
    result.ambient_temp_c       = ambient_temp_c;
    result.power_dissipation_w = power_w;

    /* T_junction = T_ambient + P * R_ja */
    result.junction_temp_c = ambient_temp_c
                           + power_w * (r_jb + r_ba);
    result.within_limits =
        (result.junction_temp_c <= max_junction_c);

    return result;
}

/* ================================================================
   L2 - Copper Balance Analysis
   ================================================================

   Asymmetric copper distribution between layers causes board warpage
   during the lamination process. This occurs because:

   1. Copper and FR4 have different CTEs:
      - Copper: 17 ppm/K
      - FR4: 14 ppm/K (X/Y), 50+ ppm/K (Z below Tg)

   2. During lamination, the stackup is heated to ~180C and pressed.
      Upon cooling to room temperature, layers with more copper
      shrink more than layers with less copper, creating a bending
      moment.

   3. The bending moment magnitude is proportional to:
      - Difference in copper coverage between layers
      - Distance from the neutral axis (outer layers have more effect)
      - Temperature difference (T_lamination - T_ambient)

   Asymmetry Index = max(|fill_i - fill_j|) for all layer pairs
   Industry guideline: < 10% asymmetry to prevent significant warpage

   IPC-2221 recommendation: Maintain ±15% copper balance on outer
   layers, ±25% on inner layers. For Class 3, tighten to ±10% and ±15%.
   ================================================================ */

copper_balance_t analyze_copper_balance(const double *copper_areas_mm2,
                                          int num_layers,
                                          double board_area_mm2)
{
    copper_balance_t balance;
    memset(&balance, 0, sizeof(balance));

    if (!copper_areas_mm2 || num_layers < 1 || board_area_mm2 <= 0.0) {
        balance.is_balanced = false;
        return balance;
    }

    balance.board_area_mm2 = board_area_mm2;
    balance.thickness_mm   = 1.6; /* standard */

    /* Compute fill percentage for each layer */
    /* For simplicity, support up to 4 layers */
    if (num_layers >= 1) {
        balance.copper_area_top_mm2 = copper_areas_mm2[0];
        balance.fill_top_pct = (copper_areas_mm2[0] / board_area_mm2) * 100.0;
    }
    if (num_layers >= 2) {
        balance.copper_area_inner1_mm2 = copper_areas_mm2[1];
        balance.fill_inner1_pct =
            (copper_areas_mm2[1] / board_area_mm2) * 100.0;
    }
    if (num_layers >= 3) {
        balance.copper_area_inner2_mm2 = copper_areas_mm2[2];
        balance.fill_inner2_pct =
            (copper_areas_mm2[2] / board_area_mm2) * 100.0;
    }
    if (num_layers >= 4) {
        balance.copper_area_bot_mm2 = copper_areas_mm2[num_layers - 1];
        balance.fill_bot_pct =
            (copper_areas_mm2[num_layers - 1] / board_area_mm2) * 100.0;
    } else if (num_layers >= 2) {
        balance.copper_area_bot_mm2 = copper_areas_mm2[num_layers - 1];
        balance.fill_bot_pct =
            (copper_areas_mm2[num_layers - 1] / board_area_mm2) * 100.0;
    }

    /* Compute asymmetry: max difference between any two layers */
    double fills[4];
    int nf = 0;
    if (num_layers >= 1) fills[nf++] = balance.fill_top_pct;
    if (num_layers >= 2) fills[nf++] = balance.fill_inner1_pct;
    if (num_layers >= 3) fills[nf++] = balance.fill_inner2_pct;
    if (num_layers >= 4) fills[nf++] = balance.fill_bot_pct;

    double max_diff = 0.0;
    for (int i = 0; i < nf; i++) {
        for (int j = i + 1; j < nf; j++) {
            double diff = fabs(fills[i] - fills[j]);
            if (diff > max_diff) max_diff = diff;
        }
    }
    balance.asymmetry_index = max_diff;
    balance.is_balanced = (max_diff < 10.0);

    return balance;
}

/* ================================================================
   L4 - IPC-2152 Trace Current Capacity
   ================================================================

   IPC-2152 provides the standard method for determining how much
   current a PCB trace can safely carry without overheating.

   The generic formula:
     I_max = k * dT^b1 * A^b2

   where:
     I_max = Maximum current (A)
     dT    = Temperature rise above ambient (C)
     A     = Trace cross-sectional area (sq mils)
     k     = 0.048 (external) or 0.024 (internal)
     b1    = 0.44
     b2    = 0.725

   Conversion: 1 sq mil = 645.16 um^2, or
               1 um wide x 1 um thick = 1 um^2 = 0.00155 sq mil

   Why external traces carry more current:
   1. External traces have better convection cooling
   2. External traces may have additional plating thickness
   3. Internal traces are surrounded by thermally insulating FR4

   The inverse formula computes required width for a given current:
     A_req = (I / (k * dT^b1))^(1/b2)
   ================================================================ */

double compute_trace_current_capacity(double trace_width_um,
                                       double copper_thickness_um,
                                       double temp_rise_c,
                                       bool is_external)
{
    if (trace_width_um <= 0.0 || copper_thickness_um <= 0.0
        || temp_rise_c <= 0.0) {
        return 0.0;
    }

    /* Cross-sectional area in square mils */
    double area_um2   = trace_width_um * copper_thickness_um;
    double area_sqmil = area_um2 * 0.00155; /* 1 um^2 = 0.00155 sq mil */

    /* IPC-2152 constants */
    double k;
    if (is_external) {
        k = 0.048;
    } else {
        k = 0.024;
    }
    double b1 = 0.44;
    double b2 = 0.725;

    /* I_max = k * dT^b1 * A^b2 */
    double I_max = k * pow(temp_rise_c, b1) * pow(area_sqmil, b2);

    return I_max;
}

double compute_required_trace_width(double current_a,
                                      double copper_thickness_um,
                                      double temp_rise_c,
                                      bool is_external)
{
    if (current_a <= 0.0 || copper_thickness_um <= 0.0
        || temp_rise_c <= 0.0) {
        return 0.0;
    }

    /* IPC-2152 constants */
    double k;
    if (is_external) {
        k = 0.048;
    } else {
        k = 0.024;
    }
    double b1 = 0.44;
    double b2 = 0.725;

    /* A_sqmil = (I / (k * dT^b1))^(1/b2) */
    double A_sqmil = pow(current_a / (k * pow(temp_rise_c, b1)), 1.0 / b2);

    /* Convert sq mil back to um^2, then divide by thickness for width */
    double A_um2 = A_sqmil / 0.00155;

    return A_um2 / copper_thickness_um;
}

/* ================================================================
   L5 - Thermal Via Array Design
   ================================================================

   Thermal vias are plated through-holes placed under a heat-
   generating component to conduct heat from the component pad
   to an internal or bottom-side copper plane.

   Thermal resistance of a single via:
     R_via = L / (k_cu * A_cross)

   where:
     L       = Board thickness (m)
     k_cu    = Copper thermal conductivity (385 W/(m*K))
     A_cross = Cross-sectional area of via copper barrel

   For a plated via with via diameter d and plating thickness t_plating:
     A_cross = pi * t_plating * (d - t_plating)

   Typical plating thickness: 25 um (1 mil).

   For N identical vias in parallel:
     R_total = R_via / N

   Required number of vias:
     N = R_via / R_required
   where R_required = max_temp_rise / power

   IPC-2152 gives additional guidance: thermal vias should have
   minimum 0.3mm diameter and be filled with epoxy (or left unfilled
   but tented) to prevent solder wicking.
   ================================================================ */

thermal_via_array_t design_thermal_vias(double power_w,
                                          double max_temp_rise_c,
                                          double board_thickness_mm,
                                          double via_diameter_mm,
                                          int max_vias)
{
    thermal_via_array_t array;
    memset(&array, 0, sizeof(array));

    if (power_w <= 0.0 || max_temp_rise_c <= 0.0 ||
        board_thickness_mm <= 0.0 || via_diameter_mm <= 0.0 ||
        max_vias < 1) {
        return array;
    }

    /* Thermal resistance required: R_req = dT_max / P */
    double R_required = max_temp_rise_c / power_w;

    /* Single via thermal resistance */
    /* k_cu = 385 W/(m*K) */
    double k_cu = 385.0;

    /* Plating thickness = 25 um = 2.5e-5 m */
    double plating_thickness_m = 25.0e-6;

    /* Via cross-sectional area (annular ring of copper):
       A = pi * (r_outer^2 - r_inner^2)
       = pi * t_plating * (d - t_plating) [approx] */
    double A_cross_m2 = M_PI * plating_thickness_m
                      * (via_diameter_mm * 1e-3 - plating_thickness_m);
    if (A_cross_m2 <= 0.0) return array;

    /* Board thickness in meters */
    double L_m = board_thickness_mm * 1e-3;

    /* R_via = L / (k * A) */
    double R_via = L_m / (k_cu * A_cross_m2);

    /* Required number of vias */
    double N_exact = R_via / R_required;
    int num_vias = (int)ceil(N_exact);
    if (num_vias < 1) num_vias = 1;
    if (num_vias > max_vias) num_vias = max_vias;

    /* Total thermal resistance with N vias in parallel */
    double R_total = R_via / (double)num_vias;

    array.num_vias = num_vias;
    array.via_diameter_mm = via_diameter_mm;
    array.via_pitch_mm = via_diameter_mm * 2.5; /* typical pitch */
    array.total_thermal_resistance_kw = R_total;
    array.copper_area_mm2 = (double)num_vias * M_PI
        * (via_diameter_mm / 2.0) * (via_diameter_mm / 2.0);
    array.board_thickness_mm = board_thickness_mm;

    return array;
}

/* ================================================================
   L4 - Board Warpage Estimation (Timoshenko Model)
   ================================================================

   Timoshenko's bimetal thermostat theory (1925) can be adapted to
   predict PCB warpage from asymmetric copper distribution.

   The PCB is modeled as a bimetal strip with:
   - Layer 1: Copper-rich side
   - Layer 2: Laminate-rich side

   Curvature formula:
     k = 6*(alpha_2 - alpha_1)*(T_cure - T_amb)*(1+m)^2 /
         (h * (3*(1+m)^2 + (1+mn)*(m^2 + 1/(mn))))

   where:
     m = t_copper / t_laminate       (thickness ratio)
     n = E_copper / E_laminate       (modulus ratio)
     alpha_1, alpha_2 = CTE values
     h = t_copper + t_laminate       (total thickness)

   For a practical PCB estimate, we use a simplified version
   adapted for the typical copper-FR4 system.
   ================================================================ */

double estimate_warpage(const copper_balance_t *balance)
{
    if (!balance || !balance->is_balanced) {
        /* Unbalanced boards: significant warpage expected */
        if (balance && balance->asymmetry_index > 20.0) {
            return 2.0; /* severe warpage: >2mm over 100mm */
        }
        if (balance && balance->asymmetry_index > 15.0) {
            return 1.0; /* moderate warpage */
        }
        return 0.5; /* mild warpage */
    }

    /* Balanced board: minimal warpage */
    if (balance->asymmetry_index < 5.0) {
        return 0.1; /* excellent: <0.1mm over 100mm */
    } else if (balance->asymmetry_index < 8.0) {
        return 0.3; /* good: 0.3mm/100mm */
    } else {
        return 0.5; /* acceptable: 0.5mm/100mm */
    }
}

/* ================================================================
   L2 - Heat Spreading Resistance
   ================================================================

   When a small heat source (e.g., a BGA package) is mounted on a
   large copper plane, heat spreads laterally before being conducted
   through the board. The spreading resistance quantifies this effect.

   For an axisymmetric geometry (circular heat source on infinite plane):
     R_spread = [1 / (pi * k * t)] * ln(r2 / r1)

   where:
     r1 = heat source radius (m)
     r2 = effective spreading radius (m)
     k  = copper thermal conductivity (385 W/(m*K))
     t  = copper plane thickness (m)

   The effective spreading radius r2 depends on:
   - Board size (can't spread beyond the board edge)
   - Adjacent heat sources (they compete for spreading)
   - Typical: r2 = min(board_radius, sqrt(t * k / h_conv))

   where h_conv is the convection coefficient (5-25 W/(m^2*K)
   for natural convection on a PCB).
   ================================================================ */

double compute_spreading_resistance(double source_radius_mm,
                                     double spreading_radius_mm,
                                     double copper_thickness_um)
{
    if (source_radius_mm <= 0.0 || spreading_radius_mm <= 0.0
        || copper_thickness_um <= 0.0) {
        return 0.0;
    }
    if (spreading_radius_mm <= source_radius_mm) {
        return 0.0; /* no spreading beyond source */
    }

    /* Copper thermal conductivity */
    double k_cu = 385.0; /* W/(m*K) */

    /* Convert to meters */
    double t_m  = copper_thickness_um * 1e-6;
    double r1_m = source_radius_mm * 1e-3;
    double r2_m = spreading_radius_mm * 1e-3;

    /* R_spread = ln(r2/r1) / (pi * k * t) */
    double R = log(r2_m / r1_m) / (M_PI * k_cu * t_m);

    return R;
}

/* ================================================================
   L5 - Effective Thermal Conductivity of PCB
   ================================================================

   A multilayer PCB is a composite material with copper layers
   embedded in FR4. The effective through-thickness thermal
   conductivity depends on the copper content and distribution.

   Using the rule of mixtures for series thermal resistance:
     1/k_eff = sum(volume_fraction_i / k_i)

   For a typical 4-layer, 1.6mm board with 35um copper:
     FR4: ~1.5mm, k=0.3 W/(m*K)
     Copper: ~0.14mm, k=385 W/(m*K)
     k_eff_inplane ~ 34 W/(m*K) (copper dominates in-plane)
     k_eff_through ~ 0.35 W/(m*K) (FR4 dominates through-thickness)

   This huge anisotropy (~100:1) means:
   - In-plane heat spreading is excellent
   - Through-thickness heat transfer is poor
   - Thermal vias are essential for through-thickness transport
   ================================================================ */

double compute_effective_thermal_conductivity(double fr4_thickness_mm,
                                               double total_cu_thickness_mm,
                                               bool in_plane)
{
    if (fr4_thickness_mm <= 0.0 || total_cu_thickness_mm <= 0.0) {
        return 0.0;
    }

    double k_fr4 = 0.30;  /* W/(m*K) */
    double k_cu  = 385.0; /* W/(m*K) */

    double total_mm = fr4_thickness_mm + total_cu_thickness_mm;
    double vf_fr4   = fr4_thickness_mm / total_mm;
    double vf_cu    = total_cu_thickness_mm / total_mm;

    if (in_plane) {
        /* Parallel model: k_parallel = vf_cu * k_cu + vf_fr4 * k_fr4 */
        return vf_cu * k_cu + vf_fr4 * k_fr4;
    } else {
        /* Series model: 1/k_series = vf_fr4/k_fr4 + vf_cu/k_cu */
        double inv_k = vf_fr4 / k_fr4 + vf_cu / k_cu;
        if (inv_k <= 0.0) return 0.0;
        return 1.0 / inv_k;
    }
}
