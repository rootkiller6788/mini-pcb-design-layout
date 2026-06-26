/**
 * @file placement_constraint.c
 * @brief Implementation of PCB component placement constraint checking
 */

#include "placement_constraint.h"
#include "placement_util.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * IPC Spacing Rules
 * ============================================================================ */

/**
 * IPC-7351B courtyard spacing table (Level B, moderate density).
 *
 * Values represent recommended minimum edge-to-edge spacing in mm.
 * These are derived from IPC-7351B courtyard excess recommendations:
 *   - Level A (General):   courtyard + 0.5mm
 *   - Level B (Moderate):  courtyard + 0.25mm
 *   - Level C (High):      courtyard + 0.1mm
 *
 * Package-to-package spacing simplified into classes by size.
 */
typedef enum {
    SIZE_CLASS_TINY   = 0,  /* 0201, 0402, SC-70    — < 1.5mm */
    SIZE_CLASS_SMALL  = 1,  /* 0603, 0805, SOT-23   — 1.5-3mm */
    SIZE_CLASS_MEDIUM = 2,  /* 1206-2512, SOIC, TSSOP, QFN — 3-12mm */
    SIZE_CLASS_LARGE  = 3,  /* QFP > 64, BGA > 100, TO-220 — > 12mm */
} SizeClass;

static SizeClass get_size_class(PackageType pkg)
{
    switch (pkg) {
    case PKG_SMD_0201: case PKG_SMD_0402: case PKG_SC_70:
        return SIZE_CLASS_TINY;
    case PKG_SMD_0603: case PKG_SMD_0805:
    case PKG_SOT_23: case PKG_SMA: case PKG_SMB:
        return SIZE_CLASS_SMALL;
    case PKG_SMD_1206: case PKG_SMD_1210: case PKG_SMD_1812:
    case PKG_SMD_2512: case PKG_SOT_223: case PKG_SOIC_8:
    case PKG_SOIC_16: case PKG_TSSOP_16: case PKG_TSSOP_20:
    case PKG_QFN_16: case PKG_QFN_32: case PKG_QFN_48:
    case PKG_QFP_32: case PKG_QFP_64: case PKG_BGA_64:
    case PKG_DIP_8: case PKG_DIP_16:
        return SIZE_CLASS_MEDIUM;
    case PKG_QFP_100: case PKG_BGA_100: case PKG_BGA_256:
    case PKG_BGA_484: case PKG_TO_220: case PKG_TO_247:
        return SIZE_CLASS_LARGE;
    default:
        return SIZE_CLASS_MEDIUM;
    }
}

/**
 * IPC spacing matrix: minimum edge-to-edge spacing per size class pair.
 * [row][col] = spacing in mm for Level B
 */
static const double ipc_spacing_matrix[4][4] = {
    /*          TINY    SMALL   MEDIUM  LARGE */
    /* TINY   */ { 0.15,  0.20,   0.25,   0.40 },
    /* SMALL  */ { 0.20,  0.25,   0.30,   0.50 },
    /* MEDIUM */ { 0.25,  0.30,   0.50,   0.75 },
    /* LARGE  */ { 0.40,  0.50,   0.75,   1.00 }
};

double placement_constraint_get_ipc_spacing(PackageType pkg_a,
                                            PackageType pkg_b,
                                            IPCDensityLevel level)
{
    SizeClass sc_a = get_size_class(pkg_a);
    SizeClass sc_b = get_size_class(pkg_b);

    double base = ipc_spacing_matrix[sc_a][sc_b];

    /* Adjust for IPC density level */
    switch (level) {
    case IPC_LEVEL_A:
        return base * 1.5;   /* +50% more margin */
    case IPC_LEVEL_B:
        return base;          /* Nominal */
    case IPC_LEVEL_C:
        return base * 0.6;   /* Tighter spacing */
    default:
        return base;
    }
}

/* ============================================================================
 * Spacing Checking
 * ============================================================================ */

