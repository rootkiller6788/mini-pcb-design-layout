/**
 * pcb_thermal_design.c - PCB Thermal Design Methods and Optimization
 *
 * L5: Copper pour sizing, PCB stack thermal optimization, derating analysis.
 * L6: Complete cooling solution design, heat sink selection, thermal runaway check.
 * L7: Arrhenius lifetime estimation for real-world reliability.
 *
 * Reference: IPC-2152, Erickson & Maksimovic (2001), TI Design Guide (2014),
 *            MIL-HDBK-217F.
 */

#include "pcb_thermal_design.h"
#include "pcb_thermal_material.h"
#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef GRAVITY
#define GRAVITY 9.80665
#endif

/* ==================================================================
 * L5/L6: Copper Pour Sizing for Component Cooling
 * ================================================================== */

int copper_pour_sizing(double power_w, double ta_c, double tj_max_c,
                        double r_jc, double board_k, double board_t_mm,
                        double h_conv, copper_weight_t copper_oz,
                        double *area_mm2_out, double *r_pour_out) {
    if (!area_mm2_out || !r_pour_out) return THERMAL_ERR_NULL_PTR;
    if (power_w <= 0.0) return THERMAL_ERR_NEG_POWER;
    if (tj_max_c <= ta_c) return THERMAL_ERR_TJ_EXCEEDED;

    /* Allowed temperature rise from case to ambient */
    double dt_max = tj_max_c - ta_c - power_w * r_jc;
    if (dt_max <= 0.0) {
        /* Need negative resistance - impossible without better Rjc or lower Ta */
        *area_mm2_out = 0.0;
        *r_pour_out = INFINITY;
        return THERMAL_ERR_TJ_EXCEEDED;
    }

    /* Required R_ambient = dT_max / P */
    double r_required = dt_max / power_w;

    /* Binary search for required copper pour area.
     * R_pour(A) = R_spread(A) + R_conv(A)
     * R_spread = ln(sqrt(A/A_source)) / (2*pi*k_eff*t_board)
     * R_conv = 1 / (h * A)
     *
     * The effective k uses the board k enhanced by copper weight on surface layers.
     * Search range: 1 mm^2 to 100000 mm^2 (100cm^2, large pour) */
    double a_low = 1.0;
    double a_high = 100000.0;
    double a_source = 25.0;  /* Assume ~5x5mm component footprint */
    double a_result = 0.0;

    /* Effective k uses board base k enhanced by copper layer contribution.
     * For 1oz Cu on FR4: k_eff_surface = (385*0.035 + board_k*1.565)/1.6 */
    double t_copper = pcb_thermal_copper_thickness(copper_oz);
    double k_eff = (385.0 * t_copper + board_k * (board_t_mm - t_copper)) / board_t_mm;
    if (k_eff <= 0.0) k_eff = 0.35;  /* Bare FR4 fallback */

    for (int iter = 0; iter < 50; iter++) {
        double a_mid = sqrt(a_low * a_high);

        /* R_spread for this area */
        double r_spread = 0.0;
        if (a_mid > a_source)
            r_spread = log(sqrt(a_mid / a_source)) / (2.0 * M_PI * k_eff * board_t_mm * 1.0e-3);
        /* R_conv for this area */
        double r_conv = 1.0e6 / (h_conv * a_mid);
        double r_total = r_spread + r_conv;

        if (r_total < r_required) {
            a_high = a_mid;
            a_result = a_mid;
        } else {
            a_low = a_mid;
        }
    }

    *area_mm2_out = a_result;
    if (a_result > 0.0) {
        double r_spread_final = log(sqrt(a_result / a_source)) /
                                (2.0 * M_PI * k_eff * board_t_mm * 1.0e-3);
        double r_conv_final = 1.0e6 / (h_conv * a_result);
        *r_pour_out = r_spread_final + r_conv_final;
    } else {
        *r_pour_out = INFINITY;
    }

    return THERMAL_OK;
}

int copper_pour_thermal_analysis(const heat_source_t *source,
                                  const copper_pour_geom_t *pour,
                                  double ta_c, double h_conv,
                                  double *tj_out) {
    if (!source || !pour || !tj_out) return THERMAL_ERR_NULL_PTR;
    if (source->power_w < 0.0) return THERMAL_ERR_NEG_POWER;

    /* Resistance network: Tj -> [Rjc] -> Tc -> [Rspread] -> Tpour -> [Rconv] -> Ta */
    double r_spread = pour->r_spreading;
    if (r_spread <= 0.0) {
        /* Compute spreading if not pre-computed */
        double k_cu = 385.0;
        r_spread = thermal_resistance_spreading(k_cu, pour->thickness_mm,
                        source->width_mm * source->length_mm, pour->area_mm2);
    }
    double r_conv = pour->r_convection > 0.0 ? pour->r_convection :
                    thermal_resistance_convection(h_conv, pour->area_mm2);

    double r_total = source->r_jc + r_spread + r_conv;
    *tj_out = ta_c + source->power_w * r_total;

    return (*tj_out > source->max_tj && source->max_tj > 0.0) ?
           THERMAL_ERR_TJ_EXCEEDED : THERMAL_OK;
}

