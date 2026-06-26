/**
 * @file    dfm_panel.c
 * @brief   PCB Panelization and Tooling - L1-L6
 *
 * @details Panelization design for manufacturing efficiency:
 *          - 2D bin packing optimization for panel utilization
 *          - V-score, tab-routing, and mouse-bite breakaway methods
 *          - Tooling hole placement for registration
 *          - Fiducial mark generation for assembly alignment
 *          - Tab placement optimization along board edges
 *          - Copper thieving design for plating uniformity
 *          - Panel cost estimation
 *
 * Knowledge Mapping:
 *   L1 - Definitions: Panel config, V-score, tab-routing, mouse-bites,
 *        tooling holes, fiducial marks, copper thieving, rail/coupon
 *   L5 - Algorithms: 2D bin packing (greedy + rotation),
 *        utilization optimization, tab spacing computation
 *   L6 - Canonical Problems: Panel utilization maximization
 *
 * Reference: IPC-2221 (Design Standard), IPC-7351 (Land Pattern)
 *            H. Dyckhoff, "Cutting and Packing in Production" (1992)
 */

#include "dfm_panel.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ================================================================
   L5/L6 - Panel Utilization Optimization (2D Bin Packing)
   ================================================================

   Panel utilization is the fraction of the panel area occupied by
   actual boards (vs. waste). Higher utilization = lower cost per board.

   The problem: Given a panel size (Pw x Ph) and a board size (Bw x Bh),
   determine the maximum number of boards that can be placed on the
   panel, considering:
   1. Board-to-board spacing (for routing/tab-routing clearance)
   2. Edge clearance (copper-to-edge, tooling rail)
   3. Board rotation (can rotate 90 deg for better fit)
   4. Panel rail allowance (for handling, tooling holes, fiducials)

   This is a 2D rectangular packing problem. For PCB manufacturing,
   boards are typically placed in a regular grid (no nesting of
   irregular shapes). We evaluate both orientations (0 and 90 degrees)
   and select the configuration with maximum utilization.

   The greedy algorithm:
   1. Compute usable area = panel_area - rail_area
   2. Compute boards per row = floor(usable_width / (board_width + spacing))
   3. Compute boards per column = floor(usable_height / (board_height + spacing))
   4. Try both orientations
   5. Return best configuration

   This is a simple but effective heuristic. For complex board shapes
   or mixed panels (different board types on same panel), more
   sophisticated algorithms (shelf, guillotine, or free-form nesting)
   may be needed.
   ================================================================ */

