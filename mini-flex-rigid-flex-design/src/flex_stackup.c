/**
 * @file flex_stackup.c
 * @brief Flex/Rigid-Flex Layer Stackup Design Implementation
 *
 * Implements the stackup construction, analysis, and validation algorithms.
 * Each function represents an independent engineering operation on the
 * layer stackup data structure.
 *
 * @module mini-flex-rigid-flex-design
 */

#include "flex_stackup.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Helper: convert dielectric type to base modulus (MPa) */
static double dielectric_modulus(flex_dielectric_type_t type) {
    switch (type) {
    case FLEX_DIELEC_POLYIMIDE:  return 2500.0;
    case FLEX_DIELEC_LCP:        return 3000.0;
    case FLEX_DIELEC_PET:        return 3800.0;
    case FLEX_DIELEC_PEN:        return 3200.0;
    case FLEX_DIELEC_PTFE:       return 500.0;
    case FLEX_DIELEC_MPI:        return 2800.0;
    case FLEX_DIELEC_EPOXY_FLEX: return 1800.0;
    default:                     return 2500.0;
    }
}

/**
 * Knowledge Point: Stackup initialization.
 * A new rigid-flex design starts with zero layers and must be built up
 * by adding layers, then assigning them to rigid or flex sections.
 * This pattern (build first, assign later) mirrors how EDA tools
 * (Altium, Cadence Allegro) define rigid-flex stackups.
 */
flex_stackup_t flex_stackup_init(const char *design_name) {
    flex_stackup_t s;
    memset(&s, 0, sizeof(s));
    if (design_name) {
        strncpy(s.design_name, design_name, sizeof(s.design_name) - 1);
    }
    strncpy(s.ipc_class, "2", sizeof(s.ipc_class) - 1);
    s.is_symmetric = 0;
    s.imbalance_warning = 0;
    return s;
}

/**
 * Knowledge Point: Adding a signal layer.
 *
 * Signal layers carry traces. In flex/rigid-flex, each signal layer
 * consists of: copper foil + dielectric substrate + optional adhesive.
 * Coverlay is applied to outer layers for protection.
 *
 * For adhesiveless construction, adhesive_thickness = 0 and
 * adhesive_type = FLEX_ADHESIVE_ADHESIVELESS.
 */
int flex_stackup_add_signal_layer(flex_stackup_t *stackup,
                                   const char *layer_name,
                                   double copper_thickness_um,
                                   flex_copper_type_t copper_type,
                                   flex_dielectric_type_t dielectric_type,
                                   double dielectric_thickness_um) {
    if (!stackup || stackup->total_layer_count >= FLEX_MAX_LAYERS)
        return -1;

    int idx = stackup->total_layer_count;
    flex_layer_t *layer = &stackup->layers[idx];

    memset(layer, 0, sizeof(flex_layer_t));
    layer->layer_index            = idx + 1;  /* 1-based */
    layer->layer_type             = FLEX_LAYER_SIGNAL;
    layer->dielectric             = dielectric_type;
    layer->copper                 = copper_type;
    layer->adhesive               = FLEX_ADHESIVE_ADHESIVELESS; /* Default */
    layer->copper_thickness_um    = copper_thickness_um;
    layer->dielectric_thickness_um = dielectric_thickness_um;
    layer->adhesive_thickness_um  = 0.0;
    layer->finished_thickness_um  = copper_thickness_um + dielectric_thickness_um;
    layer->section                = FLEX_SECTION_FLEX;  /* Default: flex */
    layer->is_coverlay_present    = (copper_thickness_um > 0) ? 1 : 0;
    layer->cover_type             = FLEX_COVER_POLYIMIDE_FILM;
    layer->cover_thickness_um     = 25.0;  /* Default 25μm coverlay */
    if (layer_name)
        strncpy(layer->layer_name, layer_name, sizeof(layer->layer_name) - 1);

    stackup->total_layer_count++;
    stackup->flex_section_count = 1;  /* At least one flex section */
    return idx + 1;
}

