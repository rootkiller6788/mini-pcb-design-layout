/*
 * hs_stackup.c — 4-Layer PCB Stackup Implementation
 *
 * Implements all functions declared in hs_stackup.h.
 * Each function corresponds to an independent knowledge point
 * from the 4-layer high-speed PCB design domain.
 *
 * Knowledge coverage:
 *   L1: Material property database, layer type definitions
 *   L2: Stackup configuration, reference plane concepts
 *   L3: Skin depth formula, effective εr computation
 *   L4: Plane capacitance law, skin effect law
 *   L5: Stackup validation algorithm, roughness correction
 */

#include "hs_stackup.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ================================================================
 * Private constants
 * ================================================================ */

#define SPEED_OF_LIGHT       299792458.0    /* c₀ in m/s */
#define VACUUM_PERMITTIVITY  8.854187817e-12 /* ε₀ in F/m */
#define VACUUM_PERMEABILITY  1.2566370614e-6 /* μ₀ = 4π×10⁻⁷ H/m */
#define COPPER_RESISTIVITY   1.72e-8         /* ρ_Cu at 20°C in Ω·m */
#define COPPER_CONDUCTIVITY  5.8e7           /* σ_Cu at 20°C in S/m */
#define MILS_TO_METERS       2.54e-5         /* 1 mil = 25.4 μm */
#define OZ_TO_MILS           1.37            /* 1 oz/ft² → 1.37 mils nominal */
#define MM_TO_METERS         0.001
#define INCH_TO_METERS       0.0254

/* ================================================================
 * L1: Material property database
 *
 * Data sourced from manufacturer datasheets:
 *   - Rogers Corp. RO4000 series datasheet
 *   - Isola 370HR datasheet
 *   - Panasonic Megtron 6/7 datasheets
 *   - IPC-4101C base material specifications
 * ================================================================ */

static const hs_material_props_t material_db[HS_MAT_COUNT] = {
    [HS_MAT_FR4_STD] = {
        HS_MAT_FR4_STD, "FR-4 Standard",
        4.3, 0.020, 250.0, 140.0, 0.30
    },
    [HS_MAT_FR4_HIGH_TG] = {
        HS_MAT_FR4_HIGH_TG, "FR-4 High-Tg",
        4.0, 0.018, 200.0, 175.0, 0.25
    },
    [HS_MAT_ROGERS_4350B] = {
        HS_MAT_ROGERS_4350B, "Rogers RO4350B",
        3.48, 0.0037, 50.0, 280.0, 0.05
    },
    [HS_MAT_ROGERS_4003C] = {
        HS_MAT_ROGERS_4003C, "Rogers RO4003C",
        3.38, 0.0027, 40.0, 280.0, 0.04
    },
    [HS_MAT_ISOLA_370HR] = {
        HS_MAT_ISOLA_370HR, "Isola 370HR",
        3.92, 0.020, 150.0, 180.0, 0.15
    },
    [HS_MAT_MEGTRON_6] = {
        HS_MAT_MEGTRON_6, "Panasonic Megtron 6",
        3.55, 0.002, 85.0, 185.0, 0.03
    },
    [HS_MAT_MEGTRON_7] = {
        HS_MAT_MEGTRON_7, "Panasonic Megtron 7",
        3.35, 0.0015, 40.0, 210.0, 0.02
    },
    [HS_MAT_PTFE] = {
        HS_MAT_PTFE, "PTFE (Teflon)",
        2.10, 0.0010, -400.0, 250.0, 0.02
    },
    [HS_MAT_ALUMINA] = {
        HS_MAT_ALUMINA, "Alumina 96%",
        9.20, 0.0002, 136.0, 1600.0, 0.01
    }
};

