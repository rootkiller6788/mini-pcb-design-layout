/**
 * @file placement_core.c
 * @brief Implementation of core PCB component placement data structures
 */

#include "placement_core.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

/* ============================================================================
 * Component Management
 * ============================================================================ */

void placement_component_init(Component* comp, const char* designator,
                              ComponentCategory category, PackageType package)
{
    if (!comp) return;

    memset(comp, 0, sizeof(Component));

    /* Set identification fields */
    if (designator) {
        strncpy(comp->designator, designator, sizeof(comp->designator) - 1);
        comp->designator[sizeof(comp->designator) - 1] = '\0';
    }
    comp->category = category;
    comp->package  = package;

    /* Default to unplaced, free */
    comp->is_placed = false;
    comp->is_fixed  = false;
    comp->rotation  = 0.0;
    comp->position.x = 0.0;
    comp->position.y = 0.0;

    /* Default minimum spacing (conservative, 1mm edge-to-edge) */
    comp->min_spacing_mm = 1.0;

    /* Default thermal properties (safe defaults) */
    comp->power_dissipation_W = 0.0;
    comp->max_junction_temp_C = 125.0;
    comp->theta_JA_C_per_W    = 100.0;
    comp->theta_JC_C_per_W    = 20.0;
}

bool placement_component_add_pad(Component* comp, uint32_t pin_number,
                                 const char* pin_name,
                                 double offset_x, double offset_y,
                                 double pad_w, double pad_h)
{
    if (!comp || comp->pad_count >= PLACEMENT_MAX_PADS) {
        return false;
    }

    uint32_t idx = comp->pad_count;
    Pad* pad = &comp->pads[idx];

    pad->pin_number = pin_number;
    if (pin_name) {
        strncpy(pad->pin_name, pin_name, sizeof(pad->pin_name) - 1);
        pad->pin_name[sizeof(pad->pin_name) - 1] = '\0';
    } else {
        pad->pin_name[0] = '\0';
    }
    pad->offset.x     = offset_x;
    pad->offset.y     = offset_y;
    pad->pad_width    = pad_w;
    pad->pad_height   = pad_h;
    pad->is_thermal_pad = false;

    comp->pad_count++;
    return true;
}

void placement_component_set_position(Component* comp, double x, double y,
                                      double rotation)
{
    if (!comp) return;

    comp->position.x = x;
    comp->position.y = y;
    /* Snap rotation to nearest 90° increment */
    comp->rotation = round(rotation / 90.0) * 90.0;
    /* Normalize to [0, 360) */
    while (comp->rotation < 0.0)   comp->rotation += 360.0;
    while (comp->rotation >= 360.0) comp->rotation -= 360.0;
    comp->is_placed = true;
}

BoundingBox placement_component_get_bounds(const Component* comp)
{
    BoundingBox bb = {0, 0, 0, 0};
    if (!comp) return bb;

    double half_w = comp->body.width  / 2.0;
    double half_h = comp->body.height / 2.0;

    /* Corners in local coordinates (centered at origin) */
    double corners[4][2] = {
        {-half_w, -half_h},
        { half_w, -half_h},
        { half_w,  half_h},
        {-half_w,  half_h}
    };

    double rad = comp->rotation * M_PI / 180.0;
    double cos_r = cos(rad);
    double sin_r = sin(rad);

    bb.x_min = DBL_MAX; bb.x_max = -DBL_MAX;
    bb.y_min = DBL_MAX; bb.y_max = -DBL_MAX;

    for (int i = 0; i < 4; i++) {
        /* Rotate corner */
        double rx = corners[i][0] * cos_r - corners[i][1] * sin_r;
        double ry = corners[i][0] * sin_r + corners[i][1] * cos_r;
        /* Translate to board position */
        double wx = comp->position.x + rx;
        double wy = comp->position.y + ry;

        if (wx < bb.x_min) bb.x_min = wx;
        if (wx > bb.x_max) bb.x_max = wx;
        if (wy < bb.y_min) bb.y_min = wy;
        if (wy > bb.y_max) bb.y_max = wy;
    }

    return bb;
}

