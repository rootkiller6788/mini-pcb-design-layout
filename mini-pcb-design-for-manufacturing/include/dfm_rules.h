/**
 * @file    dfm_rules.h
 * @brief   DFM Design Rules - L1 Definitions, L2 Concepts
 *
 * @details Comprehensive design rule definitions for PCB manufacturability.
 *          Covers trace/space, drill, mask, silkscreen, plane connections,
 *          and advanced HDI constraints.
 *
 * Knowledge Mapping:
 *   L1 - Definitions: min trace width, min spacing, annular ring,
 *        solder mask expansion, silkscreen, drill-to-copper, edge clearance
 *   L2 - Core Concepts: etching factor, mask registration, creepage,
 *        thermal relief design, voltage clearance
 *
 * Reference: IPC-2221, IPC-2222, IPC-6012
 */

#ifndef DFM_RULES_H
#define DFM_RULES_H

#include "dfm_core.h"
#include <stddef.h>
#include <stdbool.h>

/* ---- Trace Width Rules ---- */

typedef struct {
    copper_weight_t copper;
    double class1_min_width_um;
    double class2_min_width_um;
    double class3_min_width_um;
    double etch_factor_um;
} trace_width_rule_t;

double get_min_trace_width(copper_weight_t copper, ipc_class_t cls);
double get_etch_compensation(copper_weight_t copper);

/* ---- Spacing / Clearance Rules ---- */

typedef struct {
    double voltage_peak;
    double spacing_internal_um;
    double spacing_external_um;
    double spacing_coated_um;
    bool   is_high_altitude;
} voltage_spacing_rule_t;

double compute_required_spacing(double voltage_peak, bool is_external,
                                bool is_coated, bool is_high_alt);

/* ---- Annular Ring Rules ---- */

typedef struct {
    ipc_class_t ipc_class;
    double min_external_ring_um;
    double min_internal_ring_um;
    double max_breakout_pct;
} annular_ring_rule_t;

const annular_ring_rule_t* get_annular_ring_rule(ipc_class_t cls);

bool check_annular_ring(double pad_diameter_um, double drill_diameter_um,
                        bool is_external, ipc_class_t ipc_class,
                        double breakout_deg);

/* ---- Solder Mask Rules ---- */

typedef struct {
    double min_mask_expansion_um;
    double max_mask_expansion_um;
    double registration_tol_um;
    double min_mask_web_um;
    double min_mask_dam_um;
} solder_mask_rule_t;

const solder_mask_rule_t* get_solder_mask_rule(ipc_class_t cls);
double compute_mask_opening(double pad_diameter_um, double expansion_um);
bool check_mask_web(double opening1_um, double opening2_um,
                    double center_dist_um, double min_web_um);

/* ---- Silkscreen Rules ---- */

typedef struct {
    double min_line_width_um;
    double min_text_height_um;
    double min_line_spacing_um;
    double clearance_to_pad_um;
    double clearance_to_mask_um;
    double clearance_to_edge_um;
} silkscreen_rule_t;

const silkscreen_rule_t* get_silkscreen_rule(ipc_class_t cls);

/* ---- Drill Rules ---- */

typedef struct {
    double min_drill_diameter_mm;
    double max_drill_diameter_mm;
    double min_drill_to_copper_um;
    double min_slot_width_mm;
    double max_aspect_ratio;
    double min_hole_to_hole_mm;
    double min_hole_to_edge_mm;
    double laser_via_max_depth_um;
    double laser_via_max_aspect;
} drill_rule_t;

const drill_rule_t* get_drill_rule(bool is_advanced_process);

/* ---- Edge Clearance ---- */

typedef struct {
    double copper_to_edge_um;
    double drill_to_edge_um;
    double npth_to_edge_um;
    double via_to_edge_um;
    double inner_plane_pullback_um;
} edge_clearance_rule_t;

const edge_clearance_rule_t* get_edge_clearance_rule(ipc_class_t cls);

/* ---- Thermal Relief ---- */

typedef struct {
    double min_spoke_width_um;
    double max_spoke_width_um;
    double min_clearance_gap_um;
    double min_connection_um;
    int    typical_num_spokes;
    double antipad_expansion_um;
} thermal_relief_rule_t;

const thermal_relief_rule_t* get_thermal_relief_rule(ipc_class_t cls);

double compute_thermal_relief_area(double pad_diameter_um, double antipad_diameter_um,
                                    double spoke_width_um, int num_spokes,
                                    double copper_thickness_um);

/* ---- IPC-2221 Electrical Clearance ---- */

/**
 * Compute minimum electrical clearance for a given voltage.
 *
 * IPC-2221 spacing formula:
 *   For 0-15V:    d = 0.05 mm
 *   For 15-30V:   d = 0.10 mm
 *   For 30-50V:   d = 0.20 mm
 *   For 50-100V:  d = 0.40 mm
 *   For 100-150V: d = 0.60 mm
 *   For 150-170V: d = 1.00 mm
 *   For 170-250V: d = 1.25 mm
 *   For 250-300V: d = 1.50 mm
 *   For 300-500V: d = 2.50 mm
 *   >500V:        d = 0.005 * V + 0.5 mm
 *
 * Internal layers: multiply by 1.5
 * Coated: multiply by 0.5
 * High altitude >3000m: multiply by 1.3 per 1000m
 *
 * @param voltage_peak  Peak voltage (V)
 * @param is_external   External layer
 * @param is_coated     Conformal coating
 * @param altitude_m    Operating altitude (m)
 * @return              Minimum clearance in micrometers
 */
double compute_ipc2221_clearance(double voltage_peak, bool is_external,
                                  bool is_coated, double altitude_m);

/* ---- Etching Compensation ---- */

/**
 * Calculate etch compensation for reliable trace width.
 *
 * W_designed = W_target + 2 * copper_thickness * etch_factor
 *
 * @param target_width_um       Desired finished trace width
 * @param copper_thickness_um   Copper foil thickness
 * @param etch_factor           Process-dependent factor (typ 1.0)
 * @return                      Compensated design width in um
 */
double compute_etch_compensation(double target_width_um,
                                  double copper_thickness_um,
                                  double etch_factor);

#endif /* DFM_RULES_H */