/* ================================================================
 * L4: Skin depth — Fundamental physical law
 *
 * Theorem: δ = √(ρ / (π f μ₀ μ_r))
 *
 * Derivation from Maxwell's equations in a good conductor:
 *   ∇²E = jωμσ E  →  E(z) = E₀ e^(-z/δ) e^(-jz/δ)
 * where δ = √(2/(ωμσ)) = √(ρ/(πfμ₀μ_r))
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_skin_depth(double frequency_hz, double resistivity_ohm_m,
                     double relative_permeability)
{
    if (frequency_hz <= 0.0 || resistivity_ohm_m <= 0.0 ||
        relative_permeability <= 0.0) {
        return 0.0;
    }
    double mu = VACUUM_PERMEABILITY * relative_permeability;
    double skin = sqrt(resistivity_ohm_m / (M_PI * frequency_hz * mu));
    return skin;
}

/* ================================================================
 * L3: Effective εr — Frequency-dependent (Kirschning-Jansen)
 *
 * This is a dispersion model accounting for the fact that the
 * effective dielectric constant increases with frequency as more
 * field energy concentrates in the substrate.
 *
 * Reference: Kirschning & Jansen, Electronics Letters, 1982
 * Complexity: O(1)
 * ================================================================ */
double hs_effective_er_frequency(double er_bulk, double frequency_hz,
                                  double height_m, double width_m)
{
    if (er_bulk <= 1.0 || frequency_hz <= 0.0 ||
        height_m <= 0.0 || width_m <= 0.0) {
        return er_bulk;
    }

    double w_over_h = width_m / height_m;
    double er_static = hs_effective_er_static(er_bulk, height_m, width_m, 0.0);

    /* Compute the frequency-dependent term P(f) */
    double f_norm = frequency_hz * height_m * 4.0e-9;
    /* Guard: avoid numerical issues */
    if (f_norm < 1e-6) return er_static;

    double p1 = 0.27488 + w_over_h * (0.6315 + 0.525 / pow(1.0 + 0.0157 * f_norm, 20.0))
                - 0.065683 * exp(-8.7513 * w_over_h);
    double p2 = 0.33622 * (1.0 - exp(-0.03442 * er_bulk));

    double p_f = p1 * p2 * sqrt(f_norm) * (1.0 + 1.0 / (1.0 + f_norm));

    double er_eff_f = er_bulk - (er_bulk - er_static) / (1.0 + p_f);
    return er_eff_f;
}

/* ================================================================
 * L3: Static effective εr — Wheeler/Schneider formula
 *
 * For a microstrip, the effective dielectric constant is a
 * weighted average of εr (substrate) and 1 (air above).
 *
 * The Wheeler formula provides accurate εeff for 0.1 < w/h < 10.
 *
 * Reference: H.A. Wheeler, IEEE Trans. MTT, 1977
 * Complexity: O(1)
 * ================================================================ */
double hs_effective_er_static(double er_bulk, double height_m,
                               double width_m, double thickness_m)
{
    if (er_bulk <= 1.0 || height_m <= 0.0 || width_m <= 0.0) {
        return 1.0;
    }

    double w_over_h = width_m / height_m;
    /* Effective width correction for finite thickness (t > 0) */
    double w_eff = width_m;
    if (thickness_m > 0.0 && w_over_h < 1.0 / (2.0 * M_PI)) {
        double delta_w = (thickness_m / M_PI) *
                         (1.0 + log(2.0 * height_m / thickness_m));
        w_eff = width_m + delta_w;
    }

    double w_over_h_eff = w_eff / height_m;

    double er_eff;
    if (w_over_h_eff <= 1.0) {
        er_eff = (er_bulk + 1.0) / 2.0 +
                 (er_bulk - 1.0) / 2.0 *
                 (1.0 / sqrt(1.0 + 12.0 / w_over_h_eff) +
                  0.04 * pow(1.0 - w_over_h_eff, 2.0));
    } else {
        er_eff = (er_bulk + 1.0) / 2.0 +
                 (er_bulk - 1.0) / 2.0 *
                 (1.0 / sqrt(1.0 + 12.0 / w_over_h_eff));
    }

    return er_eff;
}

