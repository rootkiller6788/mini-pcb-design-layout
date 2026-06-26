/**
 * pcb_thermal_simulation.c - 2D Finite Difference Thermal Simulation for PCB
 *
 * L5: Finite Difference Method (FDM) for steady-state heat equation.
 * L6: Solving Laplacian(T) = -q/k on PCB with multiple heat sources,
 *     convection boundaries, and material inhomogeneities.
 *
 * Reference: Patankar (1980), Cengel Ch.5 (Numerical Methods),
 *            Reddy (2005), Smith (1985) "Numerical Solution of PDEs".
 */

#include "pcb_thermal_simulation.h"
#include "pcb_thermal_design.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ==================================================================
 * L5: Grid Generation and Material Mapping
 * ================================================================== */

thermal_field_t *thermal_field_create(double width_mm, double height_mm,
                                       double thickness_mm,
                                       double dx_mm, double dy_mm,
                                       const ambient_conditions_t *ambient) {
    if (width_mm <= 0.0 || height_mm <= 0.0 || dx_mm <= 0.0 || dy_mm <= 0.0)
        return NULL;

    thermal_field_t *field = (thermal_field_t *)calloc(1, sizeof(thermal_field_t));
    if (!field) return NULL;

    field->rows = (int)(height_mm / dy_mm) + 1;
    field->cols = (int)(width_mm / dx_mm) + 1;
    if (field->rows < 3) field->rows = 3;
    if (field->cols < 3) field->cols = 3;

    field->dx_mm = dx_mm;
    field->dy_mm = dy_mm;
    field->board_width_mm = width_mm;
    field->board_height_mm = height_mm;
    field->thickness_mm = thickness_mm;
    field->converged = 0;
    field->residual = INFINITY;
    field->iterations = 0;
    field->num_sources = 0;
    field->sources = NULL;

    if (ambient)
        field->ambient = *ambient;
    else
        field->ambient = ambient_default();

    /* Allocate temperature grid */
    field->T_c = (double **)malloc(field->rows * sizeof(double *));
    if (!field->T_c) { free(field); return NULL; }
    for (int i = 0; i < field->rows; i++) {
        field->T_c[i] = (double *)calloc(field->cols, sizeof(double));
        if (!field->T_c[i]) {
            for (int j = 0; j < i; j++) free(field->T_c[j]);
            free(field->T_c);
            free(field);
            return NULL;
        }
        /* Initialize to ambient temperature */
        for (int j = 0; j < field->cols; j++)
            field->T_c[i][j] = field->ambient.ambient_temp_c;
    }

    /* Allocate conductivity grid */
    field->k_xy = (double **)malloc(field->rows * sizeof(double *));
    if (!field->k_xy) {
        for (int i = 0; i < field->rows; i++) free(field->T_c[i]);
        free(field->T_c);
        free(field);
        return NULL;
    }
    for (int i = 0; i < field->rows; i++) {
        field->k_xy[i] = (double *)calloc(field->cols, sizeof(double));
        if (!field->k_xy[i]) {
            for (int j = 0; j < i; j++) free(field->k_xy[j]);
            free(field->k_xy);
            for (int j = 0; j < field->rows; j++) free(field->T_c[j]);
            free(field->T_c);
            free(field);
            return NULL;
        }
        /* Default to bare FR4 conductivity */
        for (int j = 0; j < field->cols; j++)
            field->k_xy[i][j] = 0.35;
    }

    return field;
}

void thermal_field_free(thermal_field_t *field) {
    if (!field) return;
    if (field->T_c) {
        for (int i = 0; i < field->rows; i++) free(field->T_c[i]);
        free(field->T_c);
    }
    if (field->k_xy) {
        for (int i = 0; i < field->rows; i++) free(field->k_xy[i]);
        free(field->k_xy);
    }
    if (field->sources) free(field->sources);
    free(field);
}

void thermal_field_set_uniform_k(thermal_field_t *field, double k_xy) {
    if (!field) return;
    for (int i = 0; i < field->rows; i++)
        for (int j = 0; j < field->cols; j++)
            field->k_xy[i][j] = k_xy;
}

void thermal_field_set_stackup_k(thermal_field_t *field,
                                  const pcb_stackup_t *stackup) {
    if (!field || !stackup) return;
    double k_eff = pcb_stack_effective_k_xy(stackup);
    thermal_field_set_uniform_k(field, k_eff);
}