static double component_edge_distance(const Component* a, const Component* b)
{
    /* Compute minimum edge-to-edge distance between two rotated rectangles.
     * Uses a simplified approach: center distance minus sum of half-diagonals.
     * This is conservative (may over-report violations, never misses one). */

    double dx = a->position.x - b->position.x;
    double dy = a->position.y - b->position.y;
    double center_dist = sqrt(dx * dx + dy * dy);

    /* Half-diagonal of body rectangle */
    double hw_a = a->body.width  / 2.0;
    double hh_a = a->body.height / 2.0;
    double half_diag_a = sqrt(hw_a * hw_a + hh_a * hh_a);

    double hw_b = b->body.width  / 2.0;
    double hh_b = b->body.height / 2.0;
    double half_diag_b = sqrt(hw_b * hw_b + hh_b * hh_b);

    /* Conservative estimate: minimum edge distance >= center_dist - sum(half_diag) */
    double edge_dist = center_dist - half_diag_a - half_diag_b;
    return (edge_dist > 0.0) ? edge_dist : 0.0;
}

bool placement_constraint_check_spacing(const Component* comp_a,
                                        const Component* comp_b,
                                        IPCDensityLevel level,
                                        Violation* violation)
{
    if (!comp_a || !comp_b) return true;
    if (!comp_a->is_placed || !comp_b->is_placed) return true;

    double edge_dist = component_edge_distance(comp_a, comp_b);
    double min_spacing = placement_constraint_get_ipc_spacing(
        comp_a->package, comp_b->package, level);

    /* Also check component-specific minimum spacing overrides */
    if (comp_a->min_spacing_mm > min_spacing)
        min_spacing = comp_a->min_spacing_mm;
    if (comp_b->min_spacing_mm > min_spacing)
        min_spacing = comp_b->min_spacing_mm;

    double margin = edge_dist - min_spacing;

    if (violation && margin < 0.0) {
        violation->type           = CONSTRAINT_SPACING;
        violation->severity       = (margin < -min_spacing) ? VIOLATION_ERROR : VIOLATION_WARNING;
        violation->comp_id_a      = comp_a->comp_id;
        violation->comp_id_b      = comp_b->comp_id;
        snprintf(violation->description, sizeof(violation->description),
                 "Insufficient spacing: %.3fmm < %.3fmm (IPC Level %d, %s-%s vs %s-%s)",
                 edge_dist, min_spacing, level,
                 comp_a->designator, comp_b->designator,
                 comp_a->description, comp_b->description);
        violation->measured_value = edge_dist;
        violation->limit_value    = min_spacing;
        violation->margin         = margin;
    }

    return margin >= 0.0;
}

ConstraintResult placement_constraint_check_all_spacing(
    const PlacementResult* result, IPCDensityLevel level)
{
    ConstraintResult cres = {0, NULL, true, 0.0};
    if (!result || result->component_count < 2) {
        cres.all_clear = true;
        return cres;
    }

    /* Allocate violations array (worst case: all pairs) */
    uint32_t C = result->component_count;
    uint32_t max_v = C * (C - 1) / 2;
    cres.violations = (Violation*)malloc(max_v * sizeof(Violation));
    if (!cres.violations) {
        cres.all_clear = false;
        return cres;
    }

    double worst = 0.0;
    for (uint32_t i = 0; i < C; i++) {
        for (uint32_t j = i + 1; j < C; j++) {
            if (!result->components[i].is_placed || !result->components[j].is_placed)
                continue;

            Violation v;
            memset(&v, 0, sizeof(Violation));
            if (!placement_constraint_check_spacing(&result->components[i],
                                                     &result->components[j],
                                                     level, &v)) {
                cres.violations[cres.violation_count++] = v;
                if (v.margin < worst) worst = v.margin;
            }
        }
    }

    cres.all_clear = (cres.violation_count == 0);
    cres.worst_margin = worst;
    return cres;
}

/* ============================================================================
 * Board Boundary
 * ============================================================================ */