/* ================================================================
 * L4: Plane capacitance — Fundamental electrostatic law
 *
 * C = ε₀ εr A / d
 *
 * This is the parallel-plate capacitance formula derived from
 * Gauss's law: ∮D·dA = Q → E = σ/ε → V = Ed → C = Q/V = εA/d
 * ================================================================ */
double hs_plane_capacitance(double area_m2, double separation_m, double er)
{
    if (area_m2 <= 0.0 || separation_m <= 0.0 || er <= 0.0) {
        return 0.0;
    }
    return VACUUM_PERMITTIVITY * er * area_m2 / separation_m;
}

/* ================================================================
 * L3: Plane inductance per square
 *
 * For a plane pair, the loop inductance per square is approximately:
 *   L_sq ≈ μ₀ × d   (for d << w)
 *
 * This comes from the magnetic energy stored between two
 * closely-spaced parallel current sheets.
 *
 * Reference: Novak, "PDN Design Methodologies", §3.2
 * ================================================================ */
double hs_plane_inductance_per_square(double separation_m, double width_m)
{
    if (separation_m <= 0.0 || width_m <= 0.0) {
        return 0.0;
    }
    return VACUUM_PERMEABILITY * separation_m / width_m;
}

/* ================================================================
 * L5: Trace resistance with skin effect
 *
 * At low frequencies, current is uniform through the cross-section.
 * As frequency increases, current crowds to the surface (skin effect),
 * increasing effective AC resistance.
 *
 * The transition frequency f_break occurs when δ ≈ t/2:
 *   f_break = 4ρ / (π μ₀ t²)
 *
 * For 1 oz Cu (t=35 μm): f_break ≈ 14 MHz
 * For 0.5 oz Cu (t=17.5 μm): f_break ≈ 56 MHz
 *
 * Reference: Johnson & Graham, §5.4
 * Complexity: O(1)
 * ================================================================ */
double hs_trace_resistance_per_meter(double width_m, double thickness_m,
                                      double frequency_hz,
                                      double resistivity_ohm_m)
{
    if (width_m <= 0.0 || thickness_m <= 0.0 || resistivity_ohm_m <= 0.0) {
        return 0.0;
    }

    /* DC resistance */
    double r_dc = resistivity_ohm_m / (width_m * thickness_m);

    if (frequency_hz <= 0.0) {
        return r_dc;
    }

    /* Skin depth at this frequency */
    double delta = hs_skin_depth(frequency_hz, resistivity_ohm_m, 1.0);
    if (delta <= 0.0) return r_dc;

    /* AC resistance: current flows in skin depth on both top and bottom surfaces,
     * and in both sidewalls for narrow traces. For simplicity, use the
     * model from Johnson & Graham Eq. 5.10 */
    double r_ac;
    if (delta < thickness_m / 2.0) {
        /* Skin effect dominant: current flows in δ on top+bottom+sidewalls */
        double perimeter = 2.0 * width_m + 2.0 * thickness_m;
        r_ac = resistivity_ohm_m / (perimeter * delta);
    } else {
        /* Transition region: smoothly interpolate */
        double ratio = thickness_m / (2.0 * delta);
        r_ac = r_dc * sqrt(1.0 + ratio * ratio);
    }

    return r_ac;
}

/* ================================================================
 * L1: Copper weight conversion
 *
 * Standard industry conversion: 1 oz/ft² → 34.8 μm (1.37 mils)
 *
 * This is nominal thickness after plating. Actual thickness may
 * vary by ±10% due to manufacturing tolerances.
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_copper_weight_to_thickness(double copper_weight_oz)
{
    if (copper_weight_oz <= 0.0) return 0.0;
    return copper_weight_oz * OZ_TO_MILS * MILS_TO_METERS;
}

/* ================================================================
 * L2: Stackup initialization and configuration
 *
 * These functions encode the standard 4-layer stackup configurations
 * used in the PCB industry for high-speed digital designs.
 *
 * The GND-SIG-SIG-GND configuration provides superior EMI performance
 * by shielding inner signal layers between two ground planes.
 *
 * Reference: IPC-2221, §5.2.2; Johnson & Graham, §5.6
 * Complexity: O(1)
 * ================================================================ */