int thermal_field_set_region_k(thermal_field_t *field,
                                double x_center_mm, double y_center_mm,
                                double width_mm, double height_mm,
                                double k_value) {
    if (!field) return THERMAL_ERR_NULL_PTR;

    /* Convert to grid coordinates */
    int col_min = (int)((x_center_mm - width_mm / 2.0) / field->dx_mm);
    int col_max = (int)((x_center_mm + width_mm / 2.0) / field->dx_mm);
    int row_min = (int)((y_center_mm - height_mm / 2.0) / field->dy_mm);
    int row_max = (int)((y_center_mm + height_mm / 2.0) / field->dy_mm);

    /* Clamp to grid bounds */
    if (col_min < 0) col_min = 0;
    if (col_max >= field->cols) col_max = field->cols - 1;
    if (row_min < 0) row_min = 0;
    if (row_max >= field->rows) row_max = field->rows - 1;

    for (int i = row_min; i <= row_max; i++)
        for (int j = col_min; j <= col_max; j++)
            field->k_xy[i][j] = k_value;

    return THERMAL_OK;
}

int thermal_field_set_copper_pour(thermal_field_t *field,
                                   const copper_pour_geom_t *pour,
                                   double x_center_mm, double y_center_mm) {
    if (!field || !pour) return THERMAL_ERR_NULL_PTR;

    /* Effective k in copper pour region using rule of mixtures:
     * With 1oz copper on 1.6mm FR4:
     * k_eff = (385 * 0.035 + 0.3 * 1.565) / 1.6 = (13.475 + 0.4695) / 1.6 = 8.7 W/m-K */
    double k_cu = 385.0;
    double k_fr4 = 0.30;
    double t_cu = pour->thickness_mm;
    double t_total = field->thickness_mm;
    double k_eff = (k_cu * t_cu + k_fr4 * (t_total - t_cu)) / t_total;

    return thermal_field_set_region_k(field, x_center_mm, y_center_mm,
                                      pour->width_mm, pour->length_mm, k_eff);
}

int thermal_field_add_heat_source(thermal_field_t *field,
                                   const heat_source_t *source) {
    if (!field || !source) return THERMAL_ERR_NULL_PTR;
    if (source->power_w <= 0.0) return THERMAL_ERR_NEG_POWER;

    /* Reallocate sources array */
    int n = field->num_sources + 1;
    heat_source_t **new_sources = (heat_source_t **)realloc(
        field->sources, n * sizeof(heat_source_t *));
    if (!new_sources) return THERMAL_ERR_MEMORY;
    field->sources = new_sources;

    /* Allocate and copy source */
    field->sources[n - 1] = (heat_source_t *)malloc(sizeof(heat_source_t));
    if (!field->sources[n - 1]) return THERMAL_ERR_MEMORY;
    memcpy(field->sources[n - 1], source, sizeof(heat_source_t));

    /* Set heat flux in the grid under the component */
    int col_min = (int)((source->center.x_mm - source->width_mm / 2.0) / field->dx_mm);
    int col_max = (int)((source->center.x_mm + source->width_mm / 2.0) / field->dx_mm);
    int row_min = (int)((source->center.y_mm - source->length_mm / 2.0) / field->dy_mm);
    int row_max = (int)((source->center.y_mm + source->length_mm / 2.0) / field->dy_mm);

    if (col_min < 0) col_min = 0;
    if (col_max >= field->cols) col_max = field->cols - 1;
    if (row_min < 0) row_min = 0;
    if (row_max >= field->rows) row_max = field->rows - 1;

    /* Distribute power uniformly over component area */
    int cells = (col_max - col_min + 1) * (row_max - row_min + 1);
    if (cells <= 0) cells = 1;

    /* Power density in W/m^3 = P / (cells * dx * dy * thickness)
     * per cell: q_dot = P / cells (we'll use this in the solver) */
    /* Store the power per cell implicitly through the source list */

    field->num_sources = n;
    return n - 1;  /* Return source index */
}

/* ==================================================================
 * L5: Finite Difference Solver - Steady State (Gauss-Seidel)
 * ================================================================== */

