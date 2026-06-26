/**
 * @file flex_thermal.c
 * @brief Thermal Analysis Implementation for Flex/Rigid-Flex PCBs
 *
 * Implements thermal conduction, convection, trace ampacity (IPC-2152),
 * and thermo-mechanical coupling analysis for flexible circuit designs.
 * Each function encodes one independent thermal engineering knowledge point.
 *
 * Flex circuits have unique thermal challenges: thinner profiles (< 0.5 mm),
 * lower thermal mass, polyimide's poor thermal conductivity (0.12 W/m·K),
 * and restricted airflow in tight bend installations.
 *
 * @module mini-flex-rigid-flex-design
 */

#include "flex_thermal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Physical constants */
#define STEFAN_BOLTZMANN 5.670374419e-8  /* W/(m²·K⁴) */
#define C0 299792458.0                   /* m/s */
#define RHO_CU 1.72e-8                   /* Ω·m, copper resistivity */
#define MU0 1.25663706212e-6            /* H/m */

/* ========================================================================
 * L4: Fourier's Law — Heat Conduction
 * ========================================================================*/

/**
 * Knowledge Point: 1D steady-state heat conduction through multilayer stack.
 *
 * Fourier's law of heat conduction:
 *   q = -k * A * (dT/dx)
 *
 * For steady-state 1D heat flow through n series layers:
 *   R_th = Σ (t_i / (k_i * A))   [thermal resistance in series]
 *   q = ΔT / R_th_total
 *
 * For a typical 4-layer flex (2 signal + 2 plane, 0.3 mm total):
 *   PI layers (k=0.12): R_th ≈ 0.2/(0.12*A) = 1.67/A
 *   Cu layers (k=398):  R_th ≈ 0.07/(398*A) = 1.76e-4/A
 *   Total R_th ≈ 1.67/A  (dominated by PI!)
 *
 * Key insight: The dielectric layers dominate thermal resistance.
 * Adding copper planes dramatically improves heat spreading (in-plane k),
 * but through-thickness conduction is limited by the poor PI thermal
 * conductivity.
 *
 * Reference: Fourier, "Théorie Analytique de la Chaleur", 1822
 *            Incropera & DeWitt, "Fundamentals of Heat and Mass Transfer"
 */
double flex_thermal_conduction_1d(const double *thicknesses_mm,
                                   const double *conductivities_w_mk,
                                   int num_layers,
                                   double area_mm2,
                                   double delta_t_c) {
    if (!thicknesses_mm || !conductivities_w_mk ||
        num_layers <= 0 || area_mm2 <= 0.0 || delta_t_c <= 0.0)
        return 0.0;

    double r_th_total = 0.0;
    double area_m2 = area_mm2 * 1.0e-6;

    for (int i = 0; i < num_layers; i++) {
        if (conductivities_w_mk[i] <= 0.0) continue;
        double t_m = thicknesses_mm[i] * 1.0e-3;
        r_th_total += t_m / (conductivities_w_mk[i] * area_m2);
    }

    if (r_th_total <= 0.0) return 0.0;

    return delta_t_c / r_th_total;
}

/**
 * Knowledge Point: Effective in-plane thermal conductivity (parallel rule).
 *
 * For heat flow parallel to the layers (in-plane):
 *   k_eff_∥ = Σ(k_i * t_i) / Σ t_i
 *
 * This is the "rule of mixtures" for parallel conduction — the high-k
 * copper layers provide low-resistance paths in-plane, making the
 * effective conductivity much higher than through-thickness:
 *
 * For a 4-layer flex with 35 μm Cu and 50 μm PI:
 *   k_eff_∥ = (398*0.035 + 0.12*0.05 + 398*0.035 + 0.12*0.05) / 0.17
 *            ≈ (13.93 + 0.006 + 13.93 + 0.006) / 0.17 ≈ 164 W/m·K
 *
 * This is ~1000× better than through-thickness conduction, meaning
 * heat spreads laterally much faster than it conducts through.
 *
 * Design implication: Use copper pours/planes for thermal management.
 */