bool placement_constraint_check_board_boundary(const Component* comp,
                                                const Board* board)
{
    if (!comp || !board) return false;
    if (!comp->is_placed) return true;

    BoundingBox bb = placement_component_get_bounds(comp);

    /* Component must be fully inside board outline with at least 0.5mm margin */
    double margin = 0.5; /* Edge clearance per IPC-2221 */
    if (bb.x_min < margin || bb.y_min < margin ||
        bb.x_max > board->outline.width - margin ||
        bb.y_max > board->outline.height - margin) {
        return false;
    }
    return true;
}

/* ============================================================================
 * Keep-Out Zones
 * ============================================================================ */

bool placement_constraint_check_keepout(const Component* comp,
                                        const KeepOutZone* zones,
                                        uint32_t zone_count)
{
    if (!comp || !zones || zone_count == 0) return true;
    if (!comp->is_placed) return true;

    BoundingBox bb = placement_component_get_bounds(comp);
    Rect2D comp_rect;
    comp_rect.origin.x = bb.x_min;
    comp_rect.origin.y = bb.y_min;
    comp_rect.width    = bb.x_max - bb.x_min;
    comp_rect.height   = bb.y_max - bb.y_min;

    for (uint32_t z = 0; z < zone_count; z++) {
        /* Check if mount side matches restriction */
        if ((comp->mount == MOUNT_SMD_TOP || comp->mount == MOUNT_THT_TOP)
            && !zones[z].restrict_top) continue;
        if ((comp->mount == MOUNT_SMD_BOTTOM || comp->mount == MOUNT_THT_BOTTOM)
            && !zones[z].restrict_bottom) continue;

        if (placement_util_rect_overlap(&comp_rect, &zones[z].region)) {
            return false;
        }
    }
    return true;
}

/* ============================================================================
 * Thermal Constraints
 * ============================================================================ */

/** Thermal conductivity of FR4: ~0.3 W/(m·K) */
#define FR4_THERMAL_CONDUCTIVITY 0.3

/** Convection coefficient for natural convection: ~10 W/(m²·K) */
#define NATURAL_CONVECTION_H 10.0

bool placement_constraint_check_thermal(const PlacementResult* result,
                                        double ambient_C,
                                        Violation* violation)
{
    if (!result) return true;

    bool all_ok = true;
    double worst_margin = DBL_MAX;
    Violation worst_v = {0};

    for (uint32_t i = 0; i < result->component_count; i++) {
        Component* ci = &result->components[i];
        if (!ci->is_placed || ci->power_dissipation_W <= 0.0) continue;

        /* Compute T_j for this component */
        double t_j = ambient_C + ci->power_dissipation_W * ci->theta_JA_C_per_W;

        /* Add neighbor heating effects */
        for (uint32_t j = 0; j < result->component_count; j++) {
            if (i == j) continue;
            Component* cj = &result->components[j];
            if (!cj->is_placed || cj->power_dissipation_W <= 0.0) continue;

            double coupling = placement_constraint_thermal_coupling(ci, cj,
                result->board.thickness_mm);
            t_j += cj->power_dissipation_W * coupling;
        }

        double margin = ci->max_junction_temp_C - t_j;
        if (margin < 0.0) {
            all_ok = false;
            if (margin < worst_margin) {
                worst_margin = margin;
                if (violation) {
                    violation->type      = CONSTRAINT_THERMAL;
                    violation->severity  = VIOLATION_ERROR;
                    violation->comp_id_a = ci->comp_id;
                    violation->comp_id_b = 0;
                    snprintf(violation->description, sizeof(violation->description),
                             "Thermal violation: T_j=%.1f°C > T_j_max=%.1f°C for %s",
                             t_j, ci->max_junction_temp_C, ci->designator);
                    violation->measured_value = t_j;
                    violation->limit_value    = ci->max_junction_temp_C;
                    violation->margin         = margin;
                }
            }
        }
    }

    if (!all_ok && violation) {
        *violation = worst_v;
    }
    return all_ok;
}

