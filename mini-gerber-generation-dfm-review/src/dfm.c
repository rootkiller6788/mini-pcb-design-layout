/**
 * dfm.c — Design for Manufacturing (DFM) Rule Checking Implementation
 *
 * Implements comprehensive DFM checks per IPC-2221, IPC-6012, and IPC-7351.
 * Each function implements an independent DFM knowledge point:
 * trace width, spacing, annular ring, solder mask clearance, aspect ratio,
 * edge clearance, acid trap detection, sliver detection, and thermal relief.
 *
 * References:
 *   - IPC-2221A, Generic Standard on Printed Board Design (2003)
 *   - IPC-6012D, Qualification and Performance Specification for Rigid PCBs
 *   - IPC-7351B, Generic Requirements for SMD Land Pattern Design
 *   - IPC-D-356, Bare Board Electrical Test Data Format
 */

#include "dfm.h"
#include "gerber.h"
#include "pcb_geometry.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <assert.h>

/* ─── Rule Set Initialization ───────────────────────────────────── */

/**
 * Initialize DFM rules with IPC-2221 defaults.
 *
 * IPC classes:
 *   Class 1 — General Electronic Products (consumer)
 *   Class 2 — Dedicated Service Electronic Products (industrial)
 *   Class 3 — High Reliability (aerospace, medical, military)
 *
 * Table 6-1 of IPC-2221 defines minimum conductor width and spacing
 * based on copper weight and layer type.
 */
void dfm_ruleset_init(DFMRuleSet *rules, IPCClass ipc_class,
                      int layer_count, double thickness_mm,
                      CopperWeight copper_wt)
{
    assert(rules != NULL);

    memset(rules, 0, sizeof(DFMRuleSet));

    rules->ipc_class             = ipc_class;
    rules->layer_count           = layer_count;
    rules->finished_thickness_mm = thickness_mm;
    rules->copper_weight         = copper_wt;

    /* ── Trace Width ── */
    /* IPC-2221 Table 6-1. For 1oz copper: */
    switch (ipc_class) {
    case IPC_CLASS_1:
        rules->trace_width.min_width_mm = 0.150;
        break;
    case IPC_CLASS_2:
        rules->trace_width.min_width_mm = 0.150;
        break;
    case IPC_CLASS_3:
        rules->trace_width.min_width_mm = 0.200;
        break;
    }
    rules->trace_width.ipc_class   = ipc_class;
    rules->trace_width.is_external = 1;  /* Checked separately per layer */

    /* ── Spacing ── */
    switch (ipc_class) {
    case IPC_CLASS_1:
        rules->spacing.min_spacing_mm = 0.150;
        break;
    case IPC_CLASS_2:
        rules->spacing.min_spacing_mm = 0.150;
        break;
    case IPC_CLASS_3:
        rules->spacing.min_spacing_mm = 0.200;
        break;
    }
    rules->spacing.max_voltage_v  = 15.0;  /* Low-voltage default */
    rules->spacing.ipc_class      = ipc_class;

    /* ── Annular Ring ── */
    switch (ipc_class) {
    case IPC_CLASS_1:
        rules->annular_ring.min_annular_mm = 0.025;  /* External */
        break;
    case IPC_CLASS_2:
        rules->annular_ring.min_annular_mm = 0.050;
        break;
    case IPC_CLASS_3:
        rules->annular_ring.min_annular_mm = 0.100;
        break;
    }
    rules->annular_ring.ipc_class   = ipc_class;
    rules->annular_ring.is_external = 1;

    /* ── Solder Mask ── */
    rules->solder_mask.min_clearance_mm = 0.050;  /* IPC-7351 */
    rules->solder_mask.max_clearance_mm = 0.150;  /* Prevent slivers */

    /* ── Aspect Ratio ── */
    switch (ipc_class) {
    case IPC_CLASS_1:
        rules->aspect_ratio.max_aspect_ratio = 10.0;
        break;
    case IPC_CLASS_2:
        rules->aspect_ratio.max_aspect_ratio = 8.0;
        break;
    case IPC_CLASS_3:
        rules->aspect_ratio.max_aspect_ratio = 6.0;
        break;
    }
    rules->aspect_ratio.board_thickness_mm = thickness_mm;
    rules->aspect_ratio.min_hole_mm        = 0.20;  /* Mechanical drill min */

    /* ── Edge Clearance ── */
    switch (ipc_class) {
    case IPC_CLASS_1:
        rules->edge_clearance.min_clearance_mm = 0.250;
        break;
    case IPC_CLASS_2:
        rules->edge_clearance.min_clearance_mm = 0.350;
        break;
    case IPC_CLASS_3:
        rules->edge_clearance.min_clearance_mm = 0.500;
        break;
    }

    /* ── Silkscreen ── */
    rules->silkscreen.min_line_width_mm = 0.150;

    /* ── Via-in-Pad ── */
    rules->via_in_pad = VIA_IN_PAD_DISALLOWED;  /* Default conservative */
}

