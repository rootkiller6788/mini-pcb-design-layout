/**
 * @file flex_bend.c
 * @brief Bend Mechanics Implementation — IPC-2223, Beam Theory, Fatigue Life
 *
 * Implements the complete bend analysis pipeline for flex/rigid-flex PCBs.
 * Core algorithms: minimum bend radius (IPC-2223 and beam theory), strain
 * profile, springback, and fatigue life prediction (Coffin-Manson & IPC-TM-650).
 *
 * @module mini-flex-rigid-flex-design
 */

#include "flex_bend.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Physical constants */
#define BOLTZMANN_EV 8.617333262e-5  /**< Boltzmann constant in eV/K */

/* ========================================================================
 * L4: IPC-2223 Minimum Bend Radius
 * ========================================================================*/

/**
 * Knowledge Point: IPC-2223 minimum bend radius formula.
 *
 * The IPC-2223 standard defines the minimum bend radius to prevent
 * copper cracking and dielectric damage during forming:
 *
 *   R_min = k * t
 *
 * where t = total flex thickness, and k depends on layer count:
 *
 *   1-layer (single-sided): k = 6
 *   2-layer (double-sided): k = 12
 *   Multi-layer (>2):       k = 20 (static), k = 30 (dynamic)
 *
 * For copper elongation ε_cu = 16% (RA copper):
 *   R_min = t * (1/ε_cu - 1) / 2
 *   With ε_cu = 0.16: R_min ≈ 2.6 * t (theoretical minimum)
 *
 * IPC's factor of 6 (single-sided) includes safety margin for:
 * - Manufacturing tolerances
 * - Copper work hardening during bending
 * - Stress concentration at edges
 *
 * Reference: IPC-2223C, Section 5.2.4 "Bend Radius Requirements"
 */
double flex_min_bend_radius_ipc2223(double thickness_mm,
                                     int num_copper_layers,
                                     double copper_el_limit_percent) {
    if (thickness_mm <= 0.0 || num_copper_layers <= 0)
        return 0.0;

    /* Theoretical minimum from strain limit: R_min = t * (1/ε - 1) / 2 */
    double eps = copper_el_limit_percent / 100.0;
    if (eps <= 0.0) eps = 0.01;  /* 1% minimum */
    double theoretical_min = thickness_mm * (1.0 / eps - 1.0) / 2.0;

    /* IPC-2223 practical k-factors (include manufacturing margins) */
    double k;
    if (num_copper_layers == 1) {
        k = 6.0;
    } else if (num_copper_layers == 2) {
        k = 12.0;
    } else if (num_copper_layers <= 4) {
        k = 20.0;
    } else {
        k = 25.0;  /* Conservative for ≥ 5 layers */
    }

    double ipc_min = k * thickness_mm;

    /* Return the larger of theoretical and IPC recommendation */
    return (theoretical_min > ipc_min) ? theoretical_min : ipc_min;
}

/**
 * Knowledge Point: First-principles beam theory for minimum bend radius.
 *
 * Unlike the empirical IPC k-factor method, beam theory calculates
 * R_min directly from material properties:
 *
 * At the outer fiber, strain ε = y / R (pure bending)
 * Stress σ = E * ε = E * y / R
 * Failure when σ ≥ σ_allowable, so R_min = E * y_max / σ_allowable
 *
 * For a composite beam (multilayer flex), the effective E and y_max
 * must account for all layers. The outer copper experiences the
 * highest strain and is usually the limiting factor.
 *
 * This method is preferred when:
 * - Non-standard materials are used (not in IPC k-factor tables)
 * - Design optimization requires precise analysis
 * - High-reliability applications (aerospace, medical)
 *
 * Reference: Timoshenko & Goodier, "Theory of Elasticity", 3rd Ed., Ch. 4
 */
