/*
 * hs_pdn.c - Power Distribution Network Design Implementation
 *
 * Implements all PDN analysis functions: target impedance computation,
 * decoupling capacitor modeling, plane resonance analysis, SSN estimation,
 * and decap selection optimization.
 *
 * Knowledge coverage:
 *   L1: Z_target, ESR, ESL, plane capacitance, SSN definitions
 *   L2: Decoupling hierarchy (VRM->bulk->ceramic->on-package)
 *   L3: RLC resonator models, plane pair impedance matrix
 *   L4: Target impedance law, plane capacitance law, parallel resonance
 *   L5: Decap selection algorithm, frequency sweep analysis
 *   L6: Multi-stage PDN design for DDR4/PCIe power rails
 */

#include "hs_pdn.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* Physical constants */
#define C0              299792458.0
#define EPS0            8.854187817e-12
#define MU0             1.2566370614e-6
#define RHO_COPPER      1.72e-8

/* Predefined standard capacitor values (F) for common decap selections */
__attribute__((unused))
static const double std_cap_values_f[] = {
    1.0e-7,    /* 100 nF */
    2.2e-7,    /* 220 nF */
    4.7e-7,    /* 470 nF */
    1.0e-6,    /* 1 uF */
    2.2e-6,    /* 2.2 uF */
    4.7e-6,    /* 4.7 uF */
    1.0e-5,    /* 10 uF */
    2.2e-5,    /* 22 uF */
    4.7e-5,    /* 47 uF */
    1.0e-4,    /* 100 uF */
};
#define N_STD_CAP_VALUES (sizeof(std_cap_values_f) / sizeof(std_cap_values_f[0]))

/* Default ESL values (Henries) by package size.
 * Data from Murata / TDK application notes.
 */
static double default_esl_by_package(const char *package)
{
    if (!package) return 0.5e-9;
    if (strncmp(package, "0201", 4) == 0) return 0.2e-9;
    if (strncmp(package, "0402", 4) == 0) return 0.4e-9;
    if (strncmp(package, "0603", 4) == 0) return 0.6e-9;
    if (strncmp(package, "0805", 4) == 0) return 0.9e-9;
    if (strncmp(package, "1206", 4) == 0) return 1.4e-9;
    if (strncmp(package, "1210", 4) == 0) return 1.8e-9;
    return 0.5e-9;
}

/* Default ESR values (Ohms) by capacitor type */
static double default_esr_by_type(hs_capacitor_type_t type, double cap_f)
{
    switch (type) {
    case HS_CAP_CERAMIC_X5R:
    case HS_CAP_CERAMIC_X7R:
    case HS_CAP_CERAMIC_NP0:
        if (cap_f >= 1.0e-5) return 0.005;
        if (cap_f >= 1.0e-6) return 0.010;
        if (cap_f >= 1.0e-7) return 0.025;
        return 0.050;
    case HS_CAP_TANTALUM:
        return 0.100;
    case HS_CAP_ALUM_ELEC:
        return 0.500;
    case HS_CAP_POLYMER:
        return 0.020;
    case HS_CAP_REVERSE_GEOM:
        return 0.003;
    case HS_CAP_3TERM:
        return 0.002;
    default:
        return 0.010;
    }
}

/* ================================================================
 * L4: Target impedance - Fundamental PDN design law
 *
 * Theorem (Smith et al., 1999):
 *   Z_target = deltaV_allowable / I_transient
 *
 * Where deltaV_allowable = V_nominal * ripple_tolerance
 * For a 1.0V rail with 5% ripple tolerance and 2A step:
 *   Z_target = 0.05 * 1.0 / 2.0 = 0.025 Ohm = 25 mOhm
 *
 * Reference: Smith, Anderson, Forehand, Pelc, and Roy,
 *   "Power Distribution System Design Methodology...",
 *   IEEE EPEP, 1999.
 * Complexity: O(1)
 * ================================================================ */
double hs_pdn_target_impedance(double nominal_voltage_v,
                                double ripple_tolerance,
                                double transient_current_a,
                                double margin_factor)
{
    if (nominal_voltage_v <= 0.0 || ripple_tolerance <= 0.0 ||
        transient_current_a <= 0.0 || margin_factor <= 0.0) {
        return 0.0;
    }
    double delta_v = nominal_voltage_v * ripple_tolerance;
    double z_raw = delta_v / transient_current_a;
    return z_raw / margin_factor;
}

