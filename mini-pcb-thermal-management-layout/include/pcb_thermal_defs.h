/**
 * pcb_thermal_defs.h — Core Type Definitions for PCB Thermal Management
 *
 * L1 Definitions: thermal resistance, conductivity, heat flux,
 * junction/ambient/case temperatures, thermal impedance network,
 * PCB stack thermal parameters, cooling mechanisms.
 *
 * Courses: Berkeley EE105 Analog (power dissipation), MIT 6.630 EM (heat eq),
 *          TU Munich High-Frequency Engineering (thermal management),
 *          Georgia Tech ECE 6350 EM (conduction/radiation)
 * Reference: Guenin, "Thermal Calculations for Multi-Chip Modules", Electronics Cooling (2002)
 *            Cengel, "Heat and Mass Transfer: Fundamentals and Applications" (2014)
 *            IPC-2152 "Standard for Determining Current-Carrying Capacity in Printed Board Design"
 */

#ifndef PCB_THERMAL_DEFS_H
#define PCB_THERMAL_DEFS_H

#include <stddef.h>
#include <stdint.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L1: Fundamental Thermal Quantities — 12 canonical definitions
 * ================================================================== */

/** Heat transfer mechanism — three fundamental modes plus composite.
 *  Conduction:     energy transfer through stationary matter by molecular vibration
 *  Convection:     energy transfer between surface and moving fluid
 *  Natural Conv:   fluid motion driven by buoyancy
 *  Forced Conv:    fluid motion driven by external means (fan, pump)
 *  Radiation:      energy transfer by electromagnetic waves
 *  Spreading:      lateral heat conduction in a planar medium (PCB copper plane) */
typedef enum {
    HEAT_CONDUCTION       = 0,
    HEAT_CONVECTION       = 1,
    HEAT_RADIATION        = 2,
    HEAT_NATURAL_CONV     = 3,
    HEAT_FORCED_CONV      = 4,
    HEAT_SPREADING        = 5,
    HEAT_PHASE_CHANGE     = 6
} heat_transfer_mode_t;

/** PCB board material type — each with distinct thermal properties.
 *  FR4:        standard epoxy-glass (k = 0.3 W/m-K), Tg ~130-180 C
 *  Polyimide:  high-temp flex material (k = 0.2 W/m-K), Tg >250 C
 *  Aluminum:   metal-core PCB (k = 150 W/m-K for core)
 *  Copper:     thick-copper PCB or copper-core (k = 385 W/m-K)
 *  Ceramic:    alumina/AlN PCB (k_alumina = 24 W/m-K, k_AlN = 170 W/m-K)
 *  Rogers:     high-frequency laminate (k = 0.4-0.7 W/m-K)
 *  PTFE:       Teflon-based microwave laminate (k = 0.25 W/m-K)
 *  IMS:        Insulated Metal Substrate (aluminum base + thin dielectric)
 *  BT-Epoxy:   bismaleimide-triazine, high-Tg FR4 alternative (k = 0.35 W/m-K)
 *  CEM-3:      composite epoxy material, lower cost (k = 0.4 W/m-K) */
typedef enum {
    PCB_MAT_FR4        = 0,
    PCB_MAT_POLYIMIDE  = 1,
    PCB_MAT_ALUMINUM   = 2,
    PCB_MAT_COPPER     = 3,
    PCB_MAT_CERAMIC    = 4,
    PCB_MAT_ROGERS     = 5,
    PCB_MAT_PTFE       = 6,
    PCB_MAT_IMS        = 7,
    PCB_MAT_BT_EPOXY   = 8,
    PCB_MAT_CEM3       = 9
} pcb_material_type_t;