double flex_min_bend_radius_beam_theory(const flex_bend_params_t *params) {
    if (!params || params->total_thickness_mm <= 0.0) return 0.0;

    /* For composite beam, approximate with outer copper properties */
    double effective_e = params->youngs_modulus_copper_mpa;  /* Default Cu */
    if (effective_e <= 0.0) effective_e = 117000.0;  /* MPa */

    /* Allowable stress from elongation limit */
    double eps_allowable = params->copper_elongation_limit_percent / 100.0;
    if (eps_allowable <= 0.0) eps_allowable = 0.01;
    double sigma_allowable = effective_e * eps_allowable;

    /* Distance from neutral axis to outer copper surface */
    /* Approximate: neutral axis at center for symmetric stackup */
    double na_offset = params->total_thickness_mm / 2.0;
    double y_max = params->total_thickness_mm - na_offset;

    /* R_min = E * y_max / σ_allowable */
    double r_min = effective_e * y_max / sigma_allowable;

    /* Apply safety factor of 1.5 (standard engineering practice) */
    return r_min * 1.5;
}

/* ========================================================================
 * L5: Strain and Stress Analysis
 * ========================================================================*/

/**
 * Knowledge Point: Maximum copper strain during bending.
 *
 * In pure bending, strain varies linearly with distance from the neutral
 * axis (Bernoulli-Euler beam theory):
 *
 *   ε(y) = (y - y_NA) / R
 *
 * The maximum strain occurs at the outer surface:
 *   ε_max = y_max / R   (where y_max = distance from NA to outer copper)
 *
 * For RA copper: allowable strain ≈ 16% (ductile)
 * For ED copper: allowable strain ≈ 5-8% (less ductile)
 *
 * A safety factor of 2-3× is recommended (e.g., keep ε_max < 5% even
 * if the material can handle 16%).
 *
 * Reference: Beer & Johnston, "Mechanics of Materials", Ch. 4
 */
double flex_copper_strain_percent(double bend_radius_mm,
                                   double neutral_axis_offset_mm,
                                   double outer_copper_distance_mm) {
    if (bend_radius_mm <= 0.0) return 1.0e9;  /* Infinite strain at zero radius */

    /* Distance from neutral axis to outermost fiber */
    double y_max = outer_copper_distance_mm - neutral_axis_offset_mm;
    if (y_max < 0.0) y_max = -y_max;

    /* ε = y / R, return as percentage */
    return (y_max / bend_radius_mm) * 100.0;
}

/**
 * Knowledge Point: Strain profile through the flex thickness.
 *
 * Maps the strain distribution across all layers, revealing:
 * - Which layer experiences the highest strain
 * - Whether copper layers are in tension or compression
 * - If the neutral axis shift causes unexpected strain peaks
 *
 * The linear strain distribution ε(y) = (y - y_NA) / R is sampled
 * at n_points equally spaced through the thickness.
 *
 * This profile is essential for identifying which specific copper
 * layer is the fatigue life limiter in multilayer flex designs.
 */
void flex_strain_profile(double bend_radius_mm,
                          double neutral_offset_mm,
                          double thickness_mm,
                          double *strain_profile,
                          int n_points) {
    if (!strain_profile || n_points <= 0 || bend_radius_mm <= 0.0) return;

    for (int i = 0; i < n_points; i++) {
        double y = (thickness_mm * i) / (double)(n_points - 1);
        double strain = (y - neutral_offset_mm) / bend_radius_mm;
        strain_profile[i] = strain * 100.0;  /* Convert to % */
    }
}

/**
 * Knowledge Point: Interfacial shear stress at Cu-dielectric boundary.
 *
 * When a flex circuit bends, the copper and dielectric layers want to
 * slide relative to each other (different neutral axes). The adhesive
 * bond resists this sliding, creating interfacial shear stress:
 *
 *   τ = (V * Q) / (I * b)
 *
 * where V = shear force from bending moment gradient,
 * Q = first moment of area of the copper layer about NA,
 * I = total moment of inertia, b = width.
 *
 * Simplified for uniform bending (no V gradient):
 *   τ ≈ E_cu * ε_cu * (t_cu / L_bond)
 *
 * High interfacial shear → delamination, especially at elevated
 * temperature where adhesive strength degrades.
 *
 * Reference: Suhir, "Stresses in Bi-Metal Thermostats", JAM, 1986
 */
