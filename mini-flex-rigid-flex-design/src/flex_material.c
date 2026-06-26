/**
 * @file flex_material.c
 * @brief Material property database and physics calculations for flex/rigid-flex PCB
 *
 * Implements the standard IPC material property tables and frequency/temperature
 * dependent corrections. Each function independently encodes one IPC standard
 * or one physical model.
 *
 * @module mini-flex-rigid-flex-design
 */

#include "flex_material.h"
#include <math.h>
#include <string.h>

/* ========================================================================
 * IPC-4202 Standard Material Property Tables
 *
 * These tables encode the benchmark material properties from IPC-4202/4204
 * specifications. Each material entry represents an independent knowledge
 * point about flex substrate engineering.
 * ========================================================================*/

/* --------------------------------------------------------------------------
 * L1: Knowledge Point — IPC-4202 Type A: Adhesiveless Polyimide (Kapton HN)
 *
 * Polyimide is the dominant flex substrate due to:
 * - High Tg (no true Tg — decomposition at ~500°C)
 * - Excellent chemical resistance
 * - Good dielectric properties
 * - Proven reliability over decades
 *
 * Dielectric constant εr is moderately high (~3.4), limiting high-frequency
 * performance. Modified polyimide (MPI) reduces εr to ~3.0.
 * --------------------------------------------------------------------------*/
flex_dielectric_electrical_t flex_polyimide_electrical_standard(void) {
    flex_dielectric_electrical_t p;
    p.dielectric_constant      = 3.4;
    p.dk_at_1ghz               = 3.3;
    p.dk_at_10ghz              = 3.2;
    p.loss_tangent             = 0.002;
    p.loss_tangent_at_1ghz    = 0.003;
    p.dielectric_strength      = 276.0;   /* kV/mm — very high DI strength */
    p.volume_resistivity       = 1.0e8;   /* MΩ·cm — excellent insulator */
    p.surface_resistivity      = 1.0e7;   /* MΩ */
    p.moisture_absorption      = 2.8;     /* % — moderate, can affect εr */
    return p;
}

/* --------------------------------------------------------------------------
 * L1: Knowledge Point — IPC-4202 Type C: Liquid Crystal Polymer (LCP)
 *
 * LCP is the premium flex substrate for high-frequency applications:
 * - εr ≈ 2.9 (lower than PI → thinner traces for same Z0)
 * - tanδ ≈ 0.002 at 10 GHz (excellent for mmWave)
 * - Near-zero moisture absorption (< 0.04%)
 * - Anisotropic: properties vary with orientation
 *
 * Used in: 5G mmWave flex, 77 GHz automotive radar flex, >40 Gbps interconnects
 * --------------------------------------------------------------------------*/
flex_dielectric_electrical_t flex_lcp_electrical_standard(void) {
    flex_dielectric_electrical_t lcp;
    lcp.dielectric_constant     = 2.9;
    lcp.dk_at_1ghz              = 2.9;
    lcp.dk_at_10ghz             = 2.85;
    lcp.loss_tangent            = 0.0015;
    lcp.loss_tangent_at_1ghz   = 0.002;
    lcp.dielectric_strength     = 200.0;
    lcp.volume_resistivity      = 1.0e7;
    lcp.surface_resistivity     = 1.0e6;
    lcp.moisture_absorption     = 0.04;   /* % — essentially zero, stable εr */
    return lcp;
}

/* --------------------------------------------------------------------------
 * L1: Knowledge Point — Polyimide Mechanical Properties (Kapton HN-type)
 *
 * Key design implications:
 * - Young's modulus = 2500 MPa → determines bending stiffness
 * - CTE_XY ≈ 20 ppm/°C → close to copper (17 ppm/°C) → low CTE mismatch
 * - CTE_Z ≈ 60 ppm/°C → much higher than copper → via reliability risk
 * - Elongation at break = 72% → excellent flexibility margin
 *
 * The CTE_Z mismatch is a known failure mechanism for plated through-holes
 * in rigid-flex designs during thermal cycling.
 * --------------------------------------------------------------------------*/