void copper_pour_optimal_dimensions(double area_mm2,
                                     double source_w_mm, double source_h_mm,
                                     double *pour_w_out, double *pour_h_out) {
    if (!pour_w_out || !pour_h_out || area_mm2 <= 0.0) return;

    /* Optimal shape: square around the source, extending equally in all directions.
     * For a rectangular source, the pour should maintain the source aspect ratio
     * to minimize spreading resistance (isothermal boundary condition). */

    double source_area = source_w_mm * source_h_mm;
    if (source_area <= 0.0) {
        /* Fallback to square */
        double side = sqrt(area_mm2);
        *pour_w_out = side;
        *pour_h_out = side;
        return;
    }

    /* Extend proportionally to source dimensions */
    double scale = sqrt(area_mm2 / source_area);
    *pour_w_out = source_w_mm * scale;
    *pour_h_out = source_h_mm * scale;
}

/* ==================================================================
 * L5: PCB Stack-up Thermal Optimization
 * ================================================================== */

double pcb_stack_effective_k_xy(const pcb_stackup_t *stackup) {
    if (!stackup || stackup->num_layers <= 0) return 0.0;

    /* Rule of mixtures: k_eff = sum(k_i * t_i) / sum(t_i)
     * For each layer: k_i depends on copper coverage */
    double sum_kt = 0.0, sum_t = 0.0;
    for (int i = 0; i < stackup->num_layers; i++) {
        pcb_layer_t *layer = &stackup->layers[i];
        double t = layer->thickness_mm;
        double k = layer->k_xy;
        if (t <= 0.0 || k <= 0.0) continue;
        sum_kt += k * t;
        sum_t += t;
    }
    return (sum_t > 0.0) ? sum_kt / sum_t : 0.3;  /* Default to FR4 */
}

double pcb_stack_effective_k_z(const pcb_stackup_t *stackup) {
    if (!stackup || stackup->num_layers <= 0) return 0.0;

    /* Series model: k_eff_z = sum(t_i) / sum(t_i / k_i) */
    double sum_t = 0.0, sum_tk = 0.0;
    for (int i = 0; i < stackup->num_layers; i++) {
        pcb_layer_t *layer = &stackup->layers[i];
        double t = layer->thickness_mm;
        double k = layer->k_z;
        if (t <= 0.0 || k <= 0.0) continue;
        sum_t += t;
        sum_tk += t / k;
    }
    return (sum_tk > 0.0) ? sum_t / sum_tk : 0.25;  /* Default to FR4 */
}

double pcb_stack_plane_benefit(const pcb_stackup_t *stackup) {
    if (!stackup || stackup->num_layers <= 0) return 1.0;

    /* Benefit = k_eff_with_all_layers / k_eff_without_solid_planes
     * i.e., if we converted all solid planes to FR4-only, how much worse?
     * We simulate by setting k_xy=0.3 for PWR/GND layers. */

    double sum_kt_all = 0.0, sum_kt_no_planes = 0.0, sum_t = 0.0;

    for (int i = 0; i < stackup->num_layers; i++) {
        pcb_layer_t *layer = &stackup->layers[i];
        double t = layer->thickness_mm;
        if (t <= 0.0) continue;
        sum_t += t;
        sum_kt_all += layer->k_xy * t;

        /* Without planes: PWR/GND layers conduct like bare FR4 */
        if (layer->type == PCB_LAYER_PWR_PLANE || layer->type == PCB_LAYER_GND_PLANE)
            sum_kt_no_planes += 0.30 * t;  /* Bare FR4 k_xy */
        else
            sum_kt_no_planes += layer->k_xy * t;
    }

    if (sum_kt_no_planes <= 0.0 || sum_t <= 0.0) return 1.0;
    double k_eff_all = sum_kt_all / sum_t;
    double k_eff_no = sum_kt_no_planes / sum_t;
    return k_eff_all / k_eff_no;
}

