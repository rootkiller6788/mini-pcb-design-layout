/**
 * pcb_thermal_via.c - Thermal Via Design and Optimization Implementation
 *
 * L5: Single via analysis, array optimization, efficiency modeling,
 *     filled vs unfilled analysis, effective conductivity computation.
 *
 * Reference: Li (2015), Guenin (2006), IPC-4761, IPC-2221.
 */

#include "pcb_thermal_via.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==================================================================
 * L5: Single Via Thermal Analysis
 * ================================================================== */

double thermal_via_single_resistance(const thermal_via_geometry_t *via_geom,
                                      double via_length_mm, double k_cu) {
    if (!via_geom) return INFINITY;
    if (via_length_mm <= 0.0 || k_cu <= 0.0) return INFINITY;

    /* R_via = L / (k_Cu * A_cross)
     * A_cross = pi*t_plate*(d_drill - t_plate) for unfilled annular ring
     *         = pi*d_drill^2/4 for filled with k_close to k_Cu */
    double a_cross = thermal_via_cross_section_area(via_geom);
    if (a_cross <= 0.0) return INFINITY;

    /* R = L(mm)*1e-3 / (k * A(mm2)*1e-6) = L / (k*A) * 1e3 */
    return via_length_mm / (k_cu * a_cross) * 1.0e3;
}

double thermal_via_cross_section_area(const thermal_via_geometry_t *via_geom) {
    if (!via_geom) return 0.0;

    double d = via_geom->drill_diameter_mm;
    double t = via_geom->plating_thickness_mm;

    if (d <= 0.0 || t <= 0.0) return 0.0;
    if (t >= d / 2.0) t = d / 2.0;  /* Plating cannot exceed radius */

    if (via_geom->is_filled) {
        /* Filled via: full cross-section area
         * If fill material has lower k, the effective area is proportionally reduced */
        double k_cu = 385.0;
        double fill_k = via_geom->fill_k > 0.0 ? via_geom->fill_k : 58.0; /* SAC305 */
        double area_full = M_PI * d * d / 4.0;
        double area_annular = M_PI * t * (d - t);
        /* Effective area: copper barrel + (fill area weighted by k_fill/k_cu) */
        double area_fill_equivalent = (area_full - area_annular) * fill_k / k_cu;
        return area_annular + area_fill_equivalent;  /* Copper barrel + weighted fill */
    } else {
        /* Unfilled via: annular copper ring only
         * A = pi*(d_outer^2 - d_inner^2)/4 = pi*t*(d - t) */
        return M_PI * t * (d - t);
    }
}

/* ==================================================================
 * L5: Via Array Thermal Analysis
 * ================================================================== */

double thermal_via_array_resistance(thermal_via_geometry_t *via_geom,
                                     double via_length_mm, double k_cu) {
    if (!via_geom) return INFINITY;
    if (via_geom->num_vias <= 0) return INFINITY;

    double r_single = thermal_via_single_resistance(via_geom, via_length_mm, k_cu);
    if (isinf(r_single)) return INFINITY;

    /* R_array = R_single / (N * efficiency) */
    double eta = thermal_via_efficiency(via_geom->num_vias,
                    via_geom->pitch_mm,
                    via_geom->drill_diameter_mm,
                    via_length_mm);
    if (eta <= 0.0) eta = 0.5;  /* Fallback */

    double r_array = r_single / (via_geom->num_vias * eta);
    return r_array;
}

double thermal_via_efficiency(int num_vias, double pitch_mm,
                               double drill_diameter_mm,
                               double via_length_mm) {
    if (num_vias <= 0 || pitch_mm <= 0.0 || drill_diameter_mm <= 0.0)
        return 0.0;

    /* Mutual thermal coupling reduces effective conductivity of via arrays.
     * Model based on heat conduction interference between cylinders:
     * eta = 1 / (1 + alpha * (d/pitch)^2)
     * where alpha increases with via count and via aspect ratio (L/d).
     *
     * For small arrays (N<25):  alpha ~ 0.15
     * For medium arrays:        alpha ~ 0.30
     * For large arrays:         alpha ~ 0.50
     * Aspect ratio factor: longer vias increase interference (more spreading). */
    double aspect = via_length_mm / drill_diameter_mm;
    double ar_factor = (aspect > 5.0) ? 1.0 + 0.02 * (aspect - 5.0) : 1.0;
    double alpha;
    if (num_vias <= 25)
        alpha = 0.15 * ar_factor;
    else if (num_vias <= 100)
        alpha = 0.30 * ar_factor;
    else
        alpha = 0.50 * ar_factor;

    double d_p_ratio = drill_diameter_mm / pitch_mm;
    if (d_p_ratio > 0.8) d_p_ratio = 0.8;  /* Minimum pitch constraint */

    return 1.0 / (1.0 + alpha * d_p_ratio * d_p_ratio * num_vias);
}