double flex_interfacial_shear_stress(double bend_radius_mm,
                                      double copper_thickness_um,
                                      double dielectric_thickness_um,
                                      double copper_modulus_mpa,
                                      double dielectric_modulus_mpa) {
    (void)dielectric_modulus_mpa;  /* Reserved for future composite stiffness model */
    if (bend_radius_mm <= 0.0 || dielectric_thickness_um <= 0.0)
        return 0.0;

    /* Strain at the interface */
    double interface_strain = (dielectric_thickness_um / 2000.0) / bend_radius_mm;

    /* Stress in copper at interface */
    double sigma_cu = copper_modulus_mpa * interface_strain;

    /* Shear stress approximation: σ_cu * (t_cu / bond_length) */
    double bond_length = dielectric_thickness_um / 1000.0;  /* mm */
    double tau = sigma_cu * (copper_thickness_um / 1000.0) / bond_length;

    /* Clamp to physically reasonable range */
    if (tau < 0.0) tau = -tau;
    if (tau > 1000.0) tau = 1000.0;  /* Exceeds any adhesive strength */

    return tau;
}

/**
 * Knowledge Point: Bending moment required for a given radius.
 *
 * M = EI / R  (beam curvature equation)
 *
 * where EI = flexural rigidity D, R = bend radius.
 *
 * The bending moment determines:
 * - Actuator sizing (how much force is needed to bend the flex)
 * - Connector insertion force requirements
 * - Springback magnitude (proportional to M)
 */
double flex_bending_moment(double flexural_rigidity_n_mm2,
                            double bend_radius_mm) {
    if (bend_radius_mm <= 0.0) return 0.0;
    return flexural_rigidity_n_mm2 / bend_radius_mm;
}

/* ========================================================================
 * L5: Dynamic Flex Life Prediction
 * ========================================================================*/

/**
 * Knowledge Point: Coffin-Manson fatigue model for copper.
 *
 * The Coffin-Manson relation is the fundamental low-cycle fatigue model:
 *
 *   N_f = (ε_f / ε_a)^c
 *
 * where ε_f = fatigue ductility coefficient, ε_a = cyclic strain amplitude,
 * c = fatigue ductility exponent.
 *
 * For RA copper: ε_f ≈ 0.16, c ≈ -0.5
 * For ED copper: ε_f ≈ 0.05, c ≈ -0.6
 *
 * Example: RA copper at 1% cyclic strain → N_f = (0.16/0.01)^0.5 ≈ 4 cycles?
 * Wait — this is in the plastic strain range. For elastic strain:
 *   ε_a_elastic = σ_a / E → Basquin's equation N_f = (σ_f / σ_a)^b
 *
 * For combined elastic-plastic (Morrow):
 *   ε_a = (σ_f/E)*(2N_f)^b + ε_f*(2N_f)^c
 *
 * Here we use the simplified strain-life form common in flex design.
 *
 * Reference: Coffin, Trans. ASME, 1954; Manson, NACA TN 2933, 1953
 *            Engelmaier, "Fatigue of Electronic Materials", 1982
 */
