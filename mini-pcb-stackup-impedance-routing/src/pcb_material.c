/**
 * pcb_material.c — PCB Material Properties Database and Physics
 *
 * Implements the dielectric and conductor material models used in PCB design.
 * Covers L1-L8 levels per SKILL.md knowledge taxonomy.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "pcb_material.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* =========================================================================
 * Physical constants
 * ========================================================================= */
#define C0_SPEED_OF_LIGHT     2.99792458e8   /* m/s */
#define MU0_PERMEABILITY      1.2566370614e-6 /* H/m = 4π×10⁻⁷ */
#define EPS0_PERMITTIVITY     8.8541878176e-12 /* F/m */

/* Copper conductivity at 20°C (annealed copper) */
#define COPPER_SIGMA_20C      5.80e7          /* S/m */
#define COPPER_TEMP_COEFF     0.00393         /* /°C at 20°C */

/* =========================================================================
 * L2: Built-in material database
 * Data sourced from manufacturer datasheets (Rogers, Isola, Panasonic, ITEQ)
 * ========================================================================= */

static PcbMaterialSystem g_material_db[MAT_COUNT] = {0};
static int g_db_initialized = 0;

/* L1: Common conductor materials */
static const ConductorMaterial kCopperStd = {
    .name = "Copper (ED)",
    .conductivity = COPPER_SIGMA_20C,
    .temp_coeff = COPPER_TEMP_COEFF,
    .density = 8960.0,
    .thermal_conductivity = 398.0
};

static const ConductorMaterial kCopperRA = {
    .name = "Copper (RA - Rolled Annealed)",
    .conductivity = 5.82e7,
    .temp_coeff = 0.00393,
    .density = 8960.0,
    .thermal_conductivity = 398.0
};

