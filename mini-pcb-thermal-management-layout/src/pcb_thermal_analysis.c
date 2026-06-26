/**
 * pcb_thermal_analysis.c - Steady-State and Transient Thermal Analysis Implementation
 *
 * L2: Fourier conduction, Newton convection, Stefan-Boltzmann radiation.
 * L3: Heat equation, Poisson equation, transient RC ladder networks.
 * L4: Conservation of energy, Rth series/parallel, numerical network solver.
 *
 * Reference: Fourier (1822), Cengel & Ghajar (2014), JEDEC JESD51 series.
 */

#include "pcb_thermal_analysis.h"
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define STEFAN_BOLTZMANN 5.670374419e-8  /* W/m^2-K^4 */
#define GRAVITY 9.80665                   /* m/s^2 */

/* ==================================================================
 * L2/L4: Steady-State 1D Thermal Resistance Calculations
 * ================================================================== */

double thermal_resistance_conduction(double thickness_mm, double k, double area_mm2) {
    if (thickness_mm <= 0.0 || k <= 0.0 || area_mm2 <= 0.0) return INFINITY;
    /* Fourier's Law in 1D: R_cond = L / (k * A)
     * Convert mm to m: thickness_m * 1e-3, area_m2 = area_mm2 * 1e-6
     * R = (L * 1e-3) / (k * A * 1e-6) = L / (k * A) * 1e3 */
    return thickness_mm / (k * area_mm2) * 1.0e3;
}

double thermal_resistance_convection(double h, double area_mm2) {
    if (h <= 0.0 || area_mm2 <= 0.0) return INFINITY;
    /* Newton's Law of Cooling: R_conv = 1 / (h * A)
     * A in m^2 = area_mm2 * 1e-6
     * R = 1 / (h * A * 1e-6) = 1e6 / (h * A) */
    return 1.0e6 / (h * area_mm2);
}

double thermal_resistance_radiation(double epsilon, double ts_c,
                                     double tinf_c, double area_mm2) {
    if (epsilon <= 0.0 || area_mm2 <= 0.0) return INFINITY;
    /* Stefan-Boltzmann linearized radiation:
     * q_rad = epsilon * sigma * A * (Ts^4 - Tinf^4)
     * Linearize: h_rad = epsilon * sigma * (Ts^2 + Tinf^2) * (Ts + Tinf)
     * R_rad = 1 / (h_rad * A) */
    double ts_k = ts_c + 273.15;
    double tinf_k = tinf_c + 273.15;
    if (ts_k <= 0.0 || tinf_k <= 0.0) return INFINITY;
    double h_rad = epsilon * STEFAN_BOLTZMANN *
                   (ts_k * ts_k + tinf_k * tinf_k) * (ts_k + tinf_k);
    if (h_rad <= 0.0) return INFINITY;
    return 1.0e6 / (h_rad * area_mm2);
}

double thermal_resistance_spreading(double k, double t_mm,
                                     double area_source_mm2, double area_plane_mm2) {
    if (k <= 0.0 || t_mm <= 0.0 || area_source_mm2 <= 0.0 || area_plane_mm2 <= 0.0)
        return INFINITY;
    if (area_plane_mm2 <= area_source_mm2) return 0.0;
    /* Kennedy spreading model for thin plate:
     * R_spread = 1/(2*pi*k*t) * ln(sqrt(A_plane/A_source))
     * Equivalent radius ratio: r_plane/r_source = sqrt(A_plane/A_source)
     * t in meters: t_mm * 1e-3 */
    double ratio = sqrt(area_plane_mm2 / area_source_mm2);
    return log(ratio) / (2.0 * M_PI * k * t_mm * 1.0e-3);
}

double thermal_resistance_series(double r1, double r2) {
    if (r1 < 0.0 || r2 < 0.0) return INFINITY;
    return r1 + r2;
}

double thermal_resistance_parallel(double r1, double r2) {
    if (r1 <= 0.0 || r2 <= 0.0) return INFINITY;
    return 1.0 / (1.0 / r1 + 1.0 / r2);
}

