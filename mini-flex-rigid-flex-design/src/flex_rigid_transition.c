/**
 * @file flex_rigid_transition.c
 * @brief Rigid-to-Flex Transition Zone Design Implementation
 *
 * Implements the complete transition zone analysis pipeline for rigid-flex
 * PCBs. Covers interfacial stress analysis (shear and peel), anchor tab
 * sizing, tear-stop design, impedance continuity, and thermal fatigue
 * life prediction.
 *
 * The transition zone is the single most critical reliability region in
 * rigid-flex design — most field failures originate here.
 *
 * @module mini-flex-rigid-flex-design
 */

#include "flex_rigid_transition.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ========================================================================
 * L4: Interfacial Stress Analysis — Fundamental Physics
 * ========================================================================*/

/**
 * Knowledge Point: Interfacial shear stress at rigid-flex boundary.
 *
 * When a rigid-flex PCB undergoes temperature changes, the CTE mismatch
 * between FR-4 (α ≈ 14 ppm/°C) and polyimide (α ≈ 20 ppm/°C) creates
 * shear stress at the bonded interface. This is the primary driver of
 * delamination failures.
 *
 * The Suhir model for a bimaterial interface:
 *
 *   τ_max = (Δα * ΔT * E_eff) * (β*L/2) / sinh(β*L/2)
 *
 * where β = √(G_a / (E * t * h_a)) is the shear lag parameter:
 *   - G_a = adhesive shear modulus
 *   - E = effective in-plane modulus
 *   - t = layer thickness
 *   - h_a = adhesive thickness
 *
 * Key physical insight: Longer transition zones (larger L) reduce τ_max
 * by distributing the CTE mismatch strain over a larger interface area.
 *
 * For typical FR-4/PI transition (Δα=6 ppm/°C, ΔT=200°C, L=2 mm):
 *   τ_max ≈ 8-15 MPa — near the limit of acrylic adhesives
 *   τ_max ≈ 3-6 MPa  — well within PI adhesive capability
 *
 * Reference: Suhir, "Stresses in Bi-Metal Thermostats", ASME J. Applied
 *            Mechanics, Vol. 53, No. 3, pp. 657-660, 1986
 */
double flex_transition_shear_stress(double delta_cte_ppm,
                                     double delta_temp_c,
                                     double effective_modulus_mpa,
                                     double transition_length_mm,
                                     double adhesive_shear_modulus_mpa,
                                     double layer_thickness_mm,
                                     double adhesive_thickness_mm) {
    if (delta_temp_c <= 0.0 || transition_length_mm <= 0.0 ||
        adhesive_thickness_mm <= 0.0 || layer_thickness_mm <= 0.0)
        return 0.0;

    /* CTE mismatch strain: dimensionless */
    double thermal_strain = delta_cte_ppm * 1.0e-6 * delta_temp_c;

    /* Shear lag parameter β = √(G_a / (E * t * h_a)) */
    double beta_sq = adhesive_shear_modulus_mpa /
                     (effective_modulus_mpa * layer_thickness_mm *
                      adhesive_thickness_mm);
    if (beta_sq <= 0.0) return 0.0;
    double beta = sqrt(beta_sq);

    /* Suhir's peak shear stress formula */
    double half_length = transition_length_mm / 2.0;
    double beta_l2 = beta * half_length;
    double tau_max = (effective_modulus_mpa * thermal_strain * beta_l2) /
                     sinh(beta_l2);

    /* Clamp to physical range */
    if (tau_max < 0.0) tau_max = 0.0;
    if (tau_max > 1000.0) tau_max = 1000.0;

    return tau_max;
}

/**
 * Knowledge Point: Peel (normal) stress at transition boundary.
 *
 * While shear stress causes sliding delamination, peel stress (tensile
 * stress normal to the interface) causes the layers to separate
 * perpendicularly. Peel stress peaks at the free edge:
 *
 *   σ_peel(x) = σ_0 * exp(-λ*x) * cos(λ*x)
 *
 * Simplified worst-case estimation for design:
 *   σ_peel_max ≈ τ_max * (t_layer / L_transition)^0.5
 *
 * Peel strength of typical flex adhesives:
 *   Acrylic:     0.5-1.2 N/mm
 *   Epoxy:       0.8-1.5 N/mm
 *   PI Adhesive: 1.0-2.0 N/mm
 *
 * Peel is the dominant failure mode when L_transition < 2 × t_layer.
 *
 * Reference: Williams, "Stress Singularities Resulting from Various
 *            Boundary Conditions in Angular Corners of Plates in
 *            Extension", J. Applied Mechanics, 1952
 */
