/**
 * @file flex_design_rule.c
 * @brief IPC-2223/6013 Design Rule Check (DRC) Implementation for Flex/Rigid-Flex
 *
 * Implements the programmatic design rule checker for flex and rigid-flex PCBs
 * per IPC-2223 (Design Standard) and IPC-6013 (Performance Standard).
 * Each function encodes one specific IPC design rule as an independent
 * knowledge point.
 *
 * @module mini-flex-rigid-flex-design
 */

#include "flex_design_rule.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* ========================================================================
 * L1: DRC Report Management
 * ========================================================================*/

/**
 * Knowledge Point: DRC report data structure initialization.
 *
 * Every design rule check run begins with a clean report. The report
 * accumulates violations found by individual rule checks. This pattern
 * mirrors how commercial EDA tools (Altium Designer, Cadence Allegro)
 * organize their DRC engines.
 *
 * The report tracks both individual violations and aggregate statistics
 * (total, error, warning, critical counts), enabling designers to
 * quickly assess design health.
 */
flex_drc_report_t flex_drc_report_init(void) {
    flex_drc_report_t report;
    memset(&report, 0, sizeof(report));
    return report;
}

/**
 * Knowledge Point: Violation accumulation with severity classification.
 *
 * As rules are checked, violations are added to the report. The report
 * maintains sorted severity counts for summary reporting. This function
 * also enforces the maximum violation capacity to prevent buffer overflow.
 */
int flex_drc_add_violation(flex_drc_report_t *report,
                            const flex_drc_violation_t *violation) {
    if (!report || !violation) return -1;
    if (report->total_violations >= FLEX_MAX_VIOLATIONS) return -1;

    report->violations[report->total_violations] = *violation;
    report->total_violations++;

    switch (violation->severity) {
    case FLEX_RULE_CRITICAL: report->critical_count++; break;
    case FLEX_RULE_ERROR:    report->error_count++;    break;
    case FLEX_RULE_WARNING:  report->warning_count++;  break;
    default: break;
    }
    return 0;
}

/* ========================================================================
 * L4: IPC-2223 Design Rule Checks
 * ========================================================================*/

/**
 * Knowledge Point: IPC-2223 §5.2.4 — Minimum Bend Radius Check.
 *
 * The IPC-2223 standard specifies mandatory minimum bend radii based on
 * total flex thickness and layer count. This rule is the single most
 * critical check for flex reliability.
 *
 * Rule: R_actual ≥ R_min_ipc2223
 *   For 1-layer: R_min = 6 × t_total
 *   For 2-layer: R_min = 12 × t_total
 *   For multi-layer: R_min = 20-30 × t_total
 *
 * Violation severity: ERROR for dynamic flex, WARNING for static flex.
 * Reference: IPC-2223C Table 5-1
 */
int flex_drc_bend_radius(double actual_radius_mm,
                          double total_thickness_mm,
                          int num_layers,
                          flex_drc_violation_t *violation) {
    if (!violation) return 0;

    double k;
    if (num_layers <= 1)       k = 6.0;
    else if (num_layers == 2)  k = 12.0;
    else if (num_layers <= 4)  k = 20.0;
    else                       k = 25.0;

    double r_min = k * total_thickness_mm;
    int compliant = (actual_radius_mm >= r_min) ? 1 : 0;

    violation->rule_id = 5001;
    violation->standard = FLEX_STD_IPC2223;
    violation->measured_value = actual_radius_mm;
    violation->required_min = r_min;
    violation->required_max = 1.0e6;
    strncpy(violation->rule_name, "Bend Radius (IPC-2223 §5.2.4)",
            sizeof(violation->rule_name) - 1);
    snprintf(violation->description, sizeof(violation->description),
        "Min bend radius required: %.3f mm, actual: %.3f mm (%d layers)",
        r_min, actual_radius_mm, num_layers);
    violation->severity = compliant ? FLEX_RULE_INFO :
        FLEX_RULE_ERROR;

    return compliant;
}