double thermal_resistance_n_parallel(double r_single, int n) {
    if (r_single <= 0.0 || n <= 0) return INFINITY;
    return r_single / (double)n;
}

/* ==================================================================
 * L2/L4: Junction Temperature Calculation (Complete Network Solver)
 * ================================================================== */

int calculate_junction_temperature(const heat_source_t *source,
                                    const tim_properties_t *tim,
                                    double r_sa, double r_jb, double r_ba,
                                    double ta_c,
                                    double *tj_out, double *tc_out, double *tb_out) {
    if (!source || !tj_out || !tc_out || !tb_out) return THERMAL_ERR_NULL_PTR;
    if (source->power_w < 0.0) return THERMAL_ERR_NEG_POWER;
    if (r_sa < 0.0 || r_jb < 0.0 || r_ba < 0.0) return THERMAL_ERR_INVALID_ENV;

    double r_jc = source->r_jc;
    double r_cs = (tim && tim->r_cs > 0.0) ? tim->r_cs : 0.0;

    /* Top path: Tj -> Rjc -> Tc -> Rcs -> Tsink -> Rsa -> Ta */
    double r_top = r_jc + r_cs + r_sa;
    /* Bottom path: Tj -> Rjb -> Tboard -> Rba -> Ta */
    double r_bot = r_jb + r_ba;

    /* Total Rja = R_top || R_bot (parallel paths)
     * P_top = (R_bot / (R_top + R_bot)) * P_total  (current divider analogy)
     * P_bot = (R_top / (R_top + R_bot)) * P_total
     * Tj = Ta + P_total * (R_top * R_bot) / (R_top + R_bot) */
    double r_total;
    if (r_top > 0.0 && r_bot > 0.0)
        r_total = thermal_resistance_parallel(r_top, r_bot);
    else if (r_top > 0.0)
        r_total = r_top;
    else if (r_bot > 0.0)
        r_total = r_bot;
    else
        return THERMAL_ERR_DIV_ZERO;

    *tj_out = ta_c + source->power_w * r_total;

    /* Tc = Tj - P * Rjc */
    *tc_out = *tj_out - source->power_w * r_jc;

    /* Tb = Ta + P_bot * R_ba where P_bot = P * (R_top/(R_top+R_bot)) */
    if (r_top > 0.0 && r_bot > 0.0) {
        double p_bot = source->power_w * r_top / (r_top + r_bot);
        *tb_out = ta_c + p_bot * r_ba;
    } else if (r_bot > 0.0) {
        *tb_out = ta_c + source->power_w * r_ba;
    } else {
        *tb_out = ta_c;
    }

    if (*tj_out > source->max_tj && source->max_tj > 0.0)
        return THERMAL_ERR_TJ_EXCEEDED;

    return THERMAL_OK;
}

int calculate_mutual_heating(double ta_c,
                              const heat_source_t *src1, const heat_source_t *src2,
                              double distance_mm,
                              double k_board_xy, double t_board_mm, double h_conv,
                              double *tj1_out, double *tj2_out) {
    if (!src1 || !src2 || !tj1_out || !tj2_out) return THERMAL_ERR_NULL_PTR;
    if (distance_mm < 0.0 || k_board_xy <= 0.0 || t_board_mm <= 0.0)
        return THERMAL_ERR_INVALID_ENV;

    /* Characteristic thermal spreading length: L_char = sqrt(k*t/h) */
    double l_char = 1.0;
    if (h_conv > 0.0)
        l_char = sqrt(k_board_xy * t_board_mm * 1.0e-3 / h_conv);
    else
        l_char = sqrt(k_board_xy * t_board_mm * 1.0e-3 / 10.0);

    if (l_char <= 0.0) l_char = 1.0;

    /* Mutual heating coupling: Delta_T12 = P2 * R_coupling
     * Exponential decay model: R_coupling(d) = R0 * exp(-d / L_char)
     * R0 estimated from source footprint spreading */
    double r0_1 = thermal_resistance_spreading(k_board_xy, t_board_mm,
                    src1->width_mm * src1->length_mm, M_PI * l_char * l_char * 1.0e6);
    double r0_2 = thermal_resistance_spreading(k_board_xy, t_board_mm,
                    src2->width_mm * src2->length_mm, M_PI * l_char * l_char * 1.0e6);

    if (isinf(r0_1) || r0_1 <= 0.0) r0_1 = 10.0;
    if (isinf(r0_2) || r0_2 <= 0.0) r0_2 = 10.0;

    double r_coupling_12 = r0_1 * exp(-distance_mm / l_char);
    double r_coupling_21 = r0_2 * exp(-distance_mm / l_char);

    /* Tj1 = Ta + P1*Rja1 + P2*R_coupling_12 (superposition)
     * Estimate Rja1 from single-component analysis */
    double rja1 = r0_1;
    double rja2 = r0_2;

    *tj1_out = ta_c + src1->power_w * rja1 + src2->power_w * r_coupling_12;
    *tj2_out = ta_c + src2->power_w * rja2 + src1->power_w * r_coupling_21;

    return THERMAL_OK;
}