/** Component package type — affects junction-to-case thermal resistance.
 *  SOT-23:     Rjc ~80-150 C/W
 *  SOIC-8:     Rjc ~40-70 C/W
 *  QFN:        Rjc ~5-15 C/W (exposed pad)
 *  QFP:        Rjc ~15-30 C/W
 *  BGA:        Rjc ~8-20 C/W
 *  TO-220:     Rjc ~1-5 C/W
 *  TO-247:     Rjc ~0.5-2 C/W
 *  TO-263/D2PAK: Rjc ~1-3 C/W
 *  LGA:        similar to QFN
 *  WLCSP:      Rjc ~2-10 C/W (very small)
 *  DFN:        similar to QFN but 2-sided
 *  Power-Module: Rjc ~0.2-1 C/W (GaN/SiC) */
typedef enum {
    PKG_SOT23    = 0,
    PKG_SOIC8    = 1,
    PKG_QFN      = 2,
    PKG_QFP      = 3,
    PKG_BGA      = 4,
    PKG_TO220    = 5,
    PKG_TO247    = 6,
    PKG_TO263    = 7,
    PKG_LGA      = 8,
    PKG_WLCSP    = 9,
    PKG_DFN      = 10,
    PKG_PWR_MOD  = 11
} package_type_t;

/** Cooling solution type — active and passive approaches.
 *  Copper_Pour:     enlarged copper area for heat spreading
 *  Heat_Sink_Ext:   external finned heat sink (aluminum, natural convection)
 *  Heat_Sink_Fan:   heat sink with forced air cooling
 *  Thermal_Vias:    plated through-holes conducting heat to inner layers
 *  Heat_Pipe:       two-phase heat pipe embedded in or attached to PCB
 *  Vapor_Chamber:   planar heat pipe for spreading high heat flux
 *  Peltier:         thermoelectric cooler (active heat pump)
 *  Liquid_Cold_Plate: liquid-cooled cold plate
 *  Immersion:       direct liquid immersion cooling */
typedef enum {
    COOLING_NONE            = 0,
    COOLING_COPPER_POUR     = 1,
    COOLING_HEATSINK_EXT    = 2,
    COOLING_HEATSINK_FAN    = 3,
    COOLING_THERMAL_VIAS    = 4,
    COOLING_HEAT_PIPE       = 5,
    COOLING_VAPOR_CHAMBER   = 6,
    COOLING_PELTIER         = 7,
    COOLING_LIQUID_COLD     = 8,
    COOLING_IMMERSION       = 9
} cooling_type_t;

/** PCB layer type — thermal role in stack-up. */
typedef enum {
    PCB_LAYER_SIG_TOP    = 0,
    PCB_LAYER_SIG_BOT    = 1,
    PCB_LAYER_PWR_PLANE  = 2,
    PCB_LAYER_GND_PLANE  = 3,
    PCB_LAYER_DIELECTRIC = 4,
    PCB_LAYER_MIXED      = 5,
    PCB_LAYER_THERMAL_PAD= 6,
    PCB_LAYER_SHIELD     = 7
} pcb_layer_type_t;

/** Copper weight — standard PCB copper thickness designations.
 *  0.5 oz: 17 um, 1 oz: 35 um, 2 oz: 70 um, 3 oz: 105 um,
 *  4 oz: 140 um, 6 oz: 210 um, 10 oz: 350 um */
typedef enum {
    CU_0_5OZ = 0,
    CU_1OZ   = 1,
    CU_2OZ   = 2,
    CU_3OZ   = 3,
    CU_4OZ   = 4,
    CU_6OZ   = 5,
    CU_10OZ  = 6
} copper_weight_t;

/* ==================================================================
 * L1: Core Thermal Data Structures — 10 canonical structures
 * ================================================================== */

/** Three-dimensional point on a PCB for thermal mapping.
 *  L1: Spatial coordinate for temperature field analysis. */
typedef struct {
    double x_mm;  /* X coordinate on PCB surface (mm) */
    double y_mm;  /* Y coordinate on PCB surface (mm) */
    double z_mm;  /* Z coordinate through PCB thickness (mm), 0 = top surface */
} thermal_point_t;