/* ================================================================
 * L3: Self-resonant frequency of a decoupling capacitor
 *
 * SRF = 1 / (2*pi * sqrt(ESL * C))
 *
 * At the SRF, the inductive and capacitive reactances cancel.
 * For a 100 nF MLCC (0402, ESL = 0.4 nH):
 *   SRF = 1 / (2*pi * sqrt(0.4e-9 * 1e-7)) = 25.2 MHz
 *
 * Reference: Cain, "Parasitic Inductance of Multilayer Ceramic
 *   Capacitors", AVX Technical Information, 1993.
 * Complexity: O(1)
 * ================================================================ */
double hs_decap_srf(const hs_decap_t *decap)
{
    if (!decap) return 0.0;
    if (decap->capacitance_f <= 0.0 || decap->esl_h <= 0.0) return 0.0;
    double lc_product = decap->esl_h * decap->capacitance_f;
    if (lc_product <= 0.0) return 0.0;
    return 1.0 / (2.0 * M_PI * sqrt(lc_product));
}

/* ================================================================
 * L3: Decoupling capacitor impedance vs frequency
 *
 * RLC series model:
 *   |Z(f)| = sqrt(ESR^2 + (omega*L - 1/(omega*C))^2)
 *
 * Below SRF: capacitive (|Z| ~ 1/f)
 * At SRF:    resistive (|Z| = ESR, minimum)
 * Above SRF: inductive (|Z| ~ f)
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_decap_impedance(const hs_decap_t *decap, double frequency_hz)
{
    if (!decap) return 0.0;
    if (decap->capacitance_f <= 0.0 || frequency_hz <= 0.0) return 0.0;
    double omega = 2.0 * M_PI * frequency_hz;
    double xc = 1.0 / (omega * decap->capacitance_f);
    double xl = omega * decap->esl_h;
    double reactance = xl - xc;
    return sqrt(decap->esr_ohm * decap->esr_ohm + reactance * reactance);
}

/* ================================================================
 * L4: Inter-plane capacitance - Electrostatic law
 *
 * C_plane = eps0 * er * area / separation
 *
 * For a 100x100 mm board, FR-4 (er=4.2), d=0.25 mm:
 *   C_plane = 8.85e-12 * 4.2 * 0.01 / 2.5e-4 = 1.49 nF
 *
 * This inter-plane capacitance provides essential high-frequency
 * decoupling and forms the PDN impedance floor above ~100 MHz.
 *
 * Reference: Novak, Ch.4.1; Bogatin, Ch.9
 * Complexity: O(1)
 * ================================================================ */
double hs_plane_pair_capacitance(const hs_plane_pair_t *plane)
{
    if (!plane) return 0.0;
    if (plane->plane_width_m <= 0.0 || plane->plane_height_m <= 0.0 ||
        plane->separation_m <= 0.0 || plane->dielectric_er <= 0.0) {
        return 0.0;
    }
    double area = plane->plane_width_m * plane->plane_height_m;
    return EPS0 * plane->dielectric_er * area / plane->separation_m;
}

/* ================================================================
 * L3: Plane spreading inductance
 *
 * Models the inductance seen by current spreading from the VRM
 * connection point across the plane pair.
 *
 * L_spread = (mu0 * d / pi) * ln(2*w_plane / w_contact)
 *
 * Reference: Novak, "PDN Design Methodologies", Ch.3, 2008.
 * Complexity: O(1)
 * ================================================================ */
double hs_plane_spreading_inductance(const hs_plane_pair_t *plane,
                                      double contact_width_m)
{
    if (!plane) return 0.0;
    if (plane->separation_m <= 0.0 || contact_width_m <= 0.0) return 0.0;
    double w_eff = contact_width_m;
    if (w_eff > plane->plane_width_m) w_eff = plane->plane_width_m;
    double ln_arg = 2.0 * plane->plane_width_m / w_eff;
    if (ln_arg < 1.0) ln_arg = 1.0;
    return (MU0 * plane->separation_m / M_PI) * log(ln_arg);
}

/* ================================================================
 * L4: Plane cavity resonance frequency
 *
 * For rectangular plane pair with magnetic wall boundaries:
 *   f_mn = (c0 / (2*sqrt(er))) * sqrt((m/a)^2 + (n/b)^2)
 * Lowest mode: f_res_min = c0 / (2 * max(a,b) * sqrt(er))
 *
 * For 100x80 mm board, er=4.2: f_10 = 732 MHz
 *
 * Reference: Lei, Techentin and Gilbert, IEEE Trans. ADVP, 2006.
 * Complexity: O(1)
 * ================================================================ */