double flex_thermal_conductivity_in_plane(const double *conductivities,
                                           const double *thicknesses,
                                           int num_layers) {
    if (!conductivities || !thicknesses || num_layers <= 0)
        return 0.0;

    double sum_kt = 0.0;
    double sum_t = 0.0;

    for (int i = 0; i < num_layers; i++) {
        sum_kt += conductivities[i] * thicknesses[i];
        sum_t  += thicknesses[i];
    }

    if (sum_t <= 0.0) return 0.0;
    return sum_kt / sum_t;
}

/**
 * Knowledge Point: Effective through-thickness thermal conductivity (series rule).
 *
 * For heat flow perpendicular to layers (through-thickness):
 *   k_eff_⊥ = Σ t_i / Σ(t_i / k_i)
 *
 * This is dominated by the LOWEST conductivity layer (the "weakest link"):
 *
 * For the same 4-layer flex:
 *   k_eff_⊥ = 0.17 / (0.035/398 + 0.05/0.12 + 0.035/398 + 0.05/0.12)
 *            = 0.17 / (8.8e-5 + 0.417 + 8.8e-5 + 0.417)
 *            = 0.17 / 0.834 ≈ 0.20 W/m·K
 *
 * The effective through-thickness conductivity is barely higher than
 * the PI alone (0.12) — copper layers contribute almost nothing because
 * their thickness is small and their k is shunted by the series PI.
 *
 * This is why thermal vias (copper-plated holes) are essential for
 * conducting heat through flex stackups.
 */
double flex_thermal_conductivity_through(const double *conductivities,
                                          const double *thicknesses,
                                          int num_layers) {
    if (!conductivities || !thicknesses || num_layers <= 0)
        return 0.0;

    double sum_t = 0.0;
    double sum_t_over_k = 0.0;

    for (int i = 0; i < num_layers; i++) {
        if (conductivities[i] <= 0.0) continue;
        sum_t += thicknesses[i];
        sum_t_over_k += thicknesses[i] / conductivities[i];
    }

    if (sum_t_over_k <= 0.0) return 0.0;
    return sum_t / sum_t_over_k;
}

/* ========================================================================
 * L5: Trace Ampacity — IPC-2152
 * ========================================================================*/

/**
 * Knowledge Point: IPC-2152 trace current capacity.
 *
 * IPC-2152 provides physics-based current-carrying capacity charts:
 *
 *   I = k * ΔT^b1 * A^b2
 *
 * where A = cross-sectional area (μm²), ΔT = temperature rise (°C).
 *
 * Empirical curve-fit constants for IPC-2152 (area in mil²):
 *   Outer layer: k ≈ 0.048, b1 ≈ 0.44, b2 ≈ 0.725
 *   Inner layer: k ≈ 0.024, b1 ≈ 0.44, b2 ≈ 0.725
 *
 * Flex traces have ~20% lower ampacity than equivalent rigid traces
 * due to reduced heat spreading into the thinner substrate.
 *
 * Example: 0.5 mm × 35 μm outer layer flex trace, ΔT=10°C:
 *   A = (500/25.4) × (35/25.4) ≈ 19.7 × 1.38 = 27.1 mil²
 *   I = 0.048 * 10^0.44 * 27.1^0.725 ≈ 2.8 A
 *
 * Same trace on 1.6 mm FR-4: I ≈ 3.3 A (18% higher)
 *
 * Reference: IPC-2152 "Standard for Determining Current-Carrying
 *            Capacity in Printed Board Design", 2009
 *            Brooks, "PCB Trace and Via Currents and Temperatures", 2013
 */