/* ==================================================================
 * L2/L4: Convection Coefficient Calculations
 * ================================================================== */

int natural_convection_coefficient(double ts_c, double tinf_c,
                                    double length_mm,
                                    convection_correlation_t correlation,
                                    const ambient_conditions_t *ambient,
                                    double *h_out) {
    if (!ambient || !h_out) return THERMAL_ERR_NULL_PTR;
    if (length_mm <= 0.0) return THERMAL_ERR_ZERO_AREA;

    /* Film temperature = average of surface and ambient */
    double t_film = (ts_c + tinf_c) / 2.0;
    double t_film_k = t_film + 273.15;
    double delta_t = ts_c - tinf_c;
    if (fabs(delta_t) < 0.001) delta_t = 1.0;  /* Avoid division issues */

    /* Use ambient properties if available, otherwise estimate */
    double nu_air = ambient->air_dynamic_viscosity / ambient->air_density_kgm3;
    if (nu_air <= 0.0) nu_air = 1.568e-5;  /* kinematic viscosity of air at 25 C */
    double k_air = ambient->air_conductivity;
    if (k_air <= 0.0) k_air = 0.02624;
    double pr = ambient->air_prandtl;
    if (pr <= 0.0) pr = 0.71;

    /* Volumetric thermal expansion coefficient: beta = 1/T_film for ideal gas */
    double beta = 1.0 / t_film_k;

    /* Grashof number: Gr = g * beta * |delta_T| * L^3 / nu^2 */
    double l_m = length_mm * 1.0e-3;
    double gr = GRAVITY * beta * fabs(delta_t) * l_m * l_m * l_m / (nu_air * nu_air);

    /* Rayleigh number: Ra = Gr * Pr */
    double ra = gr * pr;

    /* Nusselt number from correlation */
    double nu;
    double c, n;
    switch (correlation) {
        case CONV_CORR_NAT_VERT_PLATE:
            if (ra < 1.0e4) ra = 1.0e4;
            if (ra < 1.0e9)      { c = 0.59; n = 0.25; }  /* laminar */
            else                 { c = 0.10; n = 0.333; }  /* turbulent */
            nu = c * pow(ra, n);
            break;
        case CONV_CORR_NAT_HORIZ_TOP:
            if (ra < 1.0e4) ra = 1.0e4;
            if (ra < 1.0e7)      { c = 0.54; n = 0.25; }
            else                 { c = 0.15; n = 0.333; }
            nu = c * pow(ra, n);
            break;
        case CONV_CORR_NAT_HORIZ_BOT:
            if (ra < 1.0e4) ra = 1.0e4;
            nu = 0.27 * pow(ra, 0.25);  /* Reduced convection on bottom */
            break;
        case CONV_CORR_NAT_CYLINDER:
            if (ra < 1.0e4) ra = 1.0e4;
            if (ra < 1.0e12)     { c = 0.53; n = 0.25; }
            else                 { c = 0.11; n = 0.333; }
            nu = c * pow(ra, n);
            break;
        default:
            nu = 0.59 * pow(ra > 1.0e4 ? ra : 1.0e4, 0.25);
            break;
    }

    /* h = Nu * k / L */
    *h_out = nu * k_air / l_m;
    return THERMAL_OK;
}