void hs_stackup_init_default(hs_stackup_t *stackup)
{
    if (!stackup) return;

    memset(stackup, 0, sizeof(*stackup));
    stackup->config = HS_CONFIG_SIG_GND_PWR_SIG;

    /* L1: Top Signal */
    stackup->layers[0].layer_index = 1;
    stackup->layers[0].type = HS_LAYER_SIGNAL;
    stackup->layers[0].copper_weight = 1.0;
    stackup->layers[0].copper_thickness = 1.37 * MILS_TO_METERS;
    stackup->layers[0].copper_conductivity = COPPER_CONDUCTIVITY;
    stackup->layers[0].net_name = "SIGNAL_TOP";

    /* L2: Ground Plane */
    stackup->layers[1].layer_index = 2;
    stackup->layers[1].type = HS_LAYER_PLANE;
    stackup->layers[1].copper_weight = 1.0;
    stackup->layers[1].copper_thickness = 1.37 * MILS_TO_METERS;
    stackup->layers[1].copper_conductivity = COPPER_CONDUCTIVITY;
    stackup->layers[1].net_name = "GND";

    /* L3: Power Plane */
    stackup->layers[2].layer_index = 3;
    stackup->layers[2].type = HS_LAYER_PLANE;
    stackup->layers[2].copper_weight = 1.0;
    stackup->layers[2].copper_thickness = 1.37 * MILS_TO_METERS;
    stackup->layers[2].copper_conductivity = COPPER_CONDUCTIVITY;
    stackup->layers[2].net_name = "VCC";

    /* L4: Bottom Signal */
    stackup->layers[3].layer_index = 4;
    stackup->layers[3].type = HS_LAYER_SIGNAL;
    stackup->layers[3].copper_weight = 1.0;
    stackup->layers[3].copper_thickness = 1.37 * MILS_TO_METERS;
    stackup->layers[3].copper_conductivity = COPPER_CONDUCTIVITY;
    stackup->layers[3].net_name = "SIGNAL_BOT";

    /* Dielectric: standard FR-4, 62 mil total */
    stackup->dielectric_type = HS_MAT_FR4_STD;
    stackup->prepreg_thickness[0] = 8.0 * MILS_TO_METERS;
    stackup->prepreg_thickness[1] = 8.0 * MILS_TO_METERS;
    stackup->core_thickness = 40.0 * MILS_TO_METERS;

    /* Compute total thickness */
    double copper_total = 0.0;
    for (int i = 0; i < 4; i++) {
        copper_total += stackup->layers[i].copper_thickness;
    }
    stackup->total_thickness = copper_total +
        stackup->prepreg_thickness[0] + stackup->prepreg_thickness[1] +
        stackup->core_thickness;

    stackup->outer_roughness = HS_ROUGH_STANDARD;
    stackup->inner_roughness = HS_ROUGH_STANDARD;

    snprintf(stackup->description, sizeof(stackup->description),
             "4-Layer SIG-GND-PWR-SIG, FR-4, 62mil total, 1oz Cu");
}

