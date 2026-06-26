/**
 * pcb_thermal_analysis.h — Steady-State and Transient Thermal Analysis
 *
 * L2: Core concepts — Fourier's law, Newton's law of cooling,
 *     thermal resistance networks, heat spreading in planes.
 * L3: Mathematical structures — heat equation, Poisson equation,
 *     Kirchhoff transformation for temperature-dependent conductivity.
 * L4: Fundamental laws — Conservation of energy, Fourier conduction,
 *     Newton convection, Stefan-Boltzmann radiation, Rth series/parallel.
 *
 * Courses: MIT 6.630 EM Waves (heat eq), Berkeley EE105 (power devices),
 *          Stanford EE359 Wireless (RF PA thermal), Michigan EECS 411 Microwave
 * Reference: Fourier, "Theorie analytique de la chaleur" (1822)
 *            Cengel & Ghajar, "Heat and Mass Transfer" (2014), Ch. 1-4
 *            JEDEC JESD51-1 "Integrated Circuit Thermal Measurement Method"
 */

#ifndef PCB_THERMAL_ANALYSIS_H
#define PCB_THERMAL_ANALYSIS_H

#include "pcb_thermal_defs.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================================================================
 * L2: Steady-State 1D Thermal Resistance Calculations
 * ================================================================== */

/** Compute 1D conduction thermal resistance through a plane slab.
 *  L2/L4 - Fourier's Law: R_cond = L / (k * A)
 *  where L = thickness (m), k = thermal conductivity (W/m-K), A = cross-section (m^2)
 *
 *  This is the fundamental building block for all thermal resistance networks.
 *  For PCB analysis, used for:
 *  - Through-thickness resistance of dielectric layers
 *  - Heat sink base spreading resistance (simplified)
 *  - TIM resistance (Rcs calculation)
 *
 *  @param thickness_mm  Slab thickness (mm)
 *  @param k             Thermal conductivity (W/m-K)
 *  @param area_mm2      Cross-sectional area (mm^2)
 *  @return              Thermal resistance (C/W)
 *  Complexity: O(1) */
double thermal_resistance_conduction(double thickness_mm, double k, double area_mm2);

/** Compute convection thermal resistance.
 *  L2/L4 - Newton's Law of Cooling: q = h * A * (Ts - Tinf)
 *  Therefore: R_conv = 1 / (h * A)
 *  where h = heat transfer coefficient (W/m^2-K)
 *
 *  Natural convection h: 2-25 W/m^2-K (depending on orientation, size, delta-T)
 *  Forced convection h: 25-250 W/m^2-K (depending on velocity)
 *  PCB typical h for natural convection: ~5-15 W/m^2-K
 *
 *  @param h          Heat transfer coefficient (W/m^2-K)
 *  @param area_mm2   Surface area (mm^2)
 *  @return           Convection thermal resistance (C/W)
 *  Complexity: O(1) */
double thermal_resistance_convection(double h, double area_mm2);

/** Compute radiation thermal resistance (linearized).
 *  L2/L4 - Stefan-Boltzmann Law: q_rad = epsilon * sigma * A * (Ts^4 - Tinf^4)
 *  Linearized form: q_rad ≈ h_rad * A * (Ts - Tinf)
 *  where h_rad = epsilon * sigma * (Ts^2 + Tinf^2) * (Ts + Tinf)
 *  sigma = 5.67e-8 W/m^2-K^4 (Stefan-Boltzmann constant)
 *
 *  For typical electronics (Ts ~60-100 C, Tinf ~25 C):
 *  h_rad ≈ 5-8 W/m^2-K for epsilon = 0.9 (black anodized aluminum, typical PCB)
 *  h_rad ≈ 1-2 W/m^2-K for epsilon = 0.2 (bare aluminum, shiny copper)
 *
 *  @param epsilon       Surface emissivity (0-1)
 *  @param ts_c          Surface temperature (C)
 *  @param tinf_c        Ambient/surroundings temperature (C)
 *  @param area_mm2      Radiating surface area (mm^2)
 *  @return              Radiation thermal resistance (C/W)
 *  Complexity: O(1) */
double thermal_resistance_radiation(double epsilon, double ts_c,
                                     double tinf_c, double area_mm2);