/* ==================================================================
 * L5: Via Array Optimization
 * ================================================================== */

int thermal_via_calculate_count(double power_w, double max_delta_t,
                                 double via_length_mm, double k_cu,
                                 double drill_mm, double plate_mm,
                                 double pitch_mm, int max_vias,
                                 int *num_vias_out, double *r_achieved) {
    if (!num_vias_out || !r_achieved) return THERMAL_ERR_NULL_PTR;
    if (power_w <= 0.0 || max_delta_t <= 0.0) return THERMAL_ERR_INVALID_ENV;

    /* Required thermal resistance: R_target = delta_T / P */
    double r_target = max_delta_t / power_w;

    for (int n = 1; n <= max_vias; n++) {
        thermal_via_geometry_t via = {
            .drill_diameter_mm = drill_mm,
            .plating_thickness_mm = plate_mm,
            .pitch_mm = pitch_mm,
            .num_vias = n,
            .is_filled = 0,
            .fill_k = 0.0,
            .via_length_mm = via_length_mm
        };

        double r_array = thermal_via_array_resistance(&via, via_length_mm, k_cu);
        if (r_array <= r_target) {
            *num_vias_out = n;
            *r_achieved = r_array;
            return THERMAL_OK;
        }
    }

    /* Cannot achieve target with available vias */
    *num_vias_out = max_vias;
    thermal_via_geometry_t via_last = {
        .drill_diameter_mm = drill_mm,
        .plating_thickness_mm = plate_mm,
        .pitch_mm = pitch_mm,
        .num_vias = max_vias,
        .is_filled = 0,
        .via_length_mm = via_length_mm
    };
    *r_achieved = thermal_via_array_resistance(&via_last, via_length_mm, k_cu);
    return THERMAL_ERR_NO_CONVERGE;
}

int thermal_via_optimize_array(double footprint_width_mm,
                                double footprint_length_mm,
                                double via_length_mm, double k_cu,
                                double plate_thickness_mm,
                                thermal_via_geometry_t *optimized) {
    if (!optimized) return THERMAL_ERR_NULL_PTR;
    if (footprint_width_mm <= 0.0 || footprint_length_mm <= 0.0)
        return THERMAL_ERR_ZERO_AREA;

    /* Diameter candidates: 0.2mm (minimum drill) to 1.0mm in 0.05mm steps */
    double best_r = INFINITY;

    for (double d = 0.20; d <= 1.05; d += 0.05) {
        /* Check aspect ratio constraint: L/d <= 10 */
        if (via_length_mm / d > 10.0) continue;

        /* Minimum pitch: 2.5 * d (prevents drill breakage between holes) */
        double min_pitch = 2.5 * d;

        /* Maximum vias in width direction */
        int max_cols = (int)((footprint_width_mm - d) / min_pitch) + 1;
        if (max_cols < 1) max_cols = 1;

        /* Maximum vias in length direction */
        int max_rows = (int)((footprint_length_mm - d) / min_pitch) + 1;
        if (max_rows < 1) max_rows = 1;

        for (int rows = 1; rows <= max_rows && rows <= 20; rows++) {
            for (int cols = 1; cols <= max_cols && cols <= 20; cols++) {
                int n = rows * cols;
                if (n > 400) continue;  /* Practical upper limit */

                /* Check if array fits */
                double pitch_actual = min_pitch;
                double total_w = d + (cols - 1) * pitch_actual;
                double total_l = d + (rows - 1) * pitch_actual;
                if (total_w > footprint_width_mm || total_l > footprint_length_mm) continue;

                thermal_via_geometry_t via = {
                    .drill_diameter_mm = d,
                    .pad_diameter_mm = d * 1.5,
                    .plating_thickness_mm = plate_thickness_mm,
                    .pitch_mm = pitch_actual,
                    .num_vias = n,
                    .rows = rows,
                    .cols = cols,
                    .is_hexagonal = 0,
                    .is_filled = 0,
                    .fill_k = 0.0,
                    .via_length_mm = via_length_mm
                };

                double r = thermal_via_array_resistance(&via, via_length_mm, k_cu);
                if (r < best_r) {
                    best_r = r;
                    *optimized = via;
                }
            }
        }
    }

    if (isinf(best_r)) return THERMAL_ERR_NO_CONVERGE;

    optimized->r_theta_single = thermal_via_single_resistance(optimized, via_length_mm, k_cu);
    optimized->r_theta_array = best_r;

    return THERMAL_OK;
}