int forced_convection_coefficient(double velocity_ms, double length_mm,
                                   const ambient_conditions_t *ambient,
                                   double *h_out) {
    if (!ambient || !h_out) return THERMAL_ERR_NULL_PTR;
    if (velocity_ms <= 0.0 || length_mm <= 0.0) return THERMAL_ERR_INVALID_ENV;

    double nu_air = ambient->air_dynamic_viscosity / ambient->air_density_kgm3;
    if (nu_air <= 0.0) nu_air = 1.568e-5;
    double k_air = ambient->air_conductivity;
    if (k_air <= 0.0) k_air = 0.02624;
    double pr = ambient->air_prandtl;
    if (pr <= 0.0) pr = 0.71;

    /* Reynolds number: Re = V * L / nu */
    double l_m = length_mm * 1.0e-3;
    double re = velocity_ms * l_m / nu_air;

    /* Nusselt from flat plate correlation */
    double nu;
    if (re < 5.0e5) {
        /* Laminar: Nu = 0.664 * Re^(1/2) * Pr^(1/3) */
        nu = 0.664 * sqrt(re) * cbrt(pr);
    } else {
        /* Turbulent: Nu = 0.037 * Re^(4/5) * Pr^(1/3) */
        nu = 0.037 * pow(re, 0.8) * cbrt(pr);
    }

    *h_out = nu * k_air / l_m;
    return THERMAL_OK;
}

/* ==================================================================
 * L2/L6: Heat Sink Performance Analysis
 * ================================================================== */

int heatsink_natural_convection_r_sa(heat_sink_model_t *hs,
                                      double ts_c,
                                      const ambient_conditions_t *ambient,
                                      double *r_sa_out) {
    if (!hs || !ambient || !r_sa_out) return THERMAL_ERR_NULL_PTR;
    if (hs->base_length_mm <= 0.0 || hs->fin_height_mm <= 0.0)
        return THERMAL_ERR_ZERO_THICK;

    double h_nat;
    int ret = natural_convection_coefficient(ts_c, ambient->ambient_temp_c,
                hs->base_length_mm, CONV_CORR_NAT_VERT_PLATE, ambient, &h_nat);
    if (ret != THERMAL_OK) return ret;

    /* Base area calculation: exposed area = total - fins footprint */
    double fin_base_area = hs->num_fins * hs->fin_thickness_mm * hs->base_length_mm;
    double base_exposed_area = hs->base_width_mm * hs->base_length_mm - fin_base_area;

    /* Fin efficiency: eta = tanh(m*H) / (m*H)
     * m = sqrt(2*h/(k_fin*t_fin)) for rect fin with adiabatic tip */
    double m_fin = 0.0;
    if (hs->k_material > 0.0 && hs->fin_thickness_mm > 0.0)
        m_fin = sqrt(2.0 * h_nat / (hs->k_material * hs->fin_thickness_mm * 1.0e-3));
    double mH = m_fin * hs->fin_height_mm * 1.0e-3;
    double eta_fin = 1.0;
    if (mH > 0.001)
        eta_fin = tanh(mH) / mH;

    /* Total fin surface area (both sides + tip of each fin) */
    double a_fin_per = 2.0 * hs->fin_height_mm * hs->base_length_mm +
                       hs->fin_thickness_mm * hs->base_length_mm;
    double a_fin_total = hs->num_fins * a_fin_per;

    /* Effective area: A_base_exposed + eta_fin * A_fin_total */
    double a_effective = base_exposed_area + eta_fin * a_fin_total;

    if (a_effective <= 0.0) {
        a_effective = hs->base_width_mm * hs->base_length_mm;
    }

    *r_sa_out = thermal_resistance_convection(h_nat, a_effective);
    hs->r_sa_natural = *r_sa_out;
    hs->fin_efficiency = eta_fin;
    hs->surface_area_mm2 = a_effective;

    return THERMAL_OK;
}