double placement_constraint_thermal_coupling(const Component* comp_a,
                                              const Component* comp_b,
                                              double board_thickness_mm)
{
    if (!comp_a || !comp_b || !comp_a->is_placed || !comp_b->is_placed)
        return 0.0;

    double dx = comp_a->position.x - comp_b->position.x;
    double dy = comp_a->position.y - comp_b->position.y;
    double r  = sqrt(dx * dx + dy * dy);

    if (r < 1e-6) r = 1e-6; /* Avoid singularity at zero distance */

    /* Thermal coupling resistance for point sources on a thin plate:
     * θ_coupling ∝ 1/r for small r, with cutoff at large r.
     * Using an empirical model: θ_c = (1 / (2π * k * t)) * ln(r_max / r)
     * for r_min < r < r_max
     */
    double k = FR4_THERMAL_CONDUCTIVITY;  /* W/(m·K) for FR4 */
    double t = board_thickness_mm * 1e-3; /* Convert mm to m */
    double r_m = r * 1e-3;                /* Convert mm to m */

    /* Characteristic length for lateral heat spreading: sqrt(k * t / h) */
    double h = NATURAL_CONVECTION_H;
    double L_char = sqrt(k * t / h);  /* meters */

    if (r_m > 10.0 * L_char) return 0.0; /* Negligible coupling beyond ~10 L_char */

    /* Solution to heat equation for a point source on a thin plate with
     * convection boundary: Green's function approach.
     * Approximated for practical use: θ_coupling = K₀(r / L_char) / (π * k * t)
     * where K₀ is the modified Bessel function (approximated here).
     */
    double arg = r_m / L_char;
    /* Approximation for K₀(x) for moderate x: exp(-x) / sqrt(x) * sqrt(π/2) */
    double K0_approx;
    if (arg < 0.1) {
        K0_approx = -log(arg / 2.0) - 0.5772; /* Euler-Mascheroni constant */
    } else {
        K0_approx = exp(-arg) / sqrt(arg) * sqrt(M_PI / 2.0);
    }
    double theta_coupling = K0_approx / (M_PI * k * t);

    /* Convert to °C/W */
    return theta_coupling;
}

/* ============================================================================
 * Signal Integrity: Trace Length Check
 * ============================================================================ */