panel_optimization_result_t optimize_panel_utilization(
    double board_width_mm, double board_height_mm,
    double panel_width_mm, double panel_height_mm,
    double min_spacing_mm, double edge_clearance_mm,
    double rail_width_mm, breakaway_method_t method)
{
    panel_optimization_result_t result;
    memset(&result, 0, sizeof(result));
    result.num_configurations_evaluated = 0;

    /* Input validation */
    if (board_width_mm <= 0.0 || board_height_mm <= 0.0 ||
        panel_width_mm <= 0.0 || panel_height_mm <= 0.0 ||
        board_width_mm > panel_width_mm ||
        board_height_mm > panel_height_mm) {
        return result;
    }

    double best_util = 0.0;
    panel_config_t best_config;
    memset(&best_config, 0, sizeof(best_config));
    int best_total  = 0;

    /* Try both orientations: 0 deg and 90 deg */
    for (int rot = 0; rot < 2; rot++) {
        double bw = (rot == 0) ? board_width_mm : board_height_mm;
        double bh = (rot == 0) ? board_height_mm : board_width_mm;

        /* Usable area = panel minus rails */
        double usable_w = panel_width_mm - 2.0 * rail_width_mm;
        double usable_h = panel_height_mm - 2.0 * rail_width_mm;

        if (usable_w <= 0.0 || usable_h <= 0.0) continue;

        /* Boards per row */
        int boards_x = (int)((usable_w - edge_clearance_mm)
                      / (bw + min_spacing_mm));
        if (boards_x < 1) boards_x = 1;

        /* Boards per column */
        int boards_y = (int)((usable_h - edge_clearance_mm)
                      / (bh + min_spacing_mm));
        if (boards_y < 1) boards_y = 1;

        int total_boards = boards_x * boards_y;
        result.num_configurations_evaluated++;

        /* Compute utilization */
        double board_total_area = (double)total_boards * bw * bh;
        double panel_total_area = panel_width_mm * panel_height_mm;
        double utilization = (board_total_area / panel_total_area) * 100.0;

        if (total_boards > best_total ||
            (total_boards == best_total && utilization > best_util)) {
            best_total = total_boards;
            best_util  = utilization;

            best_config.panel_width_mm  = panel_width_mm;
            best_config.panel_height_mm = panel_height_mm;
            best_config.board_width_mm  = bw;
            best_config.board_height_mm = bh;
            best_config.board_spacing_mm = min_spacing_mm;
            best_config.boards_x  = boards_x;
            best_config.boards_y  = boards_y;
            best_config.total_boards = total_boards;
            best_config.rail_width_mm  = rail_width_mm;
            best_config.rail_height_mm = rail_width_mm;
            best_config.utilization_pct = utilization;
            best_config.breakaway = method;
            best_config.has_top_rail  = true;
            best_config.has_bot_rail  = true;
            best_config.has_left_rail = true;
            best_config.has_right_rail = true;
        }
    }

    /* Populate result */
    result.config = best_config;
    result.panel_area_mm2 = panel_width_mm * panel_height_mm;
    result.board_total_area_mm2 = (double)best_total
        * board_width_mm * board_height_mm;
    result.wasted_area_mm2 = result.panel_area_mm2
        - result.board_total_area_mm2;
    if (result.wasted_area_mm2 < 0.0) result.wasted_area_mm2 = 0.0;
    result.utilization_pct = best_util;
    result.num_configurations_evaluated = 2; /* both rotations tried */

    return result;
}

double calculate_panel_utilization(const panel_config_t *config)
{
    if (!config || config->panel_width_mm <= 0.0
        || config->panel_height_mm <= 0.0) {
        return 0.0;
    }

    double board_area = (double)config->total_boards
        * config->board_width_mm * config->board_height_mm;
    double panel_area = config->panel_width_mm * config->panel_height_mm;

    return (board_area / panel_area) * 100.0;
}

double estimate_panel_cost(double panel_width_mm, double panel_height_mm,
                            int num_layers, ipc_class_t ipc_class)
{
    if (panel_width_mm <= 0.0 || panel_height_mm <= 0.0 || num_layers < 1) {
        return 0.0;
    }

    double panel_area_mm2 = panel_width_mm * panel_height_mm;
    double area_cm2 = panel_area_mm2 * 0.01;

    /* Panel cost ~$0.03-0.08 per cm^2 depending on layers */
    double base_cm2 = 0.03;
    double layer_factor = 1.0 + 0.35 * (double)(num_layers - 2) / 2.0;
    double class_factor[] = {1.0, 1.3, 1.8};

    return area_cm2 * base_cm2 * layer_factor * class_factor[ipc_class];
}

/* ================================================================
   L5 - Tooling Hole Generation
   ================================================================

   Tooling holes are non-plated (NPTH) holes used for:
   1. Panel registration during fabrication
   2. Alignment during SMT assembly
   3. Test fixture alignment

   Standard placement (4 corners of the panel rails):
     +----+--------------------+----+
     | TL |                    | TR |
     +----+                    +----+
     |                                |
     |      Board Array Area          |
     |                                |
     +----+                    +----+
     | BL |                    | BR |
     +----+--------------------+----+

   Typical tooling hole diameter: 3.175 mm (0.125")
   Tolerance: +/- 0.05 mm
   Material of rails: typically FR4 or aluminum for stencils

   4-hole configuration is standard; some processes use 3-hole
   (prevents over-constraint).
   ================================================================ */