double hs_plane_resonance_frequency(const hs_plane_pair_t *plane)
{
    if (!plane) return 0.0;
    if (plane->plane_width_m <= 0.0 || plane->plane_height_m <= 0.0 ||
        plane->dielectric_er <= 0.0) return 0.0;
    double max_dim = (plane->plane_width_m > plane->plane_height_m) ?
                     plane->plane_width_m : plane->plane_height_m;
    return C0 / (2.0 * max_dim * sqrt(plane->dielectric_er));
}

/* ================================================================
 * L2: DC IR drop across a plane
 *
 * R_dc = rho * L / (w * t), V_drop = I * R
 *
 * For a solid plane: very low resistance (mOhm range).
 * For split planes with narrow necks: IR drop can be significant.
 *
 * Reference: IPC-2152
 * Complexity: O(1)
 * ================================================================ */
double hs_plane_ir_drop(const hs_plane_pair_t *plane, double current_a,
                         int plane_type)
{
    if (!plane) return 0.0;
    (void)plane_type;
    if (plane->plane_width_m <= 0.0 || plane->plane_height_m <= 0.0 ||
        plane->copper_thickness_m <= 0.0 || current_a <= 0.0) {
        return 0.0;
    }
    double sigma = (plane->copper_conductivity > 0.0) ?
                   plane->copper_conductivity : 5.8e7;
    double length = (plane->plane_width_m > plane->plane_height_m) ?
                    plane->plane_width_m : plane->plane_height_m;
    double width = (plane->plane_width_m < plane->plane_height_m) ?
                   plane->plane_width_m : plane->plane_height_m;
    double resistance = length / (sigma * width * plane->copper_thickness_m);
    return current_a * resistance;
}

/* ================================================================
 * L3: Plane impedance at a given frequency
 *
 * Z_plane(f) = |j*omega*L_spread + 1/(j*omega*C_plane) + R_plane|
 *
 * Combines inter-plane capacitance, spreading inductance,
 * and plane resistance into a single impedance magnitude.
 * Complexity: O(1)
 * ================================================================ */
double hs_plane_impedance(const hs_plane_pair_t *plane,
                           double frequency_hz, double contact_width_m)
{
    if (!plane || frequency_hz <= 0.0) return 0.0;
    double c_plane = hs_plane_pair_capacitance(plane);
    double l_spread = hs_plane_spreading_inductance(plane, contact_width_m);
    if (c_plane <= 0.0) return INFINITY;
    double omega = 2.0 * M_PI * frequency_hz;
    double xc = 1.0 / (omega * c_plane);
    double xl = omega * l_spread;
    /* Plane resistance: low but includes skin effect at HF */
    double r_plane = 0.0;
    if (plane->plane_width_m > 0.0 && plane->copper_thickness_m > 0.0) {
        double skin_d = sqrt(RHO_COPPER / (M_PI * frequency_hz * MU0));
        double eff_t = (skin_d < plane->copper_thickness_m) ?
                       skin_d : plane->copper_thickness_m;
        r_plane = RHO_COPPER * plane->plane_height_m /
                  (plane->plane_width_m * eff_t);
    }
    double react = xl - xc;
    return sqrt(r_plane * r_plane + react * react);
}

/* ================================================================
 * L5: Total PDN impedance - parallel combination
 *
 * Z_total(f) = 1 / (1/Z_vrm + 1/Z_plane + sum(1/Z_decap_i))
 *
 * All PDN elements combine in parallel. Each decoupling capacitor
 * provides a low-impedance path at its SRF.
 *
 * Complexity: O(N_decaps)
 * ================================================================ */