/** Compute spreading resistance for a circular heat source on a rectangular plane.
 *  L2/L4: Heat spreading in a thin plate.
 *  Using the Kennedy model (simplified):
 *  R_spread = (1 / (2 * pi * k * t)) * ln(sqrt(A_plane / A_source))
 *
 *  Spreading resistance accounts for the constriction of heat flow
 *  from a small component to a larger copper plane.
 *  This model is most accurate when the plane is relatively thin
 *  compared to the source dimensions.
 *
 *  @param k          Thermal conductivity of spreading layer (W/m-K)
 *  @param t_mm       Thickness of spreading layer (mm)
 *  @param area_source_mm2  Source footprint area (mm^2)
 *  @param area_plane_mm2   Total plane area (mm^2)
 *  @return           Spreading thermal resistance (C/W)
 *  Complexity: O(1) */
double thermal_resistance_spreading(double k, double t_mm,
                                     double area_source_mm2, double area_plane_mm2);

/** Compute series equivalent of two thermal resistances.
 *  L2/L4: R_series = R1 + R2
 *  Analogous to electrical series resistance.
 *  Used for: Rja = Rjc + Rcs + Rsa  (junction through heatsink to air) */
double thermal_resistance_series(double r1, double r2);

/** Compute parallel equivalent of two thermal resistances.
 *  L2/L4: R_parallel = 1 / (1/R1 + 1/R2)
 *  Analogous to electrical parallel resistance.
 *  Used for: dual heat paths (top side sink + board conduction)
 *  or multiple vias in parallel. */
double thermal_resistance_parallel(double r1, double r2);

/** Compute N equal thermal resistances in parallel.
 *  L2/L4: R_N = R_single / N (ideal) or R_single / (N * efficiency) (real)
 *  Used for thermal via arrays.
 *  @param r_single  Single resistance (C/W)
 *  @param n         Number in parallel
 *  @return          Combined resistance (C/W) */
double thermal_resistance_n_parallel(double r_single, int n);

/* ==================================================================
 * L2: Junction Temperature Calculation (Complete Network Solver)
 * ================================================================== */

/** Calculate junction temperature for a single heat source.
 *  L2/L4: Complete thermal resistance network analysis.
 *
 *  Network topology:
 *  Tj --[Rjc]-- Tc --[Rcs]-- Tsink --[Rsa]-- Tambient (top path)
 *  Tj --[Rjb]-- Tboard --[Rba]-- Tambient               (bottom path)
 *
 *  The two paths are in parallel from junction to ambient.
 *  This solves the complete network using Kirchhoff-like rules.
 *
 *  @param source        Heat source with Rjc, power, and max_tj set
 *  @param tim           TIM properties (set r_cs before calling, or leave as 0)
 *  @param r_sa          Sink-to-ambient resistance (C/W), from heatsink calculation
 *  @param r_jb          Junction-to-board resistance (C/W)
 *  @param r_ba          Board-to-ambient resistance (C/W)
 *  @param ta_c          Ambient temperature (C)
 *  @param tj_out        Output: calculated junction temperature (C)
 *  @param tc_out        Output: calculated case temperature (C)
 *  @param tb_out        Output: calculated board temperature (C)
 *  @return              THERMAL_OK or error code
 *  Complexity: O(1) */
int calculate_junction_temperature(const heat_source_t *source,
                                    const tim_properties_t *tim,
                                    double r_sa, double r_jb, double r_ba,
                                    double ta_c,
                                    double *tj_out, double *tc_out, double *tb_out);

/** Calculate mutual heating between two components on a PCB.
 *  L2/L4: Temperature superposition principle.
 *  The temperature rise at component i due to component j:
 *    Delta_Tij = Pj * R_thermal_coupling(i,j)
 *  where R_thermal_coupling depends on distance and board properties.
 *
 *  For an isotropic board with distance d between centers:
 *    R_coupling ≈ R_spread(|r_i - r_j|)
 *    using exponential decay model: R_coupling = R0 * exp(-d / L_char)
 *    where L_char = sqrt(k_board * t / h_conv) is the characteristic length.
 *
 *  @param ta_c           Ambient temperature (C)
 *  @param src1           First heat source
 *  @param src2           Second heat source
 *  @param distance_mm    Center-to-center distance (mm)
 *  @param k_board_xy     Board in-plane thermal conductivity (W/m-K)
 *  @param t_board_mm     Board thickness (mm)
 *  @param h_conv         Convection coefficient (W/m^2-K)
 *  @param tj1_out        Output: Tj for source 1 (with mutual heating)
 *  @param tj2_out        Output: Tj for source 2 (with mutual heating)
 *  @return               THERMAL_OK or error code
 *  Complexity: O(1) */