void generate_tooling_holes(double panel_width_mm, double panel_height_mm,
                             double rail_width_mm,
                             tooling_hole_t holes[4], int *num_holes)
{
    if (!holes || !num_holes) return;
    *num_holes = 0;

    if (panel_width_mm <= 0.0 || panel_height_mm <= 0.0
        || rail_width_mm <= 0.0) {
        return;
    }

    double offset = rail_width_mm / 2.0;
    double dia = 3.175; /* standard 0.125" tooling hole */
    double tol = 0.05;

    /* Top-Left */
    holes[0].diameter_mm  = dia;
    holes[0].center_x_mm  = offset;
    holes[0].center_y_mm  = panel_height_mm - offset;
    holes[0].is_plated    = false;
    holes[0].tolerance_mm = tol;

    /* Top-Right */
    holes[1].diameter_mm  = dia;
    holes[1].center_x_mm  = panel_width_mm - offset;
    holes[1].center_y_mm  = panel_height_mm - offset;
    holes[1].is_plated    = false;
    holes[1].tolerance_mm = tol;

    /* Bottom-Left */
    holes[2].diameter_mm  = dia;
    holes[2].center_x_mm  = offset;
    holes[2].center_y_mm  = offset;
    holes[2].is_plated    = false;
    holes[2].tolerance_mm = tol;

    /* Bottom-Right */
    holes[3].diameter_mm  = dia;
    holes[3].center_x_mm  = panel_width_mm - offset;
    holes[3].center_y_mm  = offset;
    holes[3].is_plated    = false;
    holes[3].tolerance_mm = tol;

    *num_holes = 4;
}

/* ================================================================
   L1 - Global Fiducial Mark Generation
   ================================================================

   Fiducial marks are optical alignment targets used by automated
   assembly equipment (pick-and-place machines) to locate the PCB.

   Types:
   - Global fiducials: 3 marks on the panel (or 2 on single board)
     Used to establish the coordinate system for the entire panel.
   - Local fiducials: Near fine-pitch components (>208 pins or
     <0.5mm pitch). Used to compensate for local distortion.
   - Panel fiducials: On the panel rails, used before depaneling.

   Standard fiducial design (IPC-7351):
   - Solid copper circle, 1.0-2.0 mm diameter
   - Solder mask clearance zone around fiducial (no mask)
   - Bare board clearance zone (no copper, no silkscreen nearby)
   - 3-point configuration (L-shape) to resolve rotation
   ================================================================ */

void generate_global_fiducials(double board_width_mm, double board_height_mm,
                                double edge_offset_mm,
                                fiducial_mark_t marks[3], int *num_marks)
{
    if (!marks || !num_marks) return;
    *num_marks = 0;

    if (board_width_mm <= 0.0 || board_height_mm <= 0.0
        || edge_offset_mm <= 0.0) {
        return;
    }

    double fid_dia  = 1.0; /* 1mm copper diameter */
    double mask_open = fid_dia + 0.2; /* 0.1mm mask clearance per side */
    double clearance = 2.0; /* 2mm clearance radius */

    /* Fiducial 1: Bottom-Left corner */
    marks[0].type = FIDUCIAL_GLOBAL;
    marks[0].center_x_mm = edge_offset_mm;
    marks[0].center_y_mm = edge_offset_mm;
    marks[0].copper_diameter_mm  = fid_dia;
    marks[0].mask_opening_mm     = mask_open;
    marks[0].clearance_radius_mm = clearance;

    /* Fiducial 2: Bottom-Right corner */
    marks[1].type = FIDUCIAL_GLOBAL;
    marks[1].center_x_mm = board_width_mm - edge_offset_mm;
    marks[1].center_y_mm = edge_offset_mm;
    marks[1].copper_diameter_mm  = fid_dia;
    marks[1].mask_opening_mm     = mask_open;
    marks[1].clearance_radius_mm = clearance;

    /* Fiducial 3: Top-Left corner */
    marks[2].type = FIDUCIAL_GLOBAL;
    marks[2].center_x_mm = edge_offset_mm;
    marks[2].center_y_mm = board_height_mm - edge_offset_mm;
    marks[2].copper_diameter_mm  = fid_dia;
    marks[2].mask_opening_mm     = mask_open;
    marks[2].clearance_radius_mm = clearance;

    *num_marks = 3;
}

