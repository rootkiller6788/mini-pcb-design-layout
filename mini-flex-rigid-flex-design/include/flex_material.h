/**
 * @file flex_material.h
 * @brief Flexible/Rigid-Flex PCB Material Definitions and Properties
 *
 * Covers L1 (Definitions): flex substrate types, copper types, adhesive systems,
 * key material parameters.
 * Covers L4 (Fundamental Laws): IPC-4203/4204 material specifications,
 * thermal expansion physics.
 *
 * Reference Standards:
 *   IPC-4202  Flexible Base Dielectrics for Use in Flexible Printed Boards
 *   IPC-4203  Adhesive Coated Dielectric Films for Use as Cover Sheets
 *   IPC-4204  Flexible Metal-Clad Dielectrics for Use in Fabrication of Flex
 *   IPC-4562  Metal Foil for Printed Board Applications
 *
 * @module mini-flex-rigid-flex-design
 */

#ifndef FLEX_MATERIAL_H
#define FLEX_MATERIAL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------------------------
 * L1 — Core Definitions: Flexible Substrate Materials
 * -------------------------------------------------------------------------*/

/** Dielectric material type for flex/rigid-flex substrates */
typedef enum {
    FLEX_DIELEC_POLYIMIDE = 0,    /**< Polyimide (Kapton, Apical) — industry standard */
    FLEX_DIELEC_LCP,              /**< Liquid Crystal Polymer — high freq/low moisture */
    FLEX_DIELEC_PET,              /**< Polyethylene Terephthalate — low cost, low temp */
    FLEX_DIELEC_PEN,              /**< Polyethylene Naphthalate — mid-range performance */
    FLEX_DIELEC_PTFE,             /**< PTFE (Teflon) — microwave/RF flex */
    FLEX_DIELEC_MPI,              /**< Modified Polyimide — improved RF performance */
    FLEX_DIELEC_EPOXY_FLEX,       /**< Flexible epoxy — low-end flex */
    FLEX_DIELEC_COUNT
} flex_dielectric_type_t;

/** Copper foil type for flexible circuits */
typedef enum {
    FLEX_COPPER_RA = 0,           /**< Rolled Annealed — superior flex life */
    FLEX_COPPER_ED,               /**< Electrodeposited — lower cost, rougher profile */
    FLEX_COPPER_RA_LP,            /**< RA Low Profile — smooth surface for high freq */
    FLEX_COPPER_ED_LP,            /**< ED Low Profile — improved signal integrity */
    FLEX_COPPER_COUNT
} flex_copper_type_t;

/** Adhesive system type for bonding layers */
typedef enum {
    FLEX_ADHESIVE_ACRYLIC = 0,    /**< Acrylic — good flow, lower Tg */
    FLEX_ADHESIVE_EPOXY,          /**< Epoxy — higher Tg, lower flow */
    FLEX_ADHESIVE_PI,             /**< Polyimide adhesive — highest temp */
    FLEX_ADHESIVE_PSA,            /**< Pressure Sensitive Adhesive — low temp assembly */
    FLEX_ADHESIVE_ADHESIVELESS,   /**< Adhesiveless — cast-on or sputtered copper */
    FLEX_ADHESIVE_COUNT
} flex_adhesive_type_t;

/** Stiffener material type */
typedef enum {
    FLEX_STIFFENER_NONE = 0,
    FLEX_STIFFENER_FR4,           /**< FR-4 glass epoxy laminate */
    FLEX_STIFFENER_POLYIMIDE,     /**< Polyimide film stiffener */
    FLEX_STIFFENER_ALUMINUM,      /**< Aluminum stiffener — thermal management */
    FLEX_STIFFENER_STAINLESS,     /**< Stainless steel — spring contacts */
    FLEX_STIFFENER_COPPER,        /**< Copper stiffener — thermal/electrical */
    FLEX_STIFFENER_COUNT
} flex_stiffener_type_t;