flex_dielectric_mechanical_t flex_polyimide_mechanical_standard(void) {
    flex_dielectric_mechanical_t m;
    m.youngs_modulus        = 2500.0;    /* MPa — film direction */
    m.tensile_strength      = 231.0;     /* MPa */
    m.elongation_at_break   = 72.0;      /* % — very ductile */
    m.cte_xy                = 20.0;      /* ppm/°C — close to Cu (17 ppm/°C) */
    m.cte_z                 = 60.0;      /* ppm/°C — 3x in-plane, via risk */
    m.glass_transition_tg   = 360.0;     /* °C — decomposition, no true Tg */
    m.decomposition_temp    = 500.0;     /* °C */
    m.thermal_conductivity  = 0.12;      /* W/(m·K) — poor conductor */
    m.density               = 1.42;      /* g/cm³ */
    m.moisture_expansion    = 9.0;       /* ppm/%RH */
    return m;
}

/* --------------------------------------------------------------------------
 * L1: Knowledge Point — LCP Mechanical Properties
 *
 * LCP is stiffer than PI (higher modulus) and has lower CTE, making it
 * better for dimensional stability but requires larger bend radii.
 *
 * Anisotropy note: LCP modulus can vary 2-3x between machine direction
 * and transverse direction. Design must account for this.
 * --------------------------------------------------------------------------*/
flex_dielectric_mechanical_t flex_lcp_mechanical_standard(void) {
    flex_dielectric_mechanical_t m;
    m.youngs_modulus        = 3000.0;    /* MPa */
    m.tensile_strength      = 200.0;     /* MPa */
    m.elongation_at_break   = 30.0;      /* % — less ductile than PI */
    m.cte_xy                = 17.0;      /* ppm/°C — excellent match to Cu */
    m.cte_z                 = 50.0;      /* ppm/°C */
    m.glass_transition_tg   = 280.0;     /* °C */
    m.decomposition_temp    = 450.0;     /* °C */
    m.thermal_conductivity  = 0.50;      /* W/(m·K) — better than PI */
    m.density               = 1.40;      /* g/cm³ */
    m.moisture_expansion    = 2.0;       /* ppm/%RH — very low */
    return m;
}

/* --------------------------------------------------------------------------
 * L1: Knowledge Point — PET (Polyester) Mechanical Properties
 *
 * PET is the lowest-cost flex substrate. Limitations:
 * - Tg ≈ 80°C — cannot survive lead-free soldering (260°C reflow)
 * - High moisture absorption
 * - Lower tensile strength
 *
 * Used in: disposable medical sensors, low-cost consumer flex, ID card circuits
 * Not suitable for: high-reliability, high-temperature, dynamic flex
 * --------------------------------------------------------------------------*/
flex_dielectric_mechanical_t flex_pet_mechanical_standard(void) {
    flex_dielectric_mechanical_t m;
    m.youngs_modulus        = 3800.0;    /* MPa — stiffer than PI */
    m.tensile_strength      = 170.0;     /* MPa */
    m.elongation_at_break   = 100.0;     /* % — very high elongation */
    m.cte_xy                = 25.0;      /* ppm/°C */
    m.cte_z                 = 80.0;      /* ppm/°C */
    m.glass_transition_tg   = 80.0;      /* °C — LOW, limits applications */
    m.decomposition_temp    = 300.0;     /* °C */
    m.thermal_conductivity  = 0.15;      /* W/(m·K) */
    m.density               = 1.38;      /* g/cm³ */
    m.moisture_expansion    = 15.0;      /* ppm/%RH */
    return m;
}

/* --------------------------------------------------------------------------
 * L1: Knowledge Point — Copper Foil Properties (IPC-4562)
 *
 * RA (Rolled Annealed) vs ED (Electrodeposited) copper:
 *
 * RA copper is produced by mechanically rolling and annealing, creating
 * an elongated grain structure parallel to the surface. This provides:
 * - Superior flex fatigue life (3-10x better than ED)
 * - Smoother surface (better for high-frequency SI)
 * - Lower tensile strength
 *
 * ED copper is electrochemically deposited, creating a columnar grain
 * structure. Properties:
 * - Higher tensile strength
 * - Rougher surface → better adhesion but worse SI
 * - Lower cost
 * - Shorter flex life
 *
 * For dynamic flex applications: ALWAYS use RA copper.
 * For static flex or rigid sections: ED copper is acceptable.
 * --------------------------------------------------------------------------*/