bool placement_constraint_check_trace_length(const PlacementResult* result,
                                              uint32_t net_id,
                                              double max_length_mm,
                                              Violation* violation)
{
    if (!result) return true;

    /* Find all pins on this net and compute Manhattan span */
    double min_x = DBL_MAX, max_x = -DBL_MAX;
    double min_y = DBL_MAX, max_y = -DBL_MAX;
    bool found = false;

    for (uint32_t c = 0; c < result->component_count; c++) {
        Component* comp = &result->components[c];
        if (!comp->is_placed) continue;
        for (uint32_t p = 0; p < comp->pad_count; p++) {
            if (comp->net_ids[p] == net_id) {
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
    }

    if (!found) return true;

    /* Estimated trace length = Manhattan distance of net bounding box × 1.2
     * (1.2 factor accounts for routing detours) */
    double est_length = (max_x - min_x + max_y - min_y) * 1.2;
    double margin = max_length_mm - est_length;

    if (violation && margin < 0.0) {
        violation->type           = CONSTRAINT_HIGH_SPEED;
        violation->severity       = VIOLATION_ERROR;
        violation->comp_id_a      = 0;
        violation->comp_id_b      = 0;
        snprintf(violation->description, sizeof(violation->description),
                 "Net %u trace length %.1fmm exceeds max %.1fmm",
                 net_id, est_length, max_length_mm);
        violation->measured_value = est_length;
        violation->limit_value    = max_length_mm;
        violation->margin         = margin;
    }

    return margin >= 0.0;
}

/* ============================================================================
 * Signal Integrity: Differential Pair
 * ============================================================================ */

bool placement_constraint_check_diff_pair(const PlacementResult* result,
                                           uint32_t net_id_p,
                                           uint32_t net_id_n,
                                           double max_delta_mm,
                                           Violation* violation)
{
    if (!result) return true;

    /* Compute HPWL for each net */
    double length_p = 0.0, length_n = 0.0;

    for (uint32_t pass = 0; pass < 2; pass++) {
        uint32_t nid = (pass == 0) ? net_id_p : net_id_n;
        double min_x = DBL_MAX, max_x = -DBL_MAX;
        double min_y = DBL_MAX, max_y = -DBL_MAX;
        bool found = false;

        for (uint32_t c = 0; c < result->component_count; c++) {
            Component* comp = &result->components[c];
            if (!comp->is_placed) continue;
            for (uint32_t p = 0; p < comp->pad_count; p++) {
                if (comp->net_ids[p] == nid) {
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
        }

        if (found) {
            double len = (max_x - min_x + max_y - min_y);
            if (pass == 0) length_p = len; else length_n = len;
        }
    }

    double delta = fabs(length_p - length_n);
    double margin = max_delta_mm - delta;

    if (violation && margin < 0.0) {
        violation->type           = CONSTRAINT_HIGH_SPEED;
        violation->severity       = VIOLATION_ERROR;
        violation->comp_id_a      = 0;
        violation->comp_id_b      = 0;
        snprintf(violation->description, sizeof(violation->description),
                 "Differential pair length mismatch: %.2fmm > %.2fmm "
                 "(net+ %u: %.2fmm, net- %u: %.2fmm)",
                 delta, max_delta_mm, net_id_p, length_p, net_id_n, length_n);
        violation->measured_value = delta;
        violation->limit_value    = max_delta_mm;
        violation->margin         = margin;
    }

    return margin >= 0.0;
}

/* ============================================================================
 * Manufacturing: Wave Soldering
 * ============================================================================ */

bool placement_constraint_check_wave_solder(const Component* comp,
                                             double wave_dir,
                                             Violation* violation)
{
    if (!comp) return true;
    if (!comp->is_placed) return true;

    /* Only check SMD bottom-side components */
    if (comp->mount != MOUNT_SMD_BOTTOM) return true;

    /* For rectangular SMD packages (chip resistors/capacitors),
     * the long axis should be parallel to the wave direction
     * to ensure both pads enter simultaneously, preventing tombstoning. */
    double rot_rad = comp->rotation * M_PI / 180.0;
    double wave_rad = wave_dir * M_PI / 180.0;

    /* Component's primary axis direction (width > height → X-axis aligned at 0°) */
    bool is_landscape = comp->body.width >= comp->body.height;
    double primary_axis_rad = is_landscape ? rot_rad : rot_rad + M_PI / 2.0;

    /* Angular difference between component axis and wave direction */
    double diff = fabs(primary_axis_rad - wave_rad);
    /* Normalize to [-π, π] */
    while (diff > M_PI) diff -= 2.0 * M_PI;
    while (diff < -M_PI) diff += 2.0 * M_PI;
    diff = fabs(diff);

    /* Allowable angular misalignment: ±15° for chip components */
    double max_angle = 15.0 * M_PI / 180.0;
    /* For IC packages (SOIC, TSSOP, QFP), perpendicular is ideal */
    if (comp->package >= PKG_SOIC_8 && comp->package <= PKG_QFP_100) {
        /* IC prefers perpendicular to wave */
        double ideal = M_PI / 2.0;
        diff = fabs(fabs(diff) - ideal);
        max_angle = 15.0 * M_PI / 180.0;
    }

    double margin = max_angle - diff;

    if (violation && margin < 0.0) {
        violation->type           = CONSTRAINT_MANUFACTURING;
        violation->severity       = VIOLATION_WARNING;
        violation->comp_id_a      = comp->comp_id;
        violation->comp_id_b      = 0;
        snprintf(violation->description, sizeof(violation->description),
                 "Wave solder orientation: misalignment %.1f° > %.1f° for %s",
                 diff * 180.0 / M_PI, max_angle * 180.0 / M_PI, comp->designator);
        violation->measured_value = diff * 180.0 / M_PI;
        violation->limit_value    = max_angle * 180.0 / M_PI;
        violation->margin         = margin * 180.0 / M_PI;
    }

    return margin >= -1e-9;
}

/* ============================================================================
 * Manufacturing: Shadowing
 * ============================================================================ */

bool placement_constraint_check_shadowing(const Component* comp_a,
                                           const Component* comp_b,
                                           double wave_dir,
                                           Violation* violation)
{
    if (!comp_a || !comp_b) return true;
    if (!comp_a->is_placed || !comp_b->is_placed) return true;

    /* Check height ratio */
    double h_a = comp_a->envelope.z_max - comp_a->envelope.z_min;
    double h_b = comp_b->envelope.z_max - comp_b->envelope.z_min;
    if (h_a <= 0 || h_b <= 0) return true;

    /* If height ratio is < 3:1, shadowing is not a risk */
    double taller = (h_a > h_b) ? h_a : h_b;
    double shorter = (h_a > h_b) ? h_b : h_a;
    if (taller / shorter < 3.0) return true;

    /* Check spacing in the wave direction */
    double dx = comp_b->position.x - comp_a->position.x;
    double dy = comp_b->position.y - comp_a->position.y;
    double wave_rad = wave_dir * M_PI / 180.0;

    /* Project distance onto wave direction vector */
    double proj_dist = dx * cos(wave_rad) + dy * sin(wave_rad);

    /* If taller component is up-wave from shorter and within 5mm, shadowing risk */
    const Component* upstream;
    const Component* downstream;
    if (proj_dist > 0) {
        upstream = comp_a;
        downstream = comp_b;
    } else {
        upstream = comp_b;
        downstream = comp_a;
        proj_dist = -proj_dist;
    }

    double h_up = upstream->envelope.z_max - upstream->envelope.z_min;
    double h_down = downstream->envelope.z_max - downstream->envelope.z_min;

    if (h_up > h_down * 3.0 && proj_dist < 5.0) {
        if (violation) {
            violation->type           = CONSTRAINT_MANUFACTURING;
            violation->severity       = VIOLATION_WARNING;
            violation->comp_id_a      = upstream->comp_id;
            violation->comp_id_b      = downstream->comp_id;
            snprintf(violation->description, sizeof(violation->description),
                     "Shadowing risk: %s (%.1fmm tall) shadows %s (%.1fmm tall) "
                     "at %.1fmm spacing",
                     upstream->designator, h_up, downstream->designator, h_down,
                     proj_dist);
            violation->measured_value = proj_dist;
            violation->limit_value    = 5.0;
            violation->margin         = proj_dist - 5.0;
        }
        return false;
    }
    return true;
}

/* ============================================================================
 * Mechanical: Height & Connector Clearance
 * ============================================================================ */

bool placement_constraint_check_height(const Component* comp,
                                        double max_height_mm)
{
    if (!comp) return true;
    double comp_height = comp->envelope.z_max - comp->envelope.z_min;
    return comp_height <= max_height_mm;
}

bool placement_constraint_check_connector_clearance(const Component* comp,
                                                     double clearance_x_mm,
                                                     double clearance_y_mm,
                                                     const Board* board)
{
    if (!comp || !board) return true;
    if (!comp->is_placed) return true;
    if (comp->category != COMP_CAT_CONNECTOR) return true; /* Only applies to connectors */

    BoundingBox bb = placement_component_get_bounds(comp);

    /* Connector needs clearance beyond body for mating cable/header */
    double required_x_min = bb.x_min - clearance_x_mm;
    double required_x_max = bb.x_max + clearance_x_mm;
    double required_y_min = bb.y_min - clearance_y_mm;
    double required_y_max = bb.y_max + clearance_y_mm;

    /* Check if required clearance extends beyond board */
    if (required_x_min < 0.0 || required_y_min < 0.0 ||
        required_x_max > board->outline.width ||
        required_y_max > board->outline.height) {
        return false;
    }

    return true;
}

/* ============================================================================
 * Result Management
 * ============================================================================ */

void placement_constraint_result_free(ConstraintResult* result)
{
    if (!result) return;
    free(result->violations);
    result->violations = NULL;
    result->violation_count = 0;
}

/* ============================================================================
 * Comprehensive Constraint Check
 * ============================================================================ */

ConstraintResult placement_constraint_check_all(const PlacementResult* result,
                                                 IPCDensityLevel ipc_level,
                                                 const KeepOutZone* keepout_zones,
                                                 uint32_t zone_count,
                                                 double ambient_C)
{
    ConstraintResult combined;
    memset(&combined, 0, sizeof(ConstraintResult));
    combined.all_clear = true;
    combined.worst_margin = 0.0;

    /* Allocate worst-case violation count */
    uint32_t C = result ? result->component_count : 0;
    uint32_t max_v = C * (C - 1) / 2 + C + 1; /* Pairs + single-component + thermal */
    combined.violations = (Violation*)malloc(max_v * sizeof(Violation));
    if (!combined.violations) {
        combined.all_clear = false;
        return combined;
    }

    /* 1. Spacing */
    ConstraintResult spacing = placement_constraint_check_all_spacing(result, ipc_level);
    for (uint32_t i = 0; i < spacing.violation_count; i++) {
        combined.violations[combined.violation_count++] = spacing.violations[i];
        if (spacing.violations[i].margin < combined.worst_margin) {
            combined.worst_margin = spacing.violations[i].margin;
        }
    }
    free(spacing.violations);

    /* 2. Board boundary */
    for (uint32_t i = 0; i < C; i++) {
        if (!placement_constraint_check_board_boundary(&result->components[i],
                                                        &result->board)) {
            Violation v;
            memset(&v, 0, sizeof(Violation));
            v.type      = CONSTRAINT_SPACING;
            v.severity  = VIOLATION_ERROR;
            v.comp_id_a = result->components[i].comp_id;
            snprintf(v.description, sizeof(v.description),
                     "Component %s extends beyond board boundary",
                     result->components[i].designator);
            v.measured_value = 0.0;
            v.limit_value    = 0.0;
            v.margin         = -1.0;
            combined.violations[combined.violation_count++] = v;
            if (-1.0 < combined.worst_margin) combined.worst_margin = -1.0;
        }
    }

    /* 3. Keep-out zones */
    for (uint32_t i = 0; i < C; i++) {
        if (!placement_constraint_check_keepout(&result->components[i],
                                                 keepout_zones, zone_count)) {
            Violation v;
            memset(&v, 0, sizeof(Violation));
            v.type      = CONSTRAINT_KEEPOUT;
            v.severity  = VIOLATION_ERROR;
            v.comp_id_a = result->components[i].comp_id;
            snprintf(v.description, sizeof(v.description),
                     "Component %s encroaches on keep-out zone",
                     result->components[i].designator);
            v.margin = -1.0;
            combined.violations[combined.violation_count++] = v;
            if (-1.0 < combined.worst_margin) combined.worst_margin = -1.0;
        }
    }

    /* 4. Thermal */
    Violation thermal_v;
    if (!placement_constraint_check_thermal(result, ambient_C, &thermal_v)) {
        combined.violations[combined.violation_count++] = thermal_v;
        if (thermal_v.margin < combined.worst_margin) {
            combined.worst_margin = thermal_v.margin;
        }
    }

    /* 5. Signal integrity — check all critical nets */
    for (uint32_t n = 0; n < result->net_count; n++) {
        if (result->nets[n].is_critical) {
            /* Assume max trace length = 150mm for critical nets (typical FR4, 1ns rise) */
            Violation si_v;
            if (!placement_constraint_check_trace_length(result,
                    result->nets[n].net_id, 150.0, &si_v)) {
                combined.violations[combined.violation_count++] = si_v;
                if (si_v.margin < combined.worst_margin) {
                    combined.worst_margin = si_v.margin;
                }
            }
        }
    }

    combined.all_clear = (combined.violation_count == 0);
    return combined;
}