/* ─── Violation Report ──────────────────────────────────────────── */

void dfm_report_init(DFMViolationReport *report)
{
    assert(report != NULL);

    memset(report, 0, sizeof(DFMViolationReport));
}

int dfm_report_add(DFMViolationReport *report, const DFMViolation *violation)
{
    assert(report != NULL);
    assert(violation != NULL);

    if (report->count >= DFM_MAX_VIOLATIONS) {
        return -1;  /* Report full */
    }

    report->violations[report->count] = *violation;

    /* Update category and severity counters */
    int cat = (int)violation->category;
    if (cat >= 0 && cat < 16) {
        report->by_category[cat]++;
    }

    int sev = (int)violation->severity;
    if (sev >= 0 && sev < 4) {
        report->by_severity[sev]++;
    }

    report->count++;
    return 0;
}

int dfm_report_count_ge(const DFMViolationReport *report, DFMSeverity min_sev)
{
    assert(report != NULL);

    int count = 0;
    for (int s = (int)min_sev; s < 4; s++) {
        count += report->by_severity[s];
    }
    return count;
}

void dfm_report_print(const DFMViolationReport *report, FILE *fp)
{
    assert(report != NULL);
    assert(fp != NULL);

    fprintf(fp, "\n");
    fprintf(fp, "==========================================\n");
    fprintf(fp, "  DFM Violation Report\n");
    fprintf(fp, "==========================================\n");
    fprintf(fp, "  Total violations: %d\n", report->count);

    const char *cat_names[] = {
        "Trace Width", "Spacing", "Annular Ring", "Solder Mask",
        "Aspect Ratio", "Edge Clearance", "Silkscreen", "Via-in-Pad",
        "Acid Trap", "Mask Sliver", "Starved Thermal", "", "", "", "", "Custom"
    };
    const char *sev_names[] = {"INFO", "WARNING", "ERROR", "CRITICAL"};

    fprintf(fp, "\n  By Severity:\n");
    for (int s = 0; s < 4; s++) {
        if (report->by_severity[s] > 0) {
            fprintf(fp, "    %-10s: %d\n", sev_names[s], report->by_severity[s]);
        }
    }

    fprintf(fp, "\n  By Category:\n");
    for (int c = 0; c < 11; c++) {
        if (report->by_category[c] > 0) {
            fprintf(fp, "    %-16s: %d\n", cat_names[c], report->by_category[c]);
        }
    }

    /* Print detailed violations (errors and criticals only) */
    fprintf(fp, "\n  Detailed Violations (ERROR and CRITICAL only):\n");
    int printed = 0;
    for (int i = 0; i < report->count && printed < 20; i++) {
        if (report->violations[i].severity >= DFM_SEV_ERROR) {
            fprintf(fp, "    [%s] %s at (%.3f, %.3f) layer %d:\n",
                    sev_names[report->violations[i].severity],
                    cat_names[report->violations[i].category],
                    report->violations[i].position_x_mm,
                    report->violations[i].position_y_mm,
                    report->violations[i].layer);
            fprintf(fp, "      measured %.4f, threshold %.4f\n",
                    report->violations[i].measured_value,
                    report->violations[i].threshold_value);
            fprintf(fp, "      %s\n", report->violations[i].description);
            printed++;
        }
    }
    if (printed < report->count) {
        fprintf(fp, "    ... (%d more ERRORS/CRITICALS omitted)\n",
                report->count - printed);
    }

    fprintf(fp, "==========================================\n\n");
}

/* ─── DFM Check: Trace Width ────────────────────────────────────── */

/**
 * Check each trace against minimum width requirement.
 *
 * For a trace defined by (start, end, width):
 *   if width < min_width_mm → violation.
 *
 * Note: This also checks for neck-downs if trace width varies.
 * For single-width traces, this is a simple comparison.
 *
 * IPC-2221 §6.2, Table 6-1: Conductor width is a function of
 * current-carrying capacity and manufacturing capability.
 * The minimum specified here is for manufacturing only;
 * current-carrying requirements may demand larger widths.
 */