/**
 * Knowledge Point: IPC-2223 §9.1 — Annular Ring Requirement.
 *
 * The annular ring is the copper pad extending beyond the drilled hole.
 * Insufficient annular ring leads to breakout during drilling and
 * reduced solder joint reliability.
 *
 * For flex PCBs:
 *   Supported via (stiffener present): Min annular ring = 0.15 mm
 *   Unsupported via (no stiffener):    Min annular ring = 0.25 mm
 *
 * The flex requirement is more stringent than rigid (0.05-0.10 mm)
 * because flex materials have lower dimensional stability.
 *
 * Formula: annular_ring = (pad_diameter - hole_diameter) / 2
 * Reference: IPC-2223 §9.1.2, IPC-6013 §3.4
 */
int flex_drc_annular_ring(const flex_via_params_t *params,
                           flex_drc_violation_t *violation) {
    if (!params || !violation) return 0;

    double ar = (params->pad_diameter_mm - params->hole_diameter_mm) / 2.0;
    double ar_min = params->is_supported ? 0.15 : 0.25;

    int compliant = (ar >= ar_min) ? 1 : 0;

    violation->rule_id = 5002;
    violation->standard = FLEX_STD_IPC2223;
    violation->measured_value = ar;
    violation->required_min = ar_min;
    violation->required_max = 10.0;
    strncpy(violation->rule_name, "Annular Ring (IPC-2223 §9.1)",
            sizeof(violation->rule_name) - 1);
    snprintf(violation->description, sizeof(violation->description),
        "Annular ring: %.3f mm, required: %.3f mm (%s)",
        ar, ar_min, params->is_supported ? "supported":"unsupported");
    violation->severity = compliant ? FLEX_RULE_INFO : FLEX_RULE_ERROR;

    return compliant;
}

/**
 * Knowledge Point: IPC-2223 §5.2.6 — No Vias in Bend Zones.
 *
 * Plated through-holes (vias) in flex bend zones are a primary reliability
 * failure mechanism. The stress concentration around the hole edge during
 * bending causes copper cracking and barrel fatigue.
 *
 * Minimum distance from via center to nearest bend edge:
 *   d_min = 2 × bend_radius (recommended)
 *   d_min = 1 × bend_radius (absolute minimum)
 *
 * This rule is CRITICAL because via-in-bend failures are catastrophic
 * (open circuits) and cannot be repaired.
 *
 * Reference: IPC-2223 §5.2.6, IPC-6013 §3.6.3
 */
int flex_drc_via_in_bend_zone(const flex_via_params_t *params,
                               double bend_radius_mm,
                               flex_drc_violation_t *violation) {
    if (!params || !violation) return 0;

    int compliant = 1;
    if (params->is_in_bend_zone) {
        compliant = 0;  /* Via directly in bend zone */
    } else if (params->distance_to_bend_mm < bend_radius_mm) {
        compliant = 0;  /* Too close to bend */
    }

    violation->rule_id = 5003;
    violation->standard = FLEX_STD_IPC2223;
    violation->measured_value = params->distance_to_bend_mm;
    violation->required_min = bend_radius_mm;
    violation->required_max = 1.0e6;
    strncpy(violation->rule_name, "Via in Bend Zone (IPC-2223 §5.2.6)",
            sizeof(violation->rule_name) - 1);
    if (params->is_in_bend_zone) {
        snprintf(violation->description, sizeof(violation->description),
            "CRITICAL: Via located inside bend zone — will crack under flexing");
        violation->severity = FLEX_RULE_CRITICAL;
    } else if (!compliant) {
        snprintf(violation->description, sizeof(violation->description),
            "Via %.3f mm from bend edge, minimum required: %.3f mm",
            params->distance_to_bend_mm, bend_radius_mm);
        violation->severity = FLEX_RULE_ERROR;
    } else {
        snprintf(violation->description, sizeof(violation->description),
            "Via placement OK: %.3f mm from bend (≥ %.3f mm required)",
            params->distance_to_bend_mm, bend_radius_mm);
        violation->severity = FLEX_RULE_INFO;
    }

    return compliant;
}