double thermal_via_effective_kz(const thermal_via_geometry_t *via_geom,
                                 double k_cu, double k_dielectric) {
    if (!via_geom) return k_dielectric;
    if (via_geom->num_vias <= 0 || via_geom->pitch_mm <= 0.0)
        return k_dielectric;

    /* Effective Z-conductivity using rule of mixtures (parallel model):
     * k_eff_z = (k_Cu * A_vias + k_diel * A_remaining) / A_total
     *
     * A_vias = sum of via cross-sections
     * A_total = array pitch area * num_vias (approximately) */

    double a_single = thermal_via_cross_section_area(via_geom);
    double a_vias_total = a_single * via_geom->num_vias;

    /* Approximate total array area */
    double array_width = via_geom->cols > 0 ?
        via_geom->drill_diameter_mm + (via_geom->cols - 1) * via_geom->pitch_mm : 1.0;
    double array_length = via_geom->rows > 0 ?
        via_geom->drill_diameter_mm + (via_geom->rows - 1) * via_geom->pitch_mm : 1.0;
    double a_total = array_width * array_length;
    if (a_total <= 0.0) a_total = 1.0;

    double a_remaining = a_total - a_vias_total;
    if (a_remaining < 0.0) a_remaining = 0.0;

    return (k_cu * a_vias_total + k_dielectric * a_remaining) / a_total;
}

double thermal_via_improvement_factor(const thermal_via_geometry_t *via_geom,
                                       double via_length_mm,
                                       double area_mm2,
                                       double k_cu, double k_dielectric) {
    if (!via_geom || area_mm2 <= 0.0) return 1.0;

    /* R_without_vias = L / (k_dielectric * area)
     * R_with_vias = thermal_via_array_resistance(via)
     * Improvement = R_without / R_with */

    double r_without = via_length_mm / (k_dielectric * area_mm2) * 1.0e3;
    double r_with = thermal_via_array_resistance(
        (thermal_via_geometry_t *)via_geom, via_length_mm, k_cu);

    if (isinf(r_with) || r_with <= 0.0) return 1.0;
    return r_without / r_with;
}

/* ==================================================================
 * L5: Filled vs Unfilled Via Analysis
 * ================================================================== */

void thermal_via_fill_analysis(double drill_diameter_mm, double plating_mm,
                                double fill_k,
                                double *r_unfilled_out, double *r_filled_out,
                                double *improvement) {
    if (!r_unfilled_out || !r_filled_out || !improvement) return;

    double k_cu = 385.0;
    double via_length = 1.6;  /* Standard 1.6mm board */

    /* Unfilled via geometry */
    thermal_via_geometry_t via_unfilled = {
        .drill_diameter_mm = drill_diameter_mm,
        .plating_thickness_mm = plating_mm,
        .pitch_mm = drill_diameter_mm * 2.5,
        .num_vias = 1,
        .rows = 1,
        .cols = 1,
        .is_hexagonal = 0,
        .is_filled = 0,
        .fill_k = 0.0,
        .via_length_mm = via_length
    };

    /* Filled via geometry */
    thermal_via_geometry_t via_filled = via_unfilled;
    via_filled.is_filled = 1;
    via_filled.fill_k = fill_k > 0.0 ? fill_k : 58.0;  /* SAC305 default */

    *r_unfilled_out = thermal_via_single_resistance(&via_unfilled, via_length, k_cu);
    *r_filled_out = thermal_via_single_resistance(&via_filled, via_length, k_cu);

    if (*r_filled_out > 0.0 && !isinf(*r_filled_out))
        *improvement = *r_unfilled_out / *r_filled_out;
    else
        *improvement = 1.0;
}
