#ifndef DFM_CORE_H
#define DFM_CORE_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef enum { IPC_CLASS_1 = 0, IPC_CLASS_2 = 1, IPC_CLASS_3 = 2 } ipc_class_t;
const char* ipc_class_name(ipc_class_t cls);

typedef enum { DRC_FATAL = 0, DRC_ERROR = 1, DRC_WARN = 2, DRC_INFO = 3 } drc_severity_t;

typedef enum {
    SUBSTRATE_FR4 = 0, SUBSTRATE_ROGERS_4350 = 1, SUBSTRATE_POLYIMIDE = 2,
    SUBSTRATE_CERAMIC_ALUMINA = 3, SUBSTRATE_PTFE = 4, SUBSTRATE_BT_EPOXY = 5,
    SUBSTRATE_CEM1 = 6, SUBSTRATE_CEM3 = 7
} substrate_material_t;

typedef struct {
    substrate_material_t material;
    const char *trade_name;
    double dielectric_constant;
    double loss_tangent;
    double tg_celsius;
    double cte_xy_ppm;
    double cte_z_ppm;
    double thermal_conductivity;
    bool halogen_free;
} substrate_properties_t;

const substrate_properties_t* substrate_lookup(substrate_material_t mat);

typedef enum {
    FINISH_HASL_SNPB = 0, FINISH_HASL_LF = 1, FINISH_ENIG = 2,
    FINISH_ENEPIG = 3, FINISH_OSP = 4, FINISH_IMMERSION_AG = 5,
    FINISH_IMMERSION_SN = 6, FINISH_HARD_GOLD = 7
} surface_finish_t;

typedef struct {
    surface_finish_t finish;
    const char *name;
    double thickness_um;
    double shelf_life_months;
    bool rohs_compliant;
    bool wire_bondable;
    double cost_factor;
} finish_properties_t;

const finish_properties_t* finish_lookup(surface_finish_t fin);

typedef enum {
    CU_WEIGHT_0_5_OZ = 0, CU_WEIGHT_1_0_OZ = 1, CU_WEIGHT_2_0_OZ = 2,
    CU_WEIGHT_3_0_OZ = 3, CU_WEIGHT_4_0_OZ = 4, CU_WEIGHT_6_0_OZ = 5
} copper_weight_t;

double copper_weight_to_um(copper_weight_t cw, bool plated);

typedef enum {
    LAYER_SIGNAL = 0, LAYER_POWER = 1, LAYER_GROUND = 2,
    LAYER_MIXED = 3, LAYER_DIELECTRIC = 4
} layer_type_t;

typedef struct {
    double cp, cpk, cpk_upper, cpk_lower;
    double mean_um, stddev_um, usl_um, lsl_um;
    bool capable;
} process_capability_t;

process_capability_t compute_process_capability(const double *measurements,
    size_t N, double usl_um, double lsl_um);

typedef struct {
    const char *rule_name;
    const char *description;
    double class1_value_um, class2_value_um, class3_value_um;
    bool is_maximum;
    const char *ipc_reference;
} design_rule_def_t;

typedef struct {
    double x_mm, y_mm;
    int layer;
    const char *net_name;
    const char *component;
} drc_location_t;

typedef struct {
    drc_severity_t severity;
    const char *rule_name;
    const char *message;
    drc_location_t location;
    double measured_value_um;
    double required_value_um;
    double margin_um;
    ipc_class_t ipc_class;
} drc_violation_t;

typedef struct {
    drc_violation_t *violations;
    size_t num_violations, capacity;
    size_t num_fatal, num_errors, num_warnings, num_info;
    double total_runtime_sec;
    ipc_class_t checked_class;
    bool passed;
} drc_result_t;

void drc_result_init(drc_result_t *result, ipc_class_t ipc_class, size_t capacity);
void drc_result_add(drc_result_t *result, const drc_violation_t *v);
void drc_result_free(drc_result_t *result);
void drc_result_report(const drc_result_t *result, bool show_all);

/* L3: DPMO estimation from Cpk */
double compute_dpmo_from_cpk(double cpk);

/* L2: OEE (Overall Equipment Effectiveness) */
double compute_pcb_oee(double planned_time_hr, double downtime_hr,
                       double ideal_cycle_sec, double total_units,
                       double good_units);

typedef enum {
    GERBER_TOP_COPPER = 0, GERBER_INNER1_COPPER = 1, GERBER_INNER2_COPPER = 2,
    GERBER_BOTTOM_COPPER = 3, GERBER_TOP_SOLDERMASK = 4, GERBER_BOTTOM_SOLDERMASK = 5,
    GERBER_TOP_SILKSCREEN = 6, GERBER_BOTTOM_SILKSCREEN = 7,
    GERBER_TOP_PASTE = 8, GERBER_BOTTOM_PASTE = 9, GERBER_DRILL_NC = 10,
    GERBER_BOARD_OUTLINE = 11, GERBER_SOLDERPASTE_TOP = 12, GERBER_SOLDERPASTE_BOT = 13
} gerber_layer_type_t;

typedef enum {
    VIA_THROUGH_HOLE = 0, VIA_BLIND = 1, VIA_BURIED = 2,
    VIA_MICROVIA = 3, VIA_STACKED = 4, VIA_STAGGERED = 5
} via_type_t;

typedef struct {
    via_type_t type;
    double drill_diameter_mm, pad_diameter_mm;
    double annular_ring_mm, aspect_ratio;
    int start_layer, end_layer;
    bool tented, plugged;
} via_geometry_t;

bool via_annular_ring_ok(const via_geometry_t *via, ipc_class_t ipc_class);
bool via_aspect_ratio_ok(const via_geometry_t *via);

typedef enum { FIDUCIAL_GLOBAL = 0, FIDUCIAL_LOCAL = 1, FIDUCIAL_PANEL = 2 } fiducial_type_t;

typedef struct {
    fiducial_type_t type;
    double center_x_mm, center_y_mm;
    double copper_diameter_mm, mask_opening_mm, clearance_radius_mm;
} fiducial_mark_t;

#endif