/**
 * Knowledge Point: Adding a plane layer.
 *
 * Reference planes provide return current paths and EMI shielding.
 * In flex designs, planes can be solid copper pours or hatched patterns
 * (cross-hatching improves flexibility). This function adds a solid plane;
 * hatching is a post-processing step in layout.
 */
int flex_stackup_add_plane_layer(flex_stackup_t *stackup,
                                  const char *layer_name,
                                  double copper_thickness_um,
                                  flex_copper_type_t copper_type) {
    if (!stackup || stackup->total_layer_count >= FLEX_MAX_LAYERS)
        return -1;

    int idx = stackup->total_layer_count;
    flex_layer_t *layer = &stackup->layers[idx];

    memset(layer, 0, sizeof(flex_layer_t));
    layer->layer_index           = idx + 1;
    layer->layer_type            = FLEX_LAYER_PLANE;
    layer->dielectric            = FLEX_DIELEC_POLYIMIDE;
    layer->copper                = copper_type;
    layer->copper_thickness_um   = copper_thickness_um;
    layer->dielectric_thickness_um = 25.0;  /* Thin dielectric for plane */
    layer->finished_thickness_um = copper_thickness_um + 25.0;
    layer->section               = FLEX_SECTION_FLEX;
    if (layer_name)
        strncpy(layer->layer_name, layer_name, sizeof(layer->layer_name) - 1);

    stackup->total_layer_count++;
    return idx + 1;
}

int flex_stackup_set_rigid_only(flex_stackup_t *stackup, int layer_index) {
    if (!stackup || layer_index < 1 || layer_index > stackup->total_layer_count)
        return -1;
    stackup->layers[layer_index - 1].section = FLEX_SECTION_RIGID;
    stackup->rigid_section_count = 1;
    return 0;
}

int flex_stackup_set_flex_through(flex_stackup_t *stackup, int layer_index) {
    if (!stackup || layer_index < 1 || layer_index > stackup->total_layer_count)
        return -1;
    stackup->layers[layer_index - 1].section = FLEX_SECTION_FLEX;
    return 0;
}

int flex_stackup_add_bend_zone(flex_stackup_t *stackup,
                                double start_x, double start_y,
                                double end_x, double end_y,
                                double bend_radius_mm, double bend_angle_deg,
                                int dynamic) {
    if (!stackup || bend_radius_mm <= 0.0) return -1;

    for (int i = 0; i < FLEX_MAX_BEND_ZONES; i++) {
        if (stackup->bend_zones[i].bend_radius_mm == 0.0) {
            flex_bend_zone_t *bz = &stackup->bend_zones[i];
            bz->start_x_mm = start_x;
            bz->start_y_mm = start_y;
            bz->end_x_mm = end_x;
            bz->end_y_mm = end_y;
            bz->bend_radius_mm = bend_radius_mm;
            bz->bend_angle_deg = bend_angle_deg;
            bz->is_dynamic_flex = dynamic;
            bz->expected_cycles = dynamic ? 100000.0 : 1.0;
            return i;
        }
    }
    return -1;
}

int flex_stackup_add_stiffener(flex_stackup_t *stackup,
                                flex_stiffener_type_t type,
                                double thickness_mm,
                                int bonded) {
    if (!stackup || thickness_mm <= 0.0) return -1;

    for (int i = 0; i < FLEX_MAX_STIFFENERS; i++) {
        if (stackup->stiffeners[i].thickness_mm == 0.0) {
            stackup->stiffeners[i].type = type;
            stackup->stiffeners[i].thickness_mm = thickness_mm;
            stackup->stiffeners[i].is_bonded = bonded;
            return i;
        }
    }
    return -1;
}

/* ========================================================================
 * L3: Mathematical Analysis of Stackup
 * ========================================================================*/

