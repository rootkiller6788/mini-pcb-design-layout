#ifndef PCB_MATERIAL_H
#define PCB_MATERIAL_H

#include <stddef.h>

/* ============================================================================
 * L1: Core Definitions — PCB substrate and conductor material properties
 *
 * A PCB material system is defined by its dielectric substrate (εr, tanδ),
 * conductive layers (σ, roughness), and their thermal/mechanical properties.
 * These parameters directly determine impedance, loss, and reliability.
 *
 * Fundamental relationships:
 *   v_p = c / √εeff        propagation velocity
 *   δ   = √(2/(ω·μ₀·σ))    skin depth
 *   α_d = π·f·√εeff·tanδ/c dielectric loss constant
 * ========================================================================= */

/* L1: Dielectric material definition */
typedef struct {
    const char *name;           /* Material name (e.g., "FR-4", "Rogers 4350B") */
    double    epsilon_r;        /* Relative permittivity (dielectric constant) */
    double    loss_tangent;     /* Dissipation factor tanδ at 1 GHz */
    double    sigma_dc;         /* DC conductivity (S/m), ~0 for good dielectrics */
    double    mu_r;             /* Relative permeability, typically 1.0 */
    double    tg_glass;         /* Glass transition temperature (°C) */
    double    cte_x;            /* CTE in X/Y direction (ppm/°C) */
    double    cte_z;            /* CTE in Z direction (ppm/°C) */
    double    thermal_cond;     /* Thermal conductivity (W/m·K) */
    double    water_absorption; /* Water absorption (%) */
    double    dk_tc;            /* εr temperature coefficient (ppm/°C) */
    int       is_halogen_free;  /* Halogen-free flag */
} DielectricMaterial;

/* L1: Conductor material definition */
typedef struct {
    const char *name;           /* "Copper", "Silver", "Aluminum" */
    double    conductivity;     /* Electrical conductivity (S/m) at 20°C */
    double    temp_coeff;       /* Temperature coefficient of resistance (/°C) */
    double    density;          /* Density (kg/m³) */
    double    thermal_conductivity; /* (W/m·K) */
} ConductorMaterial;

/* L1: Copper foil roughness model types */
typedef enum {
    ROUGHNESS_NONE   = 0,       /* Ideal smooth conductor */
    ROUGHNESS_HAMMERSTAD = 1,   /* Hammerstad-Jensen model (classic) */
    ROUGHNESS_HURAY_SNOWBALL = 2, /* Huray "snowball" model */
    ROUGHNESS_GROISSE = 3,      /* Groisse unified model */
    ROUGHNESS_CANNONBALL = 4    /* Cannonball-Huray variant */
} RoughnessModel;

/* L1: Copper roughness parameters */
typedef struct {
    RoughnessModel model;
    double    rms_roughness;    /* RMS surface roughness (µm), typically 0.3-10 */
    double    snowball_radius;  /* Snowball radius for Huray model (µm) */
    double    surface_area;     /* Effective surface area ratio (A_matte/A_flat) */
    int       num_spheres;      /* Number of spheres in Huray model per unit area */
} CopperRoughness;

/* L1: Frequency-dependent material property structure */
typedef struct {
    double    frequency_hz;     /* Frequency point (Hz) */
    double    epsilon_r;        /* εr at this frequency */
    double    loss_tangent;     /* tanδ at this frequency */
} MaterialFreqPoint;

/* L1: Complete material system definition */
typedef struct {
    DielectricMaterial dielectric;
    ConductorMaterial   conductor;
    CopperRoughness     roughness;
} PcbMaterialSystem;

/* ===================================================================
 * L2: Core Concepts — Material databases and frequency dependence
 *
 * PCB materials exhibit significant frequency-dependent behavior.
 * The Djordjevic-Sarkar model captures broadband εr variation:
 *   εr(f) = εr_inf + Δε / (1 + j·f/f₀)
 * =================================================================== */

/* L2: Pre-defined material database entries */
typedef enum {
    MAT_STD_FR4           = 0,
    MAT_HIGH_TG_FR4       = 1,
    MAT_ROGERS_4350B      = 2,
    MAT_ROGERS_4003C      = 3,
    MAT_ISOLA_370HR       = 4,
    MAT_ISOLA_FR408HR     = 5,
    MAT_MEGTRON_6         = 6,
    MAT_MEGTRON_7         = 7,
    MAT_PTFE_TEFLON       = 8,
    MAT_TACONIC_RF35      = 9,
    MAT_NELCO_N4000_13    = 10,
    MAT_ITEQ_IT180A       = 11,
    MAT_EM_888             = 12,
    MAT_CUSTOM             = 13,
    MAT_COUNT
} MaterialId;

/* L2: Material database query */
const PcbMaterialSystem* material_get(MaterialId id);
DielectricMaterial       material_get_dielectric(MaterialId id);
ConductorMaterial        material_get_conductor(MaterialId id);
void                     material_print(const PcbMaterialSystem *mat);

/* L2: Custom material creation */
PcbMaterialSystem material_create(const char *name, double er, double tan_d,
                                   double sigma, double tg);

/* L3: Mathematical Structures — Frequency-dependent dielectric models */

/* L3: Djordjevic-Sarkar broadband model
 *   εr(f) = εr' - j·εr''  with causal Kramers-Kronig consistency */
typedef struct {
    double    er_inf;          /* ε∞ (high-frequency limit) */
    double    delta_er;        /* Δε = εs - ε∞ */
    double    f0;              /* Pole frequency (Hz) */
    double    sigma_dc;        /* DC conductivity (S/m) */
} DjordjevicSarkarModel;