double flex_transition_peel_stress(double shear_stress_mpa,
                                    double transition_length_mm,
                                    double layer_thickness_mm) {
    if (transition_length_mm <= 0.0 || layer_thickness_mm <= 0.0)
        return 0.0;

    /* Peel stress scales with shear stress and thickness/length ratio */
    double aspect = layer_thickness_mm / transition_length_mm;
    double peel = shear_stress_mpa * sqrt(aspect);

    /* Empirical correction factor for edge effects */
    peel *= 1.5;

    return peel;
}

/**
 * Knowledge Point: Stress concentration at rigid-flex transition corner.
 *
 * The geometric discontinuity at the transition boundary (rigid section
 * ending abruptly) creates a stress concentration. The stress concentration
 * factor K_t multiplies the nominal stress:
 *
 *   σ_max = K_t * σ_nominal
 *
 * For a sharp 90° internal corner: K_t ≈ 3-5 (severe)
 * With radius r (filleted transition): K_t ≈ 1.5-2.5
 * With tapered transition:            K_t ≈ 1.2-1.8
 *
 * Recommended minimum corner radius at transition: ≥ 0.5 mm for
 * standard reliability, ≥ 1.0 mm for aerospace.
 *
 * Reference: Peterson, "Stress Concentration Factors", 4th Ed., Ch. 4
 *            (Notched plates and stepped bars under tension)
 */
double flex_transition_stress_concentration(double corner_radius_mm,
                                              double rigid_thickness_mm,
                                              double flex_thickness_mm) {
    if (rigid_thickness_mm <= 0.0) return 1.0;

    /* Step height ratio */
    double step_ratio;
    if (flex_thickness_mm <= 0.0) {
        step_ratio = 2.0;  /* Large step */
    } else if (rigid_thickness_mm > flex_thickness_mm) {
        step_ratio = (rigid_thickness_mm - flex_thickness_mm) /
                     flex_thickness_mm;
    } else {
        step_ratio = 0.0;
    }

    /* r/t ratio (radius to rigid thickness) */
    double r_over_t = corner_radius_mm / rigid_thickness_mm;

    /* Base K_t for sharp step (no radius) */
    double kt_base;
    if (step_ratio < 0.1) {
        kt_base = 1.0;
    } else if (step_ratio < 0.5) {
        kt_base = 1.5 + 2.0 * step_ratio;
    } else if (step_ratio < 2.0) {
        kt_base = 2.5 + step_ratio;
    } else {
        kt_base = 5.0;
    }

    /* Radius mitigation: r/t above 0.05 starts reducing K_t */
    if (r_over_t <= 0.0) {
        return kt_base;  /* Sharp corner — worst case */
    } else if (r_over_t < 0.2) {
        double reduction = 1.0 - 2.0 * r_over_t;
        return 1.0 + (kt_base - 1.0) * reduction;
    } else {
        /* Well-radiused transition — minimum K_t */
        return 1.0 + (kt_base - 1.0) * 0.4;
    }
}

/* ========================================================================
 * L5: Anchor Tab Design
 * ========================================================================*/

/**
 * Knowledge Point: Anchor tab length calculation.
 *
 * Anchor tabs are rectangular extensions of the flex circuit that extend
 * into the rigid section, providing mechanical anchoring against peel
 * forces. They function like "rebar" — the flex layers are embedded into
 * the rigid laminate to prevent separation.
 *
 * Required anchor tab length:
 *   L_anchor = F_thermal / (τ_bond * w_tab * n_tabs)
 *
 * where F_thermal = E_rigid * A_rigid * Δα * ΔT
 *
 * IPC-2223 minimum: L_anchor ≥ 1.0 mm for ≤ 4 rigid layers
 * IPC-2223 minimum: L_anchor ≥ 1.5 mm for > 4 rigid layers
 *
 * Key design insight: Anchor tabs work by increasing the bonded
 * interface area parallel to the peel direction, converting peel
 * stress into shear stress where adhesives are stronger.
 *
 * Reference: IPC-2223 §8.4 "Anchor Tab Design"
 */