/**
 * Knowledge Point: Flex section thickness accumulation.
 *
 * Only layers marked FLEX_SECTION_FLEX contribute to flex section thickness.
 * Rigid-only layers are mechanically absent in the bend zone.
 *
 * This is the fundamental design parameter that determines minimum bend radius:
 *   R_min ∝ t_flex (IPC-2223: more layers → larger minimum radius)
 */
double flex_stackup_flex_thickness(const flex_stackup_t *stackup) {
    if (!stackup) return 0.0;
    double total = 0.0;
    for (int i = 0; i < stackup->total_layer_count; i++) {
        if (stackup->layers[i].section == FLEX_SECTION_FLEX) {
            total += stackup->layers[i].finished_thickness_um;
        }
    }
    return total / 1000.0;  /* Convert μm → mm */
}

double flex_stackup_flex_copper_total(const flex_stackup_t *stackup) {
    if (!stackup) return 0.0;
    double total = 0.0;
    for (int i = 0; i < stackup->total_layer_count; i++) {
        if (stackup->layers[i].section == FLEX_SECTION_FLEX) {
            total += stackup->layers[i].copper_thickness_um;
        }
    }
    return total;
}

int flex_stackup_flex_layer_count(const flex_stackup_t *stackup) {
    if (!stackup) return 0;
    int count = 0;
    for (int i = 0; i < stackup->total_layer_count; i++) {
        if (stackup->layers[i].section == FLEX_SECTION_FLEX) count++;
    }
    return count;
}

/**
 * Knowledge Point: Neutral axis calculation for multilayer composite beam.
 *
 * When a beam bends, there is a plane (the neutral axis) that experiences
 * zero strain — material above is in tension, below is in compression.
 *
 * For a homogeneous beam, NA is at the geometric center.
 * For a multilayer composite (flex PCB), each layer has different E_i,
 * shifting the NA toward stiffer layers:
 *
 *   y_NA = Σ(E_i * t_i * y_i) / Σ(E_i * t_i)
 *
 * where y_i = distance from reference (bottom surface) to center of layer i.
 *
 * Design implication: Place the copper layers symmetrically about the NA
 * to minimize copper strain during bending. IPC-2223 recommends keeping
 * copper within 25% of thickness from the NA for dynamic flex.
 *
 * Reference: Timoshenko, "Strength of Materials", Part I, Chapter VI
 */
int flex_stackup_neutral_axis(const flex_stackup_t *stackup,
                               double *neutral_offset_mm) {
    if (!stackup || !neutral_offset_mm) return -1;

    double sum_ei_ti = 0.0;
    double sum_ei_ti_yi = 0.0;
    double y_current = 0.0;  /* Distance from bottom, building up */

    for (int i = 0; i < stackup->total_layer_count; i++) {
        if (stackup->layers[i].section != FLEX_SECTION_FLEX) continue;

        double t_mm = stackup->layers[i].finished_thickness_um / 1000.0;
        if (t_mm <= 0.0) continue;

        double ei = dielectric_modulus(stackup->layers[i].dielectric);
        double y_center = y_current + t_mm / 2.0;

        sum_ei_ti += ei * t_mm;
        sum_ei_ti_yi += ei * t_mm * y_center;
        y_current += t_mm;
    }

    if (sum_ei_ti <= 0.0) return -1;

    *neutral_offset_mm = sum_ei_ti_yi / sum_ei_ti;
    return 0;
}

/**
 * Knowledge Point: Flexural rigidity (bending stiffness).
 *
 * Flexural rigidity D determines how much force is needed to bend the flex.
 * Using the parallel axis theorem for each layer:
 *
 *   D = Σ E_i * (b*t_i³/12 + b*t_i*d_i²)    per unit width (b=1mm)
 *
 * where d_i = distance from layer center to neutral axis.
 *
 * Higher D means:
 * - More force needed to bend (affects actuator sizing)
 * - More springback after forming
 * - Lower strain for a given radius (positive for reliability)
 *
 * This is the flex equivalent of "board stiffness" in rigid PCB design.
 */