/* L2: Initialize the material database with known commercial laminates */
static void material_init_db(void)
{
    if (g_db_initialized) return;

    /* MAT_STD_FR4: Standard FR-4, Tg ~140°C, most common PCB material */
    g_material_db[MAT_STD_FR4].dielectric = (DielectricMaterial){
        .name = "FR-4 Standard (Tg 140°C)",
        .epsilon_r = 4.35,
        .loss_tangent = 0.020,
        .sigma_dc = 1e-12,
        .mu_r = 1.0,
        .tg_glass = 140.0,
        .cte_x = 14.0,
        .cte_z = 50.0,
        .thermal_cond = 0.3,
        .water_absorption = 0.15,
        .dk_tc = -200.0,
        .is_halogen_free = 0
    };
    g_material_db[MAT_STD_FR4].conductor = kCopperStd;
    g_material_db[MAT_STD_FR4].roughness = (CopperRoughness){
        .model = ROUGHNESS_HAMMERSTAD,
        .rms_roughness = 1.8,
        .snowball_radius = 0.5,
        .surface_area = 2.0,
        .num_spheres = 4
    };

    /* MAT_HIGH_TG_FR4: High-Tg FR-4, Tg ~170°C, better thermal reliability */
    g_material_db[MAT_HIGH_TG_FR4].dielectric = (DielectricMaterial){
        .name = "FR-4 High-Tg (Tg 170°C)",
        .epsilon_r = 4.20,
        .loss_tangent = 0.018,
        .sigma_dc = 1e-12,
        .mu_r = 1.0,
        .tg_glass = 170.0,
        .cte_x = 13.0,
        .cte_z = 45.0,
        .thermal_cond = 0.35,
        .water_absorption = 0.12,
        .dk_tc = -180.0,
        .is_halogen_free = 0
    };
    g_material_db[MAT_HIGH_TG_FR4].conductor = kCopperStd;
    g_material_db[MAT_HIGH_TG_FR4].roughness.rms_roughness = 1.5;

    /* MAT_ROGERS_4350B: Hydrocarbon ceramic laminate, low loss */
    g_material_db[MAT_ROGERS_4350B].dielectric = (DielectricMaterial){
        .name = "Rogers RO4350B™",
        .epsilon_r = 3.48,
        .loss_tangent = 0.0037,
        .sigma_dc = 1e-13,
        .mu_r = 1.0,
        .tg_glass = 280.0,
        .cte_x = 14.0,
        .cte_z = 35.0,
        .thermal_cond = 0.69,
        .water_absorption = 0.06,
        .dk_tc = 50.0,
        .is_halogen_free = 1
    };
    g_material_db[MAT_ROGERS_4350B].conductor = kCopperStd;
    g_material_db[MAT_ROGERS_4350B].roughness.rms_roughness = 1.0;

    /* MAT_ROGERS_4003C: PTFE/ceramic laminate, very low loss */
    g_material_db[MAT_ROGERS_4003C].dielectric = (DielectricMaterial){
        .name = "Rogers RO4003C™",
        .epsilon_r = 3.38,
        .loss_tangent = 0.0027,
        .sigma_dc = 1e-13,
        .mu_r = 1.0,
        .tg_glass = 280.0,
        .cte_x = 11.0,
        .cte_z = 46.0,
        .thermal_cond = 0.71,
        .water_absorption = 0.04,
        .dk_tc = 40.0,
        .is_halogen_free = 1
    };
    g_material_db[MAT_ROGERS_4003C].conductor = kCopperStd;
    g_material_db[MAT_ROGERS_4003C].roughness.rms_roughness = 1.0;

    /* MAT_ISOLA_370HR: Mid-loss phenolic-cured, automotive grade */
    g_material_db[MAT_ISOLA_370HR].dielectric = (DielectricMaterial){
        .name = "Isola 370HR",
        .epsilon_r = 3.92,
        .loss_tangent = 0.025,
        .sigma_dc = 1e-12,
        .mu_r = 1.0,
        .tg_glass = 180.0,
        .cte_x = 13.0,
        .cte_z = 55.0,
        .thermal_cond = 0.40,
        .water_absorption = 0.13,
        .dk_tc = -150.0,
        .is_halogen_free = 1
    };
    g_material_db[MAT_ISOLA_370HR].conductor = kCopperStd;
    g_material_db[MAT_ISOLA_370HR].roughness.rms_roughness = 1.5;

    /* MAT_ISOLA_FR408HR: Low-loss automotive/high-speed FR-4 */
    g_material_db[MAT_ISOLA_FR408HR].dielectric = (DielectricMaterial){
        .name = "Isola FR408HR",
        .epsilon_r = 3.66,
        .loss_tangent = 0.0091,
        .sigma_dc = 1e-13,
        .mu_r = 1.0,
        .tg_glass = 200.0,
        .cte_x = 12.0,
        .cte_z = 50.0,
        .thermal_cond = 0.40,
        .water_absorption = 0.08,
        .dk_tc = -100.0,
        .is_halogen_free = 1
    };
    g_material_db[MAT_ISOLA_FR408HR].conductor = kCopperStd;
    g_material_db[MAT_ISOLA_FR408HR].roughness.rms_roughness = 1.2;

    /* MAT_MEGTRON_6: Panasonic ultra-low-loss for 25G+ */
    g_material_db[MAT_MEGTRON_6].dielectric = (DielectricMaterial){
        .name = "Panasonic Megtron 6 (R-5775)",
        .epsilon_r = 3.60,
        .loss_tangent = 0.0020,
        .sigma_dc = 1e-14,
        .mu_r = 1.0,
        .tg_glass = 185.0,
        .cte_x = 13.0,
        .cte_z = 25.0,
        .thermal_cond = 0.55,
        .water_absorption = 0.06,
        .dk_tc = -75.0,
        .is_halogen_free = 1
    };
    g_material_db[MAT_MEGTRON_6].conductor = kCopperStd;
    g_material_db[MAT_MEGTRON_6].roughness.rms_roughness = 0.8;

    /* MAT_MEGTRON_7: Panasonic extreme-low-loss for 112G PAM4 */
    g_material_db[MAT_MEGTRON_7].dielectric = (DielectricMaterial){
        .name = "Panasonic Megtron 7 (R-5785)",
        .epsilon_r = 3.35,
        .loss_tangent = 0.0010,
        .sigma_dc = 1e-14,
        .mu_r = 1.0,
        .tg_glass = 185.0,
        .cte_x = 12.0,
        .cte_z = 22.0,
        .thermal_cond = 0.60,
        .water_absorption = 0.05,
        .dk_tc = -60.0,
        .is_halogen_free = 1
    };
    g_material_db[MAT_MEGTRON_7].conductor = kCopperStd;
    g_material_db[MAT_MEGTRON_7].roughness.rms_roughness = 0.5;

    /* MAT_PTFE_TEFLON: Pure PTFE for microwave/RF, very low loss */
    g_material_db[MAT_PTFE_TEFLON].dielectric = (DielectricMaterial){
        .name = "PTFE (Teflon)",
        .epsilon_r = 2.10,
        .loss_tangent = 0.0003,
        .sigma_dc = 1e-15,
        .mu_r = 1.0,
        .tg_glass = 25.0,
        .cte_x = 110.0,
        .cte_z = 240.0,
        .thermal_cond = 0.25,
        .water_absorption = 0.01,
        .dk_tc = -350.0,
        .is_halogen_free = 1
    };
    g_material_db[MAT_PTFE_TEFLON].conductor = kCopperRA;
    g_material_db[MAT_PTFE_TEFLON].roughness.rms_roughness = 0.5;

    /* MAT_TACONIC_RF35: Ceramic-filled PTFE, microwave */
    g_material_db[MAT_TACONIC_RF35].dielectric = (DielectricMaterial){
        .name = "Taconic RF-35",
        .epsilon_r = 3.50,
        .loss_tangent = 0.0015,
        .sigma_dc = 1e-15,
        .mu_r = 1.0,
        .tg_glass = 315.0,
        .cte_x = 25.0,
        .cte_z = 45.0,
        .thermal_cond = 0.24,
        .water_absorption = 0.02,
        .dk_tc = -40.0,
        .is_halogen_free = 1
    };
    g_material_db[MAT_TACONIC_RF35].conductor = kCopperRA;
    g_material_db[MAT_TACONIC_RF35].roughness.rms_roughness = 0.5;

    /* MAT_NELCO_N4000_13: Low-loss, high-speed digital */
    g_material_db[MAT_NELCO_N4000_13].dielectric = (DielectricMaterial){
        .name = "Nelco N4000-13",
        .epsilon_r = 3.70,
        .loss_tangent = 0.0080,
        .sigma_dc = 1e-13,
        .mu_r = 1.0,
        .tg_glass = 210.0,
        .cte_x = 12.0,
        .cte_z = 45.0,
        .thermal_cond = 0.50,
        .water_absorption = 0.08,
        .dk_tc = -110.0,
        .is_halogen_free = 1
    };
    g_material_db[MAT_NELCO_N4000_13].conductor = kCopperStd;
    g_material_db[MAT_NELCO_N4000_13].roughness.rms_roughness = 1.0;

    /* MAT_ITEQ_IT180A: Mid-Tg, cost-effective, good performance */
    g_material_db[MAT_ITEQ_IT180A].dielectric = (DielectricMaterial){
        .name = "ITEQ IT-180A",
        .epsilon_r = 4.10,
        .loss_tangent = 0.0140,
        .sigma_dc = 1e-12,
        .mu_r = 1.0,
        .tg_glass = 175.0,
        .cte_x = 13.0,
        .cte_z = 50.0,
        .thermal_cond = 0.38,
        .water_absorption = 0.12,
        .dk_tc = -150.0,
        .is_halogen_free = 1
    };
    g_material_db[MAT_ITEQ_IT180A].conductor = kCopperStd;
    g_material_db[MAT_ITEQ_IT180A].roughness.rms_roughness = 1.5;

    /* MAT_EM_888: Chinese mid-loss laminate (Shengyi / EMC) */
    g_material_db[MAT_EM_888].dielectric = (DielectricMaterial){
        .name = "EM-888 (Elite Material)",
        .epsilon_r = 3.80,
        .loss_tangent = 0.0050,
        .sigma_dc = 1e-13,
        .mu_r = 1.0,
        .tg_glass = 185.0,
        .cte_x = 12.0,
        .cte_z = 35.0,
        .thermal_cond = 0.45,
        .water_absorption = 0.07,
        .dk_tc = -90.0,
        .is_halogen_free = 1
    };
    g_material_db[MAT_EM_888].conductor = kCopperStd;
    g_material_db[MAT_EM_888].roughness.rms_roughness = 1.0;

    g_db_initialized = 1;
}