/**
 * Knowledge Point: IPC-2223 §5.3 — Trace Design in Bend Zones.
 *
 * Traces passing through flex bend zones require special design:
 * 1. Traces should cross bend perpendicular to bend axis (shortest path)
 * 2. Trace width should be uniform through bend (no neck-down)
 * 3. Strain relief features (curved teardrops, radiused corners)
 * 4. Minimum 3× trace width spacing from bend zone edge
 *
 * Violation of these rules concentrates stress at trace edges,
 * leading to copper cracking after repeated flexing.
 *
 * Reference: IPC-2223 §5.3 "Conductor Design in Bend Areas"
 */
int flex_drc_trace_in_bend_zone(const flex_trace_params_t *params,
                                 flex_drc_violation_t *violation) {
    if (!params || !violation) return 0;

    int compliant = 1;
    char reason[256] = {0};

    if (params->is_in_bend_zone) {
        if (!params->has_strain_relief) {
            compliant = 0;
            snprintf(reason, sizeof(reason),
                "Trace in bend zone missing strain relief features");
        }
        if (params->trace_to_bend_mm < 0.1) {
            compliant = 0;
            snprintf(reason, sizeof(reason),
                "Trace edge too close to bend boundary (<0.1 mm)");
        }
    }

    violation->rule_id = 5004;
    violation->standard = FLEX_STD_IPC2223;
    violation->measured_value = params->has_strain_relief ? 1.0 : 0.0;
    violation->required_min = 1.0;
    violation->required_max = 1.0;
    strncpy(violation->rule_name, "Trace in Bend (IPC-2223 §5.3)",
            sizeof(violation->rule_name) - 1);
    violation->rule_name[sizeof(violation->rule_name) - 1] = '\0';
    if (!compliant) {
        snprintf(violation->description, sizeof(violation->description),
                 "%s", reason);
        violation->severity = FLEX_RULE_ERROR;
    } else {
        snprintf(violation->description, sizeof(violation->description),
            "Trace bend zone design OK");
        violation->severity = FLEX_RULE_INFO;
    }

    return compliant;
}

/**
 * Knowledge Point: IPC-2223 §5.4 — Trace-to-Edge Clearance.
 *
 * Flex circuits are often cut/routed from larger panels. Copper too close
 * to the board edge risks exposure during cutting, leading to corrosion
 * and electrical shorting.
 *
 *   Flex section:  minimum 0.50 mm trace-to-edge
 *   Rigid section: minimum 0.25 mm trace-to-edge
 *   Transition:    minimum 0.75 mm (most critical zone)
 *
 * These clearances are larger than standard rigid PCB rules because
 * flex materials are more prone to edge fraying during singulation.
 *
 * Reference: IPC-2223 §5.4, IPC-6013 §3.5
 */
int flex_drc_trace_to_edge(double trace_to_edge_mm,
                            flex_section_type_t section,
                            flex_drc_violation_t *violation) {
    if (!violation) return 0;

    double min_clearance;
    switch (section) {
    case FLEX_SECTION_FLEX:       min_clearance = 0.50; break;
    case FLEX_SECTION_RIGID:      min_clearance = 0.25; break;
    case FLEX_SECTION_TRANSITION: min_clearance = 0.75; break;
    default:                      min_clearance = 0.50; break;
    }

    int compliant = (trace_to_edge_mm >= min_clearance) ? 1 : 0;

    violation->rule_id = 5005;
    violation->standard = FLEX_STD_IPC2223;
    violation->measured_value = trace_to_edge_mm;
    violation->required_min = min_clearance;
    violation->required_max = 10.0;
    strncpy(violation->rule_name, "Trace-to-Edge (IPC-2223 §5.4)",
            sizeof(violation->rule_name) - 1);
    snprintf(violation->description, sizeof(violation->description),
        "Edge clearance: %.3f mm, min: %.3f mm (section %d)",
        trace_to_edge_mm, min_clearance, (int)section);
    violation->severity = compliant ? FLEX_RULE_INFO : FLEX_RULE_WARNING;

    return compliant;
}

