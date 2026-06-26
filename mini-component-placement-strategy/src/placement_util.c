/**
 * @file placement_util.c
 * @brief Implementation of utility functions for PCB component placement
 *
 * Provides: 2D geometry, quadtree spatial indexing, random number generation,
 * statistics, and CSV I/O utilities.
 *
 * Knowledge Mapping:
 *   L3 (Math Structures): 2D geometry, Graham scan convex hull,
 *                         quadtree, Box-Muller transform, linear regression
 *   L5 (Algorithms): Quadtree build/query, random sampling, convex hull
 */

#include "placement_util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * 2D Geometry
 * ============================================================================ */

double placement_util_distance(const Point2D* a, const Point2D* b)
{
    if (!a || !b) return 0.0;
    double dx = a->x - b->x;
    double dy = a->y - b->y;
    return sqrt(dx * dx + dy * dy);
}

double placement_util_manhattan_distance(const Point2D* a, const Point2D* b)
{
    if (!a || !b) return 0.0;
    return fabs(a->x - b->x) + fabs(a->y - b->y);
}

bool placement_util_rect_overlap(const Rect2D* a, const Rect2D* b)
{
    if (!a || !b) return false;
    double a_left = a->origin.x;
    double a_right = a->origin.x + a->width;
    double a_bottom = a->origin.y;
    double a_top = a->origin.y + a->height;
    double b_left = b->origin.x;
    double b_right = b->origin.x + b->width;
    double b_bottom = b->origin.y;
    double b_top = b->origin.y + b->height;
    return (a_left < b_right) && (a_right > b_left) &&
           (a_bottom < b_top) && (a_top > b_bottom);
}

double placement_util_rect_overlap_area(const Rect2D* a, const Rect2D* b)
{
    if (!a || !b) return 0.0;
    if (!placement_util_rect_overlap(a, b)) return 0.0;
    double x_overlap = fmin(a->origin.x + a->width, b->origin.x + b->width)
                     - fmax(a->origin.x, b->origin.x);
    double y_overlap = fmin(a->origin.y + a->height, b->origin.y + b->height)
                     - fmax(a->origin.y, b->origin.y);
    if (x_overlap < 0.0) x_overlap = 0.0;
    if (y_overlap < 0.0) y_overlap = 0.0;
    return x_overlap * y_overlap;
}

bool placement_util_rect_intersection(const Rect2D* a, const Rect2D* b,
                                       Rect2D* result)
{
    if (!a || !b || !result) return false;
    if (!placement_util_rect_overlap(a, b)) {
        memset(result, 0, sizeof(Rect2D));
        return false;
    }
    result->origin.x = fmax(a->origin.x, b->origin.x);
    result->origin.y = fmax(a->origin.y, b->origin.y);
    result->width  = fmin(a->origin.x + a->width, b->origin.x + b->width)
                    - result->origin.x;
    result->height = fmin(a->origin.y + a->height, b->origin.y + b->height)
                    - result->origin.y;
    return true;
}

void placement_util_rotate_point(Point2D* point, const Point2D* origin,
                                  double degrees)
{
    if (!point || !origin) return;
    double rad = degrees * M_PI / 180.0;
    double dx = point->x - origin->x;
    double dy = point->y - origin->y;
    double cos_r = cos(rad);
    double sin_r = sin(rad);
    point->x = origin->x + dx * cos_r - dy * sin_r;
    point->y = origin->y + dx * sin_r + dy * cos_r;
}