double flex_trace_ampacity_ipc2152(double trace_width_um,
                                    double trace_thickness_um,
                                    double temp_rise_c,
                                    int is_outer_layer) {
    if (trace_width_um <= 0.0 || trace_thickness_um <= 0.0 ||
        temp_rise_c <= 0.0)
        return 0.0;

    /* Area in square mils (1 mil = 25.4 μm) for IPC-2152 compatibility */
    double area_mil2 = (trace_width_um / 25.4) * (trace_thickness_um / 25.4);
    double k = is_outer_layer ? 0.048 : 0.024;

    /* IPC-2152 curve fit: I = k * ΔT^0.44 * A^0.725 (A in mil²) */
    double i = k * pow(temp_rise_c, 0.44) * pow(area_mil2, 0.725);

    return i;
}

/**
 * Knowledge Point: Temperature rise for a given current (inverse ampacity).
 *
 * Given a known current, calculate the expected temperature rise:
 *
 *   ΔT = (I / (k * A^b2))^(1/b1)
 *
 * This is used to verify that existing designs won't overheat
 * under worst-case current conditions.
 *
 * For 2 A through a 200 μm × 18 μm inner flex trace:
 *   A = (200/25.4) × (18/25.4) ≈ 7.87 × 0.709 = 5.58 mil²
 *   ΔT = (2 / (0.024 * 5.58^0.725))^(1/0.44) ≈ 12°C
 *
 * Industry rule: keep ΔT < 10°C for high-reliability, < 20°C for commercial.
 */
double flex_trace_temp_rise(double current_a,
                             double trace_width_um,
                             double trace_thickness_um,
                             int is_outer_layer) {
    if (current_a <= 0.0 || trace_width_um <= 0.0 ||
        trace_thickness_um <= 0.0)
        return 0.0;

    double area_mil2 = (trace_width_um / 25.4) * (trace_thickness_um / 25.4);
    double k = is_outer_layer ? 0.048 : 0.024;

    double dt = pow(current_a / (k * pow(area_mil2, 0.725)), 1.0 / 0.44);

    return dt;
}

/**
 * Knowledge Point: Flex-specific ampacity derating.
 *
 * Flex circuits have reduced heat dissipation compared to rigid PCBs:
 *
 * - Thinner substrate → less lateral heat spreading
 * - PI k=0.12 vs FR-4 k=0.3 in-plane → 2.5× worse spreading
 * - Tighter installation → restricted natural convection
 * - Bend zones → no airflow on inner radius
 *
 * Derating factors:
 *   1-2 flex layers: 0.90 × base ampacity
 *   3-4 flex layers: 0.80 × base ampacity
 *   5+ flex layers:  0.70 × base ampacity
 *   In bend zone:    additional 0.85 × factor
 *
 * For a 4-layer flex trace in a bend zone:
 *   I_flex = I_rigid * 0.80 * 0.85 = I_rigid * 0.68
 *   → 32% derating from rigid PCB values!
 */
double flex_ampacity_derate_flex(double base_ampacity_a,
                                  int flex_layer_count,
                                  int is_in_bend_zone) {
    double derate = 1.0;

    if (flex_layer_count <= 2) {
        derate = 0.90;
    } else if (flex_layer_count <= 4) {
        derate = 0.80;
    } else if (flex_layer_count <= 6) {
        derate = 0.72;
    } else {
        derate = 0.65;
    }

    if (is_in_bend_zone) derate *= 0.85;

    return base_ampacity_a * derate;
}

/* ========================================================================
 * L5: Convection and Thermal Resistance
 * ========================================================================*/

/**
 * Knowledge Point: Convective heat transfer coefficient.
 *
 * For natural convection (vertical plate):
 *   h ≈ 1.42 * (ΔT / L_char)^0.25   [W/m²·K]
 *
 * For forced convection (laminar flow):
 *   h ≈ 3.79 * (v^0.78 / L_char^0.22)  [W/m²·K]
 *
 * where ΔT = surface-to-ambient temperature difference,
 * L_char = characteristic length (height for vertical plate),
 * v = airflow velocity (m/s).
 *
 * Typical values for flex circuits:
 *   Natural convection, still air: h ≈ 5-10 W/m²·K
 *   Forced convection, 1 m/s:     h ≈ 15-25 W/m²·K
 *   Forced convection, 3 m/s:     h ≈ 30-45 W/m²·K
 *
 * The low h in natural convection is a key limitation for flex
 * circuits in sealed enclosures (automotive ECUs, phone hinges).
 *
 * Reference: Incropera & DeWitt, "Fundamentals of Heat Transfer", Ch. 9
 */
