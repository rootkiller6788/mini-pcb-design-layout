/*
 * hs_via.c - Via Modeling and Design for High-Speed PCBs
 *
 * Implements via electrical modeling: partial inductance, pad capacitance,
 * DC resistance, stub resonance, differential via modeling, and
 * via optimization algorithms.
 *
 * Knowledge coverage:
 *   L1: Via types, barrel inductance, pad capacitance, stub resonance
 *   L2: Via transition impedance discontinuity, return path disruption
 *   L3: Partial inductance of cylindrical conductors, coaxial cap model
 *   L4: Via inductance formula, quarter-wave stub resonance
 *   L5: Via optimization (antipad tuning), stitching via count
 *   L6: Differential via model, back-drill decision algorithm
 */

#include "hs_via.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

#define C0      299792458.0
#define MU0     1.2566370614e-6
#define EPS0    8.854187817e-12
#define RHO_CU  1.72e-8

/* ================================================================
 * L4: Via partial inductance
 *
 * For a cylindrical conductor of length h and radius r (r << h):
 *   L_via = (mu0 * h / (2*pi)) * [ln(2h/r) - 1 + r/h]
 *
 * Simplified for r << h:
 *   L_via = (mu0 * h / (2*pi)) * ln(2h/r)
 *
 * In practical units (mils):
 *   L_via[nH] = 5.08 * h[mils] * [ln(2h/r) + 1]
 *
 * Typical via (h=1.57mm=62mils, r=0.127mm=5mils):
 *   L = 1.26e-6 * 1.57e-3 / (2*pi) * ln(2*1.57e-3/0.127e-3)
 *   = 1.0e-10 * ln(24.7) = 1.0e-10 * 3.21 = 0.32 nH
 *
 * At 1 GHz: Z_L = 2*pi*f*L = 2*pi*1e9*0.32e-9 = 2.0 Ohms
 * At 5 GHz: Z_L = 10 Ohms - significant!
 *
 * Reference: Johnson and Graham, Eq. 7.19; Grover, Ch.5
 * Complexity: O(1)
 * ================================================================ */
double hs_via_inductance(const hs_via_geometry_t *geo)
{
    if (!geo) return 0.0;
    double h = geo->barrel_length_m;
    double r = geo->hole_diameter_m / 2.0;
    if (h <= 0.0 || r <= 0.0) return 0.0;
    if (r >= h) return 0.0;

    /* Full formula with r/h correction term */
    double ratio = 2.0 * h / r;
    if (ratio < 1.0) ratio = 1.0;
    double correction = 1.0 - r / h; /* -1 + r/h from full formula */
    double ln_term = log(ratio) + correction;

    return (MU0 * h / (2.0 * M_PI)) * ln_term;
}

/* ================================================================
 * L3: Via pad capacitance to reference planes
 *
 * The pad-to-plane capacitance for a circular pad with antipad:
 *   C_pad = eps0 * er * pi * (d_pad^2 - d_anti^2) / (4 * clearance)
 *
 * This is the parallel-plate approximation for the annular ring
 * of copper around the hole that faces the reference plane.
 *
 * Pad area = (pi/4) * (d_pad^2 - d_hole^2)
 * But some of this faces the antipad clearance, so effective area is
 * the annular region between pad edge and antipad edge
 *
 * For d_pad=0.6mm, d_anti=0.9mm, clearance=0.2mm, er=4.0:
 *   Area = pi/4 * (0.0009^2 - 0.0006^2) + pi/4 * (0.0006^2 - 0.0003^2)
 *   C = 8.85e-12 * 4.0 * area / 0.0002 = 0.42 pF
 *
 * Typical values: 0.2-0.8 pF per pad.
 *
 * Reference: Johnson and Graham, Eq. 7.15; LaMeres, Ch.9.3.2
 * Complexity: O(1)
 * ================================================================ */