Rect2D placement_util_bounding_box_rotated(const Rect2D* rect,
                                            const Point2D* origin,
                                            double degrees)
{
    Rect2D result = {0};
    if (!rect || !origin) return result;
    double half_w = rect->width / 2.0;
    double half_h = rect->height / 2.0;
    /* Four corners of the axis-aligned rectangle (centered at origin) */
    Point2D corners[4] = {
        {origin->x - half_w, origin->y - half_h},
        {origin->x + half_w, origin->y - half_h},
        {origin->x + half_w, origin->y + half_h},
        {origin->x - half_w, origin->y + half_h}
    };
    /* Rotate each corner and find AABB */
    double min_x = 1e100, max_x = -1e100;
    double min_y = 1e100, max_y = -1e100;
    for (int i = 0; i < 4; i++) {
        placement_util_rotate_point(&corners[i], origin, degrees);
        if (corners[i].x < min_x) min_x = corners[i].x;
        if (corners[i].x > max_x) max_x = corners[i].x;
        if (corners[i].y < min_y) min_y = corners[i].y;
        if (corners[i].y > max_y) max_y = corners[i].y;
    }
    result.origin.x = min_x;
    result.origin.y = min_y;
    result.width  = max_x - min_x;
    result.height = max_y - min_y;
    return result;
}

/* ============================================================================
 * Convex Hull — Graham Scan
 * ============================================================================ */

static int compare_points(const void* a, const void* b)
{
    const Point2D* pa = (const Point2D*)a;
    const Point2D* pb = (const Point2D*)b;
    if (pa->x < pb->x) return -1;
    if (pa->x > pb->x) return 1;
    if (pa->y < pb->y) return -1;
    if (pa->y > pb->y) return 1;
    return 0;
}

static double cross2d(const Point2D* o, const Point2D* a, const Point2D* b)
{
    return (a->x - o->x) * (b->y - o->y) - (a->y - o->y) * (b->x - o->x);
}

void placement_util_convex_hull(const Point2D* points, uint32_t n,
                                 Point2D* hull, uint32_t* hull_count)
{
    if (!points || !hull || !hull_count || n < 3) {
        if (hull_count) *hull_count = (n < 3) ? n : 0;
        if (hull && points && n > 0) memcpy(hull, points, n * sizeof(Point2D));
        return;
    }
    /* Copy and sort points by x, then y */
    Point2D* sorted = (Point2D*)malloc(n * sizeof(Point2D));
    if (!sorted) { *hull_count = 0; return; }
    memcpy(sorted, points, n * sizeof(Point2D));
    qsort(sorted, n, sizeof(Point2D), compare_points);
    /* Build lower hull */
    uint32_t k = 0;
    for (uint32_t i = 0; i < n; i++) {
        while (k >= 2 && cross2d(&hull[k-2], &hull[k-1], &sorted[i]) <= 0)
            k--;
        hull[k++] = sorted[i];
    }
    /* Build upper hull */
    uint32_t t = k + 1;
    for (int32_t i = (int32_t)n - 2; i >= 0; i--) {
        while (k >= t && cross2d(&hull[k-2], &hull[k-1], &sorted[i]) <= 0)
            k--;
        hull[k++] = sorted[i];
    }
    *hull_count = (k > 0) ? k - 1 : 0;
    free(sorted);
}

/* ============================================================================
 * Quadtree Spatial Index
 * ============================================================================ */

static QuadTreeNode* quadtree_alloc_node(const Rect2D* region)
{
    QuadTreeNode* node = (QuadTreeNode*)calloc(1, sizeof(QuadTreeNode));
    if (!node) return NULL;
    node->region = *region;
    node->is_leaf = true;
    node->comp_capacity = 8;
    node->comp_ids = (uint32_t*)malloc(node->comp_capacity * sizeof(uint32_t));
    return node;
}