int heatsink_forced_convection_r_sa(heat_sink_model_t *hs,
                                     double velocity_ms,
                                     const ambient_conditions_t *ambient,
                                     double *r_sa_out) {
    if (!hs || !ambient || !r_sa_out) return THERMAL_ERR_NULL_PTR;
    if (velocity_ms <= 0.0) return THERMAL_ERR_INVALID_ENV;

    /* Characteristic length: hydraulic diameter of fin channel
     * Dh = 2*s*H / (s+H) where s = fin_spacing, H = fin_height */
    double s = hs->fin_spacing_mm;
    double H = hs->fin_height_mm;
    double dh = (s > 0.0 && H > 0.0) ? (2.0 * s * H / (s + H)) : hs->base_length_mm;

    double h_forced;
    int ret = forced_convection_coefficient(velocity_ms, dh, ambient, &h_forced);
    if (ret != THERMAL_OK) return ret;

    /* Use same fin efficiency model as natural */
    double m_fin = 0.0;
    if (hs->k_material > 0.0 && hs->fin_thickness_mm > 0.0)
        m_fin = sqrt(2.0 * h_forced / (hs->k_material * hs->fin_thickness_mm * 1.0e-3));
    double mH = m_fin * hs->fin_height_mm * 1.0e-3;
    double eta_fin = 1.0;
    if (mH > 0.001)
        eta_fin = tanh(mH) / mH;

    double a_fin_per = 2.0 * hs->fin_height_mm * hs->base_length_mm +
                       hs->fin_thickness_mm * hs->base_length_mm;
    double a_fin_total = hs->num_fins * a_fin_per;
    double base_exposed = hs->base_width_mm * hs->base_length_mm -
                          hs->num_fins * hs->fin_thickness_mm * hs->base_length_mm;
    double a_effective = base_exposed + eta_fin * a_fin_total;
    if (a_effective <= 0.0)
        a_effective = hs->base_width_mm * hs->base_length_mm;

    *r_sa_out = thermal_resistance_convection(h_forced, a_effective);
    hs->r_sa_forced = *r_sa_out;

    return THERMAL_OK;
}

/* ==================================================================
 * L3: Transient Thermal Analysis
 * ================================================================== */

double transient_lumped_capacitance(double t0_c, double tinf_c,
                                     double mass_g, double cp_jkgk,
                                     double h, double area_mm2,
                                     double time_s) {
    if (mass_g <= 0.0 || cp_jkgk <= 0.0 || h <= 0.0 || area_mm2 <= 0.0)
        return t0_c;
    if (time_s < 0.0) time_s = 0.0;

    /* Thermal capacitance: C_th = mass * cp = (mass_g * 1e-3) * cp
     * Thermal resistance: R_th = 1 / (h * A_m2) = 1e6 / (h * area_mm2)
     * Time constant: tau = R_th * C_th
     * Temperature: T(t) = Tinf + (T0 - Tinf) * exp(-t/tau) */
    double c_th = mass_g * 1.0e-3 * cp_jkgk;
    double r_th = 1.0e6 / (h * area_mm2);
    double tau = r_th * c_th;
    if (tau <= 0.0) return t0_c;

    return tinf_c + (t0_c - tinf_c) * exp(-time_s / tau);
}

double thermal_time_constant(double r_th, double mass_g, double cp_jkgk) {
    if (r_th <= 0.0 || mass_g <= 0.0 || cp_jkgk <= 0.0) return INFINITY;
    /* tau = R_th * C_th = R_th * mass * cp
     * mass in kg = mass_g * 1e-3 */
    return r_th * mass_g * 1.0e-3 * cp_jkgk;
}

double transient_foster_model(double ta_c, double power_w,
                               const double *r_vals, const double *c_vals,
                               int stages, double time_s) {
    if (!r_vals || !c_vals || stages <= 0) return ta_c;
    if (time_s <= 0.0) return ta_c;
    if (power_w <= 0.0) return ta_c;

    /* Foster network: T(t) = Ta + P * sum_i [R_i * (1 - exp(-t/tau_i))]
     * where tau_i = R_i * C_i */
    double t_sum = ta_c;
    for (int i = 0; i < stages; i++) {
        if (r_vals[i] <= 0.0) continue;
        double tau_i = r_vals[i] * c_vals[i];
        if (tau_i <= 0.0) {
            t_sum += power_w * r_vals[i];
        } else {
            t_sum += power_w * r_vals[i] * (1.0 - exp(-time_s / tau_i));
        }
    }
    return t_sum;
}