/** Thermal resistance — the fundamental quantity in electronics cooling.
 *  L1: Rtheta = DeltaT / P   (C/W)
 *  Analogous to electrical resistance R = V/I.
 *  Junction-to-ambient:  Rja = Rjc + Rcs + Rsa
 *  Junction-to-case:     Rjc — from die to package surface
 *  Case-to-sink:         Rcs — across thermal interface material
 *  Sink-to-ambient:      Rsa — from heat sink to surrounding air
 *  Junction-to-board:    Rjb — from die through leads to PCB
 *  Reference: JEDEC JESD51 series */
typedef struct {
    double r_ja;        /* Junction-to-ambient (C/W) */
    double r_jc;        /* Junction-to-case (C/W) */
    double r_cs;        /* Case-to-sink (C/W) */
    double r_sa;        /* Sink-to-ambient (C/W) */
    double r_jb;        /* Junction-to-board (C/W) */
    double r_ba;        /* Board-to-ambient (C/W) */
    double r_effective; /* Effective total resistance */
} thermal_resistance_t;

/** PCB layer description — one entry in a multi-layer stack-up.
 *  L1: Each layer has type, thickness, and copper weight.
 *  Thermal conductivity of copper: k_Cu = 385 W/(m-K) (at 25 C)
 *  Thermal conductivity of FR4:    k_FR4 = 0.3 W/(m-K)
 *  This large ratio (~1283:1) means copper planes dominate lateral heat flow. */
typedef struct {
    pcb_layer_type_t type;         /* Layer functional type */
    copper_weight_t  cu_weight;    /* Copper weight designation */
    double           thickness_mm; /* Layer thickness (mm) */
    double           k_xy;         /* In-plane thermal conductivity (W/m-K) */
    double           k_z;          /* Through-plane thermal conductivity (W/m-K) */
    double           area_mm2;     /* Copper coverage area (mm^2) */
    int              is_solid;     /* 1 = solid plane, 0 = patterned/traces */
} pcb_layer_t;

/** PCB stack-up — complete multi-layer board thermal description.
 *  L1: Defines the thermal path from component to ambient.
 *  Standard 4-layer: SIG_TOP / GND / PWR / SIG_BOT
 *  6-layer: SIG / GND / SIG / PWR / GND / SIG
 *  The number and placement of GND/PWR planes is the primary lever
 *  for PCB-level thermal management. */
typedef struct {
    pcb_layer_t        *layers;          /* Array of layers, layers[0] = top */
    int                 num_layers;      /* Total number of layers */
    pcb_material_type_t dielectric;      /* Base dielectric material */
    double              total_thickness_mm; /* Overall PCB thickness */
    double              copper_oz_total; /* Sum of copper weights across all layers */
    double              k_effective_xy;  /* Effective in-plane conductivity (rule of mixtures) */
    double              k_effective_z;   /* Effective through-plane conductivity (series) */
} pcb_stackup_t;

/** Thermal via geometry and properties.
 *  L1: A thermal via is a plated through-hole optimized for heat conduction.
 *  Unlike signal vias, thermal vias focus on maximum copper cross-section
 *  and shortest path to a thermal plane.
 *
 *  Key parameters:
 *  - Diameter: affects cross-sectional area (A = pi(d^2 - (d-2t)^2)/4)
 *  - Plating thickness: standard 25 um, can be 35 um for thermal
 *  - Pitch: center-to-center spacing in array
 *  - Array shape: rectangular grid, hexagonal grid
 *  - Filled vs unfilled: filled with solder/copper increases conductivity
 *  Reference: IPC-4761 (via protection), IPC-2221 (generic PCB design) */
typedef struct {
    double  drill_diameter_mm;    /* Finished hole diameter (mm) */
    double  pad_diameter_mm;      /* Pad diameter on outer layers (mm) */
    double  plating_thickness_mm; /* Copper plating thickness in barrel (mm) */
    double  pitch_mm;             /* Center-to-center spacing (mm) */
    int     num_vias;             /* Number of vias in array */
    int     rows;                 /* Number of rows in rectangular array */
    int     cols;                 /* Number of columns in rectangular array */
    int     is_hexagonal;         /* 1 = hexagonal (staggered) grid, 0 = rectangular */
    int     is_filled;            /* 1 = filled with conductive material, 0 = hollow */
    double  fill_k;               /* Thermal conductivity of fill material (W/m-K) */
    double  via_length_mm;        /* Length of via barrel (= dielectric thickness) */
    double  r_theta_single;       /* Thermal resistance of a single via (C/W) */
    double  r_theta_array;        /* Total thermal resistance of via array (C/W) */
} thermal_via_geometry_t;