int calculate_mutual_heating(double ta_c,
                              const heat_source_t *src1, const heat_source_t *src2,
                              double distance_mm,
                              double k_board_xy, double t_board_mm, double h_conv,
                              double *tj1_out, double *tj2_out);

/* ==================================================================
 * L2: Heat Sink Performance Analysis
 * ================================================================== */

/** Calculate natural convection heat transfer coefficient for a flat plate.
 *  L2/L4: Uses empirical Nusselt number correlation.
 *
 *  For vertical plate: Nu = 0.59 * Ra^(1/4)  for 10^4 < Ra < 10^9 (laminar)
 *                       Nu = 0.10 * Ra^(1/3)  for 10^9 < Ra < 10^13 (turbulent)
 *
 *  For horizontal plate, heated top: Nu = 0.54 * Ra^(1/4)  for 10^4 < Ra < 10^7
 *
 *  Rayleigh number: Ra = Gr * Pr = g*beta*(Ts-Tinf)*L^3 / (nu*alpha)
 *  where Gr = Grashof number, Pr = Prandtl number (~0.71 for air)
 *  g = 9.81 m/s^2, beta = 1/Tfilm (ideal gas), nu = kinematic viscosity
 *
 *  @param ts_c          Surface temperature (C)
 *  @param tinf_c        Ambient fluid temperature (C)
 *  @param length_mm     Characteristic length (mm) — plate height for vertical
 *  @param correlation   Which correlation to use
 *  @param ambient       Ambient conditions (provides air properties)
 *  @param h_out         Output: convection coefficient (W/m^2-K)
 *  @return              THERMAL_OK or error code
 *  Complexity: O(1) */
int natural_convection_coefficient(double ts_c, double tinf_c,
                                    double length_mm,
                                    convection_correlation_t correlation,
                                    const ambient_conditions_t *ambient,
                                    double *h_out);

/** Calculate forced convection heat transfer coefficient.
 *  L2/L4: Flat plate forced convection.
 *
 *  Laminar (Re < 5e5):  Nu = 0.664 * Re^(1/2) * Pr^(1/3)
 *  Turbulent (Re > 5e5): Nu = 0.037 * Re^(4/5) * Pr^(1/3)
 *
 *  Reynolds number: Re = rho * V * L / mu = V * L / nu
 *  where V = air velocity, L = plate length in flow direction
 *
 *  @param velocity_ms   Airflow velocity (m/s)
 *  @param length_mm     Plate length in flow direction (mm)
 *  @param ambient       Ambient conditions
 *  @param h_out         Output: convection coefficient (W/m^2-K)
 *  @return              THERMAL_OK or error code
 *  Complexity: O(1) */
int forced_convection_coefficient(double velocity_ms, double length_mm,
                                   const ambient_conditions_t *ambient,
                                   double *h_out);

/** Compute heat sink sink-to-ambient resistance for natural convection.
 *  L2/L6: Complete fin analysis using fin efficiency method.
 *
 *  Total: R_sa = 1 / (h * A_base + h * N_fins * eta_fin * A_fin)
 *  where eta_fin = tanh(m * H) / (m * H)
 *  m = sqrt(2 * h / (k_fin * t_fin)) for a fin with adiabatic tip
 *
 *  @param hs          Heat sink geometry (fin dimensions, count, material)
 *  @param ts_c        Estimated surface temperature (C)
 *  @param ambient     Ambient conditions
 *  @param r_sa_out    Output: sink-to-ambient resistance (C/W)
 *  @return            THERMAL_OK or error code
 *  Complexity: O(1) */
int heatsink_natural_convection_r_sa(heat_sink_model_t *hs,
                                      double ts_c,
                                      const ambient_conditions_t *ambient,
                                      double *r_sa_out);