double flex_stackup_flexural_rigidity(const flex_stackup_t *stackup) {
    if (!stackup) return 0.0;

    double neutral = 0.0;
    if (flex_stackup_neutral_axis(stackup, &neutral) != 0) return 0.0;

    double d_total = 0.0;
    double y_current = 0.0;
    const double b = 1.0;  /* Per mm width */

    for (int i = 0; i < stackup->total_layer_count; i++) {
        if (stackup->layers[i].section != FLEX_SECTION_FLEX) continue;

        double t_mm = stackup->layers[i].finished_thickness_um / 1000.0;
        if (t_mm <= 0.0) continue;

        double ei = dielectric_modulus(stackup->layers[i].dielectric);
        double y_center = y_current + t_mm / 2.0;
        double d_i = y_center - neutral;

        /* I_own = b*t³/12 (area moment of inertia about own centroid) */
        double i_own = b * t_mm * t_mm * t_mm / 12.0;
        /* I_parallel = b*t*d² (parallel axis transfer) */
        double i_parallel = b * t_mm * d_i * d_i;

        d_total += ei * (i_own + i_parallel);
        y_current += t_mm;
    }

    return d_total;  /* N·mm² per mm width */
}

/**
 * Knowledge Point: Stackup symmetry verification.
 *
 * Asymmetric stackups bow/warp during temperature changes because the
 * CTE-moment is unbalanced about the mid-plane. IPC-2223 requires
 * symmetric construction for ≥ 4-layer rigid-flex.
 *
 * Symmetry is checked by comparing layers equidistant from the center:
 * they must have identical materials, thicknesses, and copper weights.
 */
int flex_stackup_verify_symmetry(flex_stackup_t *stackup) {
    if (!stackup) return 0;

    int n = stackup->total_layer_count;
    if (n <= 1) {
        stackup->is_symmetric = 1;
        return 1;
    }

    for (int i = 0; i < n / 2; i++) {
        int j = n - 1 - i;  /* Mirror layer */
        const flex_layer_t *a = &stackup->layers[i];
        const flex_layer_t *b = &stackup->layers[j];

        /* Check material symmetry */
        if (a->dielectric != b->dielectric) {
            stackup->is_symmetric = 0;
            stackup->imbalance_warning = 1;
            return 0;
        }
        if (a->copper != b->copper) {
            stackup->is_symmetric = 0;
            stackup->imbalance_warning = 1;
            return 0;
        }
        /* Check thickness symmetry with 10% tolerance */
        double dt_a = a->finished_thickness_um;
        double dt_b = b->finished_thickness_um;
        if (dt_a > 0.0 && dt_b > 0.0) {
            double ratio = dt_a / dt_b;
            if (ratio < 0.9 || ratio > 1.1) {
                stackup->is_symmetric = 0;
                stackup->imbalance_warning = 1;
                return 0;
            }
        }
    }

    stackup->is_symmetric = 1;
    stackup->imbalance_warning = 0;
    return 1;
}

/**
 * Knowledge Point: Asymmetry metric.
 *
 * Quantifies symmetry on a [0,1] scale using a thickness-weighted moment:
 *   A = Σ|t_i * (z_i - z_mid)| / Σ(t_i * |z_i - z_mid|)
 *
 * 0 = perfect symmetry, 1 = maximum asymmetry.
 * Values > 0.1 suggest IPC-2223 compliance risk.
 */
