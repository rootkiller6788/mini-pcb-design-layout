/**
 * @file    dfm_panel.h
 * @brief   PCB Panelization and Tooling - L1 Defs, L5 Optimization
 *
 * @details Panelization design: array layout, breakaway methods,
 *          tooling features, fiducial placement, panel utilization
 *          optimization via 2D bin packing.
 *
 * Knowledge Mapping:
 *   L1 - Definitions: panel config, V-score, tab-routing, mouse-bites,
 *        tooling holes, fiducial marks, copper thieving
 *   L5 - Algorithms: 2D bin packing, utilization optimization
 *   L6 - Canonical Problems: panel utilization maximization
 *
 * Reference: IPC-2221, IPC-7351
 */

#ifndef DFM_PANEL_H
#define DFM_PANEL_H

#include "dfm_core.h"
#include <stddef.h>
#include <stdbool.h>

/* ---- Breakaway ---- */

typedef enum {
    BREAKAWAY_VSCORE     = 0,
    BREAKAWAY_TAB_ROUTE  = 1,
    BREAKAWAY_MOUSE_BITE = 2,
    BREAKAWAY_PUNCH      = 3
} breakaway_method_t;

typedef struct {
    double score_depth_top_mm;
    double score_depth_bot_mm;
    double remaining_web_mm;
    double board_thickness_mm;
    double angle_deg;
    double score_offset_mm;
} vscore_params_t;

typedef struct {
    double tab_width_mm;
    double tab_count;
    double route_width_mm;
    double route_to_copper_mm;
    double min_tab_spacing_mm;
} tab_route_params_t;

typedef struct {
    double hole_diameter_mm;
    double hole_spacing_mm;
    double web_width_mm;
    double edge_roughness_um;
} mouse_bite_params_t;

/* ---- Panel Config ---- */

typedef struct {
    double panel_width_mm;
    double panel_height_mm;
    double board_width_mm;
    double board_height_mm;
    double board_spacing_mm;
    int    boards_x;
    int    boards_y;
    int    total_boards;
    double rail_width_mm;
    double rail_height_mm;
    double utilization_pct;
    breakaway_method_t breakaway;
    bool   has_top_rail;
    bool   has_bot_rail;
    bool   has_left_rail;
    bool   has_right_rail;
} panel_config_t;

/* ---- Optimization ---- */

typedef struct {
    panel_config_t config;
    double panel_area_mm2;
    double board_total_area_mm2;
    double wasted_area_mm2;
    double utilization_pct;
    int    num_configurations_evaluated;
} panel_optimization_result_t;

panel_optimization_result_t optimize_panel_utilization(
    double board_width_mm, double board_height_mm,
    double panel_width_mm, double panel_height_mm,
    double min_spacing_mm, double edge_clearance_mm,
    double rail_width_mm, breakaway_method_t method);

double calculate_panel_utilization(const panel_config_t *config);

double estimate_panel_cost(double panel_width_mm, double panel_height_mm,
                           int num_layers, ipc_class_t ipc_class);

/* ---- Tooling ---- */

typedef struct {
    double diameter_mm;
    double center_x_mm;
    double center_y_mm;
    bool   is_plated;
    double tolerance_mm;
} tooling_hole_t;

void generate_tooling_holes(double panel_width_mm, double panel_height_mm,
                            double rail_width_mm,
                            tooling_hole_t holes[4], int *num_holes);

void generate_global_fiducials(double board_width_mm, double board_height_mm,
                               double edge_offset_mm,
                               fiducial_mark_t marks[3], int *num_marks);

/* ---- Tab Placement ---- */

int compute_tab_placements(double edge_length_mm, double tab_width_mm,
                           int min_tab_count, double min_corner_offset_mm,
                           double positions_mm[], int max_positions);

/* ---- Copper Thieving ---- */

typedef struct {
    double pattern_size_mm;
    double pattern_spacing_mm;
    double clearance_to_feature_mm;
    double fill_percentage;
    bool   is_crosshatch;
    bool   electrically_floating;
} copper_thieving_t;

double compute_copper_fill(double copper_area_mm2, double total_area_mm2);

/* V-score parameter validation */
bool validate_vscore_params(const vscore_params_t *params);

/* Panel breakeven quantity vs individual processing */
int panel_breakeven_quantity(double individual_setup_sec,
                              double individual_process_sec,
                              double panel_setup_sec,
                              double panel_process_sec_per_board,
                              double depaneling_sec_per_board,
                              double labor_rate_per_hour);

copper_thieving_t design_copper_thieving(double current_fill_pct,
                                          double target_fill_pct,
                                          double board_area_mm2,
                                          double clearance_mm);

#endif /* DFM_PANEL_H */