flex_copper_foil_t flex_copper_foil_standard(flex_copper_type_t copper_type) {
    flex_copper_foil_t c;
    c.resistivity          = 1.72;        /* μΩ·cm — pure copper */
    c.thermal_conductivity = 398.0;       /* W/(m·K) */
    c.cte                  = 17.0;        /* ppm/°C */

    switch (copper_type) {
    case FLEX_COPPER_RA:
        c.tensile_strength      = 240.0;  /* N/mm² */
        c.elongation            = 12.0;   /* % */
        c.surface_roughness_ra  = 0.3;    /* μm — smooth */
        c.surface_roughness_rz  = 2.0;    /* μm */
        c.fatigue_strength      = 150.0;  /* N/mm² — excellent */
        c.grain_size            = 5.0;    /* μm — fine, elongated grains */
        break;
    case FLEX_COPPER_ED:
        c.tensile_strength      = 350.0;  /* N/mm² — higher strength */
        c.elongation            = 5.0;    /* % — less ductile */
        c.surface_roughness_ra  = 1.5;    /* μm — rougher */
        c.surface_roughness_rz  = 8.0;    /* μm */
        c.fatigue_strength      = 80.0;   /* N/mm² — poorer fatigue */
        c.grain_size            = 20.0;   /* μm — coarser, columnar */
        break;
    case FLEX_COPPER_RA_LP:
        c.tensile_strength      = 230.0;
        c.elongation            = 10.0;
        c.surface_roughness_ra  = 0.15;   /* μm — ultra-smooth for mmWave */
        c.surface_roughness_rz  = 1.0;
        c.fatigue_strength      = 140.0;
        c.grain_size            = 4.0;
        break;
    case FLEX_COPPER_ED_LP:
        c.tensile_strength      = 320.0;
        c.elongation            = 4.0;
        c.surface_roughness_ra  = 0.8;
        c.surface_roughness_rz  = 4.0;
        c.fatigue_strength      = 70.0;
        c.grain_size            = 15.0;
        break;
    default:
        /* Default to RA */
        c.tensile_strength      = 240.0;
        c.elongation            = 12.0;
        c.surface_roughness_ra  = 0.3;
        c.surface_roughness_rz  = 2.0;
        c.fatigue_strength      = 150.0;
        c.grain_size            = 5.0;
        break;
    }
    return c;
}

/* --------------------------------------------------------------------------
 * L1: Knowledge Point — Adhesive Properties per IPC-4203
 *
 * The adhesive layer in 3-layer flex (Cu-Adhesive-PI) construction is
 * often the reliability-limiting element:
 *
 * - Acrylic: Good flow (fills gaps), lower Tg (~80°C), lower cost
 * - Epoxy: Higher Tg (~150°C), lower flow, better thermal resistance
 * - Polyimide: Highest Tg (>200°C), best for high-temp, most expensive
 * - Adhesiveless: Best performance (no weak adhesive layer), cast-on or sputtered Cu
 *
 * Adhesiveless construction (2-layer flex) is preferred for:
 * - Dynamic flex (no adhesive to fatigue)
 * - High-reliability (aerospace, medical implant)
 * - High-density (thinner profile = tighter bends)
 * --------------------------------------------------------------------------*/