/* L3: Debye relaxation model for polar dielectrics
 *   εr(ω) = ε∞ + Σ (εs_k - ε∞_k) / (1 + j·ω·τ_k) */
typedef struct {
    int       num_terms;       /* Number of Debye relaxation terms (1-5) */
    double    er_inf;          /* ε∞ */
    double    er_static[5];    /* Static permittivity for each term */
    double    tau[5];          /* Relaxation time τ for each term (seconds) */
} DebyeModel;

/* L4: Fundamental Laws — Skin depth and frequency dependence */

/* L4: Skin depth calculation from Maxwell's equations
 *   δ = √(2 / (ω·μ·σ)) = √(ρ / (π·f·μ))
 *   where ω = 2πf, μ = μ₀·μr, ρ = 1/σ */
double material_skin_depth(const PcbMaterialSystem *mat, double freq_hz);

/* L4: Effective dielectric constant with frequency
 *   Uses the log-frequency-linear model commonly used in PCB EDA tools */
double material_epsilon_r_at_freq(const DielectricMaterial *dm, double freq_hz);

/* L4: Dielectric loss per unit length
 *   α_d = π · f · √εeff · tanδ / c₀   [Np/m]
 *   Convert to dB/m: α_dB = 8.68589 · α_d [Np/m to dB/m] */
double material_dielectric_loss(const DielectricMaterial *dm,
                                 double freq_hz, double er_eff);

/* L4: Conductor loss per unit length considering roughness
 *   α_c = R_surface / (Z₀ · w)   [Np/m]
 *   with roughness enhancement factor K_sr (Hammerstad):
 *   K_sr = 1 + (2/π)·arctan(1.4·(Δ²/δ²)), Δ = RMS roughness */
double material_conductor_loss(double freq_hz, double conductivity,
                                double trace_width_m, double z0_ohm,
                                const CopperRoughness *rough);

/* L4: Hammerstad roughness correction factor
 *   K_sr = 1 + (2/π) · arctan(1.4 · (rms/δ)²) */
double material_hammerstad_factor(double rms_roughness_um, double skin_depth_um);

/* L4: Huray snowball roughness correction factor
 *   K_sr = 1 + (3/2) · (A_matte/A_flat - 1) · [1/(1 + δ/a + δ²/(2a²))] */
double material_huray_factor(double sphere_radius_um, double skin_depth_um,
                              double area_ratio);

/* L5: Algorithms — Material parameter extraction and fitting */

/* L5: Extract Debye model parameters from εr(f) measured data points
 *     Uses least-squares fitting for 1-term Debye model */
int material_fit_debye(const MaterialFreqPoint *data, int num_points,
                        DebyeModel *model_out);

/* L5: Interpolate εr between frequency points (linear in log-f) */
double material_interpolate_er(const MaterialFreqPoint *data, int num_points,
                                double freq_hz);

/* L5: Compute glass-weave effect correction for εr
 *     Accounts for resin-rich / glass-rich regions in woven glass laminates.
 *     Effective εr = v_glass·εr_glass + v_resin·εr_resin
 *     Skew factor represents the periodic εr variation from weave pattern. */
double material_glass_weave_correction(double er_resin, double er_glass,
                                        double glass_volume_fraction,
                                        double weave_pitch_mm,
                                        double trace_width_mm);

/* L6: Canonical Problems — Material selection for specific applications */

/* L6: Select optimal material for given application constraints */
typedef struct {
    double    target_impedance_ohm;
    double    max_loss_db_per_meter;
    double    max_frequency_ghz;
    double    min_tg_celsius;
    double    max_cost_per_sqft;
    double    max_dk_variation;
    int       halogen_free_required;
    int       lead_free_compatible;
} MaterialRequirement;

MaterialId material_select_for_application(const MaterialRequirement *req);
void       material_print_selection_report(const MaterialRequirement *req,
                                            MaterialId selected);

/* L6: FR-4 vs High-Frequency laminate comparison at given frequency */
void material_compare_fr4_vs_hf(double freq_ghz, double trace_len_m);

/* L7: Applications — Real-world material data */

/* L7: Industry-standard stackup material combinations (used by JLCPCB, PCBWay, etc.) */
typedef struct {
    const char *stackup_name;
    MaterialId   core_material;
    MaterialId   prepreg_material;
    double       core_thickness_mm;
    double       prepreg_thickness_mm;
    double       copper_weight_oz;
} IndustryStackupMaterial;

/* L7: Get JLCPCB-compatible stackup materials */
const IndustryStackupMaterial* material_jlcpcb_4layer_standard(void);

/* L7: Get material Dk/Df data at specific frequencies (as provided by manufacturers) */
int material_get_dk_df_table(MaterialId id,
                              double *frequencies_ghz,
                              double *dk_values,
                              double *df_values,
                              int max_points);

/* L8: Advanced — Copper foil treatment effects */

/* L8: Reverse-treated / low-profile foil analysis
 *     Rz (10-point roughness) to RMS conversion for various foil types:
 *     STD (standard), VLP (very low profile), HVLP (hyper VLP), RTF (reverse treated) */
typedef enum {
    FOIL_STD = 0,
    FOIL_VLP = 1,
    FOIL_HVLP = 2,
    FOIL_RTF = 3,
    FOIL_UTF = 4     /* Ultra-thin foil */
} CopperFoilType;

/* L8: Get roughness parameters for a given foil type */
CopperRoughness material_foil_roughness(CopperFoilType type);

/* L8: Thermal cycling reliability assessment
 *     Computes expected cycles to failure based on CTE mismatch */
double material_thermal_cycle_life(const DielectricMaterial *dm,
                                    double delta_T_celsius,
                                    double via_aspect_ratio);

#endif /* PCB_MATERIAL_H */