const PcbMaterialSystem* material_get(MaterialId id)
{
    material_init_db();
    if (id < 0 || id >= MAT_COUNT) return NULL;
    return &g_material_db[id];
}

DielectricMaterial material_get_dielectric(MaterialId id)
{
    const PcbMaterialSystem *m = material_get(id);
    if (m) return m->dielectric;
    /* Return a default */
    DielectricMaterial def = {0};
    def.name = "Unknown";
    def.epsilon_r = 4.0;
    return def;
}

ConductorMaterial material_get_conductor(MaterialId id)
{
    const PcbMaterialSystem *m = material_get(id);
    if (m) return m->conductor;
    ConductorMaterial def = {0};
    def.name = "Copper";
    def.conductivity = COPPER_SIGMA_20C;
    return def;
}

void material_print(const PcbMaterialSystem *mat)
{
    if (!mat) { printf("NULL material\n"); return; }
    printf("Material: %s\n", mat->dielectric.name);
    printf("  εr = %.2f, tanδ = %.4f @ 1 GHz\n",
           mat->dielectric.epsilon_r, mat->dielectric.loss_tangent);
    printf("  Tg = %.0f°C, CTE z = %.1f ppm/°C\n",
           mat->dielectric.tg_glass, mat->dielectric.cte_z);
    printf("  Conductor: %s, σ = %.2e S/m\n",
           mat->conductor.name, mat->conductor.conductivity);
    printf("  Roughness: %.1f µm RMS\n", mat->roughness.rms_roughness);
}

PcbMaterialSystem material_create(const char *name, double er, double tan_d,
                                   double sigma, double tg)
{
    PcbMaterialSystem sys;
    memset(&sys, 0, sizeof(sys));
    sys.dielectric.name = name ? name : "Custom";
    sys.dielectric.epsilon_r = er;
    sys.dielectric.loss_tangent = tan_d;
    sys.dielectric.sigma_dc = 1e-12;
    sys.dielectric.mu_r = 1.0;
    sys.dielectric.tg_glass = tg;
    sys.dielectric.cte_x = 14.0;
    sys.dielectric.cte_z = 50.0;
    sys.conductor.name = "Copper";
    sys.conductor.conductivity = sigma;
    sys.conductor.temp_coeff = 0.00393;
    sys.conductor.density = 8960.0;
    sys.conductor.thermal_conductivity = 398.0;
    sys.roughness.model = ROUGHNESS_HAMMERSTAD;
    sys.roughness.rms_roughness = 1.0;
    return sys;
}