/** Compute heat sink sink-to-ambient resistance for forced convection.
 *  L2/L6: Uses forced convection correlation for fin channels.
 *
 *  For airflow through parallel plate channels (fin gaps):
 *  Hydraulic diameter Dh = 2 * s * H / (s + H) where s = gap, H = fin height
 *  For narrow channels (s << H): Dh ≈ 2 * s
 *
 *  @param hs          Heat sink geometry
 *  @param velocity_ms Airflow velocity approaching fins (m/s)
 *  @param ambient     Ambient conditions
 *  @param r_sa_out    Output: sink-to-ambient resistance (C/W)
 *  @return            THERMAL_OK or error code
 *  Complexity: O(1) */
int heatsink_forced_convection_r_sa(heat_sink_model_t *hs,
                                     double velocity_ms,
                                     const ambient_conditions_t *ambient,
                                     double *r_sa_out);

/* ==================================================================
 * L3: Transient Thermal Analysis
 * ================================================================== */

/** Compute transient temperature response using lumped capacitance model.
 *  L2/L3: When Biot number Bi = h*L_c/k < 0.1, the body can be treated
 *  as having uniform temperature (no internal gradients).
 *
 *  Lumped capacitance solution:
 *    T(t) = Tinf + (T0 - Tinf) * exp(-t / tau)
 *  where tau = rho * V * cp / (h * As) = R_th * C_th
 *  C_th = thermal capacitance (J/K) = rho * V * cp
 *  R_th = 1 / (h * As)
 *
 *  For PCB components: Bi ~ h*t/k_Cu ≈ 5*0.001/385 ≈ 1.3e-5 → valid!
 *
 *  @param t0_c         Initial temperature (C)
 *  @param tinf_c       Ambient temperature (C)
 *  @param mass_g       Component mass (g)
 *  @param cp_jkgk      Specific heat (J/kg-K)
 *  @param h            Convection coefficient (W/m^2-K)
 *  @param area_mm2     Surface area for cooling (mm^2)
 *  @param time_s       Time to evaluate (seconds)
 *  @return             Temperature at time t (C)
 *  Complexity: O(1) */
double transient_lumped_capacitance(double t0_c, double tinf_c,
                                     double mass_g, double cp_jkgk,
                                     double h, double area_mm2,
                                     double time_s);

/** Compute time constant for a thermal RC network (Foster model).
 *  L2/L3: tau = R_th * C_th
 *  R_th = thermal resistance (C/W), C_th = thermal capacitance (J/K)
 *  C_th = mass * cp for lumped mass, or rho * V * cp */
double thermal_time_constant(double r_th, double mass_g, double cp_jkgk);

/** Compute transient temperature using Foster RC ladder model.
 *  L3: Foster network — multiple RC pairs in series (parallel RC to ground).
 *    T(t) = Ta + P * sum_i [ R_i * (1 - exp(-t / (R_i * C_i))) ]
 *
 *  Foster model is mathematically convenient but does NOT represent
 *  physical layers (unlike Cauer model). Used in JEDEC JESD51-14
 *  for transient thermal impedance characterization.
 *
 *  @param ta_c         Ambient temperature (C)
 *  @param power_w      Heat dissipation (W)
 *  @param r_vals       Array of thermal resistance values (C/W)
 *  @param c_vals       Array of thermal capacitance values (J/K)
 *  @param stages       Number of Foster stages
 *  @param time_s       Time to evaluate (seconds)
 *  @return             Junction temperature at time t (C)
 *  Complexity: O(stages) */
double transient_foster_model(double ta_c, double power_w,
                               const double *r_vals, const double *c_vals,
                               int stages, double time_s);

/** Compute transient temperature using Cauer RC ladder model.
 *  L3: Cauer network — alternating series R and shunt C to ground.
 *  This physically represents layers in a 1D stack.
 *
 *  The Cauer model is solved numerically via forward Euler:
 *    C_i * dTi/dt = (T_{i-1} - Ti)/R_i - (Ti - T_{i+1})/R_{i+1}
 *
 *  @param ta_c         Ambient temperature (C)
 *  @param power_w      Heat dissipation (W)
 *  @param r_vals       Array of thermal resistance values (C/W)
 *  @param c_vals       Array of thermal capacitance values (J/K)
 *  @param stages       Number of Cauer stages
 *  @param time_s       Time to evaluate (seconds)
 *  @param dt_s         Time step for numerical integration (seconds)
 *  @param tj_out       Output: junction temperature at time t (C)
 *  @return             THERMAL_OK or error code
 *  Complexity: O(stages * time_s/dt_s) */