/** Heat source — a component dissipating power on the PCB.
 *  L1: Models a single heat-dissipating device on the board.
 *  Multiple heat sources interact through thermal coupling (mutual heating).
 *  Reference: JEDEC JESD51-14, "Transient Dual Interface Test Method" */
typedef struct {
    thermal_point_t  center;        /* Center position on PCB */
    double           width_mm;      /* Package width (mm) */
    double           length_mm;     /* Package length (mm) */
    double           height_mm;     /* Package height above board (mm) */
    double           power_w;       /* Power dissipation (W) */
    package_type_t   package;       /* Package type */
    double           r_jc;          /* Junction-to-case thermal resistance (C/W) */
    double           max_tj;        /* Maximum allowed junction temperature (C) */
    double           tj;            /* Calculated junction temperature (C) */
    double           tc;            /* Calculated case temperature (C) */
    double           tb;            /* Calculated board temperature under component (C) */
    int              has_heatsink;  /* 1 = heat sink attached */
    double           heatsink_r_sa; /* Heat sink thermal resistance (C/W) */
    int              has_thermal_vias; /* 1 = thermal vias under component */
    thermal_via_geometry_t via_geom; /* Via geometry (if used) */
} heat_source_t;

/** Copper pour geometry — PCB copper area for heat spreading.
 *  L1: A solid copper area on a PCB layer used to spread heat laterally.
 *  The effective spreading angle in copper is ~45 degrees from source edge.
 *  R_spread = ln(r2/r1) / (2 * pi * k_Cu * t)
 *  where r1 = source equivalent radius, r2 = pour outer radius. */
typedef struct {
    double  width_mm;          /* Pour width (mm) */
    double  length_mm;         /* Pour length (mm) */
    double  thickness_mm;      /* Copper thickness (mm) */
    double  area_mm2;          /* Total copper area (mm^2) */
    double  source_area_mm2;   /* Component contact area (mm^2) */
    double  r_spreading;       /* Spreading thermal resistance (C/W) */
    double  r_convection;      /* Convection resistance from pour to air (C/W) */
    double  effective_h;       /* Effective heat transfer coefficient (W/m^2-K) */
} copper_pour_geom_t;

/** Heat sink geometric and thermal model.
 *  L1: Finned aluminum extrusion parameters for natural/forced convection.
 *  Fin efficiency: eta = tanh(m*H) / (m*H)
 *  where m = sqrt(2*h / (k_fin * t_fin))
 *  h = convection coefficient, k_fin = fin material conductivity,
 *  H = fin height, t_fin = fin thickness */
typedef struct {
    double  base_width_mm;     /* Heat sink base width (mm) */
    double  base_length_mm;    /* Heat sink base length (mm) */
    double  base_thickness_mm; /* Base plate thickness (mm) */
    double  fin_height_mm;     /* Fin height from base (mm) */
    double  fin_thickness_mm;  /* Thickness of each fin (mm) */
    double  fin_spacing_mm;    /* Gap between adjacent fins (mm) */
    int     num_fins;          /* Number of fins */
    double  fin_efficiency;    /* Fin heat transfer efficiency (0-1) */
    double  surface_area_mm2;  /* Total effective surface area (mm^2) */
    double  r_sa_natural;      /* Sink-to-ambient, natural convection (C/W) */
    double  r_sa_forced;       /* Sink-to-ambient, forced convection (C/W) */
    double  weight_g;          /* Heat sink mass (g) */
    double  k_material;        /* Heat sink material conductivity (W/m-K), Al = 205 */
} heat_sink_model_t;