double hs_via_pad_capacitance(const hs_via_geometry_t *geo, int pad_end)
{
    if (!geo) return 0.0;
    (void)pad_end;

    double d_pad = geo->pad_diameter_m;
    double d_anti = geo->antipad_diameter_m;

    if (d_pad <= 0.0 || d_anti <= d_pad) return 0.0;

    /* Pad annular area: region between via barrel edge and pad edge,
     * which forms a parallel-plate capacitor with the reference plane.
     * 
     * Use the antipad region area minus hole area:
     * Effective area = (pi/4)*(d_anti^2 - d_hole^2)
     * Capacitance = eps0 * er * Area / clearance_gap
     *
     * The clearance gap is (d_anti - d_pad)/2 from pad edge to plane edge.
     */
    double d_hole = geo->hole_diameter_m;
    double area_anti = (M_PI / 4.0) * (d_anti * d_anti - d_hole * d_hole);
    if (area_anti <= 0.0) area_anti = (M_PI / 4.0) * (d_pad * d_pad - d_hole * d_hole);

    double gap = (d_anti - d_pad) / 2.0;
    if (gap <= 0.0) gap = 0.1e-3; /* assume 0.1 mm minimum gap */

    /* Use er=4.0 as default FR-4 value */
    double er = 4.0;
    return EPS0 * er * area_anti / gap;
}

/* ================================================================
 * L2: Via barrel DC resistance
 *
 * R_dc = rho * h / (pi * (r_outer^2 - r_inner^2))
 *
 * The barrel is a cylindrical shell of copper with:
 *   r_outer = d_hole/2 + plating_thickness
 *   r_inner = d_hole/2
 *
 * For d_hole=0.25mm (10 mils), plating=25um (1 mil), h=1.6mm (62 mils):
 *   Cross-section = pi * ((0.15e-3)^2 - (0.125e-3)^2)
 *                 = pi * (2.25e-8 - 1.56e-8) = 2.17e-8 m^2
 *   R_dc = 1.72e-8 * 1.6e-3 / 2.17e-8 = 1.27 mOhm
 *
 * This is typically negligible for signal integrity but matters
 * for power vias carrying several amps.
 *
 * Reference: IPC-2221, Ch.6
 * Complexity: O(1)
 * ================================================================ */
double hs_via_dc_resistance(const hs_via_geometry_t *geo)
{
    if (!geo) return 0.0;
    double h = geo->barrel_length_m;
    double r_inner = geo->hole_diameter_m / 2.0;
    double r_outer = r_inner + geo->plating_thickness_m;

    if (h <= 0.0 || r_outer <= r_inner || r_outer <= 0.0) return 0.0;

    double area = M_PI * (r_outer * r_outer - r_inner * r_inner);
    if (area <= 0.0) return 0.0;

    return RHO_CU * h / area;
}

/* ================================================================
 * L5: Build complete lumped-element via model
 *
 * Combines inductance, pad capacitances, and resistance into
 * a unified model. Also computes approximate impedance and
 * stub resonant frequency.
 *
 * The via is modeled as a pi-network:
 *   Port1 --- L_via --- Port2
 *            C1  C2
 *             |   |
 *            GND GND
 *
 * Impedance: Z_via = sqrt(L_via / (C1 + C2))
 * Stub resonance: f_stub = c0 / (4 * L_stub * sqrt(er))
 *
 * Complexity: O(1)
 * ================================================================ */
hs_via_model_t hs_via_build_model(const hs_via_geometry_t *geo, double er)
{
    hs_via_model_t model;
    memset(&model, 0, sizeof(model));

    if (!geo || er <= 0.0) return model;

    /* Inductance of barrel */
    model.inductance_ph = hs_via_inductance(geo) * 1e12;

    /* Pad capacitances at both ends */
    model.capacitance_pad_ff[0] = hs_via_pad_capacitance(geo, 0) * 1e15;
    model.capacitance_pad_ff[1] = hs_via_pad_capacitance(geo, 1) * 1e15;

    /* DC resistance */
    model.resistance_mohm = hs_via_dc_resistance(geo) * 1e3;

    /* Characteristic impedance of the via:
     * Z_via = sqrt(L / (C1 + C2))
     */
    double l = hs_via_inductance(geo);
    double c_total = hs_via_pad_capacitance(geo, 0) +
                     hs_via_pad_capacitance(geo, 1);
    if (c_total > 0.0 && l > 0.0) {
        model.impedance_ohm = sqrt(l / c_total);
    }

    /* Stub quarter-wave resonance */
    if (geo->stub_length_m > 0.0) {
        model.resonant_freq_ghz = hs_via_stub_resonance(geo->stub_length_m, er) * 1e-9;
    } else {
        model.resonant_freq_ghz = 0.0; /* No stub, no resonance */
    }

    /* 3 dB bandwidth of the via pi-network:
     * BW = 1 / (pi * sqrt(L * C_equiv))
     * where C_equiv = C1*C2 / (C1+C2)
     */
    double c1 = hs_via_pad_capacitance(geo, 0);
    double c2 = hs_via_pad_capacitance(geo, 1);
    if (c1 > 0.0 && c2 > 0.0 && l > 0.0) {
        double c_equiv = (c1 * c2) / (c1 + c2);
        double lc = l * c_equiv;
        if (lc > 0.0) {
            model.bandwidth_ghz = 1.0 / (M_PI * sqrt(lc)) * 1e-9;
        }
    }

    return model;
}