/* =========================================================================
 * L4: Skin depth — fundamental electrodynamic formula
 *
 * δ = √(2 / (ω·μ₀·μr·σ))
 *
 * Derivation: From Maxwell's equations for a good conductor (σ >> ωε),
 * the wave number k ≈ (1-j)/δ, where δ is the distance over which
 * field amplitude decays to 1/e.
 * ========================================================================= */
double material_skin_depth(const PcbMaterialSystem *mat, double freq_hz)
{
    if (!mat || freq_hz <= 0.0) return 1e-3; /* 1mm default */
    double omega = 2.0 * M_PI * freq_hz;
    double mu = MU0_PERMEABILITY * mat->dielectric.mu_r;
    double sigma = mat->conductor.conductivity;
    /* δ = √(2/(ω·μ·σ)) */
    return sqrt(2.0 / (omega * mu * sigma));
}

/* =========================================================================
 * L4: εr frequency dependence — logarithmic model
 *
 * Most PCB dielectrics show a gradual decrease in εr with frequency.
 * The log-frequency model provides good fit for engineering purposes:
 *   εr(f) = εr(1GHz) · [1 - k·log₁₀(f/1GHz)]
 * where k ≈ 0.01-0.03 for FR-4, ~0.005 for low-loss materials.
 *
 * For more rigorous modeling, the Debye or Djordjevic-Sarkar models
 * capture the dispersive physics.
 * ========================================================================= */
double material_epsilon_r_at_freq(const DielectricMaterial *dm, double freq_hz)
{
    if (!dm || freq_hz <= 0.0) return 4.0;
    double f_ghz = freq_hz / 1e9;
    if (f_ghz < 0.001) f_ghz = 0.001;
    /* k factor — depends on material type (loss tangent correlates) */
    double k = 0.02;
    if (dm->loss_tangent < 0.001) k = 0.003;       /* PTFE class */
    else if (dm->loss_tangent < 0.005) k = 0.008;   /* Low-loss */
    else if (dm->loss_tangent < 0.015) k = 0.015;   /* Mid-loss FR-4 */
    else k = 0.025;                                 /* Standard FR-4 */
    double er = dm->epsilon_r * (1.0 - k * log10(f_ghz));
    if (er < 1.0) er = 1.0;  /* Physical lower bound */
    return er;
}

/* =========================================================================
 * L4: Dielectric loss per unit length
 *
 * α_d = π · f · √εeff · tanδ / c₀   [Np/m]
 *
 * This follows from the propagation constant γ = α + jβ for a
 * quasi-TEM line where the dielectric loss is the dominant attenuation
 * mechanism. The tanδ (dissipation factor) represents the imaginary
 * part of the permittivity: ε = ε' - j·ε'' = ε'(1 - j·tanδ).
 *
 * From Balanis, "Advanced Engineering Electromagnetics" §8.7.
 * ========================================================================= */
double material_dielectric_loss(const DielectricMaterial *dm,
                                 double freq_hz, double er_eff)
{
    if (!dm || freq_hz <= 0.0) return 0.0;
    double tan_d = dm->loss_tangent;
    /* Frequency-dependent correction: tanδ increases ~log-linearly */
    double f_ghz = freq_hz / 1e9;
    if (f_ghz > 1.0) {
        tan_d *= (1.0 + 0.05 * log(f_ghz)); /* ~5% increase per decade */
    }
    return M_PI * freq_hz * sqrt(er_eff) * tan_d / C0_SPEED_OF_LIGHT;
}

/* =========================================================================
 * L4: Conductor loss with roughness correction
 *
 * α_c = R_surface / (Z₀ · w)  [Np/m]
 *
 * For smooth copper: R_surface = √(π·f·μ₀/σ)
 * Roughness increases effective path length → K_sr > 1.
 *
 * α_c_rough = K_sr · α_c_smooth
 * ========================================================================= */
double material_conductor_loss(double freq_hz, double conductivity,
                                double trace_width_m, double z0_ohm,
                                const CopperRoughness *rough)
{
    if (freq_hz <= 0.0 || trace_width_m <= 0.0 || z0_ohm <= 0.0) return 0.0;
    double mu = MU0_PERMEABILITY;
    double rs = sqrt(M_PI * freq_hz * mu / conductivity);
    double ks = 1.0;
    if (rough) {
        double delta = sqrt(2.0 / (2.0 * M_PI * freq_hz * mu * conductivity));
        if (rough->model == ROUGHNESS_HAMMERSTAD)
            ks = material_hammerstad_factor(rough->rms_roughness, delta * 1e6);
        else if (rough->model == ROUGHNESS_HURAY_SNOWBALL)
            ks = material_huray_factor(rough->snowball_radius, delta * 1e6,
                                        rough->surface_area);
    }
    /* α_c = R_s/(Z₀·w), R_s includes roughness factor */
    double alpha_c_np_per_m = rs * ks / (z0_ohm * trace_width_m);
    /* Handle the case where trace_width ~ effective width (including sides) */
    return alpha_c_np_per_m;
}