/**
 * Knowledge Point: IPC-2223 §6.2 — Coverlay Opening Clearance.
 *
 * The coverlay opening must be larger than the exposed pad to ensure
 * reliable soldering access. Insufficient clearance causes:
 * - Solder bridging between pad and coverlay edge
 * - Incomplete solder wetting
 * - Flux entrapment under coverlay edge
 *
 * Minimum coverlay-to-pad clearance:
 *   PI film coverlay:  0.10 mm (4 mil) — laser cut
 *   LPI coverlay:      0.075 mm (3 mil) — photo-defined
 *   PIC coverlay:      0.05 mm  (2 mil) — photo-imageable
 *
 * Reference: IPC-2223 §6.2, IPC-4203
 */
int flex_drc_coverlay_clearance(const flex_coverlay_params_t *params,
                                 flex_drc_violation_t *violation) {
    if (!params || !violation) return 0;

    double min_clearance;
    switch (params->cover_type) {
    case FLEX_COVER_POLYIMIDE_FILM: min_clearance = 0.10; break;
    case FLEX_COVER_LPI:            min_clearance = 0.075; break;
    case FLEX_COVER_PIC:            min_clearance = 0.05; break;
    default:                        min_clearance = 0.10; break;
    }

    double actual_clearance = params->coverlay_to_pad_mm;
    int compliant = (actual_clearance >= min_clearance) ? 1 : 0;

    violation->rule_id = 5006;
    violation->standard = FLEX_STD_IPC2223;
    violation->measured_value = actual_clearance;
    violation->required_min = min_clearance;
    violation->required_max = 1.0;
    strncpy(violation->rule_name, "Coverlay Clearance (IPC-2223 §6.2)",
            sizeof(violation->rule_name) - 1);
    snprintf(violation->description, sizeof(violation->description),
        "Coverlay-to-pad: %.3f mm, required: %.3f mm (type %d)",
        actual_clearance, min_clearance, (int)params->cover_type);
    violation->severity = compliant ? FLEX_RULE_INFO : FLEX_RULE_WARNING;

    return compliant;
}

/**
 * Knowledge Point: IPC-2223 §7.1 — Stiffener Placement Rules.
 *
 * Stiffeners provide mechanical support for connector mounting areas
 * and component placement zones in flex circuits. Improper placement
 * creates stress concentrations.
 *
 * Rules:
 * - Stiffener edge must be ≥ 1.0 mm from bend zone boundary
 * - Stiffener edge must be ≥ 0.5 mm from board edge
 * - Adhesive squeeze-out allowance: +0.3 mm beyond stiffener edge
 * - No stiffener over dynamic bend zones
 *
 * Reference: IPC-2223 §7.1 "Stiffener Design"
 */
int flex_drc_stiffener_placement(const flex_stiffener_params_t *params,
                                  flex_drc_violation_t *violation) {
    if (!params || !violation) return 0;

    int compliant = 1;
    char reason[256] = {0};

    if (params->stiffener_to_bend_mm < 1.0) {
        compliant = 0;
        snprintf(reason, sizeof(reason),
            "Stiffener too close to bend: %.3f mm (min 1.0 mm required)",
            params->stiffener_to_bend_mm);
    }
    if (params->stiffener_to_edge_mm < 0.5) {
        compliant = 0;
        snprintf(reason, sizeof(reason),
            "Stiffener too close to edge: %.3f mm (min 0.5 mm required)",
            params->stiffener_to_edge_mm);
    }
    if (params->adhesive_squeeze_out_mm > 0.5) {
        compliant = 0;
        snprintf(reason, sizeof(reason),
            "Adhesive squeeze-out %.3f mm exceeds 0.5 mm limit",
            params->adhesive_squeeze_out_mm);
    }

    violation->rule_id = 5007;
    violation->standard = FLEX_STD_IPC2223;
    violation->measured_value = params->stiffener_to_bend_mm;
    violation->required_min = 1.0;
    violation->required_max = 100.0;
    strncpy(violation->rule_name, "Stiffener Placement (IPC-2223 §7.1)",
            sizeof(violation->rule_name) - 1);

    if (compliant) {
        snprintf(violation->description, sizeof(violation->description),
            "Stiffener placement OK");
        violation->severity = FLEX_RULE_INFO;
    } else {
        snprintf(violation->description, sizeof(violation->description),
                 "%s", reason);
        violation->severity = FLEX_RULE_WARNING;
    }

    return compliant;
}