flex_adhesive_properties_t flex_adhesive_properties_standard(
    flex_adhesive_type_t adhesive_type) {
    flex_adhesive_properties_t a;
    switch (adhesive_type) {
    case FLEX_ADHESIVE_ACRYLIC:
        a.dielectric_constant = 3.5;
        a.loss_tangent        = 0.02;
        a.youngs_modulus      = 500.0;    /* MPa — softer */
        a.cte                 = 80.0;     /* ppm/°C — HIGH CTE */
        a.glass_transition_tg = 80.0;     /* °C — low */
        a.flow_percent        = 40.0;     /* % — high flow */
        a.peel_strength       = 1.2;      /* N/mm */
        a.minimum_thickness   = 0.0127;   /* mm = 0.5 mil */
        break;
    case FLEX_ADHESIVE_EPOXY:
        a.dielectric_constant = 3.8;
        a.loss_tangent        = 0.025;
        a.youngs_modulus      = 3000.0;   /* MPa — stiffer */
        a.cte                 = 55.0;     /* ppm/°C */
        a.glass_transition_tg = 150.0;    /* °C */
        a.flow_percent        = 25.0;     /* % */
        a.peel_strength       = 1.0;      /* N/mm */
        a.minimum_thickness   = 0.0127;   /* mm */
        break;
    case FLEX_ADHESIVE_PI:
        a.dielectric_constant = 3.4;
        a.loss_tangent        = 0.005;
        a.youngs_modulus      = 2500.0;
        a.cte                 = 35.0;
        a.glass_transition_tg = 220.0;    /* °C — high */
        a.flow_percent        = 15.0;
        a.peel_strength       = 1.5;
        a.minimum_thickness   = 0.0127;
        break;
    case FLEX_ADHESIVE_ADHESIVELESS:
        /* Adhesiveless: copper directly on PI (cast or sputtered) */
        a.dielectric_constant = 3.4;       /* Same as base PI */
        a.loss_tangent        = 0.002;
        a.youngs_modulus      = 2500.0;    /* Same as base PI */
        a.cte                 = 20.0;      /* Same as base PI — best match */
        a.glass_transition_tg = 360.0;
        a.flow_percent        = 0.0;       /* No adhesive → no flow */
        a.peel_strength       = 0.8;       /* N/mm — lower than with adhesive */
        a.minimum_thickness   = 0.0;       /* No adhesive layer */
        break;
    case FLEX_ADHESIVE_PSA:
        a.dielectric_constant = 3.0;
        a.loss_tangent        = 0.03;
        a.youngs_modulus      = 10.0;      /* MPa — very soft, viscoelastic */
        a.cte                 = 200.0;     /* ppm/°C — HIGH */
        a.glass_transition_tg = -20.0;     /* °C — below room temp */
        a.flow_percent        = 0.0;       /* No flow — pressure sensitive */
        a.peel_strength       = 0.5;
        a.minimum_thickness   = 0.0254;    /* mm = 1 mil, thicker */
        break;
    default:
        a.dielectric_constant = 3.5;
        a.loss_tangent        = 0.02;
        a.youngs_modulus      = 500.0;
        a.cte                 = 80.0;
        a.glass_transition_tg = 80.0;
        a.flow_percent        = 40.0;
        a.peel_strength       = 1.2;
        a.minimum_thickness   = 0.0127;
        break;
    }
    return a;
}

/* ========================================================================
 * L4: Physics-Based Material Property Models
 * ========================================================================*/

/**
 * Knowledge Point: Djordjevic-Sarkar frequency-dependent dielectric model.
 *
 * All polymers exhibit dielectric relaxation: εr decreases with frequency
 * as polar molecules cannot reorient fast enough to follow the E-field.
 * The Djordjevic-Sarkar model captures this using a hyperbolic tangent:
 *
 *   εr(f) = εr' - (εr' - εr_inf) * tanh(α * log10(f / f0))
 *
 * where εr' = low-frequency permittivity, εr_inf = infinite-frequency
 * asymptotic value, α controls the transition rate.
 *
 * For polyimide: εr' ≈ 3.4, εr_inf ≈ 2.8, α ≈ 0.12, f0 ≈ 1 MHz.
 * For LCP: εr' ≈ 2.9, εr_inf ≈ 2.7, α ≈ 0.05, f0 ≈ 1 MHz.
 *
 * Reference: Djordjevic, Biljie, Likar-Smiljanic, Sarkar,
 *            "Wideband Frequency-Domain Characterization of FR-4...",
 *            IEEE Trans. EMC, 2001
 */
double flex_dk_at_frequency(double base_dk, double dk_inf,
                             double freq_hz, double ref_freq_hz) {
    if (freq_hz <= 0.0 || ref_freq_hz <= 0.0) return base_dk;
    if (dk_inf >= base_dk) return base_dk;  /* Invalid: dk_inf must be lower */

    /* Simplified Djordjevic-Sarkar: alpha ≈ 0.12 for most flex dielectrics */
    const double alpha = 0.12;
    double log_ratio = log10(freq_hz / ref_freq_hz);
    double delta = (base_dk - dk_inf) * tanh(alpha * log_ratio);
    return base_dk - delta;
}

/**
 * Knowledge Point: Moisture effect on dielectric constant (Mumby model).
 *
 * Polyimide absorbs ~2.8% moisture by weight. Water has εr ≈ 80, so
 * even small amounts of absorbed water significantly increase εr.
 *
 * For small moisture fractions M (weight %):
 *   εr_eff = εr_dry * (1 + β * M)
 *
 * where β ≈ 2.0 for polyimide (experimentally determined).
 * At 2.8% moisture: εr_eff ≈ 3.4 * (1 + 2.0 * 0.028) = 3.59 (+5.6%).
 *
 * This 5-6% impedance shift can matter for tightly controlled RF designs.
 *
 * Reference: Mumby, "The Relationship Between Dielectric Constant
 *            and Water Absorption in Polyimide Films", IEEE T-EP, 1988
 */