double flex_convection_coefficient(double delta_t_c,
                                    double char_length_m,
                                    double airflow_m_per_s) {
    if (airflow_m_per_s <= 0.01 && delta_t_c <= 0.0)
        return 5.0;  /* Default natural convection */

    if (airflow_m_per_s <= 0.01) {
        /* Natural convection */
        if (char_length_m <= 0.0) char_length_m = 0.05;  /* 50 mm default */
        return 1.42 * pow(delta_t_c / char_length_m, 0.25);
    } else {
        /* Forced convection */
        if (char_length_m <= 0.0) char_length_m = 0.05;
        return 3.79 * pow(airflow_m_per_s, 0.78) /
               pow(char_length_m, 0.22);
    }
}

/**
 * Knowledge Point: Board-level thermal resistance to ambient.
 *
 * θ_BA = 1 / (h_conv * A + ε * σ * A * (T_s² + T_a²) * (T_s + T_a))
 *
 * Simplified for small ΔT (radiation ≈ 4εσT³):
 *   θ_BA ≈ 1 / ((h_conv + h_rad) * A)
 *
 * where h_rad ≈ 4 * ε * σ * T_avg³,
 * ε = surface emissivity (PI ≈ 0.8, FR-4 ≈ 0.9, Cu ≈ 0.05).
 *
 * For a 50 × 30 mm flex in natural convection:
 *   A = 3000 mm², h_conv ≈ 8, h_rad ≈ 5
 *   θ_BA = 1 / ((8+5) * 3e-3) ≈ 26 °C/W
 *
 * At 1 W power dissipation: ΔT = 26°C — marginal for 85°C ambient!
 * This is why thermal management is critical for power components on flex.
 *
 * Reference: IPCSM-782 "Surface Mount Design and Land Pattern Standard"
 */
double flex_thermal_resistance_board(double board_area_mm2,
                                      double h_convection,
                                      double board_emissivity) {
    if (board_area_mm2 <= 0.0) return 1.0e6;

    double area_m2 = board_area_mm2 * 1.0e-6;

    /* Radiation heat transfer coefficient (approximate at 300K) */
    double t_avg = 300.0;  /* K, ~27°C */
    double h_rad = 4.0 * board_emissivity * STEFAN_BOLTZMANN *
                   t_avg * t_avg * t_avg;

    double h_total = h_convection + h_rad;
    if (h_total <= 0.0) return 1.0e6;

    return 1.0 / (h_total * area_m2);
}

/* ========================================================================
 * L5: Thermal-Mechanical Coupling
 * ========================================================================*/

/**
 * Knowledge Point: Thermal strain from CTE mismatch.
 *
 * ε_thermal = Δα * ΔT
 *
 * where Δα = |α₁ - α₂| is the CTE difference between bonded materials,
 * ΔT = temperature change from stress-free reference (lamination temp).
 *
 * For PI (α=20) on copper (α=17) from lamination (180°C) to -40°C:
 *   ε = |20-17|e-6 * |180 - (-40)| = 3e-6 * 220 = 6.6e-4 (0.066%)
 *
 * This strain must be accommodated by elastic deformation of the bond.
 * If it exceeds the adhesive's elongation capability, delamination occurs.
 *
 * This is the fundamental input to all CTE mismatch stress calculations.
 */
double flex_thermal_strain(double cte_1, double cte_2, double delta_t_c) {
    double delta_cte = (cte_1 > cte_2) ? (cte_1 - cte_2) : (cte_2 - cte_1);
    double dt = (delta_t_c > 0.0) ? delta_t_c : -delta_t_c;
    return delta_cte * 1.0e-6 * dt;
}