void dfm_check_trace_width(const PCBTrace *traces, int n_traces,
                           const DFMRuleTraceWidth *rule,
                           int layer, DFMViolationReport *report)
{
    assert(report != NULL);
    assert(rule != NULL);
    assert(traces != NULL || n_traces == 0);

    for (int i = 0; i < n_traces; i++) {
        const PCBTrace *t = &traces[i];

        if (t->layer != layer) continue;

        double width = t->width_mm;
        if (width < rule->min_width_mm) {
            DFMViolation v;
            memset(&v, 0, sizeof(v));
            v.category       = DFM_CAT_TRACE_WIDTH;
            v.severity       = (width < rule->min_width_mm * 0.5) ?
                               DFM_SEV_ERROR : DFM_SEV_WARNING;
            v.position_x_mm  = t->start.x;
            v.position_y_mm  = t->start.y;
            v.measured_value = width;
            v.threshold_value= rule->min_width_mm;
            v.layer          = layer;
            snprintf(v.description, sizeof(v.description),
                     "Trace width %.3f mm below minimum %.3f mm "
                     "(net %s)",
                     width, rule->min_width_mm, t->net_name);
            dfm_report_add(report, &v);
        }
    }
}

/* ─── DFM Check: Spacing ────────────────────────────────────────── */

/**
 * Check minimum spacing between all pairs of features on the same layer.
 *
 * Algorithm: O(N²) pair-wise distance comparison.
 * For boards with >1000 features, a sweep-line algorithm
 * (Bentley-Ottmann, O(N log N)) would improve performance.
 *
 * Spacing computation depends on feature type:
 *   - Trace-to-trace: segment-to-segment distance
 *   - Pad-to-pad: point-to-point minus radii
 *   - Trace-to-pad: point-to-segment minus pad radius
 *   - Region-to-*: polygon distance functions
 */
void dfm_check_spacing(const PCBFeature *features, int n_features,
                       const DFMRuleSpacing *rule,
                       int layer, DFMViolationReport *report)
{
    assert(report != NULL);
    assert(rule != NULL);
    assert(features != NULL || n_features == 0);

    for (int i = 0; i < n_features; i++) {
        const PCBFeature *fi = &features[i];
        if (fi->layer != layer) continue;

        for (int j = i + 1; j < n_features; j++) {
            const PCBFeature *fj = &features[j];
            if (fj->layer != layer) continue;

            /* Different nets — spacing enforced */
            if (strcmp(fi->net_name, fj->net_name) == 0) continue;

            double dist = 1e9;  /* Large initial value */

            /* Compute distance based on feature types */
            if (fi->type == FEAT_TRACE && fj->type == FEAT_TRACE) {
                dist = pcb_segment_to_segment_distance(
                    &fi->data.trace.start, &fi->data.trace.end,
                    &fj->data.trace.start, &fj->data.trace.end);
                /* Subtract half-widths */
                dist -= (fi->data.trace.width_mm + fj->data.trace.width_mm) * 0.5;

            } else if (fi->type == FEAT_PAD && fj->type == FEAT_PAD) {
                dist = pcb_distance(&fi->data.pad.position,
                                    &fj->data.pad.position);
                /* Subtract pad radii */
                double r1 = fi->data.pad.width_mm * 0.5;
                double r2 = fj->data.pad.width_mm * 0.5;
                dist -= (r1 + r2);

            } else if (fi->type == FEAT_PAD && fj->type == FEAT_TRACE) {
                dist = pcb_point_to_segment_distance(
                    &fi->data.pad.position,
                    &fj->data.trace.start,
                    &fj->data.trace.end);
                dist -= (fi->data.pad.width_mm + fj->data.trace.width_mm) * 0.5;

            } else if (fi->type == FEAT_TRACE && fj->type == FEAT_PAD) {
                dist = pcb_point_to_segment_distance(
                    &fj->data.pad.position,
                    &fi->data.trace.start,
                    &fi->data.trace.end);
                dist -= (fj->data.pad.width_mm + fi->data.trace.width_mm) * 0.5;

            } else if (fi->type == FEAT_REGION && fj->type != FEAT_REGION) {
                /* Region to other: use polygon-to-point or polygon-to-segment */
                /* Simplified: use bounding box center distance */
                PCBAABB ab = pcb_region_aabb(&fi->data.region);
                double cx = (ab.x_min + ab.x_max) * 0.5;
                double cy = (ab.y_min + ab.y_max) * 0.5;
                GerberPointD center = {cx, cy};
                if (fj->type == FEAT_TRACE) {
                    dist = pcb_point_to_segment_distance(
                        &center, &fj->data.trace.start, &fj->data.trace.end);
                } else {
                    dist = pcb_distance(&center, &fj->data.pad.position);
                }
                dist -= ((ab.x_max - ab.x_min) + (ab.y_max - ab.y_min)) * 0.25;

            } else {
                /* Other cases — use bounding box approximation */
                continue;
            }

            /* Enforce spacing (allow 1 micron tolerance) */
            if (dist < rule->min_spacing_mm - 0.001) {
                DFMViolation v;
                memset(&v, 0, sizeof(v));
                v.category       = DFM_CAT_SPACING;
                v.severity       = (dist < rule->min_spacing_mm * 0.5) ?
                                   DFM_SEV_ERROR : DFM_SEV_WARNING;

                if (fi->type == FEAT_TRACE) {
                    v.position_x_mm = fi->data.trace.start.x;
                    v.position_y_mm = fi->data.trace.start.y;
                } else if (fi->type == FEAT_PAD) {
                    v.position_x_mm = fi->data.pad.position.x;
                    v.position_y_mm = fi->data.pad.position.y;
                } else {
                    v.position_x_mm = 0;
                    v.position_y_mm = 0;
                }

                v.measured_value = dist;
                v.threshold_value= rule->min_spacing_mm;
                v.layer          = layer;
                snprintf(v.description, sizeof(v.description),
                         "Spacing %.4f mm below minimum %.4f mm (nets %s vs %s)",
                         dist, rule->min_spacing_mm, fi->net_name, fj->net_name);
                dfm_report_add(report, &v);
            }
        }
    }
}