/* ================================================================
 * L4: Stub resonant frequency - Quarter-wave theorem
 *
 * For a via stub (unused portion of barrel extending past the
 * signal layer connection to an open end):
 *
 *   f_res = c0 / (4 * L_stub * sqrt(er))
 *
 * This is a quarter-wave resonance: the stub appears as an
 * open circuit at its connection point, creating a transmission
 * null that severely degrades insertion loss.
 *
 * For FR-4 (er=4.0):
 *   L_stub=2.54mm (100mils) -> f_res = 14.8 GHz
 *   L_stub=5.08mm (200mils) -> f_res = 7.4 GHz
 *   L_stub=7.62mm (300mils) -> f_res = 4.9 GHz <- problem for 10G!
 *
 * Rule of thumb: back-drill if f_res < 3 * signal_bandwidth.
 *
 * Reference: Johnson and Graham, Eq. 7.12; Hall and Heck, Ch.6
 * Complexity: O(1)
 * ================================================================ */
double hs_via_stub_resonance(double stub_length_m, double er)
{
    if (stub_length_m <= 0.0 || er <= 0.0) return 0.0;
    return C0 / (4.0 * stub_length_m * sqrt(er));
}

/* ================================================================
 * L6: Back-drill depth decision
 *
 * Back-drilling removes the unused portion of a through-hole via
 * to eliminate stub resonance. The required depth is the distance
 * from the non-connected end to slightly past the last connected
 * signal layer.
 *
 * Decision: back-drill is needed if:
 *   f_stub_resonance < 3 * signal_bandwidth
 *
 * Required depth = total_barrel_length - layer_end_depth + margin
 * where layer_end_depth is the distance from the top to the
 * deepest connected layer.
 *
 * Typical margin: 0.15 mm (6 mils) to ensure stub removal.
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_via_backdrill_depth(const hs_via_geometry_t *geo,
                               double signal_bw_hz)
{
    if (!geo) return 0.0;

    /* Check if back-drilling is needed */
    if (geo->stub_length_m <= 0.0) return 0.0;

    double er = 4.0; /* Assume FR-4 */
    double f_res = hs_via_stub_resonance(geo->stub_length_m, er);

    /* Back-drill if resonance is within 3x signal bandwidth */
    if (f_res > 0.0 && f_res < 3.0 * signal_bw_hz) {
        /* Required depth: from bottom of board to past last connected layer */
        /* Conservative: drill to stub_length plus 0.15 mm margin */
        double depth = geo->stub_length_m + 0.15e-3;
        if (depth > geo->barrel_length_m) depth = geo->barrel_length_m;
        return depth;
    }

    return 0.0; /* Back-drilling not required */
}