/** Coverlay / covercoat type */
typedef enum {
    FLEX_COVER_POLYIMIDE_FILM = 0, /**< PI film + adhesive — most common */
    FLEX_COVER_LPI,                /**< Liquid Photoimageable — fine openings */
    FLEX_COVER_PIC,                /**< Photoimageable Coverlay — combination */
    FLEX_COVER_COUNT
} flex_cover_type_t;

/* ---------------------------------------------------------------------------
 * L1 — Material Property Structures (IPC-4202 / IPC-4204)
 * -------------------------------------------------------------------------*/

/**
 * @brief Dielectric material electrical properties
 *
 * Key parameters for signal integrity and impedance control on flex substrates.
 */
typedef struct {
    double dielectric_constant;     /**< Relative permittivity εr @ 1 MHz */
    double dk_at_1ghz;              /**< εr @ 1 GHz for high-speed design */
    double dk_at_10ghz;             /**< εr @ 10 GHz for microwave design */
    double loss_tangent;            /**< Dissipation factor tan δ @ 1 MHz */
    double loss_tangent_at_1ghz;   /**< tan δ @ 1 GHz */
    double dielectric_strength;     /**< kV/mm — breakdown voltage per thickness */
    double volume_resistivity;      /**< MΩ·cm — bulk insulation resistance */
    double surface_resistivity;     /**< MΩ — surface leakage resistance */
    double moisture_absorption;     /**< % weight gain after 24h immersion */
} flex_dielectric_electrical_t;

/**
 * @brief Dielectric material mechanical/thermal properties
 */
typedef struct {
    double youngs_modulus;         /**< MPa — tensile elastic modulus */
    double tensile_strength;       /**< MPa — ultimate tensile strength */
    double elongation_at_break;    /**< % — strain at fracture */
    double cte_xy;                 /**< ppm/°C — in-plane CTE */
    double cte_z;                  /**< ppm/°C — through-thickness CTE, key for via reliability */
    double glass_transition_tg;    /**< °C — glass transition temperature */
    double decomposition_temp;     /**< °C — onset of thermal decomposition */
    double thermal_conductivity;   /**< W/(m·K) — in-plane thermal conductivity */
    double density;                /**< g/cm³ */
    double moisture_expansion;     /**< ppm/%RH — hygroscopic expansion coefficient */
} flex_dielectric_mechanical_t;

/**
 * @brief Copper foil properties
 */
typedef struct {
    double resistivity;            /**< μΩ·cm — electrical resistivity */
    double thermal_conductivity;   /**< W/(m·K) */
    double tensile_strength;       /**< N/mm² */
    double elongation;             /**< % — elongation at break */
    double surface_roughness_ra;   /**< μm — average surface roughness */
    double surface_roughness_rz;   /**< μm — peak-to-valley roughness */
    double cte;                    /**< ppm/°C */
    double fatigue_strength;       /**< N/mm² — endurance limit for bend cycling */
    double grain_size;             /**< μm — average grain size (RA << ED) */
} flex_copper_foil_t;

/**
 * @brief Adhesive material properties
 */
typedef struct {
    double dielectric_constant;
    double loss_tangent;
    double youngs_modulus;         /**< MPa */
    double cte;                    /**< ppm/°C */
    double glass_transition_tg;    /**< °C */
    double flow_percent;           /**< % — resin flow during lamination */
    double peel_strength;          /**< N/mm */
    double minimum_thickness;      /**< mm — minimum achievable bond line */
} flex_adhesive_properties_t;

/**
 * @brief Complete material specification for one flex layer
 */
typedef struct {
    flex_dielectric_type_t dielectric_type;
    flex_copper_type_t copper_type;
    flex_adhesive_type_t adhesive_type;
    flex_dielectric_electrical_t dielectric_elec;
    flex_dielectric_mechanical_t dielectric_mech;
    flex_copper_foil_t copper_foil;
    flex_adhesive_properties_t adhesive;
    double copper_thickness_um;    /**< Copper thickness in μm */
    double dielectric_thickness_um;/**< Dielectric core thickness in μm */
    double adhesive_thickness_um;  /**< Adhesive layer thickness in μm */
    char manufacturer_grade[64];   /**< e.g., "DuPont AP8525R" */
    char ipc_slash_sheet[32];      /**< e.g., "IPC-4203/1" */
} flex_material_spec_t;