int transient_cauer_model(double ta_c, double power_w,
                           const double *r_vals, const double *c_vals,
                           int stages, double time_s, double dt_s,
                           double *tj_out);

/** Compute transient thermal impedance Zth(t) from Foster model.
 *  L3: Zth(t) = sum_i [ R_i * (1 - exp(-t / tau_i)) ]
 *  where tau_i = R_i * C_i
 *
 *  Zth(t) approaches Rja (steady-state) as t -> infinity.
 *  Used for pulsed power applications where duty cycle matters. */
double transient_thermal_impedance(const double *r_vals, const double *c_vals,
                                    int stages, double time_s);

/* ==================================================================
 * L3: Heat Equation Numerical Parameters
 * ================================================================== */

/** Compute Biot number — criterion for lumped capacitance validity.
 *  L3: Bi = h * Lc / k
 *  where Lc = V / As (characteristic length = volume / surface area)
 *  Bi < 0.1 → lumped capacitance is valid (error < 5%)
 *  Bi > 0.1 → internal temperature gradients must be considered
 *
 *  For PCB copper (k=385 W/m-K): with h=10 W/m^2-K and Lc=1mm → Bi≈2.6e-5 */
double biot_number(double h, double volume_mm3, double surface_area_mm2, double k);

/** Compute Fourier number — dimensionless time for transient conduction.
 *  L3: Fo = alpha * t / Lc^2
 *  where alpha = k/(rho*cp) = thermal diffusivity (m^2/s)
 *  Fo > 0.2 → significant temperature change has propagated through body
 *  Fo > 1.0 → near steady-state conditions */
double fourier_number(double k, double density, double cp,
                       double time_s, double char_length_mm);

/** Compute the characteristic thermal spreading length in a PCB.
 *  L3: L_char = sqrt(k_eff * t / h)
 *  where k_eff = effective in-plane thermal conductivity
 *  t = board thickness, h = convection coefficient
 *
 *  This length determines how far heat spreads laterally before
 *  being removed by convection. Beyond ~3*L_char, additional copper
 *  adds negligible benefit. */
double characteristic_spreading_length(double k_effective_xy,
                                        double thickness_mm, double h);

/* ==================================================================
 * L3: Thermal Resistance Network Matrix Solver
 * ================================================================== */

/** Solve a thermal resistance network using Kirchhoff-type nodal analysis.
 *  L3/L5: For N nodes with known power inputs and unknown temperatures.
 *
 *  The system is: G * T = Q
 *  where G = thermal conductance matrix (N x N)
 *  T = node temperature vector
 *  Q = node power input vector
 *
 *  Conductance matrix entries:
 *  G_ii = sum_j(1/R_ij) + 1/R_i_ambient  (sum of conductances to other nodes + ambient)
 *  G_ij = -1/R_ij  (off-diagonal, negative conductance)
 *
 *  This is a sparse, symmetric, positive-definite system solved by
 *  Gauss-Seidel iteration (no matrix inversion needed for thermal nets).
 *
 *  @param n_nodes        Number of thermal nodes
 *  @param r_matrix       Resistance matrix NxN (R[i][j] = resistance between i and j, 0 = no connection)
 *  @param r_ambient      Array of resistances to ambient for each node
 *  @param power_w        Array of power inputs for each node (W)
 *  @param ta_c           Ambient temperature (C)
 *  @param t_out          Output: temperature of each node (C), must be pre-allocated
 *  @param max_iter       Maximum relaxation iterations
 *  @param tol            Convergence tolerance (C)
 *  @return               THERMAL_OK or error code
 *  Complexity: O(max_iter * n_nodes^2) */
int thermal_network_solve(int n_nodes,
                           const double **r_matrix,
                           const double *r_ambient,
                           const double *power_w,
                           double ta_c,
                           double *t_out,
                           int max_iter, double tol);

#ifdef __cplusplus
}
#endif

#endif /* PCB_THERMAL_ANALYSIS_H */