void pcb_stack_optimize_layer_count(const pcb_stackup_t *stackup_template,
                                     int max_layers, double cost_per_layer,
                                     int *optimal_layers, double *benefit_score) {
    if (!stackup_template || !optimal_layers || !benefit_score) return;
    if (max_layers < 2) { *optimal_layers = 2; *benefit_score = 0.0; return; }

    double best_score = -1.0;
    int best_layers = 2;

    /* Evaluate layer counts from 2 to max_layers (only even counts for symmetry) */
    for (int n = 2; n <= max_layers; n += 2) {
        /* Thermal benefit: approximate k_eff grows with more solid planes.
         * Simplified model: each additional pair adds 1 solid plane.
         * k_eff(n) ~ k_FR4 + n_planes * delta_k / n_layers */
        int n_planes = n / 2;  /* Half the layers are planes (PWR+GND) */
        double k_eff_n = 0.30 + (double)n_planes * 50.0 / (double)n;
        double cost_n = (double)n * cost_per_layer;
        double score = k_eff_n / cost_n;
        if (score > best_score) {
            best_score = score;
            best_layers = n;
        }
    }

    *optimal_layers = best_layers;
    *benefit_score = best_score;
}

double pcb_board_to_ambient_resistance(double board_area_mm2,
                                        double board_temp_c,
                                        const ambient_conditions_t *ambient) {
    if (!ambient || board_area_mm2 <= 0.0) return INFINITY;

    /* Estimate natural convection h for a horizontal PCB plate.
     * h ~ C * (dT/L_char)^(1/4) for laminar natural convection.
     *
     * For a rectangular plate, L_char = A/P where P is perimeter.
     * Simplified: L_char = sqrt(A) for roughly square boards. */
    double l_char = sqrt(board_area_mm2) * 1.0e-3;
    double dT = board_temp_c - ambient->ambient_temp_c;
    if (dT < 1.0) dT = 10.0;  /* Minimum for convection */

    double h;
    /* Use natural convection correlation for horizontal heated surface facing up */
    double ra = GRAVITY * (1.0 / (ambient->ambient_temp_c + 298.15)) * dT *
                l_char * l_char * l_char * ambient->air_prandtl /
                (ambient->air_dynamic_viscosity / ambient->air_density_kgm3) /
                (ambient->air_dynamic_viscosity / ambient->air_density_kgm3);
    double nu_laminar = 0.54 * pow(ra, 0.25);
    double nu_turbulent = 0.15 * pow(ra, 0.333);
    /* Transitional: use max of laminar and turbulent */
    double nu = (nu_laminar > nu_turbulent) ? nu_laminar : nu_turbulent;
    h = nu * ambient->air_conductivity / l_char;

    if (h <= 0.0) h = 10.0;  /* Fallback */

    return 1.0e6 / (h * board_area_mm2);
}

/* ==================================================================
 * L5/L7: Thermal Derating and Lifetime Estimation
 * ================================================================== */

double thermal_derating_factor(double tj_actual, double tj_max, double ta_c) {
    if (tj_max <= ta_c) return 0.0;
    double derating = (tj_max - tj_actual) / (tj_max - ta_c);
    return derating;
}

double thermal_lifetime_estimate(double tj_actual_c, double tj_rated_c,
                                  double rated_life_hours, double ea_ev) {
    if (rated_life_hours <= 0.0 || ea_ev <= 0.0) return 0.0;

    /* Arrhenius acceleration factor:
     * AF = exp( (Ea/k) * (1/T_use - 1/T_stress) )
     * k_B = 8.617333262145e-5 eV/K
     * T in Kelvin
     *
     * Lifetime at T_use = rated_lifetime / AF  (if T_use > T_stress)
     * or: Lifetime at T_use = rated_lifetime * AF_improved (if T_use < T_stress) */

    double k_b = 8.617333262145e-5;
    double t_use_k = tj_actual_c + 273.15;
    double t_rated_k = tj_rated_c + 273.15;

    if (t_use_k <= 0.0 || t_rated_k <= 0.0) return rated_life_hours;

    /* AF = exp((Ea/k_b) * (1/T_rated - 1/T_actual)) */
    double exponent = (ea_ev / k_b) * (1.0 / t_rated_k - 1.0 / t_use_k);
    double af = exp(exponent);

    /* If T_actual > T_rated: lifetime = rated / AF (accelerated aging)
     * If T_actual < T_rated: lifetime = rated * AF (extended life) */
    if (tj_actual_c > tj_rated_c)
        return rated_life_hours / af;
    else
        return rated_life_hours * af;
}