/* ================================================================
 * L3: Via characteristic impedance
 *
 * Z_via = sqrt(L_via / (C1 + C2))
 *
 * The via acts as a short transmission line segment with
 * characteristic impedance Z_via. Impedance matching
 * minimizes reflections at the via transition.
 *
 * For a 50 Ohm system, the via impedance should also be ~50 Ohm.
 * This can be achieved by tuning the antipad diameter.
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_via_impedance(const hs_via_model_t *model)
{
    if (!model) return 0.0;
    return model->impedance_ohm;
}

/* ================================================================
 * L3: Via 3 dB bandwidth
 *
 * BW = 1 / (pi * sqrt(L_via * C_equiv))
 *
 * where C_equiv = C1*C2/(C1+C2) is the series combination of
 * the pad capacitances in the pi-network model.
 *
 * This is the frequency at which the via introduces 3 dB of
 * insertion loss, limiting the maximum usable signal bandwidth.
 *
 * For L=0.3 nH, C1=C2=0.4 pF:
 *   C_equiv = 0.2 pF
 *   BW = 1 / (pi * sqrt(0.3e-9 * 0.2e-12))
 *      = 1 / (pi * sqrt(6e-23))
 *      = 1 / (pi * 7.75e-12)
 *      = 41 GHz
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_via_bandwidth(const hs_via_model_t *model)
{
    if (!model) return 0.0;
    return model->bandwidth_ghz * 1e9;
}

/* ================================================================
 * L6: Differential via model
 *
 * Two single-ended vias form a differential pair when driven
 * by complementary signals.
 *
 * Odd-mode impedance: Z_odd = Z_se * (1 - k)
 * Even-mode impedance: Z_even = Z_se * (1 + k)
 * Differential impedance: Z_diff = 2 * Z_odd
 * Common-mode impedance: Z_cm = Z_even / 2
 *
 * Coupling coefficient k depends on the pitch-to-barrel-length ratio:
 *   k = exp(-2.0 * pitch / barrel_length)
 *
 * For pitch=1.0mm, barrel=1.6mm: k = exp(-1.25) = 0.29
 *
 * Skew arises from any asymmetry in the two via paths.
 *
 * Complexity: O(1)
 * ================================================================ */
hs_diff_via_model_t hs_diff_via_model(const hs_via_geometry_t *geo, double er)
{
    hs_diff_via_model_t diff_model;
    memset(&diff_model, 0, sizeof(diff_model));

    if (!geo || er <= 0.0) return diff_model;

    /* Build single-ended model for one via */
    diff_model.via_p = hs_via_build_model(geo, er);
    diff_model.via_n = hs_via_build_model(geo, er);

    /* Coupling coefficient between the two vias */
    double pitch = geo->pitch_m;
    double h = geo->barrel_length_m;

    if (pitch > 0.0 && h > 0.0) {
        diff_model.coupling_coeff = exp(-2.0 * pitch / h);
    } else {
        diff_model.coupling_coeff = 0.0;
    }

    double k = diff_model.coupling_coeff;
    double z_se = diff_model.via_p.impedance_ohm;

    /* Odd-mode impedance */
    double z_odd = z_se * (1.0 - k);
    if (z_odd < 0.0) z_odd = z_se * 0.5;

    /* Differential impedance */
    diff_model.diff_impedance_ohm = 2.0 * z_odd;

    /* Differential bandwidth (slightly higher than SE due to reduced effective L) */
    if (diff_model.via_p.bandwidth_ghz > 0.0) {
        diff_model.diff_bandwidth_ghz = diff_model.via_p.bandwidth_ghz * (1.0 + 0.3 * k);
    }

    /* Skew estimate: based on 1% geometric asymmetry */
    diff_model.skew_ps = 0.01 * h / C0 * sqrt(er) * 1e12;

    return diff_model;
}

/* ================================================================
 * L5: Optimize antipad diameter for target impedance
 *
 * Tuning the antipad diameter is the primary method for
 * adjusting via impedance. Larger antipad reduces pad-to-plane
 * capacitance, increasing impedance.
 *
 * Algorithm: Binary search on antipad diameter within
 * manufacturing constraints (min_antipad = 1.5 * d_pad,
 * max_antipad = 3.0 * d_pad typical).
 *
 * Complexity: O(log(range/tolerance))
 * ================================================================ */
int hs_via_optimize_antipad(hs_via_geometry_t *geo, double target_z, double er)
{
    if (!geo || target_z <= 0.0 || er <= 0.0) return -1;

    double d_pad = geo->pad_diameter_m;
    double d_min = d_pad * 1.2; /* Minimum antipad: 20% larger than pad */
    double d_max = d_pad * 3.0; /* Maximum antipad: 3x pad diameter */
    if (d_min < 0.3e-3) d_min = 0.3e-3;

    double best_d = geo->antipad_diameter_m;
    double best_err = INFINITY;
    int max_iter = 30;
    double tolerance = 0.01 * target_z;

    for (int iter = 0; iter < max_iter; iter++) {
        double d_mid = (d_min + d_max) / 2.0;
        geo->antipad_diameter_m = d_mid;

        hs_via_model_t model = hs_via_build_model(geo, er);
        double z = model.impedance_ohm;

        if (z <= 0.0) break;

        double err = fabs(z - target_z);
        if (err < best_err) {
            best_err = err;
            best_d = d_mid;
        }
        if (err < tolerance) break;

        if (z > target_z) {
            /* Too high: need more capacitance (smaller antipad) */
            d_max = d_mid;
        } else {
            /* Too low: need less capacitance (larger antipad) */
            d_min = d_mid;
        }
    }

    geo->antipad_diameter_m = best_d;
    return (best_err < 0.05 * target_z) ? 0 : -1;
}