/**
 * Knowledge Point: Critical temperature change for delamination.
 *
 * The temperature change at which the peel stress exceeds the bond
 * strength, causing delamination:
 *
 *   ΔT_crit = σ_peel / (E_eff * Δα)
 *
 * where σ_peel = peel strength of the adhesive bond,
 * E_eff = effective modulus of the constrained layer,
 * Δα = CTE mismatch.
 *
 * For acrylic adhesive (peel = 1.0 N/mm², E_eff = 500 MPa, Δα = 60 ppm/°C):
 *   ΔT_crit = 1.0 / (500 * 60e-6) ≈ 33°C
 *
 * This means an acrylic-bonded flex can only survive a 33°C swing before
 * peel stress exceeds bond strength. For PI adhesive (peel = 2.0 N/mm²):
 *   ΔT_crit = 2.0 / (2500 * 15e-6) ≈ 53°C
 *
 * These numbers explain why adhesiveless construction is preferred for
 * wide temperature range applications (automotive, aerospace).
 */
double flex_critical_temp_delta(double peel_strength_n_per_mm2,
                                 double effective_modulus_mpa,
                                 double cte_diff_ppm_per_c) {
    if (effective_modulus_mpa <= 0.0 || cte_diff_ppm_per_c <= 0.0)
        return 1.0e6;  /* No mismatch → infinite margin */

    return peel_strength_n_per_mm2 /
           (effective_modulus_mpa * cte_diff_ppm_per_c * 1.0e-6);
}

/**
 * Knowledge Point: Thermal time constant (lumped capacitance model).
 *
 * For a body with uniform temperature (Biot number < 0.1):
 *
 *   τ = C_th * R_th = (ρ * c_p * V) / (h * A)
 *
 * where ρ = density, c_p = specific heat, V = volume,
 * h = heat transfer coefficient, A = surface area.
 *
 * For a 50 × 30 × 0.3 mm flex (PI base, ρ=1420 kg/m³, c_p=1090 J/kg·K):
 *   V = 50e-3 * 30e-3 * 0.3e-3 = 4.5e-7 m³
 *   C_th = 1420 * 1090 * 4.5e-7 ≈ 0.70 J/K
 *   R_th = 26 °C/W (from earlier example)
 *   τ = 0.70 * 26 ≈ 18 seconds
 *
 * The flex reaches thermal equilibrium in ~3τ ≈ 54 seconds.
 * Fast thermal response is good for quick cooldown but bad for
 * thermal buffering against transient loads.
 *
 * Reference: Incropera, Ch. 5 "Transient Conduction"
 */
double flex_thermal_time_constant(double board_volume_mm3,
                                   double board_density_kg_m3,
                                   double specific_heat_j_kgk,
                                   double thermal_resistance_cw) {
    if (thermal_resistance_cw <= 0.0) return 0.0;

    double volume_m3 = board_volume_mm3 * 1.0e-9;
    double c_th = board_density_kg_m3 * specific_heat_j_kgk * volume_m3;

    return c_th * thermal_resistance_cw;
}

/**
 * Knowledge Point: Junction temperature estimation for components on flex.
 *
 * T_j = T_amb + P * (θ_JC + θ_CA)
 *
 * For components mounted on flex, θ_CA is usually higher than on rigid
 * PCB because:
 * - No thick copper planes for heat spreading
 * - Lower thermal mass for buffering
 * - Thinner board limits lateral conduction
 *
 * Typical θ_CA for flex: 40-80 °C/W (vs 20-30 for 4-layer FR-4)
 *
 * For a 0.5 W component with θ_JC=20 °C/W on flex at 85°C ambient:
 *   T_j = 85 + 0.5 * (20 + 60) = 125°C — at commercial limit!
 *
 * Design mitigation: thermal vias under pad, copper stiffener as heat
 * spreader, or aluminum stiffener as heatsink.
 *
 * Reference: JEDEC JESD51 "Methodology for Thermal Measurement"
 */
