/**
 * @file placement_optimizer.c
 * @brief Implementation of multi-objective placement optimization framework
 */

#include "placement_optimizer.h"
#include "placement_constraint.h"
#include "placement_util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * L3: HPWL Cost Function
 * ============================================================================ */

double placement_cost_hpwl(const PlacementResult* result)
{
    if (!result || result->net_count == 0) return 0.0;

    double total = 0.0;
    for (uint32_t n = 0; n < result->net_count; n++) {
        if (result->nets[n].pin_count < 2) continue;

        double min_x = 1e100, max_x = -1e100;
        double min_y = 1e100, max_y = -1e100;
        bool found = false;

        for (uint32_t c = 0; c < result->component_count; c++) {
            const Component* comp = &result->components[c];
            if (!comp->is_placed) continue;

            for (uint32_t p = 0; p < comp->pad_count; p++) {
                if (comp->net_ids[p] != result->nets[n].net_id) continue;

                /* Compute global pin position */
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
                found = true;
            }
        }

        if (found) {
            total += (max_x - min_x) + (max_y - min_y);
        }
    }
    return total;
}

/* ============================================================================
 * L3: Rectilinear Steiner Tree Estimate
 * ============================================================================ */

/**
 * Compute Steiner tree estimate using FLUTE-inspired fast heuristic.
 *
 * Since exact RST is NP-hard, we use the "minimal spanning tree × 0.9"
 * approximation which is within 10% of optimal for typical nets.
 *
 * Reference: Hwang, Richards, Winter,
 *   "The Steiner Tree Problem", Annals of Discrete Math., 1992.
 */

static double mst_length(const Point2D* points, uint32_t n)
{
    if (n < 2) return 0.0;

    bool* used = (bool*)calloc(n, sizeof(bool));
    double* min_dist = (double*)malloc(n * sizeof(double));
    if (!used || !min_dist) {
        free(used); free(min_dist);
        return 0.0;
    }

    /* Prim's algorithm for MST */
    for (uint32_t i = 0; i < n; i++) min_dist[i] = 1e100;
    min_dist[0] = 0.0;

    double total = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        /* Find minimum unused vertex */
        double best_d = 1e100;
        uint32_t best_v = 0;
        for (uint32_t v = 0; v < n; v++) {
            if (!used[v] && min_dist[v] < best_d) {
                best_d = min_dist[v];
                best_v = v;
            }
        }
        if (best_d > 1e99) break;

        used[best_v] = true;
        total += best_d;

        /* Update distances */
        for (uint32_t v = 0; v < n; v++) {
            if (used[v]) continue;
            double dx = points[best_v].x - points[v].x;
            double dy = points[best_v].y - points[v].y;
            double d = fabs(dx) + fabs(dy); /* Manhattan */
            if (d < min_dist[v]) min_dist[v] = d;
        }
    }

    free(used);
    free(min_dist);
    return total;
}

double placement_cost_steiner(const PlacementResult* result)
{
    if (!result || result->net_count == 0) return 0.0;

    Point2D pin_buf[256];
    double total_steiner = 0.0;

    for (uint32_t n = 0; n < result->net_count; n++) {
        if (result->nets[n].pin_count < 2) continue;

        uint32_t pin_count = 0;
        for (uint32_t c = 0; c < result->component_count && pin_count < 256; c++) {
            const Component* comp = &result->components[c];
            if (!comp->is_placed) continue;
            for (uint32_t p = 0; p < comp->pad_count && pin_count < 256; p++) {
                if (comp->net_ids[p] != result->nets[n].net_id) continue;
                double rad = comp->rotation * M_PI / 180.0;
                double rx = comp->pads[p].offset.x * cos(rad)
                          - comp->pads[p].offset.y * sin(rad);
                double ry = comp->pads[p].offset.x * sin(rad)
                          + comp->pads[p].offset.y * cos(rad);
                pin_buf[pin_count].x = comp->position.x + rx;
                pin_buf[pin_count].y = comp->position.y + ry;
                pin_count++;
            }
        }

        if (pin_count >= 2) {
            /* RST ≈ MST × RSMT/MST ratio (~0.85 for typical nets) */
            double mst_len = mst_length(pin_buf, pin_count);
            total_steiner += mst_len * 0.85;
        }
    }

    return total_steiner;
}

/* ============================================================================
 * L3: Thermal Cost Functions
 * ============================================================================ */