double flex_cycles_to_failure_coffin_manson(double cyclic_strain_percent,
                                              flex_copper_type_t copper_type) {
    double eps_a = cyclic_strain_percent / 100.0;
    if (eps_a <= 0.0) return 1.0e12;  /* Zero strain → infinite life */

    double eps_f;  /* Fatigue ductility coefficient */
    double c_exp;  /* Fatigue ductility exponent */

    switch (copper_type) {
    case FLEX_COPPER_RA:
    case FLEX_COPPER_RA_LP:
        eps_f = 0.16;   /* 16% — RA copper is ductile */
        c_exp = -0.5;
        break;
    case FLEX_COPPER_ED:
    case FLEX_COPPER_ED_LP:
        eps_f = 0.05;   /* 5% — ED copper is less ductile */
        c_exp = -0.6;
        break;
    default:
        eps_f = 0.10;
        c_exp = -0.55;
        break;
    }

    /* Check if strain exceeds fatigue ductility → immediate failure */
    if (eps_a >= eps_f) return 0.25;  /* Quarter cycle to failure */

    /* N_f = (ε_f / ε_a)^(-1/c) = (ε_f / ε_a)^{(-1/c)} */
    double ratio = eps_f / eps_a;
    double exponent = -1.0 / c_exp;  /* c is negative, so -1/c is positive */
    double n_f = pow(ratio, exponent);

    /* Clamp to practical range */
    if (n_f > 1.0e9) n_f = 1.0e9;
    if (n_f < 1.0) n_f = 1.0;

    return n_f;
}

/**
 * Knowledge Point: IPC-TM-650 flexural fatigue model.
 *
 * The IPC-TM-650 Method 2.4.3 empirical model:
 *   N_f = A * (R / t)^B
 *
 * where A and B are determined by fitting to test data.
 *
 * For RA copper: A ≈ 2.5e4, B ≈ 3.0
 * For ED copper: A ≈ 5.0e3, B ≈ 2.5
 *
 * The model captures the key scaling: thicker flex (larger t)
 * or tighter bend (smaller R) → shorter life.
 *
 * This is simpler than Coffin-Manson and commonly used for
 * initial design screening in industry.
 *
 * Reference: IPC-TM-650, Method 2.4.3 "Flexural Fatigue, Flexible
 *            Printed Wiring", Section 2.4.3.1
 */
double flex_cycles_ipc_tm650(double bend_radius_mm,
                              double total_thickness_mm,
                              flex_copper_type_t copper_type) {
    if (total_thickness_mm <= 0.0 || bend_radius_mm <= 0.0)
        return 0.0;

    double ratio = bend_radius_mm / total_thickness_mm;
    double a, b;

    switch (copper_type) {
    case FLEX_COPPER_RA:
    case FLEX_COPPER_RA_LP:
        a = 25000.0;
        b = 3.0;
        break;
    case FLEX_COPPER_ED:
    case FLEX_COPPER_ED_LP:
        a = 5000.0;
        b = 2.5;
        break;
    default:
        a = 10000.0;
        b = 2.8;
        break;
    }

    double n_f = a * pow(ratio, b);
    return (n_f > 1.0e9) ? 1.0e9 : n_f;
}

/**
 * Knowledge Point: Arrhenius temperature derating of fatigue life.
 *
 * Fatigue is thermally activated. Higher temperature accelerates:
 * - Dislocation motion (easier plastic deformation)
 * - Grain boundary sliding
 * - Oxidation/corrosion at crack tips
 *
 * The Arrhenius model:
 *   Life(T) = Life(25°C) * exp((Ea/k) * (1/T - 1/298))
 *
 * where Ea = activation energy (~0.8 eV for Cu fatigue),
 * k = Boltzmann constant (8.617e-5 eV/K), T in Kelvin.
 *
 * At 85°C: Life/Life_25C = exp(0.8/8.617e-5 * (1/358 - 1/298)) = 0.15
 * Fatigue life reduced to 15% of room temperature value!
 *
 * This is why temperature is critical for dynamic flex in automotive
 * (under-hood) and aerospace (engine bay) applications.
 */
double flex_cycles_temperature_derate(double cycles_at_25c,
                                       double operating_temp_c,
                                       double activation_energy_ev) {
    double t_op = operating_temp_c + 273.15;  /* Kelvin */
    double t_ref = 298.15;  /* 25°C in Kelvin */

    if (t_op <= 0.0) return cycles_at_25c;

    double factor = activation_energy_ev / BOLTZMANN_EV;
    double temp_term = (1.0 / t_op) - (1.0 / t_ref);
    double derate = exp(factor * temp_term);

    return cycles_at_25c * derate;
}