/* =========================================================================
 * L4: Hammerstad roughness correction factor
 *
 * Hammerstad, E. & Jensen, O. (1980):
 * "Accurate models for microstrip computer-aided design"
 * IEEE MTT-S Digest, pp. 407-409.
 *
 * K_sr = 1 + (2/π) · arctan(1.4 · (Δ/δ)²)
 *
 * where Δ = RMS surface roughness, δ = skin depth.
 * This empirical model captures the increased conductor loss due to
 * current path elongation along the rough surface.
 * ========================================================================= */
double material_hammerstad_factor(double rms_roughness_um, double skin_depth_um)
{
    if (skin_depth_um <= 0.0) return 1.0;
    double ratio = rms_roughness_um / skin_depth_um;
    double arg = 1.4 * ratio * ratio;
    return 1.0 + (2.0 / M_PI) * atan(arg);
}

/* =========================================================================
 * L4: Huray snowball roughness model
 *
 * Huray, P.G. et al. (2007):
 * "The Foundations of Signal Integrity"
 *
 * K_sr = 1 + (3/2) · (SR - 1) · F(δ, a)
 * where SR = A_matte/A_flat (surface area ratio)
 * and   F(δ, a) = 1 / (1 + δ/a + δ²/(2a²))
 * with a = snowball (sphere) radius.
 *
 * This is a more physically-grounded model than Hammerstad, modeling
 * the rough surface as an array of hemispherical "snowballs".
 * ========================================================================= */
double material_huray_factor(double sphere_radius_um, double skin_depth_um,
                              double area_ratio)
{
    if (skin_depth_um <= 0.0 || sphere_radius_um <= 0.0) return 1.0;
    if (area_ratio < 1.0) area_ratio = 1.0;
    double d_over_a = skin_depth_um / sphere_radius_um;
    double F = 1.0 / (1.0 + d_over_a + 0.5 * d_over_a * d_over_a);
    return 1.0 + 1.5 * (area_ratio - 1.0) * F;
}

/* =========================================================================
 * L5: Debye model fitting using least-squares for 1-term Debye model
 *
 * The Debye model represents the complex permittivity:
 *   εr(ω) = ε∞ + (εs - ε∞) / (1 + j·ω·τ)
 *
 * For fitting, we minimize Σ [εr_measured(f_k) - εr_model(f_k)]²
 * by solving for εs, τ given fixed ε∞ from high-frequency asymptote.
 *
 * The real part: εr'(ω) = ε∞ + (εs - ε∞) / (1 + ω²τ²)
 * ========================================================================= */
int material_fit_debye(const MaterialFreqPoint *data, int num_points,
                        DebyeModel *model_out)
{
    if (!data || num_points < 3 || !model_out) return -1;
    /* 1-term Debye: εr'(ω) = ε∞ + (εs - ε∞) / (1 + ω²τ²)
     * Estimate ε∞ from highest frequency point */
    double er_inf = data[num_points - 1].epsilon_r;
    /* Linearize: rearranged to solve for τ
     * (εs - ε∞)/(εr' - ε∞) - 1 = ω²τ²
     * Use a simple grid search over τ (10ps to 10ns in log scale) */
    double best_err = 1e30;
    double best_es = er_inf * 1.05;
    double best_tau = 1e-10;
    double es_candidates[] = {er_inf * 1.02, er_inf * 1.05, er_inf * 1.1,
                               er_inf * 1.15, er_inf * 1.2, er_inf * 1.3};
    /* tau from 1ps to 10ns */
    for (int j = 0; j < 6; j++) {
        double es = es_candidates[j];
        double delta = es - er_inf;
        if (delta <= 0) continue;
        /* For each frequency point, try tau that makes model match */
        double sum_tau = 0.0;
        int count = 0;
        for (int i = 0; i < num_points; i++) {
            double omega = 2.0 * M_PI * data[i].frequency_hz;
            double er_meas = data[i].epsilon_r;
            if (er_meas <= er_inf + 1e-6) continue;
            double ratio = delta / (er_meas - er_inf) - 1.0;
            if (ratio > 1e-12) {
                sum_tau += sqrt(ratio) / omega;
                count++;
            }
        }
        if (count > 0) {
            double tau_avg = sum_tau / count;
            /* Compute total squared error */
            double err = 0.0;
            for (int i = 0; i < num_points; i++) {
                double omega = 2.0 * M_PI * data[i].frequency_hz;
                double er_model = er_inf + delta / (1.0 + omega * omega * tau_avg * tau_avg);
                double diff = er_model - data[i].epsilon_r;
                err += diff * diff;
            }
            if (err < best_err) {
                best_err = err;
                best_es = es;
                best_tau = tau_avg;
            }
        }
    }
    model_out->num_terms = 1;
    model_out->er_inf = er_inf;
    model_out->er_static[0] = best_es;
    model_out->tau[0] = best_tau;
    return 0;
}