double flex_anchor_tab_length(double delta_cte_ppm,
                               double delta_temp_c,
                               double rigid_modulus_mpa,
                               double rigid_area_mm2,
                               double tab_width_mm,
                               int tab_count,
                               double adhesive_shear_strength_mpa) {
    if (tab_width_mm <= 0.0 || tab_count <= 0 ||
        adhesive_shear_strength_mpa <= 0.0)
        return 0.0;

    /* Thermal force from CTE mismatch */
    double thermal_strain = delta_cte_ppm * 1.0e-6 * delta_temp_c;
    double f_thermal = rigid_modulus_mpa * rigid_area_mm2 * thermal_strain;
    if (f_thermal < 0.0) f_thermal = -f_thermal;

    /* Required bonded area to resist F_thermal */
    double required_area = f_thermal / adhesive_shear_strength_mpa;
    double l_anchor = required_area / (tab_width_mm * tab_count);

    /* Enforce IPC-2223 minimum */
    if (l_anchor < 1.0) l_anchor = 1.0;

    /* Cap at reasonable maximum */
    if (l_anchor > 10.0) l_anchor = 10.0;

    return l_anchor;
}

/**
 * Knowledge Point: Anchor tab pull-out strength.
 *
 * The ultimate pull-out force of an anchor tab set:
 *
 *   F_pullout = τ_bond * w_tab * L_tab * n_tabs
 *
 * This is the characterization test used in IPC-TM-650 2.4.9 to
 * qualify rigid-flex bond reliability. The design must satisfy:
 *
 *   F_pullout ≥ 3 × F_thermal    (safety factor for production variability)
 *
 * For a typical design with 4 tabs, 2 mm × 3 mm each, τ_bond=1.5 MPa:
 *   F_pullout = 1.5 × 2 × 3 × 4 = 36 N
 * This exceeds typical thermal forces (5-15 N) with good margin.
 */
double flex_anchor_tab_strength(double tab_length_mm,
                                 double tab_width_mm,
                                 int tab_count,
                                 double bond_shear_strength_mpa) {
    if (tab_length_mm <= 0.0 || tab_width_mm <= 0.0 || tab_count <= 0)
        return 0.0;

    double bonded_area = tab_length_mm * tab_width_mm * (double)tab_count;
    return bond_shear_strength_mpa * bonded_area;
}

/* ========================================================================
 * L5: Tear-Stop Design (Fracture Mechanics)
 * ========================================================================*/

/**
 * Knowledge Point: Optimal tear-stop spacing from fracture mechanics.
 *
 * Tear-stops are intentional holes placed along the transition boundary
 * to arrest propagating cracks. They exploit the fundamental principle
 * of linear elastic fracture mechanics (LEFM):
 *
 * A crack propagates when the stress intensity factor K_I exceeds the
 * material fracture toughness K_IC:
 *
 *   K_I = σ * √(π * a) ≥ K_IC   →   crack grows
 *
 * The tear-stop hole removes the crack tip, reducing K_I. Optimal spacing:
 *
 *   s_optimal = √(π * K_IC² / (2 * σ²))
 *
 * This ensures that if a crack initiates between two tear-stops,
 * the stress intensity at the tear-stop hole is below K_IC.
 *
 * For PI (K_IC ≈ 3.5 MPa√m) and σ = 10 MPa:
 *   s_optimal = √(π * 3.5² / (2 * 10²)) ≈ 0.31 mm
 *
 * Reference: Griffith, "The Phenomena of Rupture and Flow in Solids",
 *            Philosophical Transactions of the Royal Society, 1921
 *            IPC-2223 §8.5 "Tear-Stop Features"
 */
double flex_tear_stop_spacing(double operational_stress_mpa,
                               double fracture_toughness_mpa_sqrtm) {
    if (operational_stress_mpa <= 0.0 ||
        fracture_toughness_mpa_sqrtm <= 0.0)
        return 10.0;  /* Default conservative spacing */

    double s_sq = M_PI * fracture_toughness_mpa_sqrtm *
                  fracture_toughness_mpa_sqrtm /
                  (2.0 * operational_stress_mpa * operational_stress_mpa);
    double s = sqrt(s_sq);

    /* Practical limits */
    if (s < 0.5)  s = 0.5;   /* Manufacturing minimum */
    if (s > 5.0)  s = 5.0;   /* Practical maximum */

    return s;
}

/**
 * Knowledge Point: Tear-stop sizing verification using critical crack length.
 *
 * The tear-stop hole must be large enough to reduce the stress
 * intensity below K_IC at its boundary:
 *
 *   d_tearstop > 2 * a_critical
 *
 * where a_critical = (K_IC / (σ * √π))² is the critical crack length.
 *
 * Undersized tear-stops provide false confidence — a crack can
 * propagate through or around an inadequate hole.
 *
 * For PI at σ = 15 MPa: a_critical = (3.5/(15*√π))² ≈ 0.017 mm
 * This is very small, meaning PI is quite flaw-tolerant.
 *
 * However, at elevated temperature (150°C), K_IC drops ~30%,
 * making tear-stop design more critical for high-temp applications.
 */