int thermal_fdm_solve_steady(thermal_field_t *field,
                              double h_top, double h_bottom,
                              double tolerance, int max_iter) {
    if (!field) return THERMAL_ERR_NULL_PTR;
    if (field->rows < 3 || field->cols < 3) return THERMAL_ERR_DIM_MISMATCH;

    int rows = field->rows, cols = field->cols;
    double dx = field->dx_mm * 1.0e-3;  /* Convert to meters */
    double dy = field->dy_mm * 1.0e-3;
    double dz = field->thickness_mm * 1.0e-3;
    double Ta = field->ambient.ambient_temp_c;

    double dx2 = dx * dx;
    double dy2 = dy * dy;

    /* Pre-compute source power density grid (W/m^3) */
    double **q_dot = (double **)malloc(rows * sizeof(double *));
    for (int i = 0; i < rows; i++) {
        q_dot[i] = (double *)calloc(cols, sizeof(double));
    }

    /* Map heat sources to power density */
    for (int s = 0; s < field->num_sources; s++) {
        heat_source_t *src = field->sources[s];
        int c_min = (int)((src->center.x_mm - src->width_mm / 2.0) / field->dx_mm);
        int c_max = (int)((src->center.x_mm + src->width_mm / 2.0) / field->dx_mm);
        int r_min = (int)((src->center.y_mm - src->length_mm / 2.0) / field->dy_mm);
        int r_max = (int)((src->center.y_mm + src->length_mm / 2.0) / field->dy_mm);
        if (c_min < 0) c_min = 0;
        if (c_max >= cols) c_max = cols - 1;
        if (r_min < 0) r_min = 0;
        if (r_max >= rows) r_max = rows - 1;
        int ncells = (c_max - c_min + 1) * (r_max - r_min + 1);
        if (ncells <= 0) ncells = 1;
        double q_per_cell = src->power_w / (ncells * dx * dy * dz);
        for (int i = r_min; i <= r_max; i++)
            for (int j = c_min; j <= c_max; j++)
                q_dot[i][j] = q_per_cell;
    }

    /* Gauss-Seidel iteration */
    field->converged = 0;
    field->iterations = 0;
    double max_change = INFINITY;

    for (int iter = 0; iter < max_iter; iter++) {
        max_change = 0.0;

        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                double t_old = field->T_c[i][j];

                /* Harmonic average of conductivity with neighbors */
                double ke = (j < cols - 1) ? 2.0 / (1.0/field->k_xy[i][j] + 1.0/field->k_xy[i][j+1]) : field->k_xy[i][j];
                double kw = (j > 0)       ? 2.0 / (1.0/field->k_xy[i][j] + 1.0/field->k_xy[i][j-1]) : field->k_xy[i][j];
                double kn = (i < rows - 1) ? 2.0 / (1.0/field->k_xy[i][j] + 1.0/field->k_xy[i+1][j]) : field->k_xy[i][j];
                double ks = (i > 0)       ? 2.0 / (1.0/field->k_xy[i][j] + 1.0/field->k_xy[i-1][j]) : field->k_xy[i][j];

                /* Get neighbor temperatures (with boundary handling) */
                double Te = (j < cols - 1) ? field->T_c[i][j+1] : t_old;
                double Tw = (j > 0)       ? field->T_c[i][j-1] : t_old;
                double Tn = (i < rows - 1) ? field->T_c[i+1][j] : t_old;
                double Ts = (i > 0)       ? field->T_c[i-1][j] : t_old;

                /* Discretized 2D heat equation:
                 * ke*(Te-Tp)/dx^2 + kw*(Tw-Tp)/dx^2 + kn*(Tn-Tp)/dy^2 + ks*(Ts-Tp)/dy^2
                 * + q_dot - (h_top+h_bottom)/dz*(Tp-Ta) = 0
                 *
                 * Note: (h_top+h_bottom)/dz models convection from top and bottom
                 * as a volumetric sink term. This is valid for thin boards where
                 * the Z-gradient can be lumped. */

                double h_eff = (h_top + h_bottom) / dz;

                double numerator = ke * Te / dx2 + kw * Tw / dx2 +
                                   kn * Tn / dy2 + ks * Ts / dy2 +
                                   q_dot[i][j] + h_eff * Ta;
                double denominator = (ke + kw) / dx2 + (kn + ks) / dy2 + h_eff;

                double t_new;
                if (denominator > 0.0)
                    t_new = numerator / denominator;
                else
                    t_new = t_old;

                field->T_c[i][j] = t_new;
                double change = fabs(t_new - t_old);
                if (change > max_change) max_change = change;
            }
        }

        field->iterations = iter + 1;

        if (max_change < tolerance) {
            field->converged = 1;
            field->residual = max_change;
            break;
        }
    }

    if (!field->converged)
        field->residual = max_change;

    /* Cleanup */
    for (int i = 0; i < rows; i++) free(q_dot[i]);
    free(q_dot);

    return field->converged ? THERMAL_OK : THERMAL_ERR_NO_CONVERGE;
}