void hs_stackup_init_config(hs_stackup_t *stackup, hs_stackup_config_t config,
                             double total_thickness_mils, hs_material_t material)
{
    if (!stackup) return;

    /* Start with defaults and then override */
    hs_stackup_init_default(stackup);

    stackup->config = config;
    stackup->dielectric_type = material;

    double total_m = total_thickness_mils * MILS_TO_METERS;
    double copper_per_layer = hs_copper_weight_to_thickness(1.0);
    double copper_total = 4.0 * copper_per_layer;
    double diel_total = total_m - copper_total;
    double diel_per_layer = diel_total / 3.0; /* 2 prepreg + 1 core = 3 dielectric layers */

    double mils_total = total_m;

    switch (config) {
    case HS_CONFIG_SIG_GND_PWR_SIG:
        stackup->prepreg_thickness[0] = diel_per_layer;
        stackup->core_thickness = diel_per_layer;
        stackup->prepreg_thickness[1] = diel_per_layer;
        snprintf(stackup->description, sizeof(stackup->description),
                 "4L SIG-GND-PWR-SIG, %.0fmil", total_thickness_mils);
        break;

    case HS_CONFIG_GND_SIG_SIG_GND:
        /* Outer layers are ground planes, inner are signals */
        stackup->layers[0].type = HS_LAYER_PLANE;
        stackup->layers[0].net_name = "GND_TOP";
        stackup->layers[1].type = HS_LAYER_SIGNAL;
        stackup->layers[1].net_name = "SIGNAL_INNER1";
        stackup->layers[2].type = HS_LAYER_SIGNAL;
        stackup->layers[2].net_name = "SIGNAL_INNER2";
        stackup->layers[3].type = HS_LAYER_PLANE;
        stackup->layers[3].net_name = "GND_BOT";
        stackup->prepreg_thickness[0] = diel_per_layer;
        stackup->core_thickness = diel_per_layer;
        stackup->prepreg_thickness[1] = diel_per_layer;
        snprintf(stackup->description, sizeof(stackup->description),
                 "4L GND-SIG-SIG-GND, %.0fmil", total_thickness_mils);
        break;

    case HS_CONFIG_SIG_GND_GND_SIG:
        stackup->layers[1].type = HS_LAYER_PLANE;
        stackup->layers[1].net_name = "GND1";
        stackup->layers[2].type = HS_LAYER_PLANE;
        stackup->layers[2].net_name = "GND2";
        stackup->prepreg_thickness[0] = diel_per_layer;
        stackup->core_thickness = diel_per_layer;
        stackup->prepreg_thickness[1] = diel_per_layer;
        snprintf(stackup->description, sizeof(stackup->description),
                 "4L SIG-GND-GND-SIG, %.0fmil", total_thickness_mils);
        break;

    case HS_CONFIG_GND_SIG_PWR_SIG:
        stackup->layers[0].type = HS_LAYER_PLANE;
        stackup->layers[0].net_name = "GND";
        stackup->layers[1].type = HS_LAYER_SIGNAL;
        stackup->layers[1].net_name = "SIGNAL_INNER";
        stackup->layers[2].type = HS_LAYER_PLANE;
        stackup->layers[2].net_name = "VCC";
        stackup->layers[3].type = HS_LAYER_SIGNAL;
        stackup->layers[3].net_name = "SIGNAL_BOT";
        stackup->prepreg_thickness[0] = diel_per_layer;
        stackup->core_thickness = diel_per_layer;
        stackup->prepreg_thickness[1] = diel_per_layer;
        snprintf(stackup->description, sizeof(stackup->description),
                 "4L GND-SIG-PWR-SIG, %.0fmil", total_thickness_mils);
        break;
    }

    stackup->total_thickness = mils_total;
}

/* ================================================================
 * L5: Stackup validation
 *
 * Validates physical consistency of a stackup definition.
 * Checks manufacturing constraints per IPC-2221.
 *
 * Returns: 0 = valid, non-zero = error code
 * ================================================================ */