double placement_cost_thermal(const PlacementResult* result, double ambient_C)
{
    if (!result) return 0.0;

    double cost = 0.0;

    for (uint32_t i = 0; i < result->component_count; i++) {
        const Component* ci = &result->components[i];
        if (!ci->is_placed || ci->power_dissipation_W <= 0.0) continue;

        /* Estimate junction temperature including neighbor heating */
        double t_j = ambient_C + ci->power_dissipation_W * ci->theta_JA_C_per_W;

        for (uint32_t j = 0; j < result->component_count; j++) {
            if (i == j) continue;
            const Component* cj = &result->components[j];
            if (!cj->is_placed || cj->power_dissipation_W <= 0.0) continue;

            double coupling = placement_constraint_thermal_coupling(
                ci, cj, result->board.thickness_mm);
            t_j += cj->power_dissipation_W * coupling;
        }

        /* Quadratic penalty for exceeding T_j_max */
        double excess = t_j - ci->max_junction_temp_C;
        if (excess > 0.0) {
            cost += excess * excess;
        }
    }

    return cost;
}

double placement_cost_thermal_gradient(const PlacementResult* result,
                                        double ambient_C)
{
    if (!result || result->component_count < 2) return 0.0;

    /* Estimate junction temperature for each heat-dissipating component */
    uint32_t n_heaters = 0;
    double* temps = (double*)malloc(result->component_count * sizeof(double));
    if (!temps) return 0.0;

    for (uint32_t i = 0; i < result->component_count; i++) {
        const Component* ci = &result->components[i];
        if (!ci->is_placed || ci->power_dissipation_W <= 0.0) continue;

        double t_j = ambient_C + ci->power_dissipation_W * ci->theta_JA_C_per_W;
        for (uint32_t j = 0; j < result->component_count; j++) {
            if (i == j) continue;
            const Component* cj = &result->components[j];
            if (!cj->is_placed || cj->power_dissipation_W <= 0.0) continue;
            double coupling = placement_constraint_thermal_coupling(
                ci, cj, result->board.thickness_mm);
            t_j += cj->power_dissipation_W * coupling;
        }
        temps[n_heaters++] = t_j;
    }

    /* Cost = variance of temperatures */
    double cost = 0.0;
    if (n_heaters >= 2) {
        double mean = placement_util_mean(temps, n_heaters);
        double var = 0.0;
        for (uint32_t i = 0; i < n_heaters; i++) {
            double diff = temps[i] - mean;
            var += diff * diff;
        }
        var /= n_heaters;
        cost = var;
    }

    free(temps);
    return cost;
}

/* ============================================================================
 * L3: Signal Integrity Cost
 * ============================================================================ */

double placement_cost_signal_integrity(const PlacementResult* result)
{
    if (!result) return 0.0;

    double cost = 0.0;
    double max_allowed_mm = 150.0; /* Default critical net max length */

    for (uint32_t n = 0; n < result->net_count; n++) {
        if (!result->nets[n].is_critical) continue;

        /* Compute net HPWL span */
        double min_x = 1e100, max_x = -1e100;
        double min_y = 1e100, max_y = -1e100;
        bool found = false;

        for (uint32_t c = 0; c < result->component_count; c++) {
            const Component* comp = &result->components[c];
            if (!comp->is_placed) continue;
            for (uint32_t p = 0; p < comp->pad_count; p++) {
                if (comp->net_ids[p] != result->nets[n].net_id) continue;
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
                found = true;
            }
        }

        if (found) {
            double span = (max_x - min_x) + (max_y - min_y);
            double excess = span - max_allowed_mm;
            if (excess > 0.0) {
                cost += excess * excess;
            }
        }
    }

    return cost;
}

/* ============================================================================
 * L3: Overlap Cost
 * ============================================================================ */

double placement_cost_overlap(const PlacementResult* result)
{
    if (!result || result->component_count < 2) return 0.0;

    double cost = 0.0;

    for (uint32_t i = 0; i < result->component_count; i++) {
        if (!result->components[i].is_placed) continue;
        BoundingBox bbi = placement_component_get_bounds(&result->components[i]);
        Rect2D ri;
        ri.origin.x = bbi.x_min;
        ri.origin.y = bbi.y_min;
        ri.width    = bbi.x_max - bbi.x_min;
        ri.height   = bbi.y_max - bbi.y_min;

        for (uint32_t j = i + 1; j < result->component_count; j++) {
            if (!result->components[j].is_placed) continue;
            BoundingBox bbj = placement_component_get_bounds(&result->components[j]);
            Rect2D rj;
            rj.origin.x = bbj.x_min;
            rj.origin.y = bbj.y_min;
            rj.width    = bbj.x_max - bbj.x_min;
            rj.height   = bbj.y_max - bbj.y_min;

            double overlap = placement_util_rect_overlap_area(&ri, &rj);
            if (overlap > 0.0) {
                cost += overlap;
            }
        }
    }

    return cost;
}