double flex_stackup_asymmetry_metric(const flex_stackup_t *stackup) {
    if (!stackup || stackup->total_layer_count == 0) return 0.0;

    double total_thickness = 0.0;
    for (int i = 0; i < stackup->total_layer_count; i++) {
        total_thickness += stackup->layers[i].finished_thickness_um;
    }
    double z_mid = total_thickness / 2000.0;  /* Half in mm */

    double z_current = 0.0;
    double num = 0.0, den = 0.0;

    for (int i = 0; i < stackup->total_layer_count; i++) {
        double t = stackup->layers[i].finished_thickness_um / 1000.0;
        double z_center = z_current + t / 2.0;
        double dz = z_center - z_mid;
        if (dz < 0.0) dz = -dz;

        num += t * dz;
        den += t * dz;  /* Same as num — weights by thickness and distance */
        /* The proper metric weights the num by (z_i - z_mid) differently */
        /* Here we use a simpler ratio */
        z_current += t;
    }

    /* Simplified asymmetry: ratio of unbalanced to total thickness */
    double unbalanced = 0.0;
    z_current = 0.0;
    for (int i = 0; i < stackup->total_layer_count / 2; i++) {
        int j = stackup->total_layer_count - 1 - i;
        double t_a = stackup->layers[i].finished_thickness_um;
        double t_b = stackup->layers[j].finished_thickness_um;
        double diff = (t_a > t_b) ? (t_a - t_b) : (t_b - t_a);
        unbalanced += diff;
    }

    double metric = unbalanced / total_thickness;
    return (metric > 1.0) ? 1.0 : metric;
}

/**
 * Knowledge Point: Warpage estimation from CTE asymmetry.
 *
 * Based on Timoshenko's bi-material thermostat model:
 *   w = (Δα * ΔT * L²) / (8 * h)
 *
 * where Δα is the effective CTE mismatch, ΔT is temperature change,
 * L is board length, h is total thickness.
 *
 * For rigid-flex, the mismatch between FR-4 (α≈14) and PI (α≈20)
 * drives warpage. At solder reflow (ΔT≈220°C), a 100 mm rigid-flex
 * with 1.6 mm thickness can warp by:
 *   w = (6e-6 * 220 * 10000) / (8 * 1.6) ≈ 1.0 mm
 *
 * This is significant and can cause soldering defects.
 *
 * Reference: Timoshenko, J. Optical Society of America, Vol. 11, 1925
 */
double flex_stackup_warpage_estimate(const flex_stackup_t *stackup,
                                      double delta_temperature,
                                      double board_length_mm) {
    if (!stackup || delta_temperature == 0.0 || board_length_mm <= 0.0)
        return 0.0;

    /* Effective CTE mismatch: use material property lookup */
    double cte_rigid = 14.0;   /* FR-4 CTE (ppm/°C) */
    double cte_flex  = 20.0;   /* PI CTE (ppm/°C) */
    double delta_cte = (cte_flex - cte_rigid) * 1.0e-6;

    double total_thickness = flex_stackup_flex_thickness(stackup);
    if (total_thickness <= 0.0) total_thickness = 0.5;  /* Default */

    double delta_t = (delta_temperature > 0.0) ? delta_temperature
                                                : -delta_temperature;
    double l2 = board_length_mm * board_length_mm;

    /* Timoshenko bi-metal formula */
    return (delta_cte * delta_t * l2) / (8.0 * total_thickness);
}

/**
 * Knowledge Point: IPC-2223 constructive rule validation.
 *
 * Automates checking of mandatory IPC-2223 construction requirements:
 * - Maximum 32 total layers (flex + rigid)
 * - Flex sections limited to practical maximum (typically 12)
 * - Copper weight limits per layer
 * - Coverlay mandatory on outer flex layers
 * - Minimum dielectric thickness
 */
int flex_stackup_validate_ipc2223(const flex_stackup_t *stackup) {
    if (!stackup) return -1;
    int violations = 0;

    /* Rule 1: Total layer count */
    if (stackup->total_layer_count > 32) violations++;

    /* Rule 2: Flex layer count practical limit */
    int flex_count = flex_stackup_flex_layer_count(stackup);
    if (flex_count > 12) violations++;

    /* Rule 3: Check each flex layer */
    for (int i = 0; i < stackup->total_layer_count; i++) {
        const flex_layer_t *l = &stackup->layers[i];
        if (l->section != FLEX_SECTION_FLEX) continue;

        /* Copper thickness limit: ≤ 70 μm (2 oz) in flex */
        if (l->copper_thickness_um > 70.0) violations++;

        /* Minimum dielectric thickness: ≥ 12.5 μm (0.5 mil) */
        if (l->dielectric_thickness_um < 12.5) violations++;

        /* Outer flex layers must have coverlay */
        if ((i == 0 || i == stackup->total_layer_count - 1) &&
            !l->is_coverlay_present) violations++;
    }

    return violations;
}