/* ==================================================================
 * L5: Successive Over-Relaxation (SOR) Solver
 * ================================================================== */

int thermal_fdm_solve_sor(thermal_field_t *field,
                           double h_top, double h_bottom,
                           double omega, double tolerance, int max_iter) {
    if (!field) return THERMAL_ERR_NULL_PTR;
    if (field->rows < 3 || field->cols < 3) return THERMAL_ERR_DIM_MISMATCH;
    if (omega <= 1.0 || omega >= 2.0) {
        /* Fall back to Gauss-Seidel */
        return thermal_fdm_solve_steady(field, h_top, h_bottom, tolerance, max_iter);
    }

    int rows = field->rows, cols = field->cols;
    double dx = field->dx_mm * 1.0e-3;
    double dy = field->dy_mm * 1.0e-3;
    double dz = field->thickness_mm * 1.0e-3;
    double Ta = field->ambient.ambient_temp_c;
    double dx2 = dx * dx, dy2 = dy * dy;

    /* Build power density grid */
    double **q_dot = (double **)malloc(rows * sizeof(double *));
    for (int i = 0; i < rows; i++)
        q_dot[i] = (double *)calloc(cols, sizeof(double));

    for (int s = 0; s < field->num_sources; s++) {
        heat_source_t *src = field->sources[s];
        int c_min = (int)((src->center.x_mm - src->width_mm/2.0) / field->dx_mm);
        int c_max = (int)((src->center.x_mm + src->width_mm/2.0) / field->dx_mm);
        int r_min = (int)((src->center.y_mm - src->length_mm/2.0) / field->dy_mm);
        int r_max = (int)((src->center.y_mm + src->length_mm/2.0) / field->dy_mm);
        if (c_min < 0) c_min = 0;
        if (c_max >= cols) c_max = cols - 1;
        if (r_min < 0) r_min = 0;
        if (r_max >= rows) r_max = rows - 1;
        int ncells = (c_max - c_min + 1) * (r_max - r_min + 1);
        if (ncells <= 0) ncells = 1;
        double q_per_cell = src->power_w / (ncells * dx * dy * dz);
        for (int i = r_min; i <= r_max; i++)
            for (int j = c_min; j <= c_max; j++)
                q_dot[i][j] = q_per_cell;
    }

    field->converged = 0;
    field->iterations = 0;
    double max_change = INFINITY;
    double h_eff = (h_top + h_bottom) / dz;

    for (int iter = 0; iter < max_iter; iter++) {
        max_change = 0.0;

        for (int i = 0; i < rows; i++) {
            for (int j = 0; j < cols; j++) {
                double t_old = field->T_c[i][j];

                double ke = (j < cols-1) ? 2.0/(1.0/field->k_xy[i][j]+1.0/field->k_xy[i][j+1]) : field->k_xy[i][j];
                double kw = (j > 0)      ? 2.0/(1.0/field->k_xy[i][j]+1.0/field->k_xy[i][j-1]) : field->k_xy[i][j];
                double kn = (i < rows-1) ? 2.0/(1.0/field->k_xy[i][j]+1.0/field->k_xy[i+1][j]) : field->k_xy[i][j];
                double ks = (i > 0)      ? 2.0/(1.0/field->k_xy[i][j]+1.0/field->k_xy[i-1][j]) : field->k_xy[i][j];

                double Te = (j < cols-1) ? field->T_c[i][j+1] : t_old;
                double Tw = (j > 0)      ? field->T_c[i][j-1] : t_old;
                double Tn = (i < rows-1) ? field->T_c[i+1][j] : t_old;
                double Ts = (i > 0)      ? field->T_c[i-1][j] : t_old;

                double numerator = ke*Te/dx2 + kw*Tw/dx2 + kn*Tn/dy2 + ks*Ts/dy2 +
                                   q_dot[i][j] + h_eff * Ta;
                double denominator = (ke+kw)/dx2 + (kn+ks)/dy2 + h_eff;
                double t_gs = (denominator > 0.0) ? numerator / denominator : t_old;

                /* SOR extrapolation: T_new = T_old + omega * (T_gs - T_old) */
                double t_new = t_old + omega * (t_gs - t_old);
                field->T_c[i][j] = t_new;

                double change = fabs(t_new - t_old);
                if (change > max_change) max_change = change;
            }
        }

        field->iterations = iter + 1;
        if (max_change < tolerance) {
            field->converged = 1;
            field->residual = max_change;
            break;
        }
    }

    if (!field->converged) field->residual = max_change;

    for (int i = 0; i < rows; i++) free(q_dot[i]);
    free(q_dot);

    return field->converged ? THERMAL_OK : THERMAL_ERR_NO_CONVERGE;
}