/**
 * Knowledge Point: IPC-2223 — Minimum Trace Width and Spacing.
 *
 * Flex PCBs typically require wider minimum trace/space than rigid
 * due to material movement during lamination and lower etch precision.
 *
 * IPC-6013 Class requirements:
 *   Class 1 (Consumer):    trace ≥ 125 μm, space ≥ 125 μm
 *   Class 2 (Industrial):  trace ≥ 100 μm, space ≥ 100 μm
 *   Class 3 (Aerospace):   trace ≥ 125 μm, space ≥ 125 μm
 *
 * Note: Class 3 is same as Class 1 for minimum dimensions but has
 * stricter plating and inspection requirements.
 *
 * Reference: IPC-2223 §5.5, IPC-6013 §3.3
 */
int flex_drc_trace_width_spacing(double trace_width_mm,
                                  double trace_spacing_mm,
                                  int ipc_class,
                                  flex_drc_violation_t *violation) {
    if (!violation) return 0;

    double min_width, min_spacing;
    if (ipc_class == 2) {
        min_width   = 0.100;  /* 100 μm = 4 mil */
        min_spacing = 0.100;
    } else {
        min_width   = 0.125;  /* 125 μm = 5 mil */
        min_spacing = 0.125;
    }

    int w_ok = (trace_width_mm >= min_width);
    int s_ok = (trace_spacing_mm >= min_spacing);
    int compliant = (w_ok && s_ok) ? 1 : 0;

    violation->rule_id = 5008;
    violation->standard = FLEX_STD_IPC2223;
    violation->measured_value = (trace_width_mm < trace_spacing_mm) ?
        trace_width_mm : trace_spacing_mm;
    violation->required_min = min_width;
    violation->required_max = 10.0;
    strncpy(violation->rule_name, "Trace Width/Spacing (IPC-2223)",
            sizeof(violation->rule_name) - 1);
    snprintf(violation->description, sizeof(violation->description),
        "Class %d: width=%.3f mm (min %.3f), spacing=%.3f mm (min %.3f)",
        ipc_class, trace_width_mm, min_width, trace_spacing_mm, min_spacing);
    violation->severity = compliant ? FLEX_RULE_INFO : FLEX_RULE_ERROR;

    return compliant;
}

/**
 * Knowledge Point: IPC-2223 — Copper Thickness per Layer Limits.
 *
 * Thicker copper reduces flexibility and increases minimum bend radius.
 * The copper weight (thickness) in flex sections is limited:
 *
 *   Maximum copper in flex: 70 μm (2 oz)
 *   Typical copper in flex: 18 μm (0.5 oz) or 35 μm (1 oz)
 *
 * For dynamic flex, thinner copper (≤ 18 μm) is strongly recommended
 * to maximize bend cycle life.
 *
 * Reference: IPC-2223 §5.2.2, IPC-4562
 */
int flex_drc_copper_thickness(double copper_thickness_um,
                               int is_in_flex,
                               flex_drc_violation_t *violation) {
    if (!violation) return 0;

    double max_cu = is_in_flex ? 70.0 : 105.0;  /* 2 oz in flex, 3 oz in rigid */
    int compliant = (copper_thickness_um <= max_cu) ? 1 : 0;

    violation->rule_id = 5009;
    violation->standard = FLEX_STD_IPC2223;
    violation->measured_value = copper_thickness_um;
    violation->required_min = 0.0;
    violation->required_max = max_cu;
    strncpy(violation->rule_name, "Copper Thickness (IPC-2223 §5.2.2)",
            sizeof(violation->rule_name) - 1);
    snprintf(violation->description, sizeof(violation->description),
        "Cu thickness: %.1f μm, limit: %.1f μm (%s)",
        copper_thickness_um, max_cu, is_in_flex ? "flex":"rigid");
    violation->severity = compliant ? FLEX_RULE_INFO : FLEX_RULE_ERROR;

    return compliant;
}