/* ============================================================================
 * L3: Density Cost
 * ============================================================================ */

double placement_cost_density(const PlacementResult* result,
                               double grid_size, double max_density)
{
    if (!result || grid_size <= 0.0) return 0.0;

    double bw = result->board.outline.width;
    double bh = result->board.outline.height;

    uint32_t cols = (uint32_t)ceil(bw / grid_size);
    uint32_t rows = (uint32_t)ceil(bh / grid_size);
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;

    double* grid = (double*)calloc(cols * rows, sizeof(double));
    if (!grid) return 0.0;

    /* Accumulate component area in each grid cell */
    for (uint32_t c = 0; c < result->component_count; c++) {
        if (!result->components[c].is_placed) continue;

        BoundingBox bb = placement_component_get_bounds(&result->components[c]);
        double comp_area = (bb.x_max - bb.x_min) * (bb.y_max - bb.y_min);

        /* Map to grid cells (simple: assign to cell containing center) */
        double cx = (bb.x_min + bb.x_max) / 2.0;
        double cy = (bb.y_min + bb.y_max) / 2.0;
        int col = (int)(cx / grid_size);
        int row = (int)(cy / grid_size);
        if (col >= 0 && col < (int)cols && row >= 0 && row < (int)rows) {
            grid[row * cols + col] += comp_area;
        }
    }

    /* Compute density cost */
    double cell_area = grid_size * grid_size;
    double cost = 0.0;

    for (uint32_t i = 0; i < rows * cols; i++) {
        double density = grid[i] / cell_area;
        if (density > max_density) {
            double excess = density - max_density;
            cost += excess * excess * cell_area;
        }
    }

    free(grid);
    return cost;
}

/* ============================================================================
 * L3: Manufacturability Cost
 * ============================================================================ */

double placement_cost_manufacturability(const PlacementResult* result)
{
    if (!result) return 0.0;

    double cost = 0.0;
    double bw = result->board.outline.width;
    double bh = result->board.outline.height;
    double edge_margin = 3.0; /* mm from board edge for edge connector / THT */

    for (uint32_t i = 0; i < result->component_count; i++) {
        const Component* comp = &result->components[i];
        if (!comp->is_placed) continue;

        BoundingBox bb = placement_component_get_bounds(comp);

        /* Edge proximity penalty */
        double edge_dist = bb.x_min;
        if (bh - bb.y_max < edge_dist) edge_dist = bh - bb.y_max;
        if (bw - bb.x_max < edge_dist) edge_dist = bw - bb.x_max;
        if (bb.y_min < edge_dist) edge_dist = bb.y_min;

        if (edge_dist < edge_margin) {
            double excess = edge_margin - edge_dist;
            cost += excess * excess;
        }

        /* Penalize excessive height variation between adjacent components
         * (indicator of potential reflow shadowing) */
        for (uint32_t j = i + 1; j < result->component_count; j++) {
            const Component* cj = &result->components[j];
            if (!cj->is_placed) continue;

            double dx = comp->position.x - cj->position.x;
            double dy = comp->position.y - cj->position.y;
            double dist = sqrt(dx * dx + dy * dy);
            if (dist > 20.0) continue; /* Only consider nearby components */

            double h_i = comp->envelope.z_max - comp->envelope.z_min;
            double h_j = cj->envelope.z_max - cj->envelope.z_min;
            if (h_i <= 0.0 || h_j <= 0.0) continue;

            double ratio = h_i / h_j;
            if (ratio < 1.0) ratio = 1.0 / ratio;

            if (ratio > 3.0) {
                cost += (ratio - 3.0) * 10.0; /* Penalize >3:1 height ratios */
            }
        }
    }

    return cost;
}

/* ============================================================================
 * Cost Weights & Total Cost
 * ============================================================================ */

void placement_cost_weights_init_default(CostWeights* weights)
{
    if (!weights) return;
    weights->w_hpwl               = 0.50;
    weights->w_thermal            = 0.15;
    weights->w_thermal_gradient   = 0.05;
    weights->w_signal_integrity   = 0.15;
    weights->w_overlap            = 0.10;
    weights->w_density            = 0.05;
    weights->w_manufacturability  = 0.00; /* Optional, off by default */
}