int thermal_runaway_check(const heat_source_t *devices, int num_devices,
                           double r_thermal_per_device, double ta_c) {
    if (!devices || num_devices <= 0) return THERMAL_ERR_NULL_PTR;

    /* Thermal runaway stability criterion for parallel devices:
     * Stable if: dP_self/dT < 1/R_thermal
     * where dP_self/dT is the change in power dissipation per degree.
     *
     * For MOSFETs: Rds(on) increases ~0.4 percent per degree C above 25 C.
     * P = I^2 * Rds(on) = I^2 * Rds25 * (1 + alpha*(Tj-25))
     * dP/dT = I^2 * Rds25 * alpha = P25 * alpha / (1+alpha*(Tj-25))
     *
     * Simplified check: temperature difference between devices < 10 C
     * indicates acceptable current sharing. */

    double t_max = -273.15;
    double t_min = 1.0e10;
    double t_avg = 0.0;

    for (int i = 0; i < num_devices; i++) {
        double tj = ta_c + devices[i].power_w * r_thermal_per_device;
        if (tj > t_max) t_max = tj;
        if (tj < t_min) t_min = tj;
        t_avg += tj;
    }
    t_avg /= num_devices;

    /* Check 1: Temperature spread */
    double spread = t_max - t_min;
    if (spread > 20.0) return THERMAL_ERR_TJ_EXCEEDED;  /* Severe imbalance */

    /* Check 2: Any device exceeding its maximum */
    for (int i = 0; i < num_devices; i++) {
        double tj = ta_c + devices[i].power_w * r_thermal_per_device;
        if (devices[i].max_tj > 0.0 && tj > devices[i].max_tj)
            return THERMAL_ERR_TJ_EXCEEDED;
    }

    return THERMAL_OK;
}

/* ==================================================================
 * L6: Complete Cooling Solution Design
 * ================================================================== */

int design_cooling_solution(heat_source_t *source,
                             double ta_c,
                             double board_area_mm2,
                             double board_k, double board_t_mm,
                             copper_weight_t copper_oz,
                             int allow_heatsink,
                             double design_margin,
                             cooling_type_t *cooling_used,
                             double *final_tj) {
    if (!source || !cooling_used || !final_tj) return THERMAL_ERR_NULL_PTR;
    if (source->power_w <= 0.0) return THERMAL_ERR_NEG_POWER;

    double r_ja_required = (source->max_tj - ta_c) / (source->power_w * design_margin);
    if (r_ja_required <= 0.0) return THERMAL_ERR_TJ_EXCEEDED;

    double r_ja_achieved = INFINITY;
    *cooling_used = COOLING_NONE;

    /* Step 1: Copper pour alone */
    double pour_area, r_pour;
    double h_nat = pcb_thermal_estimate_h(COOLING_COPPER_POUR, 0.0);
    int ret = copper_pour_sizing(source->power_w, ta_c, source->max_tj / design_margin,
                source->r_jc, board_k, board_t_mm, h_nat, copper_oz,
                &pour_area, &r_pour);
    if (ret == THERMAL_OK && pour_area <= board_area_mm2) {
        r_ja_achieved = source->r_jc + r_pour;
        *cooling_used = COOLING_COPPER_POUR;
    }

    /* Step 2: Add thermal vias (simplified) */
    if (*cooling_used == COOLING_NONE || r_ja_achieved > r_ja_required) {
        /* Estimate via contribution: 25 vias under component */
        double r_via = board_t_mm / (385.0 * M_PI * (0.3*0.3 - 0.25*0.25) / 4.0) * 1.0e3 / 25.0;
        double r_with_vias = source->r_jc + r_pour * r_via / (r_pour + r_via);
        if (r_with_vias < r_ja_required) {
            *cooling_used = COOLING_THERMAL_VIAS;
            r_ja_achieved = r_with_vias;
            source->has_thermal_vias = 1;
        }
    }

    /* Step 3: Heat sink (if allowed) */
    if (allow_heatsink && (*cooling_used == COOLING_NONE ||
                           r_ja_achieved > r_ja_required)) {
        /* Required Rsa for a heat sink */
        double r_sa_needed = r_ja_required - source->r_jc;
        if (r_sa_needed <= 0.0) r_sa_needed = 1.0;

        source->has_heatsink = 1;
        /* Estimate: natural convection Rsa ~ 25 C/W for typical 30x30mm extruded Al heatsink
         * Fin efficiency and geometry captured by this conservative estimate. */
        double r_sa_est = 25.0;
        double r_total = source->r_jc + r_sa_est;
        if (r_total < r_ja_required * design_margin) {
            *cooling_used = COOLING_HEATSINK_EXT;
            r_ja_achieved = r_total;
        }
    }

    *final_tj = ta_c + source->power_w * r_ja_achieved;

    if (*final_tj > source->max_tj && source->max_tj > 0.0)
        return THERMAL_ERR_TJ_EXCEEDED;

    return THERMAL_OK;
}