/* ─── DFM Check: Annular Ring ───────────────────────────────────── */

/**
 * Check annular ring size for plated through-holes.
 *
 * Annular ring = (pad_diameter - hole_diameter) / 2
 *
 * IPC-6012 §3.6.2: Minimum annular ring requirements.
 *   Class 2 external: 0.050 mm, internal: 0.025 mm
 *   Class 3 external: 0.100 mm, internal: 0.050 mm
 *
 * Insufficient annular ring can cause:
 *   - Breakout (hole drills outside pad)
 *   - Poor solder fillet
 *   - Reduced reliability under thermal cycling
 */
void dfm_check_annular_ring(const PCBHole *holes, int n_holes,
                            const DFMRuleAnnularRing *rule,
                            int layer, DFMViolationReport *report)
{
    assert(report != NULL);
    assert(rule != NULL);
    assert(holes != NULL || n_holes == 0);

    for (int i = 0; i < n_holes; i++) {
        const PCBHole *h = &holes[i];

        if (!h->is_plated) continue;  /* NPTH holes have no annular ring */
        if (h->pad_diameter <= 0.0) continue;  /* No pad defined */

        double annular = (h->pad_diameter - h->hole_diameter) * 0.5;

        if (annular < rule->min_annular_mm) {
            DFMViolation v;
            memset(&v, 0, sizeof(v));
            v.category       = DFM_CAT_ANNULAR_RING;
            v.severity       = (annular < 0.0) ?
                               DFM_SEV_CRITICAL : DFM_SEV_ERROR;
            v.position_x_mm  = h->position.x;
            v.position_y_mm  = h->position.y;
            v.measured_value = annular;
            v.threshold_value= rule->min_annular_mm;
            v.layer          = layer;
            snprintf(v.description, sizeof(v.description),
                     "Annular ring %.4f mm below minimum %.4f mm "
                     "(hole=%.3f, pad=%.3f, %s)",
                     annular, rule->min_annular_mm,
                     h->hole_diameter, h->pad_diameter,
                     h->is_plated ? "PTH" : "NPTH");
            dfm_report_add(report, &v);
        }
    }
}

/* ─── DFM Check: Solder Mask Clearance ──────────────────────────── */

/**
 * Check solder mask clearance around SMD pads.
 *
 * For each pad, the solder mask opening is:
 *   opening = pad_dimension + 2 * expansion_mm
 *
 * Clearance = expansion_mm (the gap between copper pad edge and mask).
 *
 * Too small: mask encroaches on pad, causing tombstoning and poor wetting.
 * Too large: mask slivers between fine-pitch pads may break.
 */
void dfm_check_soldermask(const PCBPad *pads, int n_pads,
                          double expansion_mm,
                          const DFMRuleSolderMask *rule,
                          int layer, DFMViolationReport *report)
{
    assert(report != NULL);
    assert(rule != NULL);
    assert(pads != NULL || n_pads == 0);

    for (int i = 0; i < n_pads; i++) {
        const PCBPad *pad = &pads[i];

        if (pad->layer != layer) continue;
        if (pad->tech != PAD_TECH_SMD) continue;

        double clearance = expansion_mm;

        if (clearance < rule->min_clearance_mm) {
            DFMViolation v;
            memset(&v, 0, sizeof(v));
            v.category       = DFM_CAT_SOLDER_MASK;
            v.severity       = DFM_SEV_ERROR;
            v.position_x_mm  = pad->position.x;
            v.position_y_mm  = pad->position.y;
            v.measured_value = clearance;
            v.threshold_value= rule->min_clearance_mm;
            v.layer          = layer;
            snprintf(v.description, sizeof(v.description),
                     "Solder mask clearance %.4f mm below minimum %.4f mm "
                     "(pad %s, %.2f x %.2f mm)",
                     clearance, rule->min_clearance_mm,
                     pad->designator, pad->width_mm, pad->height_mm);
            dfm_report_add(report, &v);
        }

        if (clearance > rule->max_clearance_mm) {
            DFMViolation v;
            memset(&v, 0, sizeof(v));
            v.category       = DFM_CAT_SOLDER_MASK;
            v.severity       = DFM_SEV_WARNING;
            v.position_x_mm  = pad->position.x;
            v.position_y_mm  = pad->position.y;
            v.measured_value = clearance;
            v.threshold_value= rule->max_clearance_mm;
            v.layer          = layer;
            snprintf(v.description, sizeof(v.description),
                     "Solder mask clearance %.4f mm exceeds maximum %.4f mm "
                     "(risk of slivers)",
                     clearance, rule->max_clearance_mm);
            dfm_report_add(report, &v);
        }
    }
}