int flex_tear_stop_is_adequate(double tear_stop_diameter_mm,
                                double stress_mpa,
                                double fracture_toughness_mpa_sqrtm) {
    if (tear_stop_diameter_mm <= 0.0 || stress_mpa <= 0.0)
        return 0;

    double a_crit =
        (fracture_toughness_mpa_sqrtm * fracture_toughness_mpa_sqrtm) /
        (M_PI * stress_mpa * stress_mpa);
    double min_diameter = 2.0 * a_crit;

    return (tear_stop_diameter_mm >= min_diameter) ? 1 : 0;
}

/* ========================================================================
 * L5: Impedance Continuity Across Transition
 * ========================================================================*/

/**
 * Knowledge Point: Impedance discontinuity at rigid-flex boundary.
 *
 * When a transmission line crosses from rigid to flex section, the
 * geometry change (thickness, dielectric constant) creates an impedance
 * discontinuity. This causes signal reflections:
 *
 *   Γ = (Z0_rigid - Z0_flex) / (Z0_rigid + Z0_flex)
 *
 * Acceptable limits:
 *   |Γ| < 0.05  (RL > 26 dB) — excellent, suitable for >25 Gbps
 *   |Γ| < 0.1   (RL > 20 dB) — good for ≤ 10 Gbps
 *   |Γ| < 0.2   (RL > 14 dB) — marginal, needs evaluation
 *
 * Example: 50Ω rigid to 47Ω flex → Γ = 3/97 ≈ 0.031 (excellent)
 *          50Ω rigid to 42Ω flex → Γ = 8/92 ≈ 0.087 (good)
 *          50Ω rigid to 35Ω flex → Γ = 15/85 ≈ 0.176 (marginal)
 *
 * Reflection management at the transition is critical for signal
 * integrity in rigid-flex designs carrying > 1 Gbps signals.
 *
 * Reference: IPC-2223 §10 "Signal Integrity in Rigid-Flex"
 *            Johnson & Graham, "High-Speed Digital Design", Ch. 4
 */
double flex_transition_impedance_delta(double rigid_z0_ohm,
                                        double flex_z0_ohm,
                                        double *reflection_coeff) {
    if (rigid_z0_ohm <= 0.0 || flex_z0_ohm <= 0.0) {
        if (reflection_coeff) *reflection_coeff = 1.0;
        return 1.0e6;
    }

    double delta = (rigid_z0_ohm > flex_z0_ohm) ?
                   (rigid_z0_ohm - flex_z0_ohm) :
                   (flex_z0_ohm - rigid_z0_ohm);

    double gamma = (rigid_z0_ohm - flex_z0_ohm) /
                   (rigid_z0_ohm + flex_z0_ohm);
    if (gamma < 0.0) gamma = -gamma;

    if (reflection_coeff) *reflection_coeff = gamma;

    return delta;
}

/**
 * Knowledge Point: Exponential impedance taper design.
 *
 * To minimize reflections at the rigid-flex transition, the impedance
 * can be gradually tapered from Z0_rigid to Z0_flex:
 *
 *   Z0(x) = Z0_rigid * exp((x/L) * ln(Z0_flex / Z0_rigid))
 *
 * This exponential taper has the theoretically optimal reflection profile
 * for a given length (Klopfenstein, 1956). The reflection coefficient
 * decreases with increasing taper length:
 *
 *   |Γ| ≈ 0.5 * |ln(Z0_flex / Z0_rigid)| * (λ / L_taper)
 *
 * For a 10% impedance delta and L_taper = λ/4: |Γ| ≈ 0.013 (excellent)
 * For the same delta and L_taper = λ/10: |Γ| ≈ 0.033 (good)
 *
 * In flex design, the taper is implemented by gradually changing trace
 * width through the transition zone (typically 1-3 mm long).
 *
 * Reference: Klopfenstein, "A Transmission Line Taper of Improved Design",
 *            Proc. IRE, Vol. 44, No. 1, pp. 31-35, 1956
 */