/* ============================================================================
 * Board Management
 * ============================================================================ */

void placement_board_init(Board* board, const char* name,
                          double width, double height, uint32_t layers)
{
    if (!board) return;
    memset(board, 0, sizeof(Board));

    if (name) {
        strncpy(board->board_name, name, sizeof(board->board_name) - 1);
    }
    board->outline.origin.x = 0.0;
    board->outline.origin.y = 0.0;
    board->outline.width    = width;
    board->outline.height   = height;
    board->layer_count       = (layers > 16) ? 16 : layers;
    board->thickness_mm      = 1.6;   /* Standard 1.6mm FR4 */

    /* Default manufacturing limits (standard PCB fab) */
    board->min_trace_width_mm    = 0.15;  /* 6 mil */
    board->min_trace_spacing_mm  = 0.15;  /* 6 mil */
    board->min_via_drill_mm      = 0.30;  /* 12 mil */
    board->min_via_annular_ring_mm = 0.125; /* 5 mil */

    /* Default placement grid (1mm) */
    board->grid_x_mm = 1.0;
    board->grid_y_mm = 1.0;
}

bool placement_board_add_layer(Board* board, uint32_t layer_id,
                               const char* name, bool is_signal, bool is_plane,
                               double cu_weight, double thickness)
{
    if (!board || layer_id >= board->layer_count) {
        return false;
    }

    BoardLayer* layer = &board->layers[layer_id];
    layer->layer_id = layer_id;
    if (name) {
        strncpy(layer->layer_name, name, sizeof(layer->layer_name) - 1);
        layer->layer_name[sizeof(layer->layer_name) - 1] = '\0';
    }
    layer->is_signal       = is_signal;
    layer->is_plane        = is_plane;
    layer->copper_weight_oz = cu_weight;
    layer->thickness_mm    = thickness;
    return true;
}

/* ============================================================================
 * Net Management
 * ============================================================================ */

void placement_net_init(Net* net, uint32_t id, const char* name)
{
    if (!net) return;
    memset(net, 0, sizeof(Net));

    net->net_id = id;
    if (name) {
        strncpy(net->net_name, name, sizeof(net->net_name) - 1);
        net->net_name[sizeof(net->net_name) - 1] = '\0';
    }
    net->pin_count         = 0;
    net->is_critical       = false;
    net->is_power_net      = false;
    net->target_impedance  = 0.0;
    net->max_current_A     = 0.0;
}

/* ============================================================================
 * Placement Result Management
 * ============================================================================ */

bool placement_result_init(PlacementResult* result, const Board* board,
                           uint32_t max_components, uint32_t max_nets)
{
    if (!result || !board) return false;

    memset(result, 0, sizeof(PlacementResult));
    result->board = *board;

    result->components = (Component*)calloc(max_components, sizeof(Component));
    if (!result->components) return false;

    result->nets = (Net*)calloc(max_nets, sizeof(Net));
    if (!result->nets) {
        free(result->components);
        result->components = NULL;
        return false;
    }

    result->component_count = 0;
    result->net_count       = 0;
    /* Max counts stored implicitly via allocation size */
    return true;
}

void placement_result_free(PlacementResult* result)
{
    if (!result) return;
    free(result->components);
    free(result->nets);
    result->components = NULL;
    result->nets = NULL;
    result->component_count = 0;
    result->net_count = 0;
}

Point2D placement_compute_centroid(const PlacementResult* result)
{
    Point2D centroid = {0.0, 0.0};
    if (!result || result->component_count == 0) return centroid;

    double sum_x = 0.0, sum_y = 0.0;
    uint32_t count = 0;

    for (uint32_t i = 0; i < result->component_count; i++) {
        if (result->components[i].is_placed) {
            sum_x += result->components[i].position.x;
            sum_y += result->components[i].position.y;
            count++;
        }
    }

    if (count > 0) {
        centroid.x = sum_x / count;
        centroid.y = sum_y / count;
    }
    return centroid;
}