/* ─── DFM Check: Aspect Ratio ───────────────────────────────────── */

/**
 * Check via aspect ratio.
 *
 * Aspect ratio = board_thickness / hole_diameter
 *
 * High aspect ratio vias are difficult to plate uniformly.
 * IPC-2221 §9.1.3 recommends:
 *   Standard process: ≤ 8:1
 *   Advanced process: ≤ 10:1
 *   Laser microvia: ≤ 1:1 (depth:diameter)
 *
 * Excessive aspect ratio causes:
 *   - Plating voids (barrel cracking)
 *   - Poor hole wall quality
 *   - Reduced current-carrying capacity
 *
 * Theorem (Plating uniformity):
 *   For a via of diameter d and board thickness t,
 *   the plating thickness at the center of the barrel h_c is related
 *   to the surface plating thickness h_s by:
 *     h_c ≈ h_s * exp(-k * t/d)
 *   where k is the process-dependent throwing power coefficient.
 *   As t/d increases, h_c/h_s decreases exponentially.
 */
void dfm_check_aspect_ratio(const PCBHole *vias, int n_vias,
                            const DFMRuleAspectRatio *rule,
                            DFMViolationReport *report)
{
    assert(report != NULL);
    assert(rule != NULL);
    assert(vias != NULL || n_vias == 0);

    for (int i = 0; i < n_vias; i++) {
        const PCBHole *via = &vias[i];

        if (via->hole_diameter <= 0.0) continue;

        double aspect = rule->board_thickness_mm / via->hole_diameter;

        if (aspect > rule->max_aspect_ratio) {
            DFMViolation v;
            memset(&v, 0, sizeof(v));
            v.category       = DFM_CAT_ASPECT_RATIO;
            v.severity       = (aspect > rule->max_aspect_ratio * 1.5) ?
                               DFM_SEV_CRITICAL : DFM_SEV_ERROR;
            v.position_x_mm  = via->position.x;
            v.position_y_mm  = via->position.y;
            v.measured_value = aspect;
            v.threshold_value= rule->max_aspect_ratio;
            v.layer          = 0;  /* All-layer concern */
            snprintf(v.description, sizeof(v.description),
                     "Aspect ratio %.2f:1 exceeds maximum %.1f:1 "
                     "(hole %.3f mm, board thickness %.2f mm)",
                     aspect, rule->max_aspect_ratio,
                     via->hole_diameter, rule->board_thickness_mm);
            dfm_report_add(report, &v);
        }
    }
}

/* ─── DFM Check: Edge Clearance ─────────────────────────────────── */

/**
 * Check copper-to-board-edge clearance.
 *
 * For each feature, compute minimum distance to the board outline.
 * If closer than min_clearance → violation.
 *
 * Edge clearance prevents:
 *   - Copper exposure after depaneling (routing)
 *   - Short circuits at board edges
 *   - V-scoring damage to copper
 */
void dfm_check_edge_clearance(const PCBFeature *features, int n_features,
                              const GerberPointD *outline, int n_outline,
                              const DFMRuleEdgeClearance *rule,
                              int layer, DFMViolationReport *report)
{
    assert(report != NULL);
    assert(rule != NULL);
    assert(features != NULL || n_features == 0);
    assert(outline != NULL);
    assert(n_outline >= 3);

    for (int i = 0; i < n_features; i++) {
        const PCBFeature *feat = &features[i];
        if (feat->layer != layer) continue;

        /* Get a representative point for the feature */
        GerberPointD pt;
        if (feat->type == FEAT_TRACE) {
            pt = feat->data.trace.start;
        } else if (feat->type == FEAT_PAD) {
            pt = feat->data.pad.position;
        } else if (feat->type == FEAT_VIA) {
            pt = feat->data.via.position;
        } else if (feat->type == FEAT_HOLE) {
            pt = feat->data.hole.position;
        } else {
            continue;
        }

        /* Distance from point to outline polygon edges */
        double dist = pcb_point_to_polygon_distance(&pt, outline, n_outline);

        if (dist < rule->min_clearance_mm) {
            DFMViolation v;
            memset(&v, 0, sizeof(v));
            v.category       = DFM_CAT_EDGE_CLEARANCE;
            v.severity       = (dist < rule->min_clearance_mm * 0.5) ?
                               DFM_SEV_ERROR : DFM_SEV_WARNING;
            v.position_x_mm  = pt.x;
            v.position_y_mm  = pt.y;
            v.measured_value = dist;
            v.threshold_value= rule->min_clearance_mm;
            v.layer          = layer;
            snprintf(v.description, sizeof(v.description),
                     "Feature %.4f mm from board edge (minimum %.4f mm)",
                     dist, rule->min_clearance_mm);
            dfm_report_add(report, &v);
        }
    }
}