int heatsink_select_from_catalog(double r_sa_required,
                                  double max_width_mm, double max_height_mm,
                                  double max_length_mm,
                                  double forced_velocity_ms,
                                  heat_sink_model_t *selected) {
    if (!selected) return THERMAL_ERR_NULL_PTR;

    /* Standard extruded heat sink profiles (simplified catalog).
     * Each profile is defined by width, length, fin height, count, and spacing.
     * Sorted by decreasing cooling capability (increasing size). */

    /* Profile 1: Small (20x20x10mm, 5 fins) - natural Rsa ~40 C/W */
    /* Profile 2: Medium (30x30x20mm, 6 fins) - natural Rsa ~25 C/W */
    /* Profile 3: Large (50x50x25mm, 10 fins) - natural Rsa ~12 C/W */
    /* Profile 4: X-Large (80x80x40mm, 12 fins) - forced Rsa ~5 C/W */

    struct { double w, l, h, t_fin, s_fin; int n_fins; double r_sa_nat, r_sa_forced; } catalog[] = {
        {20, 20, 10, 1.0, 3.0, 5,  40.0, 15.0},
        {30, 30, 20, 1.0, 4.0, 6,  25.0, 8.0},
        {50, 40, 25, 1.5, 5.0, 8,  12.0, 4.0},
        {50, 50, 25, 1.5, 5.0, 10, 10.0, 3.0},
        {80, 80, 40, 2.0, 6.0, 12, 6.0, 2.0},
        {100, 100, 50, 2.0, 8.0, 12, 3.5, 1.0}
    };

    int n_catalog = sizeof(catalog) / sizeof(catalog[0]);
    double best_r = INFINITY;
    int best_idx = -1;

    for (int i = 0; i < n_catalog; i++) {
        if (catalog[i].w > max_width_mm || catalog[i].l > max_length_mm ||
            catalog[i].h > max_height_mm)
            continue;

        double r_sa = (forced_velocity_ms > 0.0) ?
                       catalog[i].r_sa_forced : catalog[i].r_sa_nat;

        if (r_sa <= r_sa_required && r_sa < best_r) {
            best_r = r_sa;
            best_idx = i;
        }
    }

    if (best_idx < 0) {
        /* No heatsink fits - return the largest that fits */
        for (int i = n_catalog - 1; i >= 0; i--) {
            if (catalog[i].w <= max_width_mm && catalog[i].l <= max_length_mm &&
                catalog[i].h <= max_height_mm) {
                best_idx = i;
                break;
            }
        }
        if (best_idx < 0) return THERMAL_ERR_NOT_IMPL;
    }

    /* Populate selected heat sink */
    selected->base_width_mm = catalog[best_idx].w;
    selected->base_length_mm = catalog[best_idx].l;
    selected->fin_height_mm = catalog[best_idx].h;
    selected->fin_thickness_mm = catalog[best_idx].t_fin;
    selected->fin_spacing_mm = catalog[best_idx].s_fin;
    selected->num_fins = catalog[best_idx].n_fins;
    selected->base_thickness_mm = 3.0;
    selected->k_material = 205.0;
    selected->r_sa_natural = catalog[best_idx].r_sa_nat;
    selected->r_sa_forced = catalog[best_idx].r_sa_forced;

    return THERMAL_OK;
}

void thermal_rise_time(double r_ja, double mass_g, double cp_jkgk,
                        double *t_rise_63pct, double *t_rise_95pct,
                        double *t_rise_99pct) {
    if (!t_rise_63pct || !t_rise_95pct || !t_rise_99pct) return;

    /* tau = R_th * C_th = r_ja * mass_g * 1e-3 * cp */
    double tau = r_ja * mass_g * 1.0e-3 * cp_jkgk;

    /* T(t) = Tfinal * (1 - exp(-t/tau))
     * For 63 percent: exp(-t/tau) = 0.37 -> t = tau
     * For 95 percent: exp(-t/tau) = 0.05 -> t = -ln(0.05)*tau = 3*tau
     * For 99 percent: exp(-t/tau) = 0.01 -> t = -ln(0.01)*tau = 4.605*tau */

    *t_rise_63pct = tau;
    *t_rise_95pct = 3.0 * tau;
    *t_rise_99pct = 4.605 * tau;
}