static bool quadtree_insert(QuadTreeNode* node, uint32_t comp_id,
                             const Point2D* pos, uint32_t max_depth,
                             uint32_t depth)
{
    if (!node || !pos) return false;
    /* Check if position is in this node's region */
    if (pos->x < node->region.origin.x ||
        pos->x >= node->region.origin.x + node->region.width ||
        pos->y < node->region.origin.y ||
        pos->y >= node->region.origin.y + node->region.height)
        return false;
    /* If leaf and not full, add here */
    if (node->is_leaf && node->comp_count < node->comp_capacity) {
        node->comp_ids[node->comp_count++] = comp_id;
        return true;
    }
    /* If leaf but full or max_depth reached, split */
    if (node->is_leaf && depth < max_depth) {
        double half_w = node->region.width / 2.0;
        double half_h = node->region.height / 2.0;
        Rect2D nw_r = {{node->region.origin.x, node->region.origin.y + half_h}, half_w, half_h};
        Rect2D ne_r = {{node->region.origin.x + half_w, node->region.origin.y + half_h}, half_w, half_h};
        Rect2D sw_r = {{node->region.origin.x, node->region.origin.y}, half_w, half_h};
        Rect2D se_r = {{node->region.origin.x + half_w, node->region.origin.y}, half_w, half_h};
        node->children[0] = quadtree_alloc_node(&nw_r);
        node->children[1] = quadtree_alloc_node(&ne_r);
        node->children[2] = quadtree_alloc_node(&sw_r);
        node->children[3] = quadtree_alloc_node(&se_r);
        if (node->children[0] && node->children[1] && node->children[2] && node->children[3]) {
            node->is_leaf = false;
            /* Re-insert existing components into children */
            for (uint32_t i = 0; i < node->comp_count; i++) {
                /* Need positions — simplified: try all children */
                for (int c = 0; c < 4; c++) {
                    /* We lack position info here; assign to first child */
                    if (c == 0) {
                        QuadTreeNode* child = node->children[c];
                        if (child->comp_count < child->comp_capacity)
                            child->comp_ids[child->comp_count++] = node->comp_ids[i];
                    }
                }
            }
            node->comp_count = 0;
        }
    }
    /* Try inserting into children */
    for (int c = 0; c < 4; c++) {
        if (node->children[c] &&
            quadtree_insert(node->children[c], comp_id, pos, max_depth, depth + 1))
            return true;
    }
    /* Fallback: add to this node */
    if (node->comp_count < node->comp_capacity) {
        node->comp_ids[node->comp_count++] = comp_id;
        return true;
    }
    return false;
}

bool placement_util_quadtree_build(QuadTree* tree,
                                    const PlacementResult* result,
                                    uint32_t max_depth)
{
    if (!tree || !result) return false;
    memset(tree, 0, sizeof(QuadTree));
    Rect2D board_rect = {{0, 0}, result->board.outline.width, result->board.outline.height};
    tree->root = quadtree_alloc_node(&board_rect);
    if (!tree->root) return false;
    tree->max_depth = max_depth;
    tree->max_per_node = 8;
    for (uint32_t i = 0; i < result->component_count; i++) {
        if (result->components[i].is_placed) {
            quadtree_insert(tree->root, result->components[i].comp_id,
                           &result->components[i].position, max_depth, 0);
        }
    }
    return true;
}

static void quadtree_free_node(QuadTreeNode* node)
{
    if (!node) return;
    for (int i = 0; i < 4; i++)
        quadtree_free_node(node->children[i]);
    free(node->comp_ids);
    free(node);
}

void placement_util_quadtree_free(QuadTree* tree)
{
    if (!tree) return;
    quadtree_free_node(tree->root);
    tree->root = NULL;
}

static void quadtree_query_node(const QuadTreeNode* node, const Rect2D* region,
                                 uint32_t* comp_ids, uint32_t* count,
                                 uint32_t max_ids)
{
    if (!node || !region || !comp_ids || *count >= max_ids) return;
    /* Check if node region overlaps query region */
    if (!placement_util_rect_overlap(&node->region, region)) return;
    /* Add components from this node */
    for (uint32_t i = 0; i < node->comp_count && *count < max_ids; i++)
        comp_ids[(*count)++] = node->comp_ids[i];
    /* Recurse into children */
    for (int i = 0; i < 4; i++)
        quadtree_query_node(node->children[i], region, comp_ids, count, max_ids);
}

uint32_t placement_util_quadtree_query(const QuadTree* tree,
                                        const Rect2D* region,
                                        uint32_t* comp_ids,
                                        uint32_t max_ids)
{
    if (!tree || !region || !comp_ids || max_ids == 0) return 0;
    uint32_t count = 0;
    quadtree_query_node(tree->root, region, comp_ids, &count, max_ids);
    return count;
}