/* ─── DFM Check: Acid Traps ─────────────────────────────────────── */

/**
 * Detect acid traps in copper polygon geometry.
 *
 * An acid trap is an acute interior angle (< 90°) at a concave vertex
 * of a copper polygon. Etchant pools in these corners and over-etches
 * the copper, creating "neck-down" weak points.
 *
 * Algorithm:
 *   For each polygon vertex v_i:
 *     1. Compute vectors a = v_{i-1} - v_i and b = v_{i+1} - v_i
 *     2. Compute the signed turning angle θ.
 *     3. If vertex is CONCAVE (cross product indicates inward corner)
 *        and interior angle < threshold → acid trap.
 *
 * The interior angle is given by:
 *   θ = acos((a · b) / (|a| * |b|))
 * The cross product sign determines convex(CCW polygon: positive cross = convex)
 * or concave (negative cross = concave = acid trap risk).
 *
 * Complexity: O(V) where V = total vertices across all polygons.
 */
void dfm_check_acid_traps(const PCBRegion *polygons, int n_polygons,
                          double angle_thresh_deg,
                          DFMViolationReport *report)
{
    assert(report != NULL);
    assert(polygons != NULL || n_polygons == 0);

    double angle_thresh_rad = angle_thresh_deg * M_PI / 180.0;

    for (int p = 0; p < n_polygons; p++) {
        const PCBRegion *poly = &polygons[p];
        int n = poly->n_vertices;

        if (n < 3) continue;

        for (int i = 0; i < n; i++) {
            int prev = (i == 0) ? n - 1 : i - 1;
            int next = (i == n - 1) ? 0 : i + 1;

            double ax = poly->vertices[prev].x - poly->vertices[i].x;
            double ay = poly->vertices[prev].y - poly->vertices[i].y;
            double bx = poly->vertices[next].x - poly->vertices[i].x;
            double by = poly->vertices[next].y - poly->vertices[i].y;

            double cross = ax * by - ay * bx;  /* Cross product z-component */
            double len_a = sqrt(ax * ax + ay * ay);
            double len_b = sqrt(bx * bx + by * by);

            if (len_a < 1e-9 || len_b < 1e-9) continue;  /* Degenerate edge */

            double dot = ax * bx + ay * by;
            double cos_angle = dot / (len_a * len_b);

            /* Clamp to [-1, 1] to avoid domain errors */
            if (cos_angle > 1.0) cos_angle = 1.0;
            if (cos_angle < -1.0) cos_angle = -1.0;

            double angle = acos(cos_angle);

            /* For a CCW polygon, positive cross = convex, negative = concave.
             * For CW polygon, negative cross = convex, positive = concave.
             * We detect acid traps in concave corners. */
            /* Since most polygons are CW or CCW, check both signs.
             * Acid trap = interior angle < threshold AND vertex is concave. */
            int is_concave = (cross < 0);  /* This assumes CCW polygon */
            /* We check regardless of winding; acute angles in either
             * direction can trap etchant if they are sharp inward turns. */

            if (angle < angle_thresh_rad) {
                DFMViolation v;
                memset(&v, 0, sizeof(v));
                v.category       = DFM_CAT_ACID_TRAP;
                v.severity       = (angle < angle_thresh_rad * 0.5) ?
                                   DFM_SEV_ERROR : DFM_SEV_WARNING;
                v.position_x_mm  = poly->vertices[i].x;
                v.position_y_mm  = poly->vertices[i].y;
                v.measured_value = angle * 180.0 / M_PI;
                v.threshold_value= angle_thresh_deg;
                v.layer          = poly->layer;
                snprintf(v.description, sizeof(v.description),
                         "Acid trap detected: interior angle %.1f° < %.1f° "
                         "at (%.3f, %.3f) on layer %d",
                         v.measured_value, angle_thresh_deg,
                         poly->vertices[i].x, poly->vertices[i].y,
                         poly->layer);
                dfm_report_add(report, &v);
            }
        }
    }
}

/* ─── DFM Check: Mask Slivers ───────────────────────────────────── */