/* ---------------------------------------------------------------------------
 * L1 — Complete Board-Level Material Stack
 * -------------------------------------------------------------------------*/

/**
 * @brief Stiffener specification
 */
typedef struct {
    flex_stiffener_type_t type;
    double thickness_mm;
    double youngs_modulus;         /**< MPa */
    double cte;                    /**< ppm/°C */
    double thermal_conductivity;   /**< W/(m·K) */
    int is_bonded;                 /**< 1 = bonded with adhesive, 0 = mechanical attach */
    flex_adhesive_type_t bond_adhesive;
    double bond_adhesive_thickness_mm;
} flex_stiffener_spec_t;

#define FLEX_MAX_LAYERS 32
#define FLEX_MAX_STIFFENERS 8
#define FLEX_MAX_BEND_ZONES 4

/**
 * @brief Layer type in the stack
 */
typedef enum {
    FLEX_LAYER_SIGNAL = 0,
    FLEX_LAYER_PLANE,
    FLEX_LAYER_MIXED,
    FLEX_LAYER_ADHESIVE_ONLY,
    FLEX_LAYER_COUNT
} flex_layer_type_t;

/**
 * @brief Board section type (rigid vs flex zone)
 */
typedef enum {
    FLEX_SECTION_RIGID = 0,       /**< FR-4 or rigid laminate section */
    FLEX_SECTION_FLEX,            /**< Flexible section (bend zone) */
    FLEX_SECTION_TRANSITION       /**< Rigid-to-flex transition zone */
} flex_section_type_t;

/* ---------------------------------------------------------------------------
 * L4 — Material Property Database & Lookup Functions
 *
 * Knowledge point: Each function below maps to a specific IPC material spec
 * and provides the engineering parameters needed for design calculations.
 * -------------------------------------------------------------------------*/

/**
 * @brief Retrieve standard polyimide (Kapton HN-type) electrical properties.
 *
 * IPC-4202 Type A: Adhesiveless polyimide, εr ≈ 3.4, tanδ ≈ 0.002.
 * This function returns the benchmark values used as reference in IPC-4202.
 *
 * Complexity: O(1) — table lookup
 */
flex_dielectric_electrical_t flex_polyimide_electrical_standard(void);

/**
 * @brief Retrieve LCP (Liquid Crystal Polymer) electrical properties.
 *
 * LCP offers εr ≈ 2.9, tanδ ≈ 0.002 at 10 GHz — superior for mmWave flex.
 * Reference: IPC-4202 Type C.
 *
 * Complexity: O(1) — table lookup
 */
flex_dielectric_electrical_t flex_lcp_electrical_standard(void);

/**
 * @brief Retrieve standard polyimide mechanical/thermal properties.
 *
 * Young's modulus ≈ 2500 MPa (film direction), CTE ≈ 20 ppm/°C (in-plane),
 * Tg ~ 360°C (no true Tg — decomposition). Key for bend stress calculations.
 */
flex_dielectric_mechanical_t flex_polyimide_mechanical_standard(void);

/**
 * @brief Retrieve LCP mechanical/thermal properties.
 *
 * Young's modulus ≈ 3000 MPa (oriented), CTE ≈ 17 ppm/°C.
 * Note: LCP is anisotropic — properties vary with orientation.
 */
flex_dielectric_mechanical_t flex_lcp_mechanical_standard(void);

/**
 * @brief Retrieve PET mechanical properties for low-cost flex.
 *
 * Young's modulus ≈ 3800 MPa, Tg ≈ 80°C — limited to room temp apps.
 */
flex_dielectric_mechanical_t flex_pet_mechanical_standard(void);