/* ================================================================
   L5 - Tab Placement Optimization
   ================================================================

   Tab-routing uses small tabs of laminate to hold boards in the
   panel during assembly. After assembly, tabs are broken to
   separate individual boards.

   Tab placement rules:
   - Minimum tabs per board edge: varies (typically 3-5)
   - Tab width: 3-5 mm typical
   - Corner offset: 5-10 mm from corners (stress concentration)
   - Spacing: 50-75 mm between tabs

   The algorithm computes optimal tab positions along an edge:
     1. Reserve corner offsets from both ends
     2. Distribute remaining tabs evenly along the edge
     3. Return positions as distances from the edge start

   Perforation (mouse-bite) alternative:
   - 0.5 mm holes, 0.5 mm spacing along the break line
   - Provides cleaner break than V-scoring
   - Better for boards with components near the edge
   ================================================================ */

int compute_tab_placements(double edge_length_mm, double tab_width_mm,
                            int min_tab_count, double min_corner_offset_mm,
                            double positions_mm[], int max_positions)
{
    if (!positions_mm || max_positions <= 0 ||
        edge_length_mm <= 0.0 || tab_width_mm <= 0.0) {
        return 0;
    }

    if (min_tab_count < 1) min_tab_count = 1;

    /* Available length for tab placement */
    double available = edge_length_mm - 2.0 * min_corner_offset_mm;
    if (available <= 0.0) return 0;

    /* Maximum tabs that can fit */
    double max_tabs_capacity = available / tab_width_mm;
    int num_tabs = (int)max_tabs_capacity;
    if (num_tabs < min_tab_count) num_tabs = min_tab_count;
    if (num_tabs > max_positions) num_tabs = max_positions;
    if (num_tabs < 1) return 0;

    /* Evenly distribute tabs */
    double spacing = available / (double)num_tabs;
    double start   = min_corner_offset_mm + spacing / 2.0;

    for (int i = 0; i < num_tabs; i++) {
        positions_mm[i] = start + (double)i * spacing;
        /* Clamp to valid range */
        if (positions_mm[i] < min_corner_offset_mm)
            positions_mm[i] = min_corner_offset_mm;
        if (positions_mm[i] > edge_length_mm - min_corner_offset_mm)
            positions_mm[i] = edge_length_mm - min_corner_offset_mm;
    }

    return num_tabs;
}

/* ================================================================
   L6 - Copper Thieving Design
   ================================================================

   Copper thieving (also called copper balancing or copper fill)
   adds non-functional copper patterns to regions of the PCB that
   would otherwise have very low copper density. This serves:

   1. Plating uniformity: During electroplating, current density is
      proportional to copper area. Isolated traces in sparse areas
      get over-plated, while dense areas get under-plated. Adding
      copper in sparse areas equalizes the plating current density.

   2. Mechanical balance: Asymmetric copper distribution across the
      panel causes warpage during lamination. Copper thieving
      equalizes the copper area on each layer.

   3. Chemical usage: More uniform copper area means more uniform
      etchant consumption across the panel.

   Design parameters:
   - Pattern: Crosshatch (grid of lines) or solid fill with dots
   - Pattern size: 2-10 mm squares typical
   - Spacing: 0.5-2 mm between pattern elements
   - Clearance to features: 0.5-2 mm (prevent interference)
   - Fill target: 50-80% of the sparse area
   - Electrical: Floating or connected to ground (if grounded,
     provides EMI shielding benefit)

   This function computes target copper fill percentage and generates
   recommended thieving parameters.
   ================================================================ */

double compute_copper_fill(double copper_area_mm2, double total_area_mm2)
{
    if (total_area_mm2 <= 0.0) return 0.0;
    if (copper_area_mm2 < 0.0) copper_area_mm2 = 0.0;
    return (copper_area_mm2 / total_area_mm2) * 100.0;
}

copper_thieving_t design_copper_thieving(double current_fill_pct,
                                           double target_fill_pct,
                                           double board_area_mm2,
                                           double clearance_mm)
{
    copper_thieving_t th;
    memset(&th, 0, sizeof(th));

    if (current_fill_pct >= target_fill_pct || board_area_mm2 <= 0.0) {
        th.fill_percentage = current_fill_pct;
        return th;
    }

    /* How much area needs to be added */
    double fill_needed_pct = target_fill_pct - current_fill_pct;
    double area_to_fill_mm2 = board_area_mm2 * fill_needed_pct / 100.0;

    /* Typical pattern size: 2-5mm depending on available area */
    if (area_to_fill_mm2 > board_area_mm2 * 0.3) {
        th.pattern_size_mm = 5.0;  /* large fill needed */
    } else {
        th.pattern_size_mm = 2.0;  /* small fill */
    }

    th.pattern_spacing_mm = 1.0;
    th.clearance_to_feature_mm = clearance_mm;
    th.fill_percentage = target_fill_pct;
    th.is_crosshatch = true;  /* crosshatch is industry standard */
    th.electrically_floating = true; /* safer default */

    return th;
}