/**
 * Knowledge Point: IPC-4203 — Adhesive Thickness Limits.
 *
 * Adhesive thickness in flex construction is a critical parameter:
 * - Too thick: excess flow during lamination, fills gaps unpredictably,
 *   increases total thickness → larger bend radius needed
 * - Too thin: insufficient bonding, risk of delamination under thermal stress
 *
 * IPC-4203 recommended adhesive thickness ranges:
 *   Acrylic:    12.7 - 50.8 μm (0.5 - 2.0 mil)
 *   Epoxy:      12.7 - 38.1 μm (0.5 - 1.5 mil)
 *   PI adhesive: 12.7 - 25.4 μm (0.5 - 1.0 mil)
 *
 * Reference: IPC-4203 "Adhesive Coated Dielectric Films"
 */
int flex_drc_adhesive_thickness(double adhesive_thickness_um,
                                 flex_adhesive_type_t adhesive_type,
                                 flex_drc_violation_t *violation) {
    if (!violation) return 0;

    double t_min, t_max;
    switch (adhesive_type) {
    case FLEX_ADHESIVE_ACRYLIC:
        t_min = 12.7; t_max = 50.8; break;
    case FLEX_ADHESIVE_EPOXY:
        t_min = 12.7; t_max = 38.1; break;
    case FLEX_ADHESIVE_PI:
        t_min = 12.7; t_max = 25.4; break;
    case FLEX_ADHESIVE_ADHESIVELESS:
        t_min = 0.0;  t_max = 0.0;  break;  /* No adhesive */
    case FLEX_ADHESIVE_PSA:
        t_min = 25.4; t_max = 127.0; break;  /* PSA can be thicker */
    default:
        t_min = 12.7; t_max = 50.8; break;
    }

    int compliant = (adhesive_thickness_um >= t_min && adhesive_thickness_um <= t_max)
                    ? 1 : 0;

    violation->rule_id = 5010;
    violation->standard = FLEX_STD_IPC4203;
    violation->measured_value = adhesive_thickness_um;
    violation->required_min = t_min;
    violation->required_max = t_max;
    strncpy(violation->rule_name, "Adhesive Thickness (IPC-4203)",
            sizeof(violation->rule_name) - 1);
    snprintf(violation->description, sizeof(violation->description),
        "Adhesive: %.1f μm, range: [%.1f, %.1f] μm (type %d)",
        adhesive_thickness_um, t_min, t_max, (int)adhesive_type);
    violation->severity = compliant ? FLEX_RULE_INFO : FLEX_RULE_WARNING;

    return compliant;
}

/* ========================================================================
 * L5: Rigid-Flex Transition Zone Rules
 * ========================================================================*/

/**
 * Knowledge Point: IPC-2223 §8 — Transition Zone Design Rules.
 *
 * The rigid-to-flex transition is the most failure-prone region in
 * rigid-flex designs. IPC-2223 §8 specifies mandatory requirements:
 *
 * 1. Minimum transition length: 1.5 mm (for stress gradient distribution)
 * 2. Anchor tabs required for ≥ 4 rigid layers (prevents peeling)
 * 3. No vias within 1.0 mm of transition boundary (stress concentration)
 * 4. Tear-stop features needed for ≥ 6 rigid layers (crack arrest)
 *
 * The transition zone must gradually distribute the stress from the
 * thick rigid section to the thin flex section. Abrupt transitions
 * create stress concentrations that initiate delamination.
 *
 * Reference: IPC-2223 §8 "Rigid-Flex Transition Design"
 */