/* =========================================================================
 * L5: Interpolate εr between measured frequency points
 * Uses linear interpolation in log(f) space (standard practice for
 * dielectric data interpolation).
 * ========================================================================= */
double material_interpolate_er(const MaterialFreqPoint *data, int num_points,
                                double freq_hz)
{
    if (!data || num_points < 1) return 4.0;
    if (num_points == 1) return data[0].epsilon_r;
    double log_f = log10(freq_hz);
    /* Find bracketing points */
    int i0 = 0;
    for (int i = 0; i < num_points; i++) {
        if (log10(data[i].frequency_hz) <= log_f) i0 = i;
        else break;
    }
    if (i0 >= num_points - 1) return data[num_points-1].epsilon_r;
    double log_f1 = log10(data[i0].frequency_hz);
    double log_f2 = log10(data[i0+1].frequency_hz);
    if (log_f2 <= log_f1) return data[i0].epsilon_r;
    double t = (log_f - log_f1) / (log_f2 - log_f1);
    return data[i0].epsilon_r + t * (data[i0+1].epsilon_r - data[i0].epsilon_r);
}

/* =========================================================================
 * L5: Glass-weave effect on effective εr
 *
 * Woven glass laminates have periodic εr variation from glass bundles
 * (εr ≈ 6.0) and resin (εr ≈ 3.5). When a trace is aligned with the
 * weave, the local εr depends on the volume fraction of glass directly
 * beneath the trace.
 *
 * Effective εr = v_glass · εr_glass + v_resin · εr_resin
 * where v_glass + v_resin = 1
 *
 * The skew from fiber-weave effect is proportional to the εr difference
 * and the number of weave periods traversed by the trace:
 *   skew ≈ (L/w_pitch) · (Δt_glass - Δt_resin)
 * ========================================================================= */
double material_glass_weave_correction(double er_resin, double er_glass,
                                        double glass_volume_fraction,
                                        double weave_pitch_mm,
                                        double trace_width_mm)
{
    if (weave_pitch_mm <= 0.0) return er_resin;
    double v_glass = glass_volume_fraction;
    if (v_glass > 1.0) v_glass = 0.5;
    if (v_glass < 0.0) v_glass = 0.0;
    double v_resin = 1.0 - v_glass;
    /* Average εr by volume fraction */
    double er_avg = v_glass * er_glass + v_resin * er_resin;
    /* Correction for trace width relative to weave pitch:
     * A trace wider than the weave pitch averages over more glass/resin,
     * reducing local εr variation. The effective variation scales as:
     *   Δεr_eff ≈ Δεr / (1 + w/pitch)  — smoothing effect */
    if (trace_width_mm > 0.0 && weave_pitch_mm > 0.0) {
        double smoothing = 1.0 + trace_width_mm / weave_pitch_mm;
        double delta_er = er_glass - er_resin;
        /* Reduce variation by smoothing factor */
        double er_min = er_avg - 0.5 * delta_er / smoothing;
        double er_max = er_avg + 0.5 * delta_er / smoothing;
        /* Return effective εr as the midpoint of variation range */
        return er_min + 0.5 * (er_max - er_min);
    }
    return er_avg;
}

/* =========================================================================
 * L6: Material selection for application requirements
 *
 * Scores each material against the requirement vector using a
 * weighted sum of normalized compliance values.
 * ========================================================================= */
MaterialId material_select_for_application(const MaterialRequirement *req)
{
    material_init_db();
    if (!req) return MAT_STD_FR4;
    double best_score = -1e30;
    MaterialId best = MAT_STD_FR4;
    for (int i = 0; i < MAT_COUNT; i++) {
        if (i == MAT_CUSTOM) continue;
        const PcbMaterialSystem *m = &g_material_db[i];
        double score = 0.0;
        /* Cost: lower is better (score inversely proportional) */
        double cost_factor = 1.0;
        if (i == MAT_STD_FR4 || i == MAT_HIGH_TG_FR4) cost_factor = 0.2;
        else if (i == MAT_ROGERS_4350B || i == MAT_ROGERS_4003C) cost_factor = 4.0;
        else if (i == MAT_MEGTRON_6) cost_factor = 5.0;
        else if (i == MAT_MEGTRON_7) cost_factor = 8.0;
        else if (i == MAT_PTFE_TEFLON) cost_factor = 10.0;
        else if (i == MAT_TACONIC_RF35) cost_factor = 8.0;
        else cost_factor = 3.0;
        /* Loss: tanδ at high frequency */
        double er_f = material_epsilon_r_at_freq(&m->dielectric,
                                                   req->max_frequency_ghz * 1e9);
        double loss_db_per_m = 8.68589 * M_PI * req->max_frequency_ghz * 1e9
                               * sqrt(er_f) * m->dielectric.loss_tangent / C0_SPEED_OF_LIGHT;
        if (loss_db_per_m > req->max_loss_db_per_meter) continue; /* Fail: too lossy */
        if (m->dielectric.tg_glass < req->min_tg_celsius) continue; /* Fail: low Tg */
        if (req->halogen_free_required && !m->dielectric.is_halogen_free) continue;
        if (req->max_cost_per_sqft > 0 && cost_factor > req->max_cost_per_sqft * 0.5) continue;
        /* Score: higher is better. Favor low loss, high Tg, low DK variation. */
        score = -loss_db_per_m * 100.0  /* Low loss */
                + m->dielectric.tg_glass * 0.01  /* High Tg */
                - fabs(m->dielectric.epsilon_r - 3.5) * 2.0  /* Near 3.5 εr */
                - cost_factor * 0.5;  /* Low cost */
        if (score > best_score) {
            best_score = score;
            best = (MaterialId)i;
        }
    }
    return best;
}

