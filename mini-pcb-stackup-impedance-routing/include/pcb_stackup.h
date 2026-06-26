#ifndef PCB_STACKUP_H
#define PCB_STACKUP_H

#include "pcb_material.h"
#include "pcb_impedance.h"

/* ============================================================================
 * L1: Core Definitions — PCB Stackup
 *
 * A PCB stackup defines the physical arrangement of conducting and insulating
 * layers. Each layer has a type (signal, plane, mixed), copper weight,
 * and is separated by dielectric (core or prepreg).
 *
 * Key design rules:
 *  - Symmetry about center (minimizes warpage)
 *  - Signal layers adjacent to planes (controlled impedance)
 *  - Orthogonal routing on adjacent signal layers
 *  - Adequate plane capacitance for PDN
 * ========================================================================= */

/* L1: Layer type */
typedef enum {
    LAYER_SIGNAL    = 0,
    LAYER_PLANE     = 1,
    LAYER_MIXED     = 2,   /* Signal + plane pour on same layer */
    LAYER_SOLDERMASK = 3,
    LAYER_SILKSCREEN = 4
} LayerType;

/* L1: A single layer in the stackup */
typedef struct {
    const char   *name;           /* Layer name, e.g., "TOP", "GND02", "PWR03" */
    LayerType     type;
    double        copper_weight_oz; /* 0.5, 1.0, 2.0 oz/ft² */
    double        copper_thickness_um; /* Computed: 1 oz = 35 µm */
    double        finished_thickness_um; /* After plating */
    const DielectricMaterial *material; /* May be NULL for conductor layers */
    int           is_core;         /* Core vs prepreg (for dielectrics) */
    double        dielectric_thickness_um; /* For dielectric layers */
    int           signal_layer_index; /* 1-indexed signal layer number (0 if plane) */
} LayerDefinition;

/* L1: Complete PCB stackup definition */
typedef struct {
    const char     *name;
    int             num_layers;     /* Total layer count (2, 4, 6, 8, 10, 12, ...) */
    int             num_signal_layers;
    int             num_plane_layers;
    LayerDefinition layers[32];     /* Max 32 layers */
    double          total_thickness_mm;
    double          copper_weight_outer_oz;
    double          copper_weight_inner_oz;
    const DielectricMaterial *core_material;
    const DielectricMaterial *prepreg_material;
} PcbStackup;

/* L1: Impedance target for a specific signal layer */
typedef struct {
    int    signal_layer_index;
    double target_z0_ohm;
    double tolerance_pct;
    double trace_width_um;    /* Computed required width */
    double trace_spacing_um;  /* For differential pairs */
    int    is_differential;
} LayerImpedanceTarget;

/* ===================================================================
 * L2: Core Concepts — Stackup construction and validation
 * =================================================================== */

/* L2: Initialize a new stackup builder */
PcbStackup stackup_create(const char *name, int num_layers);

/* L2: Add a conductor layer (signal or plane) */
void stackup_add_conductor_layer(PcbStackup *stackup, int position,
                                  const char *name, LayerType type,
                                  double copper_weight_oz, int signal_idx);

/* L2: Add a dielectric layer between conductors */
void stackup_add_dielectric_layer(PcbStackup *stackup, int position,
                                   const char *name, const DielectricMaterial *mat,
                                   int is_core, double thickness_um);

/* L2: Build and validate the stackup
 *     Checks: layer sequence alternating conductor/dielectric,
 *     symmetry, minimum thickness, etc.
 *     Returns 0 on success, error code on failure. */
int stackup_build(PcbStackup *stackup);

/* L2: Print the stackup table (like a fab drawing) */
void stackup_print(const PcbStackup *stackup);

/* L2: Validate stackup symmetry (warpage prevention)
 *     Checks that layer thicknesses are symmetric about the center line.
 *     Returns percentage of asymmetry (0 = perfect). */
double stackup_symmetry_check(const PcbStackup *stackup);

/* L2: Validate impedance control feasibility
 *     Checks if target impedances can be achieved with given stackup geometry */
int stackup_impedance_feasibility(const PcbStackup *stackup,
                                   const LayerImpedanceTarget *targets,
                                   int num_targets,
                                   double *required_heights_um);

/* ===================================================================
 * L3: Mathematical Structures — Stackup optimization
 * =================================================================== */

/* L3: Compute plane capacitance for PDN
 *     C_plane = ε₀·εr·A / d
 *     where A = plane area (mm²), d = separation (µm)
 *     Returns capacitance in nF */
double stackup_plane_capacitance(const PcbStackup *stackup,
                                  double area_mm2);

/* L3: Compute inter-plane inductance for PDN
 *     L_plane ≈ μ₀·d·l / w  (for rectangular plane pair)
 *     Returns inductance in pH */
double stackup_plane_inductance(const PcbStackup *stackup,
                                 double plane_width_mm, double plane_length_mm);

/* L3: Compute layer-to-layer crosstalk coupling coefficient
 *     Based on mutual capacitance and inductance between adjacent layers.
 *     K = |K_L - K_C| (inductive vs capacitive coupling) */
double stackup_layer_coupling(const PcbStackup *stackup,
                               int layer_a, int layer_b);

/* L3: Compute the complete stackup RLGC matrix
 *     For N signal layers, produces N×N coupling matrix */
void stackup_rlgc_matrix(const PcbStackup *stackup, int num_signal_layers,
                          double **R_matrix, double **L_matrix,
                          double **G_matrix, double **C_matrix);

/* ===================================================================
 * L4: Fundamental Laws — Design rules and constraints
 * =================================================================== */