void flex_impedance_taper_exponential(double rigid_z0_ohm,
                                       double flex_z0_ohm,
                                       double taper_length_mm,
                                       int num_points,
                                       double *impedance_profile) {
    (void)taper_length_mm;  /* Length implicit in number of sample points */
    if (!impedance_profile || num_points <= 0 ||
        rigid_z0_ohm <= 0.0 || flex_z0_ohm <= 0.0)
        return;

    /* ln(Zflex / Zrigid) */
    double log_ratio = log(flex_z0_ohm / rigid_z0_ohm);

    for (int i = 0; i < num_points; i++) {
        double x = (num_points > 1) ?
                   (double)i / (double)(num_points - 1) : 0.5;
        /* Z0(x) = Z0_start * exp(x * ln(Z0_end / Z0_start)) */
        impedance_profile[i] = rigid_z0_ohm * exp(x * log_ratio);
    }
}

/* ========================================================================
 * L5: Thermal Fatigue Life Prediction for Transition
 * ========================================================================*/

/**
 * Knowledge Point: Engelmaier-Wild model for adhesive/solder fatigue.
 *
 * Thermal cycling causes fatigue in the adhesive bond at the rigid-flex
 * transition. The Engelmaier-Wild strain-life model:
 *
 *   N_f = 0.5 * (Δγ / 2*ε_f)^(1/c)
 *
 * where:
 *   Δγ = shear strain range per cycle
 *   ε_f = fatigue ductility coefficient
 *   c = fatigue ductility exponent (typically -0.4 to -0.7)
 *
 * For acrylic adhesive bond (ε_f ≈ 0.3, c ≈ -0.5):
 *   At Δγ = 0.01 (1% strain): N_f ≈ 0.5 * (0.01/0.6)^(-2) ≈ 1800 cycles
 *   At Δγ = 0.001 (0.1% strain): N_f ≈ 0.5 * (0.001/0.6)^(-2) ≈ 180,000 cycles
 *
 * This exponential sensitivity to strain range is why minimizing CTE
 * mismatch is the primary design strategy for long life.
 *
 * Reference: Engelmaier, "Fatigue Life of Leadless Chip Carrier Solder
 *            Joints During Power Cycling", IEEE CHMT, 1983
 *            Wild, "Some Fatigue Properties of Solders and Solder Joints",
 *            IBM Report No. 74Z000481, 1974
 */
double flex_transition_thermal_life(double shear_strain_per_cycle,
                                     double fatigue_ductility,
                                     double fatigue_exponent) {
    if (shear_strain_per_cycle <= 0.0 || fatigue_ductility <= 0.0)
        return 1.0e12;

    double strain_ratio = shear_strain_per_cycle / (2.0 * fatigue_ductility);

    if (strain_ratio >= 1.0) return 0.25;  /* Immediate failure */

    double n_f = 0.5 * pow(strain_ratio, 1.0 / fatigue_exponent);

    /* Clamp */
    if (n_f < 0.25) n_f = 0.25;
    if (n_f > 1.0e9) n_f = 1.0e9;

    return n_f;
}

/* ========================================================================
 * L5: Complete Transition Analysis
 * ========================================================================*/

/**
 * Knowledge Point: Unified transition zone analysis pipeline.
 *
 * Runs the complete transition zone reliability analysis:
 * 1. Calculate interfacial shear and peel stresses
 * 2. Determine stress concentration factor
 * 3. Verify anchor tab and tear-stop adequacy
 * 4. Check impedance continuity
 * 5. Predict thermal cycle life
 * 6. Assign overall robustness rating
 *
 * The analysis produces actionable design improvement recommendations
 * suitable for design review sign-off.
 */