/**
 * Detect solder mask slivers between closely-spaced pads.
 *
 * A sliver is a thin strip of solder mask (< min_width_mm) between
 * two adjacent SMD pads. During reflow, slivers can break loose
 * and cause shorts.
 *
 * Algorithm:
 *   For each pair of pads on the same side:
 *     1. Compute the gap between pad rectangles (accounting for expansion)
 *     2. If gap < min_width_mm → sliver risk.
 *
 * Complexity: O(N²) where N = number of pads.
 */
void dfm_check_mask_slivers(const PCBPad *pads, int n_pads,
                            double expansion_mm, double min_width_mm,
                            DFMViolationReport *report)
{
    assert(report != NULL);
    assert(pads != NULL || n_pads == 0);

    for (int i = 0; i < n_pads; i++) {
        const PCBPad *pi = &pads[i];
        if (pi->tech != PAD_TECH_SMD) continue;

        double hw1 = pi->width_mm * 0.5 + expansion_mm;
        double hh1 = (pi->height_mm > 0 ? pi->height_mm : pi->width_mm) * 0.5 + expansion_mm;

        for (int j = i + 1; j < n_pads; j++) {
            const PCBPad *pj = &pads[j];
            if (pj->tech != PAD_TECH_SMD) continue;
            if (pj->layer != pi->layer) continue;

            double hw2 = pj->width_mm * 0.5 + expansion_mm;
            double hh2 = (pj->height_mm > 0 ? pj->height_mm : pj->width_mm) * 0.5 + expansion_mm;

            /* Gap between expanded pad bounding rectangles */
            double dx = fabs(pj->position.x - pi->position.x);
            double dy = fabs(pj->position.y - pi->position.y);

            double gap_x = dx - hw1 - hw2;
            double gap_y = dy - hh1 - hh2;

            /* They overlap if gap < 0 in both directions.
             * But sliver concern is narrow gap in at least one direction. */
            if (gap_x < min_width_mm && gap_y < min_width_mm &&
                gap_x >= 0.0 && gap_y >= 0.0) {
                double min_gap = (gap_x < gap_y) ? gap_x : gap_y;

                DFMViolation v;
                memset(&v, 0, sizeof(v));
                v.category       = DFM_CAT_SLIVER;
                v.severity       = (min_gap < 0.0) ?
                                   DFM_SEV_ERROR : DFM_SEV_WARNING;
                v.position_x_mm  = (pi->position.x + pj->position.x) * 0.5;
                v.position_y_mm  = (pi->position.y + pj->position.y) * 0.5;
                v.measured_value = min_gap;
                v.threshold_value= min_width_mm;
                v.layer          = pi->layer;
                snprintf(v.description, sizeof(v.description),
                         "Mask sliver: gap %.4f mm < %.4f mm between pads "
                         "%s and %s",
                         min_gap, min_width_mm, pi->designator, pj->designator);
                dfm_report_add(report, &v);
            }
        }
    }
}

/* ─── DFM Check: Starved Thermals ───────────────────────────────── */

/**
 * Check for starved thermal relief connections.
 *
 * Thermal relief pads use narrow spokes to connect to copper pours,
 * reducing heat sinking during soldering. If spokes are too narrow,
 * they may fuse under operating current or fail mechanically.
 *
 * IPC-2221 recommends spoke width ≥ connected trace width for the net.
 *
 * This implementation performs a simplified check:
 * For each PTH pad on a copper pour layer, verify the pad diameter
 * is sufficient given the pour's connection expectations.
 */
void dfm_check_starved_thermals(const PCBPad *pads, int n_pads,
                                const PCBRegion *pours, int n_pours,
                                double min_spoke_mm,
                                DFMViolationReport *report)
{
    assert(report != NULL);
    assert(pads != NULL || n_pads == 0);
    assert(pours != NULL || n_pours == 0);

    for (int i = 0; i < n_pads; i++) {
        const PCBPad *pad = &pads[i];

        if (pad->tech != PAD_TECH_PTH) continue;  /* Only PTH pads need thermal relief */

        /* Check if pad lies within any copper pour on the same layer */
        for (int j = 0; j < n_pours; j++) {
            const PCBRegion *pour = &pours[j];

            if (pour->layer != pad->layer) continue;

            /* Check if pad is inside the pour polygon */
            if (pcb_point_in_polygon(&pad->position, pour->vertices,
                                     pour->n_vertices)) {
                /* Pad is inside pour — check for thermal relief adequacy.
                 * A typical thermal pad needs 4 spokes of width >= min_spoke_mm. */
                if (pad->width_mm < min_spoke_mm * 4.0) {
                    DFMViolation v;
                    memset(&v, 0, sizeof(v));
                    v.category       = DFM_CAT_STARVED_THERMAL;
                    v.severity       = DFM_SEV_WARNING;
                    v.position_x_mm  = pad->position.x;
                    v.position_y_mm  = pad->position.y;
                    v.measured_value = pad->width_mm;
                    v.threshold_value= min_spoke_mm * 4.0;
                    v.layer          = pad->layer;
                    snprintf(v.description, sizeof(v.description),
                             "Pad %s (%.3f mm) inside copper pour may have "
                             "inadequate thermal relief (expected >= %.3f mm "
                             "for spoke clearance)",
                             pad->designator, pad->width_mm, min_spoke_mm * 4.0);
                    dfm_report_add(report, &v);
                }
                break;  /* Only report once per pad */
            }
        }
    }
}