/* L4: Minimum trace width for given copper weight
 *     IPC-2221 class-based minimum trace width (mm)
 *     Class 1: General, Class 2: Dedicated Service, Class 3: High Reliability */
double stackup_min_trace_width(int ipc_class, double copper_weight_oz);

/* L4: Minimum spacing between traces (IPC-2221)
 *     Voltage-dependent clearance requirements */
double stackup_min_spacing(double voltage_v, int ipc_class, int is_internal);

/* L4: Compute conductor current-carrying capacity
 *     IPC-2152: I = k · ΔT^β1 · A^β2
 *     where A = w·t is cross-sectional area in mils²
 *     Returns max current in amps */
double stackup_trace_current_capacity(double width_mm, double thickness_um,
                                       double temp_rise_c, int is_internal);

/* L4: Compute temperature rise for given current
 *     Inverse of IPC-2152 current capacity relation */
double stackup_trace_temp_rise(double width_mm, double thickness_um,
                                double current_a, int is_internal);

/* L4: Compute via current capacity
 *     Based on barrel wall cross-sectional area.
 *     I_max = k · π·(D_outer² - D_inner²)/4 · J_max */
double stackup_via_current_capacity(double drill_diameter_mm,
                                     double plating_thickness_um,
                                     int num_vias);

/* ===================================================================
 * L5: Algorithms — Stackup design automation
 * =================================================================== */

/* L5: Auto-determine dielectric thickness to meet impedance targets
 *     Given desired Z₀ and trace width range, compute required height h.
 *     Uses bisection method on the impedance equation inverse. */
double stackup_auto_dielectric_height(double target_z0, double trace_width_um,
                                       double er, double thickness_um,
                                       double w_min_um, double w_max_um);

/* L5: Layer assignment algorithm for N-layer board
 *     Assigns signal/plane layers optimally for SI and EMI.
 *     Strategy: alternating S-G-S-P-S-G-S... (8 layers)
 *               or G-S-G-P-G-S-G... (6 layers) */
int stackup_auto_assign_layers(PcbStackup *stackup, int num_layers,
                                int num_power_rails, int impedance_control_layers);

/* L5: Compute optimal stackup for minimum thickness given Z₀ constraints */
int stackup_minimize_thickness(PcbStackup *stackup,
                                const LayerImpedanceTarget *targets,
                                int num_targets,
                                double max_thickness_mm);

/* ===================================================================
 * L6: Canonical Problems — Standard stackups
 * =================================================================== */

/* L6: 4-layer standard stackup (SIG-GND-PWR-SIG)
 *     JLCPCB JLC2313 / standard 1.6mm design */
PcbStackup stackup_standard_4layer(void);

/* L6: 6-layer stackup (SIG-GND-SIG-SIG-PWR-SIG)
 *     Common 1.6mm design for medium complexity boards */
PcbStackup stackup_standard_6layer(void);

/* L6: 8-layer high-density stackup
 *     (SIG-GND-SIG-PWR-GND-SIG-GND-SIG) */
PcbStackup stackup_standard_8layer(void);

/* L6: 10-layer advanced stackup for complex mixed-signal designs */
PcbStackup stackup_standard_10layer(void);

/* L6: Compute impedance targets for all signal layers of a stackup */
int stackup_compute_impedance_targets(const PcbStackup *stackup,
                                       double target_z0,
                                       LayerImpedanceTarget *targets,
                                       int max_targets);

/* ===================================================================
 * L7: Applications — Industry-specific stackup designs
 * =================================================================== */

/* L7: Automotive (ISO 26262) qualified stackup design
 *     Requirements: high Tg, halogen-free, CAF-resistant */
PcbStackup stackup_automotive_iso26262(int num_layers, double total_thickness_mm);

/* L7: Aerospace (IPC-6012 Class 3/A) stackup
 *     Requirements: high reliability, thermal cycling endurance */
PcbStackup stackup_aerospace_class3(int num_layers, double total_thickness_mm);

/* L7: Smartphone HDI (High-Density Interconnect) stackup
 *     Any-layer via, thin core, laser-drilled microvias */
PcbStackup stackup_smartphone_hdi(int num_layers);

/* ===================================================================
 * L8: Advanced — High-frequency and exotic stackup designs
 * =================================================================== */

/* L8: 112G PAM4 stackup design considerations
 *     Ultra-low-loss materials, backdrilling, tight impedance control */
typedef struct {
    double max_il_db_at_nyquist;   /* Max insertion loss at Nyquist (28 GHz for 112G) */
    double impedance_variation_pct; /* ±% Z₀ variation budget */
    double skew_ps;                 /* Max intra-pair skew */
    double material_df_at_10ghz;    /* Max dissipation factor at 10 GHz */
} Stackup112GRequirements;

PcbStackup stackup_112g_pam4(const Stackup112GRequirements *req);

/* L8: Rigid-flex stackup transitions
 *     Models the impedance discontinuity at rigid-flex transition */
typedef struct {
    double rigid_z0;
    double flex_z0;
    double transition_length_mm;
    double reflection_coeff;
    double max_insertion_loss_perturbation_db;
} RigidFlexTransition;

RigidFlexTransition stackup_rigid_flex_transition(
    const PcbStackup *rigid, const PcbStackup *flex,
    double transition_length_mm, double freq_ghz);

/* L8: Embedded capacitance layer design
 *     Using thin (<25µm) high-Dk material as distributed capacitance */
double stackup_embedded_capacitance(const PcbStackup *stackup,
                                     double high_dk, double thin_core_um,
                                     double area_mm2);

#endif /* PCB_STACKUP_H */