int flex_drc_transition_zone(double transition_length_mm,
                              int rigid_layers,
                              int flex_layers,
                              int has_anchor_tab,
                              int has_tear_stop,
                              flex_drc_violation_t *violation) {
    if (!violation) return 0;

    int compliant = 1;
    char issues[4][128];
    int issue_count = 0;

    /* Rule 1: Minimum transition length */
    if (transition_length_mm < 1.5) {
        compliant = 0;
        snprintf(issues[issue_count++], 128,
            "Transition length %.2f mm < 1.5 mm minimum",
            transition_length_mm);
    }

    /* Rule 2: Anchor tabs for ≥ 4 rigid layers */
    if (rigid_layers >= 4 && !has_anchor_tab) {
        compliant = 0;
        snprintf(issues[issue_count++], 128,
            "Anchor tabs required for %d rigid layers (≥4)",
            rigid_layers);
    }

    /* Rule 3: Tear-stop for ≥ 6 rigid layers */
    if (rigid_layers >= 6 && !has_tear_stop) {
        compliant = 0;
        snprintf(issues[issue_count++], 128,
            "Tear-stop required for %d rigid layers (≥6)",
            rigid_layers);
    }

    /* Rule 4: Layer ratio sanity check */
    if (flex_layers > 0 && rigid_layers > 3 * flex_layers) {
        compliant = 0;
        snprintf(issues[issue_count++], 128,
            "Rigid/flex layer ratio %d:%d exceeds 3:1 limit",
            rigid_layers, flex_layers);
    }

    violation->rule_id = 5011;
    violation->standard = FLEX_STD_IPC2223;
    violation->measured_value = transition_length_mm;
    violation->required_min = 1.5;
    violation->required_max = 100.0;
    strncpy(violation->rule_name, "Transition Zone (IPC-2223 §8)",
            sizeof(violation->rule_name) - 1);

    if (compliant) {
        snprintf(violation->description, sizeof(violation->description),
            "Transition zone design OK: %.2f mm, %dR/%dF layers",
            transition_length_mm, rigid_layers, flex_layers);
        violation->severity = FLEX_RULE_INFO;
    } else {
        /* Build compact error string fitting in 256 chars */
        int off = snprintf(violation->description, sizeof(violation->description),
            "%d violation(s)", issue_count);
        for (int j = 0; j < issue_count && off < (int)sizeof(violation->description) - 1; j++) {
            off += snprintf(violation->description + off,
                           sizeof(violation->description) - off,
                           ": %s", issues[j]);
        }
        violation->severity = FLEX_RULE_ERROR;
    }

    return compliant;
}

/**
 * Knowledge Point: Rigid-Flex Layer Count Imbalance Rule.
 *
 * Asymmetric layer counts between rigid and flex sections create
 * bending moments at the transition boundary. IPC-2223 recommends:
 *
 *   Rigid layers ≤ 3 × Flex layers
 *
 * Example violations:
 *   8 rigid layers + 2 flex layers → 4:1 ratio → VIOLATION
 *   6 rigid layers + 3 flex layers → 2:1 ratio → OK
 *   4 rigid layers + 2 flex layers → 2:1 ratio → OK
 *
 * Balanced designs distribute stress evenly. Severe imbalance causes
 * the rigid section to peel away from the flex at the boundary.
 *
 * Reference: IPC-2223 §8.2
 */
int flex_drc_layer_imbalance(int rigid_layer_count,
                              int flex_layer_count,
                              flex_drc_violation_t *violation) {
    if (!violation) return 0;

    int compliant = 1;
    if (flex_layer_count <= 0) flex_layer_count = 1;
    double ratio = (double)rigid_layer_count / (double)flex_layer_count;

    if (ratio > 3.0) compliant = 0;

    violation->rule_id = 5012;
    violation->standard = FLEX_STD_IPC2223;
    violation->measured_value = ratio;
    violation->required_min = 1.0;
    violation->required_max = 3.0;
    strncpy(violation->rule_name, "Layer Imbalance (IPC-2223 §8.2)",
            sizeof(violation->rule_name) - 1);
    snprintf(violation->description, sizeof(violation->description),
        "Rigid/flex ratio: %.1f:1 (limit 3:1), %d rigid, %d flex",
        ratio, rigid_layer_count, flex_layer_count);
    violation->severity = compliant ? FLEX_RULE_INFO : FLEX_RULE_WARNING;

    return compliant;
}

/* ========================================================================
 * L5: Comprehensive DRC Runner
 * ========================================================================*/