int transient_cauer_model(double ta_c, double power_w,
                           const double *r_vals, const double *c_vals,
                           int stages, double time_s, double dt_s,
                           double *tj_out) {
    if (!r_vals || !c_vals || !tj_out || stages <= 0)
        return THERMAL_ERR_NULL_PTR;
    if (dt_s <= 0.0 || time_s < 0.0) return THERMAL_ERR_INVALID_ENV;

    /* Cauer network solved via forward Euler integration.
     * Number of nodes = stages (each capacitor voltage is a node temp)
     * T[0] = junction, T[stages] = ambient = ta_c
     *
     * For stage i (1-indexed in physics, 0-indexed in code):
     * C_i * dTi/dt = (T_{i-1} - Ti)/R_i - (Ti - T_{i+1})/R_{i+1}
     * with T_{stages} = Ta (ambient, last node grounded through R_{stages})
     *
     * Simplified: dT_i/dt = [ (T_{i-1}-T_i)/R_i - (T_i-T_{i+1})/R_{i+1} ] / C_i
     *
     * At junction (i=0): C0*dT0/dt = P - (T0-T1)/R0
     * Actually: T0 is not a capacitor node in Cauer - it's the input.
     * Let's use the standard Cauer representation:
     * T0 = junction temp. The first R (Rjc) connects junction to first C node.
     *
     * Forward Euler: T_i(t+dt) = T_i(t) + dt * dT_i/dt */

    /* Allocate node temperatures */
    double *T = (double *)calloc(stages + 1, sizeof(double));
    if (!T) return THERMAL_ERR_MEMORY;

    /* Initialize all nodes at ambient temperature */
    for (int i = 0; i <= stages; i++) T[i] = ta_c;
    T[0] = ta_c;  /* Junction starts at ambient */

    int n_steps = (int)(time_s / dt_s);
    if (n_steps < 1) n_steps = 1;

    for (int step = 0; step < n_steps; step++) {
        double *T_new = (double *)calloc(stages + 1, sizeof(double));
        if (!T_new) { free(T); return THERMAL_ERR_MEMORY; }

        /* Node 0 (junction): connected to node 1 through R_vals[0]
         * Heat input = power_w, heat outflow = (T0 - T1)/R0
         * C0 * dT0/dt = P - (T0-T1)/R0
         * Nodes 1..stages-1: C_i * dTi/dt = (T_{i-1}-Ti)/R_{i-1} - (Ti-T_{i+1})/R_i
         * Node stages (ambient): fixed at Ta, no dynamics.
         *
         * Wait - in Cauer, the R ladder has stages series elements. Let me re-check.
         * Cauer ladder: P -> R0 -> (C0 to gnd) -> R1 -> (C1 to gnd) -> ... -> Ta
         *
         * Node 0 (after R0, before C0): T0 - first C node
         * Actually, standard Cauer representation:
         * Tj = junction. R0 = junction-to-first-node.
         * For Cauer, we solve the nodal equations directly:
         * Node 0 is the first internal node (after R0). P flows into node 0.
         * Let's follow the thermal analog to electrical RC ladder. */

        /* More standard approach:
         * The Cauer model represents a 1D heat diffusion path.
         * R0 connects Tj to first internal node T[0].
         * Then R1 connects T[0] to second internal node T[1], etc.
         * C[i] is the capacitance from node i to ground (ambient).
         *
         * At junction: Tj is NOT a dynamic node. Tj = P*R0 + T[0]
         * Node equations: C[i] * dT[i]/dt = (T[i-1] - T[i])/R[i] - (T[i] - T[i+1])/R[i+1]
         *
         * Where T[-1] concept = Tj, and T[stages] concept = Ta (last node has R to Ta)
         *
         * We implement this with forward Euler on T[0]..T[stages-1] */

        /* Node 0: C[0]*dT[0]/dt = P - (T[0]-T[1])/R[1]  (since Tj = T[0] + P*R[0])
         * Actually simpler: input heat P flows through R[0] to node 0.
         * The current (heat flow) entering node 0 = P.
         * For node 0: C[0]*dT[0]/dt = P - (T[0]-T[1])/R[1]  (outflow through R[1])
         *
         * For intermediate nodes i: C[i]*dT[i]/dt = (T[i-1]-T[i])/R[i] - (T[i]-T[i+1])/R[i+1]
         *
         * For last node: C[N-1]*dT[N-1]/dt = (T[N-2]-T[N-1])/R[N-1] - (T[N-1]-Ta)/R[N] */

        /* Node 0 */
        double q_in = power_w;
        double q_out = (stages > 1) ? (T[0] - T[1]) / r_vals[1] : (T[0] - ta_c) / r_vals[0];
        double dT0 = (q_in - q_out) / c_vals[0];
        T_new[0] = T[0] + dt_s * dT0;

        /* Intermediate nodes 1..stages-2 */
        for (int i = 1; i < stages - 1; i++) {
            double q_in_node = (T[i-1] - T[i]) / r_vals[i];
            double q_out_node = (T[i] - T[i+1]) / r_vals[i+1];
            double dTi = (q_in_node - q_out_node) / c_vals[i];
            T_new[i] = T[i] + dt_s * dTi;
        }

        /* Last node stages-1 */
        if (stages > 1) {
            int last = stages - 1;
            double q_in_last = (T[last-1] - T[last]) / r_vals[last];
            double q_out_last = (T[last] - ta_c) / r_vals[stages]; /* R[stages] to ambient */
            double dTlast = (q_in_last - q_out_last) / c_vals[last];
            T_new[last] = T[last] + dt_s * dTlast;
        } else {
            /* Single stage: direct to ambient */
            T_new[0] = T[0] + dt_s * (power_w - (T[0]-ta_c)/r_vals[0]) / c_vals[0];
        }

        /* Update temperatures */
        for (int i = 0; i < stages; i++) T[i] = T_new[i];
        free(T_new);

        /* Numerical stability check */
        for (int i = 0; i < stages; i++) {
            if (isnan(T[i]) || isinf(T[i])) { free(T); return THERMAL_ERR_NO_CONVERGE; }
        }
    }

    /* Junction temperature = T[0] + P*R[0] (voltage drop across first resistor) */
    *tj_out = T[0] + power_w * r_vals[0];
    free(T);
    return THERMAL_OK;
}