double hs_pdn_total_impedance(const hs_pdn_network_t *network,
                               double frequency_hz, double contact_width_m)
{
    if (!network || frequency_hz <= 0.0) return 0.0;
    double omega = 2.0 * M_PI * frequency_hz;
    double admittance = 0.0;

    /* VRM contribution */
    {
        double r_vrm = network->vrm.output_impedance_ohm;
        double bw = network->vrm.bandwidth_hz;
        double l_vrm = (bw > 0.0) ? r_vrm / (2.0 * M_PI * bw) : 0.0;
        double xl_vrm = omega * l_vrm;
        double z_vrm_mag = sqrt(r_vrm * r_vrm + xl_vrm * xl_vrm);
        if (z_vrm_mag > 0.0) admittance += 1.0 / z_vrm_mag;
    }

    /* Plane contribution */
    if (network->plane.plane_width_m > 0.0) {
        double z_plane = hs_plane_impedance(&network->plane, frequency_hz,
                                             contact_width_m);
        if (z_plane > 0.0 && isfinite(z_plane)) admittance += 1.0 / z_plane;
    }

    /* Bulk capacitors */
    for (int i = 0; i < network->num_bulk; i++) {
        double z = hs_decap_impedance(&network->bulk_caps[i], frequency_hz);
        if (z > 0.0 && isfinite(z)) admittance += 1.0 / z;
    }

    /* Ceramic capacitors */
    for (int i = 0; i < network->num_ceramic; i++) {
        double z = hs_decap_impedance(&network->ceramic_caps[i], frequency_hz);
        if (z > 0.0 && isfinite(z)) admittance += 1.0 / z;
    }

    /* On-board high-frequency capacitors */
    for (int i = 0; i < network->num_onboard; i++) {
        double z = hs_decap_impedance(&network->on_board_caps[i], frequency_hz);
        if (z > 0.0 && isfinite(z)) admittance += 1.0 / z;
    }

    if (admittance <= 0.0) return INFINITY;
    return 1.0 / admittance;
}

/* ================================================================
 * L5: PDN frequency sweep analysis
 *
 * Sweeps from f_min to f_max (logarithmically spaced) computing
 * the PDN impedance at each frequency point.
 *
 * Algorithm: O(N_points * N_decaps)
 *
 * Reference: Smith et al., 1999; Novak, 2008
 * ================================================================ */
void hs_pdn_analyze(const hs_pdn_network_t *network,
                     double f_min_hz, double f_max_hz, int n_points,
                     hs_pdn_impedance_point_t *profile,
                     hs_pdn_result_t *result)
{
    if (!network || !profile || !result || n_points < 2 ||
        f_min_hz <= 0.0 || f_max_hz <= f_min_hz) {
        if (result) memset(result, 0, sizeof(*result));
        return;
    }

    memset(result, 0, sizeof(*result));

    /* Compute target impedance for typical 1V/5%/2A */
    result->target_impedance_ohm = hs_pdn_target_impedance(1.0, 0.05, 2.0, 1.0);

    /* DC IR drop */
    result->dc_voltage_drop_v = hs_plane_ir_drop(&network->plane,
                                                   network->vrm.max_current_a, 0);
    if (network->vrm.output_voltage_v > 0.0) {
        result->dc_voltage_drop_percent = 100.0 * result->dc_voltage_drop_v /
                                          network->vrm.output_voltage_v;
    }

    /* Plane capacitance */
    double c_plane = hs_plane_pair_capacitance(&network->plane);
    result->plane_capacitance_nf = c_plane * 1e9;

    /* First plane resonance */
    result->first_plane_resonance_hz =
        hs_plane_resonance_frequency(&network->plane);

    /* Frequency sweep (logarithmic spacing) */
    double log_f_min = log10(f_min_hz);
    double log_f_max = log10(f_max_hz);
    double max_z = 0.0;
    double max_z_freq = f_min_hz;
    int exceed_count = 0;

    for (int i = 0; i < n_points; i++) {
        double log_f = log_f_min + (log_f_max - log_f_min) *
                      (double)i / (double)(n_points - 1);
        double f = pow(10.0, log_f);
        double z = hs_pdn_total_impedance(network, f, 0.005);

        profile[i].frequency_hz = f;
        profile[i].impedance_ohm = z;
        profile[i].phase_deg = 0.0;

        if (isfinite(z) && z > max_z) {
            max_z = z;
            max_z_freq = f;
        }
        if (isfinite(z) && z > result->target_impedance_ohm) {
            exceed_count++;
        }
    }

    result->max_impedance_ohm = max_z;
    result->max_impedance_freq_hz = max_z_freq;
    result->is_compliant = (max_z <= result->target_impedance_ohm) ? 1 : 0;

    /* Estimate decaps needed */
    if (max_z > result->target_impedance_ohm &&
        result->target_impedance_ohm > 0.0 && isfinite(max_z)) {
        double ratio = max_z / result->target_impedance_ohm;
        result->num_decaps_required = (int)ceil(ratio * ratio);
    } else {
        result->num_decaps_required = 0;
    }

    /* SSN estimate (32 DDR drivers as example) */
    result->ssn_voltage_mv = hs_ssn_estimate(32, 0.020, 0.5e-9, 1.0e-9) * 1000.0;
}