double flex_dk_moisture_correction(double dk_dry, double moisture_percent) {
    if (moisture_percent < 0.0) moisture_percent = 0.0;
    if (moisture_percent > 10.0) moisture_percent = 10.0;  /* Saturation cap */

    double moisture_fraction = moisture_percent / 100.0;
    double beta = 2.0;  /* Polyimide empirical coefficient */
    return dk_dry * (1.0 + beta * moisture_fraction);
}

/**
 * Knowledge Point: Thermal mismatch stress (thermoelasticity).
 *
 * When two bonded materials with different CTE values undergo a temperature
 * change, they want to expand/contract by different amounts. The constraint
 * of the bond creates stress:
 *
 *   σ = E * Δα * ΔT    (simplified 1D)
 *
 * where E is the modulus of the constrained material, Δα = α₁ - α₂ is the
 * CTE mismatch, and ΔT is the temperature change from the stress-free
 * reference (typically the lamination/cure temperature).
 *
 * Example: PI (α=20) bonded to ED copper (α=17) with ΔT = 200°C:
 *   σ = 117000 MPa * |20-17|e-6 * 200 = 70.2 MPa
 * This is below copper's yield strength (~240 MPa), so it's acceptable.
 *
 * But with acrylic adhesive (α=80) and ΔT = 200°C:
 *   σ_adhesive = 500 * |80-20|e-6 * 200 = 6 MPa
 * This exceeds typical acrylic peel strength (~1.2 N/mm), risking delamination.
 *
 * Reference: Timoshenko, "Analysis of Bi-Metal Thermostats", JOSA, 1925
 */
double flex_cte_mismatch_stress(double e_modulus, double cte_a,
                                 double cte_b, double delta_t) {
    if (delta_t < 0.0) delta_t = -delta_t;  /* Magnitude of change matters */
    double delta_cte = (cte_a > cte_b) ? (cte_a - cte_b) : (cte_b - cte_a);
    /* Convert CTE from ppm/°C to dimensionless strain per °C */
    double delta_cte_per_c = delta_cte * 1.0e-6;
    return e_modulus * delta_cte_per_c * delta_t;
}

/**
 * Knowledge Point: Glass transition depression by moisture (Kelley-Bueche).
 *
 * Absorbed water acts as a plasticizer in polymers, reducing Tg:
 *
 *   Tg(wet) = (φ_p * Tg_p + φ_w * Tg_w) / (φ_p + φ_w)
 *
 * where φ_p and φ_w are volume fractions, Tg_p is dry polymer Tg,
 * and Tg_w ≈ -135°C for water.
 *
 * For PET (Tg_dry = 80°C) at 1% moisture:
 *   φ_w ≈ 0.01 (approximate), φ_p ≈ 0.99
 *   Tg_wet ≈ (0.99*80 + 0.01*(-135)) / 1.0 ≈ 77.9°C
 * Small effect, but important for PET which is already marginal for soldering.
 *
 * Reference: Kelley & Bueche, J. Polymer Science, 1961
 */
double flex_tg_moisture_shift(double tg_dry, double moisture_percent) {
    if (moisture_percent <= 0.0) return tg_dry;

    double moisture_fraction = moisture_percent / 100.0;
    /* Volume fraction ≈ weight fraction (approximation for polymers) */
    double phi_w = moisture_fraction;
    double phi_p = 1.0 - phi_w;
    double tg_water = -135.0;  /* °C — Tg of water (plasticizer) */

    /* Kelvin-Bueche: weighted average of component Tg values */
    return (phi_p * tg_dry + phi_w * tg_water) / (phi_p + phi_w);
}

/**
 * Knowledge Point: Intrinsic impedance of a dielectric medium.
 *
 * The wave impedance of a plane wave in a dielectric medium:
 *
 *   η = √(μ₀ / (ε₀ * εr))
 *
 * Normalized to free space (η₀ = √(μ₀/ε₀) = 377 Ω):
 *   η_r = η₀ / √εr
 *
 * This value provides an upper bound on achievable transmission line
 * impedance for a given dielectric. In practice, microstrip Z0 is always
 * less than η due to the partial air dielectric.
 *
 * For PI (εr = 3.4): η ≈ 377/√3.4 ≈ 204 Ω
 * For LCP (εr = 2.9): η ≈ 377/√2.9 ≈ 221 Ω
 */
double flex_intrinsic_impedance(double dk) {
    if (dk <= 1.0) return 377.0;  /* Free space */
    return 377.0 / sqrt(dk);
}