int hs_stackup_validate(const hs_stackup_t *stackup)
{
    if (!stackup) return -1;

    /* Check 1: All layers have positive dimensions */
    for (int i = 0; i < 4; i++) {
        if (stackup->layers[i].copper_thickness <= 0.0) return 100 + i;
        if (stackup->layers[i].copper_conductivity <= 0.0) return 200 + i;
    }

    /* Check 2: Dielectric thicknesses are positive */
    if (stackup->prepreg_thickness[0] <= 0.0) return 300;
    if (stackup->prepreg_thickness[1] <= 0.0) return 301;
    if (stackup->core_thickness <= 0.0) return 302;

    /* Check 3: Total thickness matches sum of parts (within 1% tolerance) */
    double computed_total = 0.0;
    for (int i = 0; i < 4; i++) {
        computed_total += stackup->layers[i].copper_thickness;
    }
    computed_total += stackup->prepreg_thickness[0] +
                      stackup->prepreg_thickness[1] +
                      stackup->core_thickness;

    double tolerance = 0.01 * stackup->total_thickness;
    if (fabs(computed_total - stackup->total_thickness) > tolerance) {
        return 500;
    }

    /* Check 4: Reference plane adjacency for signal layers */
    for (int i = 0; i < 4; i++) {
        if (stackup->layers[i].type == HS_LAYER_SIGNAL) {
            int has_adjacent_plane = 0;
            if (i > 0 && stackup->layers[i-1].type == HS_LAYER_PLANE) {
                has_adjacent_plane = 1;
            }
            if (i < 3 && stackup->layers[i+1].type == HS_LAYER_PLANE) {
                has_adjacent_plane = 1;
            }
            if (!has_adjacent_plane) return 400 + i;
        }
    }

    /* Check 5: Copper thickness within manufacturing limits */
    for (int i = 0; i < 4; i++) {
        if (stackup->layers[i].copper_thickness > 6.0 * OZ_TO_MILS * MILS_TO_METERS) {
            return 600 + i; /* Max 6 oz copper */
        }
    }

    /* Check 6: Minimum dielectric thickness (2 mils = 0.051mm for FR-4) */
    double min_diel = 2.0 * MILS_TO_METERS;
    if (stackup->prepreg_thickness[0] < min_diel) return 700;
    if (stackup->prepreg_thickness[1] < min_diel) return 701;
    if (stackup->core_thickness < min_diel) return 702;

    return 0; /* Valid */
}

/* ================================================================
 * L1: Material property lookup
 * Complexity: O(1)
 * ================================================================ */
const hs_material_props_t *hs_material_get_props(hs_material_t material)
{
    if (material < 0 || material >= HS_MAT_COUNT) return NULL;
    return &material_db[material];
}

/* ================================================================
 * L5: Roughness correction — Hammerstad-Bekkadal model
 *
 * The surface roughness of copper increases AC resistance
 * because the effective path length for current flow is longer.
 *
 * K_sr = 1 + (2/π) × arctan(1.4 × (Rq/δ)²)
 *
 * K_sr is applied as a multiplier to the smooth-surface AC resistance.
 *
 * For Rq = 0.4 μm, δ = 0.66 μm (10 GHz): K_sr ≈ 1.15 (15% increase)
 * For Rq = 0.4 μm, δ = 0.21 μm (100 GHz): K_sr ≈ 1.35 (35% increase)
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_roughness_correction(double rms_roughness_m, double skin_depth_m)
{
    if (rms_roughness_m < 0.0 || skin_depth_m <= 0.0) {
        return 1.0;
    }
    if (rms_roughness_m == 0.0) return 1.0;

    double ratio = rms_roughness_m / skin_depth_m;
    double arg = 1.4 * ratio * ratio;
    double correction = 1.0 + (2.0 / M_PI) * atan(arg);
    return correction;
}

/* ================================================================
 * L3: Average dielectric constant (thickness-weighted)
 *
 * For a stack with N different dielectric layers, the effective
 * bulk permittivity is the thickness-weighted average.
 *
 * Complexity: O(N)
 * ================================================================ */