/* ========================================================================
 * L5: Springback Analysis
 * ========================================================================*/

/**
 * Knowledge Point: Springback after bending.
 *
 * When a flex circuit is bent, part of the deformation is elastic and
 * recovers when the forming force is removed. The springback angle:
 *
 *   Δθ = θ * (3 * σy * R) / (E * t)    [simplified]
 *
 * where σy = yield strength of the composite.
 *
 * Springback is larger for:
 * - Higher strength materials (more elastic recovery)
 * - Larger bend radii (less plastic deformation)
 * - Thinner materials
 *
 * For typical PI flex: Springback ≈ 2-5° for a 90° bend.
 * Designers should overbend by the springback amount to achieve
 * the target final angle.
 *
 * Reference: Kalpakjian & Schmid, "Manufacturing Engineering and
 *            Technology", Ch. 16 "Sheet Metal Forming"
 */
double flex_springback_angle(double bend_angle_deg,
                              double bend_radius_mm,
                              double thickness_mm,
                              double yield_strength_mpa,
                              double elastic_modulus_mpa) {
    if (thickness_mm <= 0.0 || elastic_modulus_mpa <= 0.0 || bend_radius_mm <= 0.0)
        return 0.0;

    /* Springback ratio: how much of the bend recovers */
    double ratio = (3.0 * yield_strength_mpa * bend_radius_mm) /
                   (elastic_modulus_mpa * thickness_mm);

    /* Springback angle */
    double springback = bend_angle_deg * ratio;

    /* Clamp to sensible range */
    if (springback > bend_angle_deg) springback = bend_angle_deg * 0.5;
    if (springback < 0.0) springback = 0.0;

    return springback;
}

/* ========================================================================
 * L5: Copper Grain Orientation Optimization
 * ========================================================================*/

/**
 * Knowledge Point: Copper grain orientation effect on flex life.
 *
 * RA copper has elongated grains in the rolling direction. Bending
 * perpendicular to the grain direction:
 * - Puts grain boundaries under tension (weaker)
 * - BUT: the elongated grains act as crack arrestors
 * - Net effect: ~15% lower stress and ~2× longer fatigue life
 *
 * Best practice: Orient the copper foil so that the bend axis
 * is PARALLEL to the rolling direction (bend perpendicular to grain).
 *
 * For ED copper: grain orientation is less significant (columnar, not
 * elongated), so orientation matters less.
 */
flex_bend_grain_orientation_t flex_optimal_grain_orientation(
    double bend_direction_deg) {
    /* Normalize to [0, 360) */
    while (bend_direction_deg < 0.0) bend_direction_deg += 360.0;
    while (bend_direction_deg >= 360.0) bend_direction_deg -= 360.0;

    /* For RA copper: optimal is bend axis parallel to grain (0°) */
    double diff_parallel = (bend_direction_deg < 180.0) ?
        bend_direction_deg : (360.0 - bend_direction_deg);
    double diff_perp = (bend_direction_deg < 90.0) ?
        (90.0 - bend_direction_deg) : (bend_direction_deg < 270.0) ?
        (bend_direction_deg - 90.0) : (360.0 - bend_direction_deg + 90.0);

    if (diff_parallel <= 22.5 || diff_parallel >= 337.5) {
        return FLEX_BEND_PARALLEL_GRAIN;
    } else if (diff_perp <= 22.5) {
        return FLEX_BEND_PERPENDICULAR_GRAIN;
    } else {
        return FLEX_BEND_45DEG_GRAIN;
    }
}

/* ========================================================================
 * L5: Complete Bend Analysis
 * ========================================================================*/

/**
 * Knowledge Point: Unified bend analysis pipeline.
 *
 * This function orchestrates all bend mechanics calculations to produce
 * a complete reliability assessment. It determines:
 *
 * 1. IPC-2223 compliance (pass/fail)
 * 2. Safety factors (bend radius and strain)
 * 3. Expected fatigue life (cycles to failure)
 * 4. Failure mode identification
 *
 * The failure mode classification follows IPC-6013:
 * - Mode 1 (copper_crack): Excessive tensile strain in copper
 * - Mode 2 (delamination): Interfacial shear exceeding bond strength
 * - Mode 3 (buckling): Compressive instability in thin layers
 */