int flex_transition_analyze(const flex_transition_params_t *params,
                             flex_transition_result_t *result) {
    if (!params || !result) return -1;
    memset(result, 0, sizeof(flex_transition_result_t));

    /* CTE mismatch */
    double delta_cte = (params->rigid_cte_xy_ppm > params->flex_cte_xy_ppm) ?
        params->rigid_cte_xy_ppm - params->flex_cte_xy_ppm :
        params->flex_cte_xy_ppm - params->rigid_cte_xy_ppm;

    double delta_t = params->temp_range_max_c - params->temp_range_min_c;
    if (delta_t < 0.0) delta_t = -delta_t;

    /* Effective properties */
    double e_eff = (params->rigid_modulus_mpa + params->flex_modulus_mpa) / 2.0;
    double layer_thick = (params->rigid_thickness_mm +
                          params->flex_thickness_mm) / 2.0;

    /* Bond adhesive properties (acrylic defaults) */
    double g_a = 500.0;
    double h_a = 0.025;

    /* Shear stress at interface */
    result->max_shear_stress_mpa = flex_transition_shear_stress(
        delta_cte, delta_t, e_eff, params->transition_length_mm,
        g_a, layer_thick, h_a);

    /* Peel stress */
    result->max_peel_stress_mpa = flex_transition_peel_stress(
        result->max_shear_stress_mpa, params->transition_length_mm,
        params->flex_thickness_mm);

    /* Stress concentration */
    result->stress_concentration_factor =
        flex_transition_stress_concentration(
            0.25, params->rigid_thickness_mm,
            params->flex_thickness_mm);

    /* Minimum transition length */
    result->min_transition_length_mm = 1.5;
    if (delta_cte > 10.0) result->min_transition_length_mm = 2.5;
    if (params->rigid_thickness_mm > 1.6)
        result->min_transition_length_mm += 0.5;

    /* Anchor tab strength */
    if (params->has_anchor_tab) {
        result->anchor_tab_strength_n = flex_anchor_tab_strength(
            params->anchor_tab_length_mm,
            params->anchor_tab_width_mm,
            params->anchor_tab_count,
            1.2);  /* N/mm² — acrylic bond shear strength */
    }

    /* Impedance continuity */
    if (params->rigid_z0_ohm > 0.0 && params->flex_z0_ohm > 0.0) {
        result->impedance_discontinuity_ohm =
            flex_transition_impedance_delta(
                params->rigid_z0_ohm, params->flex_z0_ohm,
                &result->reflection_coefficient);
    }

    /* Thermal cycle life */
    double shear_strain = delta_cte * 1.0e-6 * delta_t;
    result->estimated_thermal_life = flex_transition_thermal_life(
        shear_strain, 0.3, -0.5);

    /* Transition rating */
    result->transition_rating = flex_transition_rating(result);

    /* Generate recommendation */
    if (result->transition_rating >= 4) {
        snprintf(result->recommendation, sizeof(result->recommendation),
            "Robust transition design. No changes needed.");
    } else if (result->transition_rating >= 2) {
        snprintf(result->recommendation, sizeof(result->recommendation),
            "Consider: %.1f mm min transition length, verify anchor tabs",
            result->min_transition_length_mm);
    } else {
        snprintf(result->recommendation, sizeof(result->recommendation),
            "Redesign required: extend transition to %.1f mm, add "
            "tear-stops, verify impedance continuity",
            result->min_transition_length_mm);
    }

    return 0;
}

/**
 * Knowledge Point: Transition design robustness rating.
 *
 * Rates the transition zone design on a 0-5 scale based on key
 * reliability indicators:
 *
 * 5 = Aerospace/medical implant grade: All safety margins ≥ 3×
 * 4 = Industrial high-reliability: All safety margins ≥ 2×
 * 3 = Standard commercial: Meets IPC minimum requirements
 * 2 = Marginal: Some requirements barely met, monitoring recommended
 * 1 = Poor: Multiple IPC violations, redesign recommended
 * 0 = Unacceptable: Guaranteed early failure
 *
 * The rating algorithm combines stress margins, design feature
 * completeness, and impedance quality into a single score.
 */
int flex_transition_rating(const flex_transition_result_t *result) {
    if (!result) return 0;

    int score = 5;  /* Start perfect, deduct for issues */

    /* Shear stress margin vs typical bond strength ~15 MPa */
    if (result->max_shear_stress_mpa > 15.0) score -= 2;
    else if (result->max_shear_stress_mpa > 10.0) score -= 1;

    /* Peel stress margin */
    if (result->max_peel_stress_mpa > 5.0) score -= 2;
    else if (result->max_peel_stress_mpa > 3.0) score -= 1;

    /* Stress concentration */
    if (result->stress_concentration_factor > 3.0) score -= 1;
    if (result->stress_concentration_factor > 4.0) score -= 1;

    /* Impedance reflection */
    if (result->reflection_coefficient > 0.2) score -= 1;
    if (result->reflection_coefficient > 0.3) score -= 1;

    /* Thermal life */
    if (result->estimated_thermal_life < 100.0) score -= 2;
    else if (result->estimated_thermal_life < 1000.0) score -= 1;

    /* Anchor tab safety */
    if (result->anchor_tab_strength_n < 10.0 &&
        result->anchor_tab_strength_n > 0.0)
        score -= 1;

    /* Clamp */
    if (score < 0) score = 0;
    if (score > 5) score = 5;

    return score;
}