PlacementCost placement_cost_compute_total(const PlacementResult* result,
                                            const CostWeights* weights,
                                            double ambient_C,
                                            double grid_size,
                                            double max_density)
{
    PlacementCost pc;
    memset(&pc, 0, sizeof(PlacementCost));

    if (!result || !weights) return pc;

    pc.wire_length_cost       = placement_cost_hpwl(result);
    pc.thermal_cost           = placement_cost_thermal(result, ambient_C);
    pc.signal_integrity_cost  = placement_cost_signal_integrity(result);
    pc.overlap_cost           = placement_cost_overlap(result);
    pc.density_cost           = placement_cost_density(result, grid_size, max_density);
    pc.manufacturability_cost = placement_cost_manufacturability(result);

    /* Weighted total */
    pc.total_cost = weights->w_hpwl              * pc.wire_length_cost
                  + weights->w_thermal           * pc.thermal_cost
                  + weights->w_thermal_gradient  * 0.0 /* gradient computed separately */
                  + weights->w_signal_integrity  * pc.signal_integrity_cost
                  + weights->w_overlap           * pc.overlap_cost
                  + weights->w_density           * pc.density_cost
                  + weights->w_manufacturability * pc.manufacturability_cost;

    return pc;
}

/* ============================================================================
 * L8: Pareto Front
 * ============================================================================ */

void placement_pareto_front_init(ParetoFront* front, uint32_t capacity)
{
    if (!front) return;
    front->count    = 0;
    front->capacity = capacity;
    front->points   = (ObjectivePoint*)malloc(capacity * sizeof(ObjectivePoint));
}

void placement_pareto_front_free(ParetoFront* front)
{
    if (!front) return;
    free(front->points);
    front->points   = NULL;
    front->count    = 0;
    front->capacity = 0;
}

bool placement_pareto_dominates(const ObjectivePoint* a,
                                 const ObjectivePoint* b)
{
    if (!a || !b) return false;

    bool at_least_one_strict = false;

    if (a->hpwl > b->hpwl) return false;
    if (a->hpwl < b->hpwl) at_least_one_strict = true;

    if (a->thermal > b->thermal) return false;
    if (a->thermal < b->thermal) at_least_one_strict = true;

    if (a->signal_integrity > b->signal_integrity) return false;
    if (a->signal_integrity < b->signal_integrity) at_least_one_strict = true;

    return at_least_one_strict;
}

bool placement_pareto_front_insert(ParetoFront* front,
                                    const ObjectivePoint* point)
{
    if (!front || !point) return false;

    /* Check if new point is dominated by any existing point */
    for (uint32_t i = 0; i < front->count; i++) {
        if (placement_pareto_dominates(&front->points[i], point)) {
            return false; /* Point is dominated — discard */
        }
    }

    /* Remove existing points that are dominated by the new point */
    uint32_t write_idx = 0;
    for (uint32_t i = 0; i < front->count; i++) {
        if (!placement_pareto_dominates(point, &front->points[i])) {
            front->points[write_idx++] = front->points[i];
        }
    }
    front->count = write_idx;

    /* Insert new point */
    if (front->count < front->capacity) {
        front->points[front->count++] = *point;
        return true;
    }

    /* If full, discard point (shouldn't happen with proper capacity) */
    return false;
}

double placement_pareto_hypervolume(const ParetoFront* front,
                                     const ObjectivePoint* ref_point)
{
    if (!front || front->count == 0 || !ref_point) return 0.0;

    /* Sort points by hpwl ascending */
    /* Simple approach: compute hypervolume as sum of rectangular contributions
     * for 2D case (hpwl vs thermal). For 3D, use a Lebesgue-measure estimate. */

    /* Lebesgue measure for sorted Pareto front */
    double hv = 0.0;
    double prev_hpwl = 0.0;

    /* Sort by hpwl */
    ObjectivePoint* sorted = (ObjectivePoint*)malloc(
        front->count * sizeof(ObjectivePoint));
    if (!sorted) return 0.0;
    memcpy(sorted, front->points, front->count * sizeof(ObjectivePoint));

    for (uint32_t i = 0; i < front->count; i++) {
        for (uint32_t j = i + 1; j < front->count; j++) {
            if (sorted[j].hpwl < sorted[i].hpwl) {
                ObjectivePoint tmp = sorted[i];
                sorted[i] = sorted[j];
                sorted[j] = tmp;
            }
        }
    }

    double max_thermal_seen = 0.0;

    for (uint32_t i = 0; i < front->count; i++) {
        double dh = sorted[i].hpwl - prev_hpwl;
        if (sorted[i].thermal > max_thermal_seen) {
            max_thermal_seen = sorted[i].thermal;
        }
        double dt = ref_point->thermal - max_thermal_seen;
        if (dh > 0.0 && dt > 0.0) {
            hv += dh * dt;
        }
        prev_hpwl = sorted[i].hpwl;
    }

    free(sorted);
    return hv;
}