/** Thermal interface material (TIM) properties.
 *  L1: TIM fills microscopic air gaps between component and heat sink.
 *  Without TIM, air gaps (k = 0.026 W/m-K) create high thermal resistance.
 *  Key metric: Rcs = BLT / (k * A) + R_contact
 *  where BLT = bond line thickness (actual joint thickness after compression).
 *
 *  Common TIM types:
 *  - Thermal grease:  k = 0.5-5 W/m-K,  BLT ~25-50 um
 *  - Phase change:    k = 0.5-5 W/m-K,  BLT ~12-25 um
 *  - Gap pad:         k = 1-15 W/m-K,   BLT ~0.5-5 mm
 *  - Graphite sheet:  k = 200-500 W/m-K (in-plane), ~5 W/m-K (through)
 *  - Indium solder:   k = 86 W/m-K,     BLT ~25-50 um
 *  - Silver epoxy:    k = 2-8 W/m-K,    BLT ~25-100 um */
typedef struct {
    double  k_tim;              /* Bulk thermal conductivity (W/m-K) */
    double  thickness_mm;       /* Nominal thickness before compression (mm) */
    double  blt_mm;             /* Bond line thickness after compression (mm) */
    double  contact_area_mm2;   /* Contact area (mm^2) */
    double  contact_pressure_psi; /* Mounting pressure (psi) */
    int     is_phase_change;    /* 1 = phase change material */
    double  phase_change_temp;  /* Phase change temperature (C) */
    double  r_cs;               /* Calculated case-to-sink resistance (C/W) */
} tim_properties_t;

/** Ambient environment conditions.
 *  L1: Defines the boundary conditions for the thermal system.
 *  Altitude matters: at 3000m, convection is ~30% less effective
 *  due to reduced air density. */
typedef struct {
    double  ambient_temp_c;         /* Ambient air temperature (C) */
    double  pressure_kpa;           /* Atmospheric pressure (kPa), sea level = 101.325 */
    double  altitude_m;             /* Altitude above sea level (m) */
    double  air_density_kgm3;       /* Air density (kg/m^3) */
    double  air_conductivity;       /* Air thermal conductivity (W/m-K), ~0.026 at 25 C */
    double  air_dynamic_viscosity;  /* Dynamic viscosity (Pa-s), ~1.85e-5 at 25 C */
    double  air_prandtl;            /* Prandtl number (dimensionless), ~0.71 for air */
    int     is_enclosed;            /* 1 = enclosed chassis, 0 = open air */
    double  enclosure_volume_m3;    /* Enclosure volume (m^3) for enclosed designs */
    double  airflow_velocity_ms;    /* Forced airflow velocity (m/s) */
} ambient_conditions_t;

/** Steady-state temperature field — 2D grid of temperature values.
 *  L2: Represents the solution of the steady-state heat equation on a PCB.
 *  Each grid cell contains the local temperature after thermal equilibrium. */
typedef struct {
    double            **T_c;         /* Temperature grid [row][col] (C) */
    double            **k_xy;        /* Thermal conductivity grid (W/m-K) */
    int                 rows;        /* Number of grid rows */
    int                 cols;        /* Number of grid columns */
    double              dx_mm;       /* Grid spacing in X (mm) */
    double              dy_mm;       /* Grid spacing in Y (mm) */
    double              board_width_mm;  /* Overall board width (mm) */
    double              board_height_mm; /* Overall board height (mm) */
    double              thickness_mm;    /* Board thickness (mm) */
    heat_source_t     **sources;     /* Array of heat sources on the board */
    int                 num_sources; /* Number of heat sources */
    ambient_conditions_t ambient;    /* Ambient conditions */
    int                 converged;   /* 1 = solution converged */
    double              residual;    /* Final residual */
    int                 iterations;  /* Number of iterations to convergence */
} thermal_field_t;

/* ==================================================================
 * L1: Error and Status Codes — 14 thermal-specific error conditions
 * ================================================================== */