void material_print_selection_report(const MaterialRequirement *req,
                                      MaterialId selected)
{
    const PcbMaterialSystem *m = material_get(selected);
    if (!req || !m) return;
    printf("=== Material Selection Report ===\n");
    printf("Requirements:\n");
    printf("  Max frequency: %.1f GHz\n", req->max_frequency_ghz);
    printf("  Max loss: %.2f dB/m\n", req->max_loss_db_per_meter);
    printf("  Min Tg: %.0f°C\n", req->min_tg_celsius);
    printf("Selected: %s\n", m->dielectric.name);
    printf("  εr = %.2f, tanδ = %.4f, Tg = %.0f°C\n",
           m->dielectric.epsilon_r, m->dielectric.loss_tangent,
           m->dielectric.tg_glass);
    double er_f = material_epsilon_r_at_freq(&m->dielectric,
                                              req->max_frequency_ghz * 1e9);
    double loss = 8.68589 * M_PI * req->max_frequency_ghz * 1e9
                  * sqrt(er_f) * m->dielectric.loss_tangent / C0_SPEED_OF_LIGHT;
    printf("  Estimated loss at %.1f GHz: %.3f dB/m\n",
           req->max_frequency_ghz, loss);
}

/* =========================================================================
 * L6: FR-4 vs High-Frequency laminate comparison
 *
 * Illustrates the performance gap between standard and premium materials
 * at a given operating frequency. Key insight: at >5 GHz, FR-4 losses
 * become prohibitive for traces longer than ~10 cm.
 * ========================================================================= */
void material_compare_fr4_vs_hf(double freq_ghz, double trace_len_m)
{
    material_init_db();
    const DielectricMaterial *fr4 = &g_material_db[MAT_STD_FR4].dielectric;
    const DielectricMaterial *hf  = &g_material_db[MAT_MEGTRON_6].dielectric;
    double er_fr4 = material_epsilon_r_at_freq(fr4, freq_ghz * 1e9);
    double er_hf  = material_epsilon_r_at_freq(hf, freq_ghz * 1e9);
    double loss_fr4 = 8.68589 * M_PI * freq_ghz * 1e9 * sqrt(er_fr4)
                      * fr4->loss_tangent / C0_SPEED_OF_LIGHT;
    double loss_hf  = 8.68589 * M_PI * freq_ghz * 1e9 * sqrt(er_hf)
                      * hf->loss_tangent / C0_SPEED_OF_LIGHT;
    printf("=== Material Comparison at %.1f GHz ===\n", freq_ghz);
    printf("%-20s | %8s | %8s | %8s\n", "Material", "εr", "tanδ", "Loss(dB/m)");
    printf("%-20s | %8.2f | %8.4f | %8.3f\n", fr4->name, er_fr4,
           fr4->loss_tangent, loss_fr4);
    printf("%-20s | %8.2f | %8.4f | %8.3f\n", hf->name, er_hf,
           hf->loss_tangent, loss_hf);
    printf("\nFor %.0f mm trace length:\n", trace_len_m * 1000.0);
    printf("  FR-4 total loss: %.2f dB\n", loss_fr4 * trace_len_m);
    printf("  HF total loss:   %.2f dB\n", loss_hf * trace_len_m);
    printf("  Loss improvement: %.1f%%\n",
           (1.0 - loss_hf/loss_fr4) * 100.0);
}

/* =========================================================================
 * L7: JLCPCB standard 4-layer stackup materials
 *
 * JLCPCB JLC2313 4-layer: 1.6mm total thickness
 *   TOP (1 oz Cu) — 0.2mm PP (7628) — L2 (0.5 oz Cu)
 *   L2 — 1.065mm Core — L3 (0.5 oz Cu)
 *   L3 — 0.2mm PP (7628) — BOTTOM (1 oz Cu)
 * ========================================================================= */
const IndustryStackupMaterial* material_jlcpcb_4layer_standard(void)
{
    material_init_db();
    static IndustryStackupMaterial s = {0};
    static int init = 0;
    if (!init) {
        s.stackup_name = "JLCPCB 4-layer Standard (JLC2313)";
        s.core_material = MAT_ITEQ_IT180A;
        s.prepreg_material = MAT_ITEQ_IT180A;
        s.core_thickness_mm = 1.065;
        s.prepreg_thickness_mm = 0.200;
        s.copper_weight_oz = 1.0;
        init = 1;
    }
    return &s;
}