double placement_estimate_wire_length(const PlacementResult* result)
{
    if (!result || result->net_count == 0) return 0.0;

    double total_hpwl = 0.0;

    for (uint32_t n = 0; n < result->net_count; n++) {
        if (result->nets[n].pin_count < 2) continue;

        double min_x = DBL_MAX, max_x = -DBL_MAX;
        double min_y = DBL_MAX, max_y = -DBL_MAX;
        bool has_pin = false;

        /* Find bounding box of all pins on this net */
        for (uint32_t c = 0; c < result->component_count; c++) {
            Component* comp = &result->components[c];
            if (!comp->is_placed) continue;

            for (uint32_t p = 0; p < comp->pad_count; p++) {
                if (comp->net_ids[p] != result->nets[n].net_id) continue;

                /* Compute pin position in board coordinates */
                double rad = comp->rotation * M_PI / 180.0;
                double rx = comp->pads[p].offset.x * cos(rad)
                          - comp->pads[p].offset.y * sin(rad);
                double ry = comp->pads[p].offset.x * sin(rad)
                          + comp->pads[p].offset.y * cos(rad);
                double px = comp->position.x + rx;
                double py = comp->position.y + ry;

                if (px < min_x) min_x = px;
                if (px > max_x) max_x = px;
                if (py < min_y) min_y = py;
                if (py > max_y) max_y = py;
                has_pin = true;
            }
        }

        if (has_pin) {
            total_hpwl += (max_x - min_x) + (max_y - min_y);
        }
    }

    return total_hpwl;
}

bool placement_is_position_legal(const PlacementResult* result,
                                 const Component* comp, double x, double y)
{
    if (!result || !comp) return false;

    /* Check board boundary */
    double half_w = comp->body.width  / 2.0;
    double half_h = comp->body.height / 2.0;
    /* Conservative check using circumscribed circle radius of body rectangle */
    double r = sqrt(half_w * half_w + half_h * half_h);

    if (x - r < 0.0 || x + r > result->board.outline.width ||
        y - r < 0.0 || y + r > result->board.outline.height) {
        return false;
    }

    /* Check overlap with already-placed components */
    BoundingBox bb_new;
    /* Temporarily create a placed version for bounds computation */
    Component temp = *comp;
    temp.position.x = x;
    temp.position.y = y;
    /* Keep original rotation */
    bb_new = placement_component_get_bounds(&temp);

    for (uint32_t i = 0; i < result->component_count; i++) {
        if (!result->components[i].is_placed) continue;
        if (&result->components[i] == comp) continue; /* Skip self */

        BoundingBox bb_existing = placement_component_get_bounds(
            &result->components[i]);

        /* Axis-aligned bounding box overlap test (fast rejection) */
        if (bb_new.x_max <= bb_existing.x_min || bb_new.x_min >= bb_existing.x_max ||
            bb_new.y_max <= bb_existing.y_min || bb_new.y_min >= bb_existing.y_max) {
            continue; /* No overlap possible */
        }

        /* AABB overlap detected — check minimum spacing */
        double min_dist = comp->min_spacing_mm;
        if (result->components[i].min_spacing_mm > min_dist) {
            min_dist = result->components[i].min_spacing_mm;
        }

        /* Center-to-center distance */
        double dx = x - result->components[i].position.x;
        double dy = y - result->components[i].position.y;
        double dist = sqrt(dx * dx + dy * dy);

        /* Minimum center distance = sum of half-diagonals + spacing */
        double half_diag_new = sqrt(half_w * half_w + half_h * half_h);
        double hbw = result->components[i].body.width  / 2.0;
        double hbh = result->components[i].body.height / 2.0;
        double half_diag_existing = sqrt(hbw * hbw + hbh * hbh);

        if (dist < half_diag_new + half_diag_existing + min_dist) {
            return false;
        }
    }

    return true;
}
