/**
 * pcb_thermal_simulation.h — 2D Finite Difference Thermal Simulation for PCB
 *
 * L5: Finite Difference Method (FDM) for steady-state heat equation.
 * L6: Solving ∇^2T = -q̇/k on a PCB with multiple heat sources,
 *     convection boundaries, and material inhomogeneities.
 *
 * Courses: MIT 6.630 EM Waves, Berkeley EE117 EM, Stanford EE247 Optical,
 *          TU Munich High-Frequency Eng., ETH 227-0455 EM
 * Reference: Patankar, "Numerical Heat Transfer and Fluid Flow" (1980)
 *            Reddy, "An Introduction to the Finite Element Method" (2005)
 *            Cengel, "Heat and Mass Transfer", Ch. 5 (Numerical Methods)
 */

#ifndef PCB_THERMAL_SIMULATION_H
#define PCB_THERMAL_SIMULATION_H

#include "pcb_thermal_defs.h"
#include "pcb_thermal_analysis.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L5: Grid Generation and Material Mapping
 * ================================================================== */

/** Initialize a thermal field structure for FDM simulation.
 *  L5: Allocates and zeros the temperature and conductivity grids.
 *
 *  @param width_mm       Board width (mm)
 *  @param height_mm      Board height (mm)
 *  @param thickness_mm   Board thickness (mm)
 *  @param dx_mm          Grid spacing in X (mm)
 *  @param dy_mm          Grid spacing in Y (mm)
 *  @param ambient        Ambient conditions for boundary
 *  @return               Initialized thermal field, or NULL on error
 *  Complexity: O(rows * cols) */
thermal_field_t *thermal_field_create(double width_mm, double height_mm,
                                       double thickness_mm,
                                       double dx_mm, double dy_mm,
                                       const ambient_conditions_t *ambient);

/** Free a thermal field and all associated memory.
 *  Complexity: O(rows * cols) */
void thermal_field_free(thermal_field_t *field);

/** Set the board thermal conductivity grid uniformly.
 *  L5: Assign k_xy to all cells. For a uniform board this is constant.
 *  For multi-layer boards, k_xy = effective in-plane conductivity. */
void thermal_field_set_uniform_k(thermal_field_t *field, double k_xy);

/** Set the thermal conductivity grid from a PCB stack-up.
 *  L5: Maps the multi-layer stack properties to the 2D grid.
 *  k_effective_xy is used for the in-plane grid values. */
void thermal_field_set_stackup_k(thermal_field_t *field,
                                  const pcb_stackup_t *stackup);

/** Set thermal conductivity for a region (e.g., copper pour area).
 *  L5: Creates a rectangular region of different conductivity,
 *  representing a copper pour or thermal pad on a specific layer. */
int thermal_field_set_region_k(thermal_field_t *field,
                                double x_center_mm, double y_center_mm,
                                double width_mm, double height_mm,
                                double k_value);

/** Set a copper pour region using pour geometry.
 *  L5: Maps a copper pour geometry to the grid, computing the
 *  contribution of the copper to the effective in-plane conductivity
 *  in the covered region.
 *
 *  Effective k in pour region = (k_Cu * t_cu + k_FR4 * t_FR4) / (t_cu + t_FR4)
 *  For typical 1oz on 1.6mm FR4: k_eff = (385*0.035 + 0.3*1.565) / 1.6 = 8.7 W/m-K */
int thermal_field_set_copper_pour(thermal_field_t *field,
                                   const copper_pour_geom_t *pour,
                                   double x_center_mm, double y_center_mm);

/** Add a heat source to the thermal field.
 *  L5: Maps component power dissipation to cells under its footprint.
 *  The heat flux is distributed uniformly over the component area.
 *  Components are stored for later temperature queries.
 *
 *  @return index of new source, or -1 on error */
int thermal_field_add_heat_source(thermal_field_t *field,
                                   const heat_source_t *source);

/* ==================================================================
 * L5: Finite Difference Solver — Steady State
 * ================================================================== */

/** Solve the steady-state heat equation using Gauss-Seidel iteration.
 *  L5/L6: The 2D steady-state heat equation with sources:
 *    ∂/∂x(k ∂T/∂x) + ∂/∂y(k ∂T/∂y) + q̇''' = 0
 *
 *  Finite difference discretization (5-point stencil):
 *    k_e*(Te - Tp)/dx^2 + k_w*(Tw - Tp)/dx^2 +
 *    k_n*(Tn - Tp)/dy^2 + k_s*(Ts - Tp)/dy^2 + q̇''' = 0
 *
 *  Solving for Tp:
 *    Tp = (k_e*Te + k_w*Tw + k_n*Tn*dx^2/dy^2 + k_s*Ts*dx^2/dy^2 + q̇'''*dx^2)
 *         / (k_e + k_w + (k_n + k_s)*dx^2/dy^2 + h*dx^2/k)
 *
 *  Boundary conditions (convection at edges):
 *    -k*∂T/∂n = h*(T - Ta)  →  implemented as convective BC
 *
 *  Convergence criteria: max(|T_new - T_old|) < tolerance
 *
 *  @param field         Thermal field (initialized with geometry and sources)
 *  @param h_top         Top surface convection coefficient (W/m^2-K)
 *  @param h_bottom      Bottom surface convection coefficient (W/m^2-K)
 *  @param tolerance     Convergence tolerance (C)
 *  @param max_iter      Maximum iterations
 *  @return              THERMAL_OK or error code
 *  Complexity: O(max_iter * rows * cols) */