double transient_thermal_impedance(const double *r_vals, const double *c_vals,
                                    int stages, double time_s) {
    if (!r_vals || !c_vals || stages <= 0) return 0.0;
    if (time_s <= 0.0) return 0.0;

    /* Zth(t) = sum_i [R_i * (1 - exp(-t/tau_i))]  where tau_i = R_i * C_i */
    double zth = 0.0;
    for (int i = 0; i < stages; i++) {
        if (r_vals[i] <= 0.0) continue;
        double tau_i = r_vals[i] * c_vals[i];
        if (tau_i <= 0.0) {
            zth += r_vals[i];
        } else {
            zth += r_vals[i] * (1.0 - exp(-time_s / tau_i));
        }
    }
    return zth;
}

/* ==================================================================
 * L3: Dimensionless Numbers for Thermal Analysis
 * ================================================================== */

double biot_number(double h, double volume_mm3, double surface_area_mm2, double k) {
    if (surface_area_mm2 <= 0.0 || k <= 0.0) return INFINITY;
    /* Lc = V / As (characteristic length)
     * Bi = h * Lc / k
     * All in SI: h (W/m^2-K), Lc (m) from mm3/mm2 * 1e-3, k (W/m-K) */
    double lc_m = (volume_mm3 / surface_area_mm2) * 1.0e-3;
    return h * lc_m / k;
}

double fourier_number(double k, double density, double cp,
                       double time_s, double char_length_mm) {
    if (density <= 0.0 || cp <= 0.0 || char_length_mm <= 0.0) return 0.0;
    /* Fo = alpha * t / Lc^2
     * alpha = k / (rho * cp)  in m^2/s
     * Lc^2 in m^2 = (char_length_mm * 1e-3)^2 */
    double alpha = k / (density * cp);
    double lc2 = char_length_mm * char_length_mm * 1.0e-6;
    return alpha * time_s / lc2;
}

