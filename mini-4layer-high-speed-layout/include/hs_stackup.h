/*
 * hs_stackup.h — 4-Layer PCB Stackup Definition and Material Library
 *
 * Core Definitions (L1):
 *   - Layer types: SIGNAL, PLANE, PREPREG, CORE
 *   - Material properties: εr, tanδ, σ (conductivity)
 *   - Stackup configurations: GND-SIG-SIG-GND, SIG-GND-PWR-SIG
 *   - Copper weight, dielectric thickness, trace width
 *
 * Core Concepts (L2):
 *   - Reference plane assignment
 *   - Prepreg vs Core dielectric
 *   - Controlled impedance stackup design
 *   - Skin effect and copper roughness
 *
 * Mathematical Structures (L3):
 *   - Skin depth δ = √(ρ / (π f μ))
 *   - Effective εr for multilayer
 *   - Plane capacitance per unit area
 *
 * Fundamental Laws (L4):
 *   - Skin effect depth formula
 *   - Plane capacitance C = ε₀ εr A / d
 *   - Copper resistivity temperature dependence
 *
 * References:
 *   - IPC-2221 Generic Standard on Printed Board Design
 *   - IPC-4101 Specification for Base Materials
 *   - Johnson & Graham, High-Speed Digital Design, Ch.4-5
 *   - Bogatin, Signal and Power Integrity Simplified, Ch.9
 */

#ifndef HS_STACKUP_H
#define HS_STACKUP_H

#include <stdint.h>
#include <stddef.h>

/* ================================================================
 * L1: Core Definitions — Material & Layer Types
 * ================================================================ */

/** Material type for dielectric layers */
typedef enum {
    HS_MAT_FR4_STD        = 0,  /* Standard FR-4, εr=4.2-4.5 */
    HS_MAT_FR4_HIGH_TG    = 1,  /* High-Tg FR-4, Tg>170°C */
    HS_MAT_ROGERS_4350B   = 2,  /* Rogers 4350B, εr=3.48±0.05 */
    HS_MAT_ROGERS_4003C   = 3,  /* Rogers 4003C, εr=3.38±0.05 */
    HS_MAT_ISOLA_370HR    = 4,  /* Isola 370HR, εr=3.92 */
    HS_MAT_MEGTRON_6      = 5,  /* Panasonic Megtron 6, εr=3.55 */
    HS_MAT_MEGTRON_7      = 6,  /* Panasonic Megtron 7, εr=3.35 */
    HS_MAT_PTFE            = 7,  /* Teflon/PTFE, εr=2.1 */
    HS_MAT_ALUMINA         = 8,  /* Alumina 96%, εr=9.2 */
    HS_MAT_COUNT           = 9
} hs_material_t;

/** Layer function type in the 4-layer stackup */
typedef enum {
    HS_LAYER_SIGNAL  = 0,  /* Signal routing layer */
    HS_LAYER_PLANE   = 1,  /* Power or ground plane */
    HS_LAYER_MIXED   = 2   /* Mixed signal + plane fill */
} hs_layer_type_t;

/** Dielectric placement in stackup */
typedef enum {
    HS_DIEL_PREPREG = 0,   /* Prepreg: between foil and core (outer to inner) */
    HS_DIEL_CORE    = 1    /* Core: laminated dielectric with copper both sides */
} hs_dielectric_placement_t;

/** Copper surface roughness spec (for loss modeling) */
typedef enum {
    HS_ROUGH_STANDARD = 0,  /* Standard electrodeposited: Rq ≈ 0.4 μm */
    HS_ROUGH_LOW      = 1,  /* Low-profile: Rq ≈ 0.25 μm */
    HS_ROUGH_VLP      = 2,  /* Very low profile: Rq ≈ 0.15 μm */
    HS_ROUGH_ULP      = 3   /* Ultra low profile: Rq ≈ 0.10 μm */
} hs_roughness_t;

/** Standard stackup configuration identifier */
typedef enum {
    HS_CONFIG_SIG_GND_PWR_SIG = 0,  /* Outer signals, inner planes */
    HS_CONFIG_GND_SIG_SIG_GND = 1,  /* Outer ground shields, inner signals */
    HS_CONFIG_SIG_GND_GND_SIG = 2,  /* Two ground planes between signals */
    HS_CONFIG_GND_SIG_PWR_SIG = 3   /* Ground outer, PWR inner with SIG */
} hs_stackup_config_t;