double hs_average_er(const double *er_values, const double *thicknesses_m,
                      size_t count)
{
    if (!er_values || !thicknesses_m || count == 0) return 1.0;

    double sum_er_t = 0.0;
    double sum_t = 0.0;

    for (size_t i = 0; i < count; i++) {
        if (thicknesses_m[i] <= 0.0) continue;
        sum_er_t += er_values[i] * thicknesses_m[i];
        sum_t += thicknesses_m[i];
    }

    if (sum_t <= 0.0) return 1.0;
    return sum_er_t / sum_t;
}

/* ================================================================
 * L3: Propagation velocity
 *
 * v = c₀ / √εeff
 *
 * For FR-4 (εeff ≈ 3.4): v ≈ 1.63e8 m/s
 * For Rogers 4350B (εeff ≈ 3.0): v ≈ 1.73e8 m/s
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_propagation_velocity(double er_effective)
{
    if (er_effective <= 0.0) return 0.0;
    return SPEED_OF_LIGHT / sqrt(er_effective);
}

/* ================================================================
 * L3: Propagation delay per meter
 *
 * t_pd = 1/v = √εeff / c₀  [s/m]
 *
 * For FR-4: t_pd ≈ 6.16 ns/m ≈ 156 ps/inch
 * For Rogers: t_pd ≈ 5.77 ns/m ≈ 147 ps/inch
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_propagation_delay_per_meter(double er_effective)
{
    if (er_effective <= 0.0) return 0.0;
    return sqrt(er_effective) / SPEED_OF_LIGHT;
}

/* ================================================================
 * L2: Stackup visualization
 * Complexity: O(1)
 * ================================================================ */
void hs_stackup_print(const hs_stackup_t *stackup, char units)
{
    if (!stackup) return;

    const hs_material_props_t *mat = hs_material_get_props(stackup->dielectric_type);
    const char *mat_name = mat ? mat->name : "Unknown";

    printf("===============================================\n");
    printf(" 4-Layer PCB Stackup: %s\n", stackup->description);
    printf(" Material: %s (εr=%.2f, tanδ=%.4f)\n",
           mat_name, mat ? mat->er : 0.0, mat ? mat->tan_d : 0.0);
    printf("===============================================\n");

    for (int i = 0; i < 4; i++) {
        const hs_layer_entry_t *l = &stackup->layers[i];
        const char *type_str;
        switch (l->type) {
        case HS_LAYER_SIGNAL: type_str = "SIGNAL"; break;
        case HS_LAYER_PLANE:  type_str = "PLANE";  break;
        case HS_LAYER_MIXED:  type_str = "MIXED";  break;
        default: type_str = "UNKNOWN";
        }

        if (units == 'm') {
            printf(" L%d: %-6s  Cu: %.0fμm  Net: %s\n",
                   l->layer_index, type_str,
                   l->copper_thickness * 1e6, l->net_name);
        } else {
            printf(" L%d: %-6s  Cu: %.2fmil  Net: %s\n",
                   l->layer_index, type_str,
                   l->copper_thickness / MILS_TO_METERS, l->net_name);
        }
    }

    /* Print dielectric layers between copper layers */
    if (units == 'm') {
        printf(" Prepreg (L1-L2): %.0fμm\n", stackup->prepreg_thickness[0] * 1e6);
        printf(" Core (L2-L3):    %.0fμm\n", stackup->core_thickness * 1e6);
        printf(" Prepreg (L3-L4): %.0fμm\n", stackup->prepreg_thickness[1] * 1e6);
        printf(" Total thickness: %.0fμm\n", stackup->total_thickness * 1e6);
    } else {
        printf(" Prepreg (L1-L2): %.1fmil\n",
               stackup->prepreg_thickness[0] / MILS_TO_METERS);
        printf(" Core (L2-L3):    %.1fmil\n",
               stackup->core_thickness / MILS_TO_METERS);
        printf(" Prepreg (L3-L4): %.1fmil\n",
               stackup->prepreg_thickness[1] / MILS_TO_METERS);
        printf(" Total thickness: %.1fmil\n",
               stackup->total_thickness / MILS_TO_METERS);
    }
    printf("===============================================\n");
}