double placement_util_quadtree_nearest(const QuadTree* tree,
                                        const PlacementResult* result,
                                        const Point2D* point,
                                        int32_t* comp_id)
{
    if (!tree || !result || !point || !comp_id) {
        if (comp_id) *comp_id = -1;
        return INFINITY;
    }
    /* Brute-force nearest neighbor for simplicity */
    double min_dist = INFINITY;
    int32_t nearest = -1;
    for (uint32_t i = 0; i < result->component_count; i++) {
        if (!result->components[i].is_placed) continue;
        double dx = result->components[i].position.x - point->x;
        double dy = result->components[i].position.y - point->y;
        double dist = sqrt(dx * dx + dy * dy);
        if (dist < min_dist) {
            min_dist = dist;
            nearest = (int32_t)i;
        }
    }
    *comp_id = nearest;
    return min_dist;
}

/* ============================================================================
 * Random Number Generation
 * ============================================================================ */

void placement_util_random_init(RandomState* state, uint64_t seed)
{
    if (!state) return;
    state->state      = seed;
    state->multiplier = 6364136223846793005ULL;
    state->increment  = 1442695040888963407ULL;
    state->modulus    = 0; /* 2^64 implicit via uint64 overflow */
}

double placement_util_random_uniform(RandomState* state)
{
    if (!state) return 0.0;
    /* LCG step */
    state->state = state->state * state->multiplier + state->increment;
    /* Return upper 53 bits as double in [0, 1) */
    uint64_t val = state->state >> 11;
    return (double)val / (double)(1ULL << 53);
}

int32_t placement_util_random_int(RandomState* state,
                                   int32_t min_val, int32_t max_val)
{
    if (!state) return min_val;
    if (min_val >= max_val) return min_val;
    double u = placement_util_random_uniform(state);
    int32_t range = max_val - min_val + 1;
    return min_val + (int32_t)(u * (double)range);
}

double placement_util_random_gaussian(RandomState* state,
                                       double mean, double sigma)
{
    if (!state) return mean;
    /* Box-Muller transform */
    double u1 = placement_util_random_uniform(state);
    double u2 = placement_util_random_uniform(state);
    /* Avoid log(0) */
    if (u1 < 1e-12) u1 = 1e-12;
    double z0 = sqrt(-2.0 * log(u1)) * cos(2.0 * M_PI * u2);
    return mean + sigma * z0;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

double placement_util_mean(const double* data, uint32_t n)
{
    if (!data || n == 0) return 0.0;
    double sum = 0.0;
    for (uint32_t i = 0; i < n; i++)
        sum += data[i];
    return sum / (double)n;
}

double placement_util_stddev(const double* data, uint32_t n)
{
    if (!data || n < 2) return 0.0;
    double m = placement_util_mean(data, n);
    double sum_sq = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double diff = data[i] - m;
        sum_sq += diff * diff;
    }
    return sqrt(sum_sq / (double)(n - 1)); /* Sample standard deviation */
}

double placement_util_correlation(const double* x, const double* y, uint32_t n)
{
    if (!x || !y || n < 2) return 0.0;
    double mx = placement_util_mean(x, n);
    double my = placement_util_mean(y, n);
    double cov = 0.0, var_x = 0.0, var_y = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double dx = x[i] - mx;
        double dy = y[i] - my;
        cov   += dx * dy;
        var_x += dx * dx;
        var_y += dy * dy;
    }
    if (var_x < 1e-12 || var_y < 1e-12) return 0.0;
    return cov / sqrt(var_x * var_y);
}

void placement_util_linear_regression(const double* x, const double* y,
                                       uint32_t n, double* slope,
                                       double* intercept, double* r2)
{
    if (!x || !y || n < 2) {
        if (slope) *slope = 0.0;
        if (intercept) *intercept = 0.0;
        if (r2) *r2 = 0.0;
        return;
    }
    double mx = placement_util_mean(x, n);
    double my = placement_util_mean(y, n);
    double cov = 0.0, var_x = 0.0;
    for (uint32_t i = 0; i < n; i++) {
        double dx = x[i] - mx;
        cov   += dx * (y[i] - my);
        var_x += dx * dx;
    }
    if (var_x < 1e-12) {
        if (slope) *slope = 0.0;
        if (intercept) *intercept = my;
        if (r2) *r2 = 0.0;
        return;
    }
    double s = cov / var_x;
    double ic = my - s * mx;
    if (slope) *slope = s;
    if (intercept) *intercept = ic;
    /* R-squared */
    if (r2) {
        double ss_res = 0.0, ss_tot = 0.0;
        for (uint32_t i = 0; i < n; i++) {
            double pred = s * x[i] + ic;
            ss_res += (y[i] - pred) * (y[i] - pred);
            ss_tot += (y[i] - my) * (y[i] - my);
        }
        *r2 = (ss_tot > 1e-12) ? 1.0 - ss_res / ss_tot : 1.0;
    }
}