double thermal_sor_optimal_omega(int rows, int cols, double bc_factor) {
    /* Optimal SOR parameter for Poisson equation on rectangular grid:
     * omega_opt = 2 / (1 + sqrt(1 - rho_jacobi^2))
     * where rho_jacobi = cos(pi/N) for N x N grid with Dirichlet BC.
     * For convection BC, multiply by bc_factor (~0.95-0.98). */
    int n = (rows > cols) ? rows : cols;
    if (n < 3) n = 3;
    double rho = cos(M_PI / (double)(n + 1));
    double omega = 2.0 / (1.0 + sqrt(1.0 - rho * rho));
    /* Apply boundary condition reduction */
    omega *= bc_factor;
    if (omega < 1.0) omega = 1.1;
    if (omega > 1.95) omega = 1.95;
    return omega;
}

double thermal_fdm_compute_residual(const thermal_field_t *field,
                                     double h_top, double h_bottom) {
    if (!field || field->rows < 3 || field->cols < 3) return INFINITY;

    double dx = field->dx_mm * 1.0e-3;
    double dy = field->dy_mm * 1.0e-3;
    double dz = field->thickness_mm * 1.0e-3;
    double dx2 = dx * dx, dy2 = dy * dy;
    double Ta = field->ambient.ambient_temp_c;
    double h_eff = (h_top + h_bottom) / dz;

    double sum_sq = 0.0;
    int count = 0;

    for (int i = 1; i < field->rows - 1; i++) {
        for (int j = 1; j < field->cols - 1; j++) {
            double Tp = field->T_c[i][j];
            double Te = field->T_c[i][j+1];
            double Tw = field->T_c[i][j-1];
            double Tn = field->T_c[i+1][j];
            double Ts = field->T_c[i-1][j];
            double kp = field->k_xy[i][j];

            /* Residual of discretized equation:
             * r = k*(Te+Tw-2*Tp)/dx^2 + k*(Tn+Ts-2*Tp)/dy^2 - h_eff*(Tp-Ta) */
            double rx = kp * (Te + Tw - 2.0 * Tp) / dx2;
            double ry = kp * (Tn + Ts - 2.0 * Tp) / dy2;
            double rc = -h_eff * (Tp - Ta);
            double r = rx + ry + rc;

            sum_sq += r * r;
            count++;
        }
    }

    return (count > 0) ? sqrt(sum_sq / count) : INFINITY;
}

/* ==================================================================
 * L5: Post-Processing and Analysis
 * ================================================================== */

int thermal_fdm_extract_junction_temps(const thermal_field_t *field,
                                        double *tj_array) {
    if (!field || !tj_array) return THERMAL_ERR_NULL_PTR;
    if (field->num_sources <= 0) return THERMAL_ERR_DIM_MISMATCH;

    for (int s = 0; s < field->num_sources; s++) {
        heat_source_t *src = field->sources[s];

        /* Average temperature in cells under component footprint */
        int c_min = (int)((src->center.x_mm - src->width_mm/2.0) / field->dx_mm);
        int c_max = (int)((src->center.x_mm + src->width_mm/2.0) / field->dx_mm);
        int r_min = (int)((src->center.y_mm - src->length_mm/2.0) / field->dy_mm);
        int r_max = (int)((src->center.y_mm + src->length_mm/2.0) / field->dy_mm);
        if (c_min < 0) c_min = 0;
        if (c_max >= field->cols) c_max = field->cols - 1;
        if (r_min < 0) r_min = 0;
        if (r_max >= field->rows) r_max = field->rows - 1;

        double t_avg = 0.0;
        int count = 0;
        for (int i = r_min; i <= r_max; i++) {
            for (int j = c_min; j <= c_max; j++) {
                t_avg += field->T_c[i][j];
                count++;
            }
        }

        if (count > 0) t_avg /= count;
        else t_avg = field->ambient.ambient_temp_c;

        /* Tj = T_board_under + P * Rjc */
        tj_array[s] = t_avg + src->power_w * src->r_jc;
    }

    return THERMAL_OK;
}