int flex_bend_analyze(const flex_bend_params_t *params,
                       flex_bend_result_t *result) {
    if (!params || !result) return -1;
    memset(result, 0, sizeof(flex_bend_result_t));

    /* Calculate layers with copper */
    int cu_layers = 0;
    double cu_fatigue_el = params->copper_elongation_limit_percent;
    if (cu_fatigue_el <= 0.0) cu_fatigue_el = 16.0;
    if (params->copper_thickness_total_um > 0.0) cu_layers = params->num_layers;

    /* Minimum bend radius */
    result->min_bend_radius_mm = flex_min_bend_radius_ipc2223(
        params->total_thickness_mm, cu_layers, cu_fatigue_el);

    result->actual_bend_radius_mm = params->bend_radius_mm;
    if (result->min_bend_radius_mm > 0.0) {
        result->safety_factor = params->bend_radius_mm /
                                result->min_bend_radius_mm;
    } else {
        result->safety_factor = 1.0;
    }

    /* Neutral axis (simplified: center for symmetric) */
    result->neutral_axis_offset_mm = params->total_thickness_mm / 2.0;

    /* Maximum copper strain */
    result->max_copper_strain_percent = flex_copper_strain_percent(
        params->bend_radius_mm,
        result->neutral_axis_offset_mm,
        params->total_thickness_mm);

    /* Strain safety factor */
    if (result->max_copper_strain_percent > 0.0) {
        result->strain_safety_factor = cu_fatigue_el /
                                       result->max_copper_strain_percent;
    } else {
        result->strain_safety_factor = 99.0;
    }

    /* IPC-2223 compliance */
    result->is_compliant_ipc2223 = (result->safety_factor >= 1.0) ? 1 : 0;

    /* Fatigue life */
    if (params->is_dynamic) {
        double cm_life = flex_cycles_to_failure_coffin_manson(
            result->max_copper_strain_percent, FLEX_COPPER_RA);
        double ipc_life = flex_cycles_ipc_tm650(
            params->bend_radius_mm, params->total_thickness_mm, FLEX_COPPER_RA);
        result->estimated_cycles_to_failure = (cm_life < ipc_life) ?
                                               cm_life : ipc_life;
    } else {
        result->estimated_cycles_to_failure = 1.0e9;  /* Static: infinite */
    }

    /* Failure mode classification */
    result->failure_mode = 0;
    if (result->max_copper_strain_percent > cu_fatigue_el) {
        result->failure_mode = 1;  /* Copper cracking */
        snprintf(result->failure_description, sizeof(result->failure_description),
            "Copper strain %.2f%% exceeds allowable %.2f%% — cracking expected",
            result->max_copper_strain_percent, cu_fatigue_el);
    } else if (result->safety_factor < 1.0) {
        result->failure_mode = 1;
        snprintf(result->failure_description, sizeof(result->failure_description),
            "Bend radius %.3f mm below IPC-2223 minimum %.3f mm",
            params->bend_radius_mm, result->min_bend_radius_mm);
    } else {
        snprintf(result->failure_description, sizeof(result->failure_description),
            "Design passes IPC-2223 bend radius requirement (SF=%.1f×)",
            result->safety_factor);
    }

    /* Bend force */
    double d_flex = params->youngs_modulus_copper_mpa *
                    params->total_thickness_mm * params->total_thickness_mm *
                    params->total_thickness_mm / 12.0;
    result->bend_force_n_per_mm = d_flex / params->bend_radius_mm;

    /* Springback */
    result->springback_angle_deg = flex_springback_angle(
        params->bend_angle_deg, params->bend_radius_mm,
        params->total_thickness_mm, 200.0 /* yield */,
        params->youngs_modulus_copper_mpa);

    return 0;
}