typedef enum {
    THERMAL_OK              =  0,  /* Success */
    THERMAL_ERR_NULL_PTR    = -1,  /* NULL pointer argument */
    THERMAL_ERR_INVALID_MAT = -2,  /* Invalid material type */
    THERMAL_ERR_NO_CONVERGE = -3,  /* Solver did not converge */
    THERMAL_ERR_TJ_EXCEEDED = -4,  /* Junction temperature exceeds max */
    THERMAL_ERR_DIM_MISMATCH= -5,  /* Array dimension mismatch */
    THERMAL_ERR_MEMORY      = -6,  /* Memory allocation failure */
    THERMAL_ERR_NEG_TEMP    = -7,  /* Temperature below absolute zero */
    THERMAL_ERR_NEG_POWER   = -8,  /* Negative power dissipation */
    THERMAL_ERR_ZERO_AREA   = -9,  /* Zero contact area */
    THERMAL_ERR_ZERO_THICK  = -10, /* Zero layer thickness */
    THERMAL_ERR_INVALID_ENV = -11, /* Invalid ambient conditions */
    THERMAL_ERR_DIV_ZERO    = -12, /* Division by zero */
    THERMAL_ERR_NOT_IMPL    = -13  /* Feature not yet implemented */
} thermal_error_t;

/** Convection correlation type — empirical models for heat transfer coefficient.
 *  Natural convection (vertical/horizontal plate):
 *    Nu = C * (Gr*Pr)^n  ->  h = Nu * k / L
 *  Forced convection (flat plate):
 *    Nu = 0.664 * Re^(1/2) * Pr^(1/3)  (laminar, Re < 5e5)
 *    Nu = 0.037 * Re^(4/5) * Pr^(1/3)  (turbulent, Re > 5e5)
 *  Reference: Incropera & DeWitt, "Fundamentals of Heat and Mass Transfer" */
typedef enum {
    CONV_CORR_NAT_VERT_PLATE  = 0,  /* Vertical plate natural convection */
    CONV_CORR_NAT_HORIZ_TOP   = 1,  /* Horizontal plate, heated top surface */
    CONV_CORR_NAT_HORIZ_BOT   = 2,  /* Horizontal plate, heated bottom surface */
    CONV_CORR_FORCED_FLAT     = 3,  /* Forced convection, flat plate */
    CONV_CORR_FORCED_CYLINDER = 4,  /* Forced convection, cylinder in cross flow */
    CONV_CORR_FORCED_ARRAY    = 5,  /* Forced convection, fin array */
    CONV_CORR_NAT_CYLINDER    = 6,  /* Natural convection, horizontal cylinder */
    CONV_CORR_MIXED           = 7   /* Mixed (natural + forced) convection */
} convection_correlation_t;

/** Complete material thermal property record.
 *  L1: Every material used in PCB thermal analysis needs these properties.
 *  rho = density (kg/m^3), cp = specific heat capacity (J/kg-K),
 *  k = thermal conductivity (W/m-K), alpha = thermal diffusivity (m^2/s) = k/(rho*cp),
 *  CTE = coefficient of thermal expansion (ppm/K). */
typedef struct {
    const char *name;               /* Material name */
    double      density_kgm3;       /* Density rho (kg/m^3) */
    double      spec_heat_jkgk;     /* Specific heat capacity cp (J/kg-K) */
    double      k_xy;               /* In-plane thermal conductivity (W/m-K) */
    double      k_z;                /* Through-plane thermal conductivity (W/m-K) */
    double      diffusivity;        /* Thermal diffusivity alpha (m^2/s) = k/(rho*cp) */
    double      cte_ppm_per_k;      /* CTE in X-Y (ppm/K) */
    double      cte_z_ppm_per_k;    /* CTE in Z (ppm/K), critical for via reliability */
    double      tg_c;               /* Glass transition temperature (C), for polymers */
    double      max_temp_c;         /* Maximum continuous operating temperature (C) */
    int         is_electrical_conductor; /* 1 = conductive (copper, aluminum) */
} material_property_t;