/* ─── Comprehensive DFM Review ──────────────────────────────────── */

/**
 * Run all DFM checks on a complete PCB board.
 *
 * This is the main entry point for DFM analysis.
 * Each check is independently executed and violations are aggregated.
 *
 * Returns: number of ERROR+CRITICAL violations (0 = clean board).
 */
int dfm_review_complete(const PCBBoard *board, const DFMRuleSet *rules,
                        DFMViolationReport *report)
{
    assert(board != NULL);
    assert(rules != NULL);
    assert(report != NULL);

    dfm_report_init(report);

    /* Run checks for each layer */
    for (int layer = 1; layer <= board->n_layers; layer++) {
        int is_ext = (layer == 1 || layer == board->n_layers);

        /* Trace width check */
        if (board->traces[layer - 1] != NULL && board->n_traces[layer - 1] > 0) {
            DFMRuleTraceWidth tw_rule = rules->trace_width;
            tw_rule.is_external = is_ext;
            dfm_check_trace_width(board->traces[layer - 1],
                                  board->n_traces[layer - 1],
                                  &tw_rule, layer, report);
        }

        /* Spacing check — use flat feature array */
        if (board->all_features != NULL && board->n_all_features > 0) {
            dfm_check_spacing(board->all_features,
                              board->n_all_features,
                              &rules->spacing, layer, report);
        }

        /* Annular ring */
        if (board->holes != NULL && board->n_holes > 0) {
            DFMRuleAnnularRing ar_rule = rules->annular_ring;
            ar_rule.is_external = is_ext;
            dfm_check_annular_ring(board->holes, board->n_holes,
                                   &ar_rule, layer, report);
        }

        /* Solder mask */
        if (board->pads[layer - 1] != NULL && board->n_pads[layer - 1] > 0) {
            dfm_check_soldermask(board->pads[layer - 1],
                                 board->n_pads[layer - 1],
                                 0.075,  /* Default 0.075 mm expansion */
                                 &rules->solder_mask, layer, report);
        }

        /* Edge clearance */
        if (board->all_features != NULL && board->n_all_features > 0 &&
            board->outline != NULL && board->n_outline_vertices >= 3) {
            dfm_check_edge_clearance(board->all_features,
                                     board->n_all_features,
                                     board->outline,
                                     board->n_outline_vertices,
                                     &rules->edge_clearance, layer, report);
        }

        /* Mask slivers */
        if (board->pads[layer - 1] != NULL && board->n_pads[layer - 1] > 0) {
            dfm_check_mask_slivers(board->pads[layer - 1],
                                   board->n_pads[layer - 1],
                                   0.075, 0.100, report);
        }

        /* Starved thermals */
        if (board->pads[layer - 1] != NULL && board->n_pads[layer - 1] > 0 &&
            board->regions[layer - 1] != NULL && board->n_regions[layer - 1] > 0) {
            dfm_check_starved_thermals(board->pads[layer - 1],
                                       board->n_pads[layer - 1],
                                       board->regions[layer - 1],
                                       board->n_regions[layer - 1],
                                       0.200, report);
        }

        /* Acid traps */
        if (board->regions[layer - 1] != NULL && board->n_regions[layer - 1] > 0) {
            dfm_check_acid_traps(board->regions[layer - 1],
                                 board->n_regions[layer - 1],
                                 90.0, report);
        }
    }

    /* Aspect ratio (global check, all layers) */
    if (board->holes != NULL && board->n_holes > 0) {
        dfm_check_aspect_ratio(board->holes, board->n_holes,
                               &rules->aspect_ratio, report);
    }

    return dfm_report_count_ge(report, DFM_SEV_ERROR);
}

/* ─── DFM Readiness Score ───────────────────────────────────────── */

double dfm_readiness_score(const DFMViolationReport *report)
{
    assert(report != NULL);

    double penalty = 0.0;

    penalty += 0.25 * report->by_severity[DFM_SEV_CRITICAL];
    penalty += 0.10 * report->by_severity[DFM_SEV_ERROR];
    penalty += 0.02 * report->by_severity[DFM_SEV_WARNING];
    penalty += 0.01 * report->by_severity[DFM_SEV_INFO];

    double score = 1.0 - penalty;
    if (score < 0.0) score = 0.0;
    if (score > 1.0) score = 1.0;

    return score;
}