double flex_junction_temperature(double ambient_temp_c,
                                  double power_dissipation_w,
                                  double theta_jc_cw,
                                  double theta_ca_cw) {
    return ambient_temp_c + power_dissipation_w * (theta_jc_cw + theta_ca_cw);
}

/* ========================================================================
 * L5: Complete Thermal Analysis
 * ========================================================================*/

/**
 * Knowledge Point: Unified thermal analysis for flex PCB design.
 *
 * Combines trace ampacity, thermal resistance, and temperature
 * prediction into a single analysis call. This is the thermal
 * equivalent of SPICE simulation for flex circuits.
 *
 * The analysis sequence:
 * 1. Calculate trace DC resistance → I²R power dissipation
 * 2. Determine max safe current per IPC-2152
 * 3. Estimate temperature rise and junction temperature
 * 4. Compute CTE mismatch stress for reliability prediction
 * 5. Calculate thermal time constant for transient analysis
 */
int flex_thermal_analyze(const flex_thermal_config_t *config,
                          flex_thermal_result_t *result) {
    if (!config || !result) return -1;
    memset(result, 0, sizeof(flex_thermal_result_t));

    /* Trace DC resistance */
    double area_m2 = config->copper_trace_width_um * 1.0e-6 *
                     config->copper_trace_thickness_um * 1.0e-6;
    if (area_m2 > 0.0) {
        double length_m = config->trace_length_mm * 1.0e-3;
        result->trace_resistance_ohm = RHO_CU * length_m / area_m2;
    }

    /* Safe current per IPC-2152 */
    result->max_trace_current_a = flex_trace_ampacity_ipc2152(
        config->copper_trace_width_um,
        config->copper_trace_thickness_um,
        10.0,  /* 10°C rise target */
        config->is_outer_layer);

    /* Temperature rise for a unit test current */
    if (result->trace_resistance_ohm > 0.0) {
        double test_current = 1.0;  /* 1A test */
        double power = test_current * test_current * result->trace_resistance_ohm;
        result->power_dissipation_w = power;

        double dt_trace = flex_trace_temp_rise(
            test_current,
            config->copper_trace_width_um,
            config->copper_trace_thickness_um,
            config->is_outer_layer);
        result->trace_temperature_rise_c = dt_trace;
    }

    /* Convection coefficient */
    double char_length = (config->board_length_mm > config->board_width_mm) ?
        config->board_length_mm * 1.0e-3 : config->board_width_mm * 1.0e-3;
    double h = flex_convection_coefficient(
        (config->max_operating_temp_c - config->ambient_temp_c),
        char_length * 1.0e-3, config->airflow_m_per_s);

    /* Board thermal resistance */
    double board_area = config->board_width_mm * config->board_length_mm;
    result->thermal_resistance_board = flex_thermal_resistance_board(
        board_area, h, 0.85);  /* 0.85 emissivity for PI */

    /* Junction temperature estimate */
    result->junction_temp_est_c = config->ambient_temp_c +
        result->power_dissipation_w * result->thermal_resistance_board;

    /* CTE mismatch stress (PI vs Cu) */
    double pi_cte = 20.0;
    double cu_cte = 17.0;
    double delta_t = config->max_operating_temp_c - config->ambient_temp_c;
    double strain = flex_thermal_strain(pi_cte, cu_cte, delta_t);
    /* σ = E_cu * ε, E_cu ≈ 117 GPa */
    result->cte_mismatch_stress_mpa = 117000.0 * strain;

    /* Thermal time constant */
    double volume = config->board_width_mm * config->board_length_mm *
                    config->board_thickness_mm;
    result->thermal_time_constant_s = flex_thermal_time_constant(
        volume, 1420.0, 1090.0,  /* PI properties */
        result->thermal_resistance_board);

    return 0;
}