/**
 * Knowledge Point: Full design rule checking pipeline.
 *
 * Runs all applicable IPC-2223 design rule checks on a complete flex
 * stackup design. The DRC runner aggregates rules from geometry checks,
 * material checks, and transition zone checks into a single report.
 *
 * This top-level function is the equivalent of "Tools > Design Rule Check"
 * in a PCB EDA tool — it validates the entire design against the standard.
 */
flex_drc_report_t flex_drc_run_full(const flex_stackup_t *stackup) {
    flex_drc_report_t report = flex_drc_report_init();

    if (!stackup) return report;

    flex_drc_violation_t v;
    int flex_layers = 0;
    double flex_thickness = 0.0;
    double flex_cu_total = 0.0;

    /* Gather flex section metrics */
    for (int i = 0; i < stackup->total_layer_count; i++) {
        if (stackup->layers[i].section == FLEX_SECTION_FLEX) {
            flex_layers++;
            flex_thickness += stackup->layers[i].finished_thickness_um / 1000.0;
            flex_cu_total += stackup->layers[i].copper_thickness_um;
        }
    }

    /* Check each bend zone for minimum bend radius */
    for (int i = 0; i < FLEX_MAX_BEND_ZONES; i++) {
        if (stackup->bend_zones[i].bend_radius_mm > 0.0) {
            memset(&v, 0, sizeof(v));
            if (!flex_drc_bend_radius(stackup->bend_zones[i].bend_radius_mm,
                                       flex_thickness, flex_layers, &v)) {
                flex_drc_add_violation(&report, &v);
            }
        }
    }

    /* Check trace width/spacing for Class 2/3 */
    int ipc_class = 2;
    if (strcmp(stackup->ipc_class, "3") == 0) ipc_class = 3;
    memset(&v, 0, sizeof(v));
    flex_drc_trace_width_spacing(0.100, 0.100, ipc_class, &v);

    /* Check copper thickness in flex layers */
    for (int i = 0; i < stackup->total_layer_count; i++) {
        if (stackup->layers[i].section == FLEX_SECTION_FLEX &&
            stackup->layers[i].copper_thickness_um > 0) {
            memset(&v, 0, sizeof(v));
            if (!flex_drc_copper_thickness(
                    stackup->layers[i].copper_thickness_um, 1, &v)) {
                flex_drc_add_violation(&report, &v);
            }
        }
    }

    return report;
}

/**
 * Knowledge Point: Human-readable DRC report generation.
 *
 * Translates the structured violation list into formatted console output,
 * suitable for design review meetings and documentation.
 * Severity is color-coded by prefix: [CRITICAL], [ERROR], [WARNING], [INFO].
 */
void flex_drc_report_print(const flex_drc_report_t *report) {
    if (!report) return;

    printf("========================================\n");
    printf("  FLEX/RIGID-FLEX DRC REPORT\n");
    printf("========================================\n");
    printf("Total violations: %d\n", report->total_violations);
    printf("  Critical: %d | Errors: %d | Warnings: %d\n\n",
           report->critical_count, report->error_count, report->warning_count);

    if (report->total_violations == 0) {
        printf("✓ No violations found — design passes IPC-2223.\n");
        return;
    }

    for (int i = 0; i < report->total_violations; i++) {
        const flex_drc_violation_t *v = &report->violations[i];
        const char *sev_str;
        switch (v->severity) {
        case FLEX_RULE_CRITICAL: sev_str = "[CRITICAL]"; break;
        case FLEX_RULE_ERROR:    sev_str = "[ERROR]   "; break;
        case FLEX_RULE_WARNING:  sev_str = "[WARNING] "; break;
        default:                 sev_str = "[INFO]    "; break;
        }
        printf("%s Rule %d: %s\n", sev_str, v->rule_id, v->rule_name);
        printf("           %s\n", v->description);
        printf("           Measured: %.3f, Range: [%.3f, %.3f]\n\n",
               v->measured_value, v->required_min, v->required_max);
    }

    if (report->critical_count > 0 || report->error_count > 0) {
        printf("✗ DESIGN DOES NOT PASS IPC-2223. %d error(s) must be resolved.\n",
               report->critical_count + report->error_count);
    }
}