/* ==================================================================
 * L1: Convenience Inline Functions
 * ================================================================== */

/** Initialize thermal point to origin. */
static inline thermal_point_t thermal_point_init(void) {
    return (thermal_point_t){.x_mm = 0.0, .y_mm = 0.0, .z_mm = 0.0};
}

/** Initialize ambient conditions to standard sea level 25 C. */
static inline ambient_conditions_t ambient_default(void) {
    return (ambient_conditions_t){
        .ambient_temp_c = 25.0,
        .pressure_kpa = 101.325,
        .altitude_m = 0.0,
        .air_density_kgm3 = 1.184,
        .air_conductivity = 0.02624,
        .air_dynamic_viscosity = 1.849e-5,
        .air_prandtl = 0.71,
        .is_enclosed = 0,
        .enclosure_volume_m3 = 0.0,
        .airflow_velocity_ms = 0.0
    };
}

/** Compute effective junction-to-ambient resistance from component.
 *  Top path: Rjc + Rcs + Rsa
 *  Board path: Rjb
 *  Parallel combination: 1 / (1/R_top + 1/R_jb)
 *  Reference: JEDEC JESD51-2, "Integrated Circuits Thermal Test Method" */
static inline double thermal_rja_effective(const thermal_resistance_t *rt) {
    if (!rt) return INFINITY;
    double r_top_path = rt->r_jc + rt->r_cs + rt->r_sa;
    if (r_top_path <= 0.0) return rt->r_jb;
    if (rt->r_jb <= 0.0) return r_top_path;
    return 1.0 / (1.0 / r_top_path + 1.0 / rt->r_jb);
}

/** Compute junction temperature from power and thermal resistance.
 *  L1: Tj = Ta + P * Rja  (steady-state, single source)
 *  This is the most fundamental equation in electronics thermal management. */
static inline double tj_from_power(double ta_c, double power_w, double rja) {
    return ta_c + power_w * rja;
}

/** Compute heat flux through a surface.
 *  L1: q" = P / A  (W/m^2)
 *  Heat flux determines whether a cooling solution is adequate.
 *  Natural convection can handle ~0.1-0.5 W/cm^2 without heatsink.
 *  Forced convection: ~1-5 W/cm^2 with heatsink.
 *  Liquid cooling: >10 W/cm^2. */
static inline double heat_flux_wm2(double power_w, double area_mm2) {
    if (area_mm2 <= 0.0) return INFINITY;
    return power_w / (area_mm2 * 1.0e-6);
}

/** Compute copper thickness from weight designation.
 *  L1: 1 oz copper = 34.79 um per IPC standard */
static inline double copper_thickness_mm(copper_weight_t wt) {
    const double t_per_oz = 0.03479;
    switch (wt) {
        case CU_0_5OZ: return 0.5 * t_per_oz;
        case CU_1OZ:   return 1.0 * t_per_oz;
        case CU_2OZ:   return 2.0 * t_per_oz;
        case CU_3OZ:   return 3.0 * t_per_oz;
        case CU_4OZ:   return 4.0 * t_per_oz;
        case CU_6OZ:   return 6.0 * t_per_oz;
        case CU_10OZ:  return 10.0 * t_per_oz;
        default:      return 0.0;
    }
}

/** Validate a thermal resistance struct — all values must be non-negative. */
static inline int thermal_resistance_is_valid(const thermal_resistance_t *rt) {
    if (!rt) return 0;
    return (rt->r_jc >= 0.0 && rt->r_cs >= 0.0 && rt->r_sa >= 0.0 &&
            rt->r_jb >= 0.0 && rt->r_ja >= 0.0);
}

/** Validate heat source has realistic power and temperature limits. */
static inline int heat_source_is_valid(const heat_source_t *src) {
    if (!src) return 0;
    if (src->power_w < 0.0 || src->max_tj < -50.0) return 0;
    if (src->width_mm <= 0.0 || src->length_mm <= 0.0) return 0;
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* PCB_THERMAL_DEFS_H */