/* ================================================================
   L1 - V-Score Parameter Validation
   ================================================================

   V-scoring cuts grooves into both sides of the panel, leaving a
   thin web of material that can be broken to separate boards.

   Key parameters:
   - Score depth: 1/3 of board thickness from each side
     Remaining web = thickness - 2 * depth (typically 0.3-0.5mm)
   - Score angle: 30-45 deg (wider = stronger connection)
   - Score offset: distance from score center to board outline

   Web thickness trade-off:
   - Too thin (< 0.2mm): Boards break during assembly handling
   - Too thick (> 0.6mm): Difficult to break, rough edge

   Reference web thickness guidelines:
   | Board Thickness | Total Score | Remaining | Safety  |
   | (mm)            | (mm)        | Web (mm)  | Margin  |
   +-----------------+-------------+-----------+---------+
   | 0.8             | 0.4         | 0.4       | OK      |
   | 1.0             | 0.5         | 0.5       | OK      |
   | 1.6             | 1.0         | 0.6       | Good    |
   | 2.0             | 1.2         | 0.8       | Good    |
   | 2.4             | 1.4         | 1.0       | Good    |
   | 3.2             | 2.0         | 1.2       | Good    |
   ================================================================ */

bool validate_vscore_params(const vscore_params_t *params)
{
    if (!params) return false;
    if (params->board_thickness_mm <= 0.0) return false;

    double total_score = params->score_depth_top_mm
                       + params->score_depth_bot_mm;
    if (total_score <= 0.0) return false;

    double remaining_web = params->board_thickness_mm - total_score;
    if (remaining_web < 0.1 || remaining_web > 1.5) return false;

    /* Angle must be 20-60 degrees */
    if (params->angle_deg < 20.0 || params->angle_deg > 60.0) return false;

    return true;
}

/* ================================================================
   L5 - Panel Break-even Quantity
   ================================================================

   Determine whether panelization is economically justified.
   Single-board processing has lower setup cost but higher per-unit
   processing cost. Panelization amortizes setup over multiple boards
   but adds depaneling cost.

   This function computes the minimum quantity at which panelization
   becomes cheaper than processing individual boards.

   Reference: Boothroyd, "Product Design for Manufacture and Assembly"
   ================================================================ */

int panel_breakeven_quantity(double individual_setup_sec,
                              double individual_process_sec,
                              double panel_setup_sec,
                              double panel_process_sec_per_board,
                              double depaneling_sec_per_board,
                              double labor_rate_per_hour)
{
    if (labor_rate_per_hour <= 0.0) return 1;

    /* Cost per board for individual processing */
    double indiv_labor_sec = individual_setup_sec + individual_process_sec;

    /* Cost per board for panel processing */
    double panel_labor_sec = panel_setup_sec + panel_process_sec_per_board
                           + depaneling_sec_per_board;

    /* Labor hours to cost */
    double indiv_cost = indiv_labor_sec * labor_rate_per_hour / 3600.0;
    (void)panel_labor_sec; /* used in the breakeven calculation below */

    /* Panel setup is amortized over quantity */
    /* At breakeven: indiv_cost * Q = panel_setup_cost + panel_per_unit * Q */
    /* Q = panel_setup_cost / (indiv_cost - panel_per_unit) */
    double panel_setup_cost = panel_setup_sec * labor_rate_per_hour / 3600.0;
    double panel_per_unit_cost = (panel_process_sec_per_board
                                 + depaneling_sec_per_board)
                                 * labor_rate_per_hour / 3600.0;

    double denominator = indiv_cost - panel_per_unit_cost;
    if (denominator <= 0.0) return 1; /* panel always more expensive */

    double q = panel_setup_cost / denominator;
    int breakeven = (int)ceil(q);
    if (breakeven < 1) breakeven = 1;

    return breakeven;
}