/**
 * @brief Get copper foil properties by type.
 * @param copper_type RA or ED copper type
 * @return Standard copper foil properties per IPC-4562
 *
 * RA copper: finer grain structure, 3-10x better bend fatigue life.
 * ED copper: columnar grain, rougher surface (better adhesion, worse SI).
 */
flex_copper_foil_t flex_copper_foil_standard(flex_copper_type_t copper_type);

/**
 * @brief Get adhesive properties by type.
 * @param adhesive_type The adhesive system
 * @return Standard adhesive properties
 *
 * Key knowledge: The adhesive layer is the weakest link in flex thermal cycling.
 * CTE mismatch between adhesive and PI causes delamination if > 30 ppm/°C delta.
 */
flex_adhesive_properties_t flex_adhesive_properties_standard(
    flex_adhesive_type_t adhesive_type);

/**
 * @brief Estimate dielectric constant at arbitrary frequency using
 *        the Djordjevic-Sarkar model.
 *
 * εr(f) = εr' - (εr' - εr_inf) * tanh(α * log10(f / f0))
 *
 * Simplified for engineering use with published material parameters.
 *
 * @param base_dk Dielectric constant at reference frequency
 * @param dk_inf High-frequency asymptotic dielectric constant
 * @param freq_hz Frequency of interest in Hz
 * @param ref_freq_hz Reference frequency of base_dk in Hz
 * @return Estimated εr at freq_hz
 *
 * Complexity: O(1)
 * Reference: Djordjevic, Sarkar et al., IEEE T-MTT, 2001
 */
double flex_dk_at_frequency(double base_dk, double dk_inf,
                             double freq_hz, double ref_freq_hz);

/**
 * @brief Calculate the dielectric constant from bulk material to effective
 *        value considering moisture absorption.
 *
 * εr_eff = εr_dry * (1 + β * M), where β ≈ 2.0 for polyimide,
 * M = moisture content (fraction).
 *
 * @param dk_dry Dielectric constant at 0% moisture
 * @param moisture_percent Moisture absorption in weight %
 * @return Effective dielectric constant including moisture effect
 *
 * Reference: Mumby, IEEE T-EP, 1988
 */
double flex_dk_moisture_correction(double dk_dry, double moisture_percent);

/**
 * @brief Compute the CTE mismatch stress at interface between two bonded
 *        materials undergoing a temperature change.
 *
 * σ = E * Δα * ΔT  (simplified 1D thermoelastic stress)
 *
 * @param e_modulus Young's modulus of the layer of interest (MPa)
 * @param cte_a CTE of material A (ppm/°C)
 * @param cte_b CTE of material B (ppm/°C)
 * @param delta_t Temperature change from stress-free state (°C)
 * @return Thermal mismatch stress in MPa
 *
 * This is the fundamental stress driving delamination in rigid-flex
 * transition zones. See IPC-2223 §5.2.
 */
double flex_cte_mismatch_stress(double e_modulus, double cte_a,
                                 double cte_b, double delta_t);

/**
 * @brief Estimate the glass transition temperature shift due to moisture
 *        absorption in polymers (Kelley-Bueche model).
 *
 * Tg(wet) = (αp*Tgp + αw*Tgw) / (αp + αw)
 *
 * where αp, αw are volume fractions; Tgp, Tgw are component Tg values.
 *
 * @param tg_dry Dry Tg in °C
 * @param moisture_percent Moisture absorption (% weight)
 * @return Estimated wet Tg in °C
 */
double flex_tg_moisture_shift(double tg_dry, double moisture_percent);

/**
 * @brief Calculate the characteristic impedance of the material system
 *        (not the trace — the bulk dielectric). Used for material comparison.
 *
 * Z0_material = sqrt(μ0 / (ε0 * εr)) / 2π = 60/√εr  (free-space normalization)
 *
 * @param dk Relative permittivity
 * @return Intrinsic impedance of the dielectric relative to free space (Ω)
 */
double flex_intrinsic_impedance(double dk);

#ifdef __cplusplus
}
#endif

#endif /* FLEX_MATERIAL_H */