/**
 * Material properties database entry.
 *
 * Theory: εr (relative permittivity) and tanδ (loss tangent) determine
 * signal propagation speed and dielectric loss. σ determines conductor loss.
 *
 * @field name:     Material designation
 * @field er:       Relative dielectric constant (εr) at 1 GHz
 * @field tan_d:    Loss tangent (dissipation factor) at 1 GHz
 * @field er_tc:    Thermal coefficient of εr (ppm/°C)
 * @field tg:       Glass transition temperature (°C)
 * @field dk_stability: εr variation across 100 MHz – 10 GHz
 */
typedef struct {
    hs_material_t id;
    const char   *name;
    double        er;              /* Relative permittivity at 1 GHz */
    double        tan_d;           /* Loss tangent at 1 GHz */
    double        er_tc;           /* Temperature coefficient (ppm/°C) */
    double        tg;              /* Tg in °C; 0 if not applicable */
    double        dk_stability;    /* Max Δεr over 0.1-10 GHz */
} hs_material_props_t;

/**
 * Single layer in the 4-layer stackup.
 *
 * The 4-layer board has two outer layers (top/bottom foil) and
 * two inner layers separated by a core dielectric, with prepreg
 * bonding the outer foils to the core.
 *
 * Typical cross-section (SIG-GND-PWR-SIG):
 *   L1 (Top Signal):    Copper foil + plating
 *   Prepreg:            Bonding dielectric
 *   L2 (GND Plane):     Copper on core top
 *   Core:               C-stage laminate
 *   L3 (PWR Plane):     Copper on core bottom
 *   Prepreg:            Bonding dielectric
 *   L4 (Bottom Signal): Copper foil + plating
 */
typedef struct {
    int            layer_index;    /* 1-based: 1=Top, 4=Bottom */
    hs_layer_type_t type;
    double         copper_weight;  /* oz/ft² (0.5, 1.0, 2.0) */
    double         copper_thickness; /* Computed thickness in mils */
    double         copper_conductivity; /* S/m, default 5.8e7 for Cu */
    const char    *net_name;       /* Primary net assignment (e.g., "GND", "VCC") */
} hs_layer_entry_t;

/**
 * Complete 4-layer stackup definition.
 *
 * This structure captures the full physical cross-section of a
 * 4-layer PCB, including material choices, dielectric thicknesses,
 * and copper specifications for all layers.
 *
 * Knowledge mapping:
 *   L1: Physical geometry definition
 *   L2: Reference plane concept — layers 2,3 provide return paths
 *   L3: Multilayer geometry matrix for field solver input
 *   L4: IPC-2221 design rules implicit in thickness constraints
 */
typedef struct {
    hs_stackup_config_t config;
    hs_layer_entry_t    layers[4];
    hs_material_t       dielectric_type;  /* Material for both core and prepreg */
    double              prepreg_thickness[2]; /* [0]=between L1-L2, [1]=between L3-L4 (mils) */
    double              core_thickness;       /* Between L2-L3 (mils) */
    double              total_thickness;      /* Total board thickness (mils) */
    hs_roughness_t      outer_roughness;      /* Outer layer copper roughness */
    hs_roughness_t      inner_roughness;      /* Inner layer copper roughness */
    char                description[128];     /* Human-readable description */
} hs_stackup_t;

/* ================================================================
 * L2-L4: Mathematical models for stackup physical properties
 * ================================================================ */

/**
 * Compute skin depth in copper at a given frequency.
 *
 * Theorem (Skin Effect):
 *   δ = √(ρ / (π f μ₀ μr))
 * where ρ = resistivity (Ω·m), f = frequency (Hz), μ₀ = 4π×10⁻⁷ H/m.
 *
 * For copper at 20°C: ρ ≈ 1.72×10⁻⁸ Ω·m, μr ≈ 1
 *   δ ≈ 66 / √f   (δ in μm, f in MHz)
 *   δ ≈ 2.09 / √f (δ in mils, f in GHz)
 *
 * At 1 GHz: δ ≈ 2.09 mils ≈ 0.066 mils effective AC resistance increase
 * At 10 GHz: δ ≈ 0.66 mils — significant roughness impact
 *
 * Reference: Johnson & Graham, Eq. 5.1; Ramo, Whinnery & Van Duzer, §5.04
 *
 * @param frequency_hz: Signal frequency in Hz (> 0)
 * @param resistivity_ohm_m: Conductor resistivity in Ω·m
 * @param relative_permeability: μr of conductor (~1 for Cu)
 * @return Skin depth in meters; returns 0 on invalid input
 */
double hs_skin_depth(double frequency_hz, double resistivity_ohm_m,
                     double relative_permeability);