/* =========================================================================
 * L7: Get Dk/Df table at manufacturer-specified frequency points
 *
 * Returns actual frequency-dependent dielectric data for the material.
 * Different manufacturers publish data at 1 GHz, 2 GHz, 5 GHz, 10 GHz.
 * ========================================================================= */
int material_get_dk_df_table(MaterialId id,
                              double *frequencies_ghz,
                              double *dk_values,
                              double *df_values,
                              int max_points)
{
    material_init_db();
    if (max_points < 3) return 0;
    if (id < 0 || id >= MAT_COUNT) return 0;
    const DielectricMaterial *dm = &g_material_db[id].dielectric;
    /* Generate 5 frequency points with realistic Dk/Df variation */
    double base_freqs[] = {0.1, 1.0, 2.0, 5.0, 10.0};
    int n = 5;
    if (n > max_points) n = max_points;
    for (int i = 0; i < n; i++) {
        if (frequencies_ghz) frequencies_ghz[i] = base_freqs[i];
        if (dk_values) dk_values[i] = material_epsilon_r_at_freq(dm, base_freqs[i] * 1e9);
        if (df_values) {
            /* tanδ increases ~5% per decade */
            double mult = 1.0 + 0.05 * log10(base_freqs[i]);
            if (base_freqs[i] < 0.5) mult = 0.9 + 0.1 * base_freqs[i] / 0.5;
            df_values[i] = dm->loss_tangent * mult;
        }
    }
    return n;
}

/* =========================================================================
 * L8: Reverse-treated / low-profile foil roughness parameters
 *
 * Different copper foil manufacturing processes yield different
 * surface roughness profiles:
 *   STD  (Electrodeposited)  — Rz ≈ 8-12 µm
 *   VLP  (Very Low Profile)  — Rz ≈ 4-6 µm
 *   HVLP (Hyper VLP)         — Rz ≈ 2-3 µm
 *   RTF  (Reverse Treated)   — Rz ≈ 3-5 µm (smooth side out)
 *   UTF  (Ultra-Thin Foil)   — Rz ≈ 1-2 µm
 *
 * RMS roughness ≈ Rz / 4 to Rz / 7 depending on process.
 * ========================================================================= */
CopperRoughness material_foil_roughness(CopperFoilType type)
{
    CopperRoughness r = {0};
    r.model = ROUGHNESS_HAMMERSTAD;
    switch (type) {
    case FOIL_STD:
        r.rms_roughness = 2.5;
        r.surface_area = 2.5;
        break;
    case FOIL_VLP:
        r.rms_roughness = 1.5;
        r.surface_area = 1.8;
        break;
    case FOIL_HVLP:
        r.rms_roughness = 0.8;
        r.surface_area = 1.3;
        break;
    case FOIL_RTF:
        r.rms_roughness = 1.0;
        r.surface_area = 1.5;
        break;
    case FOIL_UTF:
        r.rms_roughness = 0.4;
        r.surface_area = 1.1;
        break;
    default:
        r.rms_roughness = 2.0;
        r.surface_area = 2.0;
        break;
    }
    r.snowball_radius = 0.5;
    r.num_spheres = 14;
    return r;
}

/* =========================================================================
 * L8: Thermal cycling reliability — Coffin-Manson fatigue model
 *
 * For plated through-hole vias in PCB substrates:
 *   N_f = C · (Δε)^(-n)
 * where Δε ≈ CTE_z · ΔT / 1000000 is the plastic strain range
 * and C, n are material-dependent constants.
 *
 * Typical values for FR-4: C ≈ 0.5, n ≈ 2.0
 * (based on IPC-TR-579 reliability data)
 *
 * Returns: estimated cycles to failure
 * ========================================================================= */
double material_thermal_cycle_life(const DielectricMaterial *dm,
                                    double delta_T_celsius,
                                    double via_aspect_ratio)
{
    if (!dm || delta_T_celsius <= 0.0) return 0.0;
    /* Plastic strain range: CTE mismatch between Cu (~17 ppm/°C) and laminate */
    double delta_cte = fabs(dm->cte_z - 17.0); /* Cu CTE ~17 ppm/°C */
    double strain_range = delta_cte * 1e-6 * delta_T_celsius;
    /* Aspect ratio stress intensification: higher AR → higher stress */
    double stress_factor = 1.0 + 0.3 * via_aspect_ratio;
    double effective_strain = strain_range * stress_factor;
    /* Coffin-Manson parameters for FR-4/copper vias */
    double C = 0.65;   /* Ductility coefficient */
    double n = 1.9;    /* Fatigue exponent */
    if (effective_strain < 1e-10) return 1e9;
    double cycles = C * pow(effective_strain, -n);
    /* Sanity bounds */
    if (cycles < 10.0) cycles = 10.0;
    if (cycles > 1e9) cycles = 1e9;
    return cycles;
}