int thermal_fdm_solve_steady(thermal_field_t *field,
                              double h_top, double h_bottom,
                              double tolerance, int max_iter);

/** Solve using Successive Over-Relaxation (SOR) for faster convergence.
 *  L5: SOR accelerates Gauss-Seidel by extrapolating the correction.
 *
 *  T_new = T_old + omega * (T_gs - T_old)
 *  where T_gs is the Gauss-Seidel value and omega ∈ (1, 2) is the
 *  relaxation factor. omega=1 recovers Gauss-Seidel.
 *
 *  Optimal omega for Poisson equation on rectangular grid:
 *    omega_opt = 2 / (1 + sin(pi/(N+1)))
 *  where N = max(rows, cols).
 *
 *  SOR typically converges 2-5x faster than Gauss-Seidel.
 *
 *  @param field     Thermal field
 *  @param h_top     Top convection coefficient
 *  @param h_bottom  Bottom convection coefficient
 *  @param omega     Relaxation factor (1 < omega < 2)
 *  @param tolerance Convergence tolerance
 *  @param max_iter  Maximum iterations
 *  @return          THERMAL_OK or error code
 *  Complexity: O(max_iter * rows * cols) */
int thermal_fdm_solve_sor(thermal_field_t *field,
                           double h_top, double h_bottom,
                           double omega, double tolerance, int max_iter);

/** Calculate the optimal SOR relaxation parameter.
 *  L5: omega_opt = 2 / (1 + sqrt(1 - rho_jacobi^2))
 *  where rho_jacobi = cos(pi/M) for M x N grid with Dirichlet BC
 *  For convection BC, a slight reduction (0.95-0.98 factor) is used.
 *
 *  @param rows       Grid rows
 *  @param cols       Grid cols
 *  @param bc_factor  Reduction for convection BC (typically 0.96)
 *  @return           Optimal omega (1 < omega < 2) */
double thermal_sor_optimal_omega(int rows, int cols, double bc_factor);

/** Compute the residual of the discretized heat equation.
 *  L5: r = Ax - b, where A is the discretized PDE operator.
 *  The residual norm is used to monitor convergence and assess
 *  solution quality. A well-converged solution has residual < 1e-6.
 *
 *  @param field     Thermal field with current solution
 *  @param h_top     Top convection coefficient
 *  @param h_bottom  Bottom convection coefficient
 *  @return          L2 norm of residual
 *  Complexity: O(rows * cols) */
double thermal_fdm_compute_residual(const thermal_field_t *field,
                                     double h_top, double h_bottom);

/* ==================================================================
 * L5: Post-Processing and Visualization
 * ================================================================== */

/** Extract junction temperature for all components in the field.
 *  L5: For each component, compute Tj = T_field_under_component + P * Rjc.
 *  The field temperature under the component is the average of grid cells
 *  covered by the component footprint.
 *
 *  @param field        Thermal field with converged solution
 *  @param tj_array     Output: array of junction temperatures, must be
 *                      pre-allocated with field->num_sources entries
 *  @return             THERMAL_OK or error code
 *  Complexity: O(num_sources * footprint_cells) */
int thermal_fdm_extract_junction_temps(const thermal_field_t *field,
                                        double *tj_array);

/** Find the maximum temperature in the thermal field.
 *  L5: Scans the entire grid for the hottest point.
 *  Useful for quick hot-spot identification. */
double thermal_fdm_max_temperature(const thermal_field_t *field);

/** Find the location of the maximum temperature on the board.
 *  L5: Returns the (x, y) coordinates of the hottest grid point.
 *  Useful for thermal design verification and hot-spot mitigation. */
thermal_point_t thermal_fdm_max_temperature_location(const thermal_field_t *field);

/** Compute the average board temperature.
 *  L5: Area-weighted average of all grid cell temperatures.
 *  Used for overall thermal performance assessment. */
double thermal_fdm_average_temperature(const thermal_field_t *field);

/** Compute thermal gradient magnitude at each point.
 *  L5: |∇T| = sqrt((∂T/∂x)^2 + (∂T/∂y)^2)
 *  Central difference: ∂T/∂x ≈ (T[i+1][j] - T[i-1][j]) / (2*dx)
 *
 *  High gradients indicate areas of concentrated heat flux,
 *  which are candidates for additional copper or thermal vias.
 *
 *  @param field    Thermal field
 *  @param grad_x   Output: X-gradient grid (field->rows x field->cols), or NULL
 *  @param grad_y   Output: Y-gradient grid, or NULL
 *  @param grad_mag Output: Gradient magnitude grid, or NULL
 *  @return         THERMAL_OK or error code
 *  Complexity: O(rows * cols) */
int thermal_fdm_compute_gradient(const thermal_field_t *field,
                                  double **grad_x, double **grad_y,
                                  double **grad_mag);

/** Estimate the area where temperature exceeds a specified threshold.
 *  L5: Counts grid cells where T > T_threshold and returns total area. */
double thermal_fdm_area_above_temperature(const thermal_field_t *field,
                                           double t_threshold_c);

#ifdef __cplusplus
}
#endif

#endif /* PCB_THERMAL_SIMULATION_H */