double characteristic_spreading_length(double k_effective_xy,
                                        double thickness_mm, double h) {
    if (h <= 0.0) h = 5.0;  /* Minimum natural convection */
    if (k_effective_xy <= 0.0 || thickness_mm <= 0.0) return 0.0;
    /* L_char = sqrt(k_eff * t / h)
     * All in SI: k (W/m-K), t (m), h (W/m^2-K) -> L_char (m) -> return mm */
    double l_m = sqrt(k_effective_xy * thickness_mm * 1.0e-3 / h);
    return l_m * 1.0e3;
}

/* ==================================================================
 * L3/L5: Thermal Resistance Network Matrix Solver (Gauss-Seidel)
 * ================================================================== */

int thermal_network_solve(int n_nodes,
                           const double **r_matrix,
                           const double *r_ambient,
                           const double *power_w,
                           double ta_c,
                           double *t_out,
                           int max_iter, double tol) {
    if (!r_matrix || !r_ambient || !power_w || !t_out) return THERMAL_ERR_NULL_PTR;
    if (n_nodes <= 0 || max_iter <= 0 || tol <= 0.0) return THERMAL_ERR_DIM_MISMATCH;

    /* Build conductance matrix G and solve iteratively.
     * For each node i:
     * G_ii = sum_{j != i} (1/R_ij) + 1/R_i_ambient
     * G_ij = -1/R_ij  (i != j)
     * System: G * T_new = Q_vector (power inputs at each node)
     *
     * Gauss-Seidel iteration for node i:
     * T_i^(k+1) = (Q_i + sum_{j<i} G_ij*T_j^(k+1) + sum_{j>i} G_ij*T_j^(k)) / (-G_ii)
     *
     * Actually for thermal network: G*T = Q + G_ambient*Ta
     * where Q contains power inputs at nodes.
     *
     * Better: Use relaxation method directly on the network.
     * For node i: heat balance: (Ti - Tj)/R_ij sums to Q_i + (Ta - Ti)/R_i_ambient
     * Solve for Ti:
     * Ti = (Q_i + sum_j(Tj/R_ij) + Ta/R_i_ambient) / (sum_j(1/R_ij) + 1/R_i_ambient)
     *
     * This is the Jacobi iteration. Gauss-Seidel uses latest values. */

    /* Initialize temperatures to ambient */
    for (int i = 0; i < n_nodes; i++) t_out[i] = ta_c;

    double *t_old = (double *)malloc(n_nodes * sizeof(double));
    if (!t_old) return THERMAL_ERR_MEMORY;

    int converged = 0;
    for (int iter = 0; iter < max_iter; iter++) {
        /* Save old temperatures */
        memcpy(t_old, t_out, n_nodes * sizeof(double));

        double max_change = 0.0;

        for (int i = 0; i < n_nodes; i++) {
            double numerator = power_w[i];
            double denominator = 0.0;

            /* Add ambient contribution */
            if (r_ambient[i] > 0.0) {
                numerator += ta_c / r_ambient[i];
                denominator += 1.0 / r_ambient[i];
            }

            /* Add coupling to other nodes */
            for (int j = 0; j < n_nodes; j++) {
                if (i == j) continue;
                if (r_matrix[i][j] > 0.0) {
                    /* Use latest available temperature (Gauss-Seidel) */
                    numerator += t_out[j] / r_matrix[i][j];
                    denominator += 1.0 / r_matrix[i][j];
                }
            }

            if (denominator > 0.0) {
                double t_new = numerator / denominator;
                double change = fabs(t_new - t_old[i]);
                if (change > max_change) max_change = change;
                t_out[i] = t_new;
            }
        }

        if (max_change < tol) {
            converged = 1;
            break;
        }
    }

    free(t_old);

    if (!converged) return THERMAL_ERR_NO_CONVERGE;
    return THERMAL_OK;
}