/**
 * Compute effective dielectric constant for a microstrip line
 * using the frequency-dependent dispersion model.
 *
 * Formula (Kirschning-Jansen dispersion):
 *   εeff(f) = εr - (εr - εeff(0)) / (1 + P(f))
 * where P(f) is a frequency-dependent factor.
 *
 * Reference: M. Kirschning and R.H. Jansen, "Accurate model for effective
 *   dielectric constant …", Electronics Letters, 1982.
 *
 * @param er_bulk: Bulk relative permittivity of substrate
 * @param frequency_hz: Frequency in Hz
 * @param height_m: Substrate height in meters (> 0)
 * @param width_m: Trace width in meters (> 0)
 * @return Effective εr at given frequency
 */
double hs_effective_er_frequency(double er_bulk, double frequency_hz,
                                  double height_m, double width_m);

/**
 * Compute the static (low-frequency) effective dielectric constant
 * for a microstrip using the Wheeler-Schneider formula.
 *
 * For w/h ≤ 1:
 *   εeff = (εr+1)/2 + (εr-1)/2 * [1/√(1+12h/w) + 0.04(1-w/h)²]
 * For w/h > 1:
 *   εeff = (εr+1)/2 + (εr-1)/2 * [1/√(1+12h/w)]
 *
 * Reference: H.A. Wheeler, "Transmission-line properties of a strip...",
 *   IEEE Trans. MTT, 1977.
 *
 * @param er_bulk: Bulk εr of dielectric
 * @param height_m: Dielectric height in m
 * @param width_m: Trace width in m
 * @param thickness_m: Trace thickness in m (≥ 0)
 * @return Static effective εr
 */
double hs_effective_er_static(double er_bulk, double height_m,
                               double width_m, double thickness_m);

/**
 * Calculate plane-to-plane capacitance for a parallel-plate pair.
 *
 * Fundamental Law (L4):
 *   C = ε₀ εr A / d
 * where A = area (m²), d = separation (m)
 *
 * For a 10 cm × 10 cm board with 10 mil core (FR-4 εr=4.2):
 *   C ≈ 8.85e-12 × 4.2 × 0.01 / 2.54e-4 ≈ 1.46 nF
 *
 * This inter-plane capacitance provides essential high-frequency
 * decoupling in the PDN.
 *
 * @param area_m2: Overlap area in m²
 * @param separation_m: Dielectric thickness in m
 * @param er: Dielectric constant
 * @return Capacitance in Farads
 */
double hs_plane_capacitance(double area_m2, double separation_m, double er);

/**
 * Calculate plane-to-plane inductance per square.
 *
 * For a plane pair separated by dielectric:
 *   L_sq = μ₀ d / w   (approximate per-square model)
 * where d = separation, w = width
 *
 * More precisely (for high-frequency current path):
 *   L_sq ≈ μ₀ × d    (H per square, d in m)
 *
 * Reference: Istvan Novak, "Power Distribution Network Design Methodologies"
 *
 * @param separation_m: Dielectric thickness in m
 * @param width_m: Effective width in m
 * @return Inductance per square in H
 */
double hs_plane_inductance_per_square(double separation_m, double width_m);

/**
 * Compute copper trace resistance per unit length,
 * including skin effect correction.
 *
 * DC resistance: R_dc = ρ / (w × t)  [Ω/m]
 * AC resistance: R_ac ≈ ρ / (w × δ)  when δ << t
 *
 * Full model: R(f) = R_dc × √(1 + (f/f_break)²)
 * where f_break = ρ / (π μ₀ t²) is the frequency where δ ≈ t/2
 *
 * Reference: Johnson & Graham, Eq. 5.7-5.10
 *
 * @param width_m: Trace width in m
 * @param thickness_m: Trace thickness in m
 * @param frequency_hz: Frequency in Hz
 * @param resistivity_ohm_m: Resistivity in Ω·m
 * @return Resistance per meter in Ω/m
 */
double hs_trace_resistance_per_meter(double width_m, double thickness_m,
                                      double frequency_hz,
                                      double resistivity_ohm_m);

/**
 * Compute copper weight to thickness conversion.
 * 1 oz/ft² copper → 1.37 mils (34.8 μm) nominal thickness.
 *
 * Standard PCB copper weights:
 *   0.25 oz → 0.34 mils (8.7 μm) — rarely used
 *   0.5 oz  → 0.68 mils (17.4 μm) — common for inner layers
 *   1.0 oz  → 1.37 mils (34.8 μm) — standard outer
 *   2.0 oz  → 2.74 mils (69.6 μm) — power planes
 *   3.0 oz  → 4.11 mils (104.4 μm) — high-current
 *
 * @param copper_weight_oz: Copper weight in oz/ft² (> 0)
 * @return Thickness in meters
 */