/* ================================================================
 * L4: Simultaneous Switching Noise (SSN) estimate
 *
 * When N CMOS outputs switch simultaneously:
 *   V_ssn = N * L_eff * (di/dt)
 *
 * For 32 drivers at 20 mA/driver, 0.5 ns edge, L_eff = 1 nH:
 *   di/dt = 20e-3 / 0.5e-9 = 4e7 A/s
 *   V_ssn = 32 * 1e-9 * 4e7 = 1.28 V
 *
 * Reference: Senthinathan and Prince, "Simultaneous Switching Noise
 *   of CMOS Devices and Systems", Kluwer, 1994.
 * Complexity: O(1)
 * ================================================================ */
double hs_ssn_estimate(int num_drivers, double current_per_driver_a,
                        double rise_time_s, double effective_inductance_h)
{
    if (num_drivers <= 0 || current_per_driver_a <= 0.0 ||
        rise_time_s <= 0.0) return 0.0;
    double di_dt = current_per_driver_a / rise_time_s;
    double v_ssn = num_drivers * effective_inductance_h * di_dt;
    /* 70 percent alignment factor for timing skew */
    v_ssn *= 0.70;
    return v_ssn;
}

/* ================================================================
 * L5: Decap selection algorithm
 *
 * Selects optimal mix of bulk and ceramic decoupling capacitors
 * to meet Z_target across the frequency range.
 *
 * Algorithm:
 *   1. Start with plane capacitance as HF floor
 *   2. Add ceramic decaps for mid-frequency (1-100 MHz)
 *   3. Add bulk decaps for low frequency (DC to VRM BW)
 *   4. Verify across check frequencies
 *
 * Reference: Smith et al., 1999; Novak, 2008, Ch.5
 * Complexity: O(N_decaps * N_freq_checks)
 * ================================================================ */
int hs_decap_select(hs_pdn_network_t *network,
                     int max_bulk, int max_ceramic,
                     double target_z_ohm, double f_max_hz,
                     int *num_selected_bulk, int *num_selected_ceramic)
{
    if (!network || !num_selected_bulk || !num_selected_ceramic) return -1;
    *num_selected_bulk = 0;
    *num_selected_ceramic = 0;
    if (target_z_ohm <= 0.0 || f_max_hz <= 0.0 || max_bulk < 0 || max_ceramic < 0)
        return -1;

    /* Step 1: Check plane capacitance sufficiency at f_max */
    double c_plane = hs_plane_pair_capacitance(&network->plane);
    double omega_max = 2.0 * M_PI * f_max_hz;
    (void)c_plane; (void)omega_max; /* used for advanced mode analysis */

    /* Step 2: Add ceramic capacitors for mid-to-high frequencies */
    int n_ceramic = 0;
    double f_work = f_max_hz;

    while (n_ceramic < max_ceramic && f_work >= 1e5) {
        network->num_ceramic = n_ceramic;
        network->num_bulk = 0;
        double z_check = hs_pdn_total_impedance(network, f_work, 0.005);
        if (isfinite(z_check) && z_check <= target_z_ohm) break;

        /* Select capacitor: use 100 nF for >100 MHz, 470 nF for 10-100 MHz */
        double cap_val = (f_work > 1e8) ? 1.0e-7 : 4.7e-7;
        hs_decap_init(&network->ceramic_caps[n_ceramic],
                       HS_CAP_CERAMIC_X7R, cap_val, "0402");
        n_ceramic++;

        if (n_ceramic < max_ceramic) {
            network->num_ceramic = n_ceramic;
            network->num_bulk = 0;
            z_check = hs_pdn_total_impedance(network, f_work, 0.005);
            if (isfinite(z_check) && z_check <= target_z_ohm) break;
            hs_decap_init(&network->ceramic_caps[n_ceramic],
                           HS_CAP_CERAMIC_X7R, cap_val, "0402");
            n_ceramic++;
        }
        f_work /= 3.0;
    }
    *num_selected_ceramic = n_ceramic;

    /* Step 3: Add bulk capacitors for low frequency */
    int n_bulk = 0;
    network->num_ceramic = *num_selected_ceramic;

    /* Check at 100 kHz - typical VRM bandwidth limit */
    double z_low = hs_pdn_total_impedance(network, 1e5, 0.005);
    while (n_bulk < max_bulk && isfinite(z_low) && z_low > target_z_ohm) {
        double bulk_c = (n_bulk == 0) ? 1.0e-5 : 4.7e-5;
        hs_decap_init(&network->bulk_caps[n_bulk],
                       HS_CAP_TANTALUM, bulk_c, "1206");
        n_bulk++;
        network->num_bulk = n_bulk;
        z_low = hs_pdn_total_impedance(network, 1e5, 0.005);
    }
    *num_selected_bulk = n_bulk;

    /* Step 4: Verify at check frequencies */
    int compliant = 1;
    double f_checks[] = {1e4, 1e5, 1e6, 1e7, 1e8, 5e8};
    for (int i = 0; i < 6 && compliant; i++) {
        if (f_checks[i] > f_max_hz) break;
        double z = hs_pdn_total_impedance(network, f_checks[i], 0.005);
        if (!isfinite(z) || z > target_z_ohm) compliant = 0;
    }

    return compliant ? 0 : 1;
}