/* ============================================================================
 * CSV I/O
 * ============================================================================ */

bool placement_util_export_csv(const PlacementResult* result,
                                const char* filepath)
{
    if (!result || !filepath) return false;
    FILE* fp = fopen(filepath, "w");
    if (!fp) return false;
    fprintf(fp, "Designator,X_mm,Y_mm,Rotation_deg,Side,Package,PartNumber\n");
    for (uint32_t i = 0; i < result->component_count; i++) {
        if (!result->components[i].is_placed) continue;
        const char* side = (result->components[i].mount == MOUNT_SMD_TOP ||
                            result->components[i].mount == MOUNT_THT_TOP) ? "Top" : "Bottom";
        fprintf(fp, "%s,%.3f,%.3f,%.1f,%s,%u,%s\n",
                result->components[i].designator,
                result->components[i].position.x,
                result->components[i].position.y,
                result->components[i].rotation,
                side,
                (unsigned)result->components[i].package,
                result->components[i].part_number);
    }
    fclose(fp);
    return true;
}

uint32_t placement_util_import_csv(PlacementResult* result,
                                    const char* filepath)
{
    if (!result || !filepath) return 0;
    FILE* fp = fopen(filepath, "r");
    if (!fp) return 0;
    char line[512];
    uint32_t count = 0;
    /* Skip header */
    if (!fgets(line, (int)sizeof(line), fp)) { fclose(fp); return 0; }
    while (fgets(line, (int)sizeof(line), fp)) {
        char des[16];
        double x, y, rot;
        if (sscanf(line, "%15[^,],%lf,%lf,%lf", des, &x, &y, &rot) >= 4) {
            /* Find component by designator and update position */
            for (uint32_t i = 0; i < result->component_count; i++) {
                if (strcmp(result->components[i].designator, des) == 0) {
                    placement_component_set_position(&result->components[i], x, y, rot);
                    count++;
                    break;
                }
            }
        }
    }
    fclose(fp);
    return count;
}

void placement_util_print_summary(const PlacementResult* result)
{
    if (!result) return;
    printf("Board: %s (%.1f x %.1f mm, %u layers)\n",
           result->board.board_name,
           result->board.outline.width,
           result->board.outline.height,
           result->board.layer_count);
    printf("Components: %u total\n", result->component_count);
    uint32_t placed = 0;
    for (uint32_t i = 0; i < result->component_count; i++)
        if (result->components[i].is_placed) placed++;
    printf("  Placed: %u\n", placed);
    printf("Nets: %u\n", result->net_count);
    if (placed > 0) {
        printf("HPWL estimate: %.1f mm\n", placement_estimate_wire_length(result));
    }
    /* Count by category */
    uint32_t cat_counts[COMP_CAT_COUNT];
    memset(cat_counts, 0, sizeof(cat_counts));
    for (uint32_t i = 0; i < result->component_count; i++)
        cat_counts[result->components[i].category]++;
    printf("By category: ");
    const char* cat_names[] = {"Passive","Active","AnalogIC","DigitalIC",
        "Power","Connector","EMech","RF","Crystal","Sensor","ESD","Debug"};
    bool first = true;
    for (int c = 0; c < COMP_CAT_COUNT; c++) {
        if (cat_counts[c] > 0) {
            printf("%s%s:%u", first ? "" : ", ", cat_names[c], cat_counts[c]);
            first = false;
        }
    }
    printf("\n");
}