double hs_copper_weight_to_thickness(double copper_weight_oz);

/**
 * Initialize a standard 4-layer stackup with default values.
 *
 * Default configuration: SIG(TOP)-GND-PWR-SIG(BOTTOM)
 * Material: Standard FR-4, 62 mil total thickness
 * Layer allocation:
 *   L1: Signal (1 oz Cu)
 *   L2: Ground plane (1 oz Cu)
 *   L3: Power plane (1 oz Cu)
 *   L4: Signal (1 oz Cu)
 * Prepreg: 8 mil each side, Core: 40 mil
 *
 * @param stackup: Pointer to stackup struct to initialize
 */
void hs_stackup_init_default(hs_stackup_t *stackup);

/**
 * Initialize a specific stackup configuration.
 *
 * @param stackup: Pointer to stackup struct
 * @param config: Configuration identifier
 * @param total_thickness_mils: Target total board thickness
 * @param material: Dielectric material type
 */
void hs_stackup_init_config(hs_stackup_t *stackup, hs_stackup_config_t config,
                             double total_thickness_mils, hs_material_t material);

/**
 * Validate stackup physical consistency.
 *
 * Checks:
 *   1. Total thickness matches sum of layers + dielectrics
 *   2. No negative thicknesses
 *   3. Signal layers have adjacent reference planes
 *   4. Layer ordering is physically realizable
 *   5. Copper thickness within manufacturing limits
 *
 * @param stackup: Stackup to validate
 * @return 0 if valid, non-zero error code otherwise
 */
int hs_stackup_validate(const hs_stackup_t *stackup);

/**
 * Get material properties from the built-in library.
 *
 * @param material: Material identifier
 * @return Pointer to material properties; NULL if invalid material
 */
const hs_material_props_t *hs_material_get_props(hs_material_t material);

/**
 * Calculate the copper roughness correction factor for loss modeling.
 *
 * The Hammerstad-Bekkadal model:
 *   K_sr = 1 + (2/π) * arctan(1.4 × (Rq/δ)²)
 *
 * where Rq = RMS surface roughness, δ = skin depth.
 * This factor multiplies the smooth-surface AC resistance.
 *
 * Reference: Hammerstad & Bekkadal, "Microstrip Handbook", ELAB report, 1975.
 *
 * @param rms_roughness_m: RMS surface roughness in m
 * @param skin_depth_m: Skin depth at frequency of interest in m
 * @return Roughness correction factor (≥ 1.0)
 */
double hs_roughness_correction(double rms_roughness_m, double skin_depth_m);

/**
 * Compute the average dielectric constant for a mixed-dielectric
 * layer stack (weighted average by thickness).
 *
 * For a stack with N dielectric regions:
 *   εr_avg = Σ(εr_i × h_i) / Σ(h_i)
 *
 * @param er_values: Array of relative permittivity values
 * @param thicknesses_m: Array of corresponding thicknesses in m
 * @param count: Number of regions
 * @return Thickness-weighted average εr
 */
double hs_average_er(const double *er_values, const double *thicknesses_m,
                      size_t count);

/**
 * Compute the propagation velocity in a dielectric medium.
 *
 * v = c / √εeff
 * where c = 2.99792458×10⁸ m/s (speed of light in vacuum)
 *
 * @param er_effective: Effective relative permittivity (> 0)
 * @return Propagation velocity in m/s
 */
double hs_propagation_velocity(double er_effective);

/**
 * Compute propagation delay per unit length (time per meter).
 *
 * t_pd = √εeff / c  [s/m]
 *
 * For FR-4 (εeff ≈ 3.4): t_pd ≈ 6.16 ns/m ≈ 156 ps/inch
 * For Rogers 4350B (εeff ≈ 3.0): t_pd ≈ 5.77 ns/m ≈ 147 ps/inch
 *
 * @param er_effective: Effective dielectric constant
 * @return Delay in seconds per meter
 */
double hs_propagation_delay_per_meter(double er_effective);

/**
 * Print a human-readable cross-section of the stackup.
 *
 * @param stackup: Stackup to print
 * @param units: 'm' for metric, 'i' for imperial (mils)
 */
void hs_stackup_print(const hs_stackup_t *stackup, char units);

#endif /* HS_STACKUP_H */