double thermal_fdm_max_temperature(const thermal_field_t *field) {
    if (!field || !field->T_c) return -273.15;
    double t_max = -273.15;
    for (int i = 0; i < field->rows; i++)
        for (int j = 0; j < field->cols; j++)
            if (field->T_c[i][j] > t_max)
                t_max = field->T_c[i][j];
    return t_max;
}

thermal_point_t thermal_fdm_max_temperature_location(const thermal_field_t *field) {
    thermal_point_t pt = {0.0, 0.0, 0.0};
    if (!field || !field->T_c) return pt;

    double t_max = -273.15;
    int max_i = 0, max_j = 0;
    for (int i = 0; i < field->rows; i++) {
        for (int j = 0; j < field->cols; j++) {
            if (field->T_c[i][j] > t_max) {
                t_max = field->T_c[i][j];
                max_i = i;
                max_j = j;
            }
        }
    }

    pt.x_mm = max_j * field->dx_mm;
    pt.y_mm = max_i * field->dy_mm;
    return pt;
}

double thermal_fdm_average_temperature(const thermal_field_t *field) {
    if (!field || !field->T_c || field->rows <= 0 || field->cols <= 0) return 0.0;
    double sum = 0.0;
    int count = 0;
    for (int i = 0; i < field->rows; i++)
        for (int j = 0; j < field->cols; j++) {
            sum += field->T_c[i][j];
            count++;
        }
    return (count > 0) ? sum / count : 0.0;
}

int thermal_fdm_compute_gradient(const thermal_field_t *field,
                                  double **grad_x, double **grad_y,
                                  double **grad_mag) {
    if (!field || field->rows < 3 || field->cols < 3) return THERMAL_ERR_NULL_PTR;

    double dx = field->dx_mm;
    double dy = field->dy_mm;

    for (int i = 1; i < field->rows - 1; i++) {
        for (int j = 1; j < field->cols - 1; j++) {
            /* Central difference for gradient */
            double gx = (field->T_c[i][j+1] - field->T_c[i][j-1]) / (2.0 * dx);
            double gy = (field->T_c[i+1][j] - field->T_c[i-1][j]) / (2.0 * dy);

            if (grad_x) grad_x[i][j] = gx;
            if (grad_y) grad_y[i][j] = gy;
            if (grad_mag) grad_mag[i][j] = sqrt(gx * gx + gy * gy);
        }
    }

    /* Zero out boundaries */
    for (int j = 0; j < field->cols; j++) {
        if (grad_x) grad_x[0][j] = grad_x[field->rows-1][j] = 0.0;
        if (grad_y) grad_y[0][j] = grad_y[field->rows-1][j] = 0.0;
        if (grad_mag) grad_mag[0][j] = grad_mag[field->rows-1][j] = 0.0;
    }
    for (int i = 0; i < field->rows; i++) {
        if (grad_x) grad_x[i][0] = grad_x[i][field->cols-1] = 0.0;
        if (grad_y) grad_y[i][0] = grad_y[i][field->cols-1] = 0.0;
        if (grad_mag) grad_mag[i][0] = grad_mag[i][field->cols-1] = 0.0;
    }

    return THERMAL_OK;
}

double thermal_fdm_area_above_temperature(const thermal_field_t *field,
                                           double t_threshold_c) {
    if (!field || !field->T_c) return 0.0;
    int count_above = 0;
    for (int i = 0; i < field->rows; i++)
        for (int j = 0; j < field->cols; j++)
            if (field->T_c[i][j] > t_threshold_c)
                count_above++;
    return count_above * field->dx_mm * field->dy_mm;
}