/* ================================================================
 * L1: Initialize a decoupling capacitor with standard values
 *
 * Fills ESR and ESL from typical manufacturer data based on
 * capacitor type, value, and package size.
 *
 * Reference: Murata GRM, TDK C, Kemet T491 series datasheets
 * Complexity: O(1)
 * ================================================================ */
void hs_decap_init(hs_decap_t *decap, hs_capacitor_type_t type,
                    double capacitance_f, const char *package)
{
    if (!decap) return;
    memset(decap, 0, sizeof(*decap));
    decap->type = type;
    decap->capacitance_f = capacitance_f;
    decap->esr_ohm = default_esr_by_type(type, capacitance_f);
    decap->esl_h = default_esl_by_package(package);
    if (package) {
        strncpy(decap->package_code, package, sizeof(decap->package_code) - 1);
        decap->package_code[sizeof(decap->package_code) - 1] = '\0';
    }
    switch (type) {
    case HS_CAP_CERAMIC_X5R:
        decap->rated_voltage_v = 6.3;
        decap->temperature_coeff = 0.15;
        break;
    case HS_CAP_CERAMIC_X7R:
        decap->rated_voltage_v = 16.0;
        decap->temperature_coeff = 0.15;
        break;
    case HS_CAP_CERAMIC_NP0:
        decap->rated_voltage_v = 50.0;
        decap->temperature_coeff = 0.0003;
        break;
    case HS_CAP_TANTALUM:
        decap->rated_voltage_v = 10.0;
        decap->temperature_coeff = 0.10;
        decap->leakage_current_a = 0.01 * capacitance_f * decap->rated_voltage_v;
        break;
    case HS_CAP_ALUM_ELEC:
        decap->rated_voltage_v = 16.0;
        decap->temperature_coeff = 0.20;
        decap->leakage_current_a = 0.03 * capacitance_f * decap->rated_voltage_v;
        break;
    case HS_CAP_POLYMER:
        decap->rated_voltage_v = 6.3;
        decap->temperature_coeff = 0.05;
        break;
    case HS_CAP_REVERSE_GEOM:
        decap->rated_voltage_v = 6.3;
        decap->temperature_coeff = 0.15;
        decap->esl_h *= 0.3;
        break;
    case HS_CAP_3TERM:
        decap->rated_voltage_v = 6.3;
        decap->temperature_coeff = 0.15;
        decap->esl_h *= 0.1;
        break;
    }
}

/* ================================================================
 * L1: Initialize a typical VRM
 *
 * Sets VRM parameters based on output voltage:
 *   - Core rails (<=1.2V): BW=100kHz, Z_out=5mOhm
 *   - I/O rails (<=3.3V):  BW=50kHz, Z_out=10mOhm
 *   - Higher voltage:      BW=20kHz, Z_out=50mOhm
 * Complexity: O(1)
 * ================================================================ */
void hs_vrm_init_typical(hs_vrm_t *vrm, double output_voltage_v,
                          double max_current_a)
{
    if (!vrm) return;
    memset(vrm, 0, sizeof(*vrm));
    vrm->output_voltage_v = output_voltage_v;
    vrm->max_current_a = max_current_a;
    if (output_voltage_v <= 1.2) {
        vrm->bandwidth_hz = 100e3;
        vrm->output_impedance_ohm = 0.005;
        vrm->efficiency = 0.88;
    } else if (output_voltage_v <= 3.3) {
        vrm->bandwidth_hz = 50e3;
        vrm->output_impedance_ohm = 0.010;
        vrm->efficiency = 0.92;
    } else {
        vrm->bandwidth_hz = 20e3;
        vrm->output_impedance_ohm = 0.050;
        vrm->efficiency = 0.90;
    }
}