/**
 * Knowledge Point: Stackup description (human-readable summary).
 *
 * Generates a text summary useful for documentation and design review.
 * Critical for communicating the stackup intent to fabrication vendors.
 */
int flex_stackup_describe(const flex_stackup_t *stackup,
                           char *buffer, size_t buffer_size) {
    if (!stackup || !buffer || buffer_size == 0) return 0;

    int written = snprintf(buffer, buffer_size,
        "Design: %s | IPC Class: %s\n"
        "Layers: %d total | Rigid sections: %d | Flex sections: %d\n"
        "Flex thickness: %.3f mm | Flex copper: %.1f um\n"
        "Symmetric: %s | Asymmetry: %.3f\n"
        "Stiffeners: %d | Bend zones: %d\n",
        stackup->design_name, stackup->ipc_class,
        stackup->total_layer_count,
        stackup->rigid_section_count, stackup->flex_section_count,
        flex_stackup_flex_thickness(stackup),
        flex_stackup_flex_copper_total(stackup),
        stackup->is_symmetric ? "YES" : "NO",
        flex_stackup_asymmetry_metric(stackup),
        (stackup->stiffeners[0].thickness_mm > 0.0) ? 1 : 0,
        (stackup->bend_zones[0].bend_radius_mm > 0.0) ? 1 : 0);

    return (written > 0 && written < (int)buffer_size) ? written : 0;
}

/**
 * Knowledge Point: Manufacturing cost index.
 *
 * Models the relative cost of a rigid-flex design compared to a
 * baseline 2-layer simple flex. Cost drivers include:
 *
 * - Layer count: primary cost driver (non-linear scaling)
 * - Adhesiveless: ~1.3× premium over adhesive-based
 * - Stiffeners: each adds ~15% cost
 * - Bend zones: complex tooling increases cost
 *
 * This model helps designers understand cost trade-offs early in
 * the design process.
 */
double flex_stackup_cost_index(const flex_stackup_t *stackup) {
    if (!stackup) return 0.0;

    double cost = 1.0;
    int flex_layers = flex_stackup_flex_layer_count(stackup);

    /* Base cost scales with layer count */
    if (flex_layers <= 2) {
        cost *= 1.0;
    } else if (flex_layers <= 4) {
        cost *= 1.8;
    } else if (flex_layers <= 6) {
        cost *= 3.0;
    } else if (flex_layers <= 8) {
        cost *= 4.5;
    } else {
        cost *= 6.0 + (flex_layers - 8) * 0.5;
    }

    /* Adhesiveless premium */
    for (int i = 0; i < stackup->total_layer_count; i++) {
        if (stackup->layers[i].adhesive == FLEX_ADHESIVE_ADHESIVELESS) {
            cost *= 1.3;
            break;  /* Apply once */
        }
    }

    /* Stiffener cost */
    for (int i = 0; i < FLEX_MAX_STIFFENERS; i++) {
        if (stackup->stiffeners[i].thickness_mm > 0.0) {
            cost *= 1.15;
        }
    }

    /* Bend zone complexity */
    int bend_count = 0;
    for (int i = 0; i < FLEX_MAX_BEND_ZONES; i++) {
        if (stackup->bend_zones[i].bend_radius_mm > 0.0) bend_count++;
    }
    if (bend_count > 1) cost *= (1.0 + (bend_count - 1) * 0.1);

    return cost;
}