/* ================================================================
 * L5: Ground stitching via count
 *
 * Stitching vias connect ground planes together at regular
 * intervals along a high-speed signal path. They provide
 * a low-inductance return current path and reduce EMI.
 *
 * Maximum spacing: lambda/10 at the highest frequency component.
 * Number of vias = ceil(route_length / spacing)
 *
 * For 10 Gbps signal (BW=7 GHz on FR-4, lambda=2.3 cm):
 *   Spacing = 2.3 mm
 *   For 10 cm route: N = ceil(100/2.3) = 44 vias
 *
 * Practical guideline: spacing = 0.5-2 cm for digital, 1-5 mm for RF.
 *
 * Reference: Bogatin, Ch.12; Johnson and Graham, Ch.7
 * Complexity: O(1)
 * ================================================================ */
double hs_stitching_via_count(double route_length_m, double max_freq_hz,
                               double er_eff)
{
    if (route_length_m <= 0.0 || max_freq_hz <= 0.0 || er_eff <= 0.0) return 0.0;

    /* Wavelength at max frequency */
    double lambda = C0 / (max_freq_hz * sqrt(er_eff));
    if (lambda <= 0.0) return 0.0;

    /* Spacing = lambda / 10 */
    double spacing = lambda / 10.0;
    if (spacing <= 0.0) return 0.0;

    /* Number of vias = ceil(route_length / spacing) + 1 for end via */
    return ceil(route_length_m / spacing) + 1.0;
}

/* ================================================================
 * L6: Via manufacturability check
 *
 * Verifies via geometry against standard PCB manufacturing
 * constraints (IPC-2221 / IPC-6012).
 *
 * Checks:
 *   1. Hole diameter >= minimum drill size (0.15 mm typical)
 *   2. Aspect ratio (barrel/hole) <= max_aspect_ratio (8:1 typical)
 *   3. Annular ring (pad - hole)/2 >= 0.05 mm minimum
 *   4. Antipad >= 1.2 * pad diameter
 *   5. Stub length <= max allowed (or must back-drill)
 *
 * Returns 0 if feasible, error code otherwise:
 *   1 = hole too small
 *   2 = aspect ratio exceeded
 *   3 = annular ring too small
 *   4 = antipad too small
 *   5 = stub too long
 *
 * Complexity: O(1)
 * ================================================================ */
int hs_via_check_feasibility(const hs_via_geometry_t *geo,
                              const hs_via_constraints_t *constraints)
{
    if (!geo || !constraints) return -1;

    /* Check 1: Minimum hole diameter */
    double d_hole_min = 0.15e-3; /* 0.15 mm = 6 mils typical */
    if (geo->hole_diameter_m < d_hole_min) return 1;

    /* Check 2: Aspect ratio */
    if (geo->hole_diameter_m > 0.0 && constraints->max_aspect_ratio > 0.0) {
        double aspect = geo->barrel_length_m / geo->hole_diameter_m;
        if (aspect > constraints->max_aspect_ratio) return 2;
    }

    /* Check 3: Minimum annular ring */
    double annular_ring = (geo->pad_diameter_m - geo->hole_diameter_m) / 2.0;
    double min_ring = 0.05e-3; /* 0.05 mm = 2 mils minimum */
    if (annular_ring < min_ring) return 3;

    /* Check 4: Antipad to pad ratio */
    if (geo->pad_diameter_m > 0.0 && constraints->min_antipad_ratio > 0.0) {
        double ratio = geo->antipad_diameter_m / geo->pad_diameter_m;
        if (ratio < constraints->min_antipad_ratio) return 4;
    }

    /* Check 5: Stub length */
    if (geo->stub_length_m > constraints->max_stub_length_m &&
        constraints->max_stub_length_m > 0.0) {
        return 5;
    }

    return 0; /* Feasible */
}
