/*
 * hs_impedance.c — Characteristic & Differential Impedance Implementation
 *
 * Implements the analytical impedance formulas for all common
 * PCB transmission line geometries.
 *
 * Knowledge coverage:
 *   L1: Z₀, Z_diff, Z_odd, Z_even definitions
 *   L2: Microstrip, stripline, CPW geometry types
 *   L3: Conformal mapping methods, mode decomposition
 *   L4: IPC-2141, Wheeler, Hammerstad-Jensen, Cohn formulas
 *   L5: Newton-Raphson solver for trace width
 *   L6: Differential pair impedance design
 */

#include "hs_impedance.h"
#include <math.h>
#include <stdio.h>

#define C0          299792458.0
#define ETA0        376.730313668   /* eta0 = sqrt(mu0/eps0) = 120pi Ohm */
#define MIN_WIDTH_H 0.01
#define MAX_WIDTH_H 100.0

/* Hyperbolic cotangent: coth(x) = cosh(x)/sinh(x) */
static inline double coth(double x) {
    if (x == 0.0) return INFINITY;
    double sh = sinh(x);
    if (sh == 0.0) return INFINITY;
    return cosh(x) / sh;
}

/* ================================================================
 * Private helpers
 * ================================================================ */

/**
 * Compute self-capacitance from Z₀ and εeff.
 * C = √εeff / (c₀ × Z₀)  [F/m]
 */
static double self_c_from_z0(double z0, double er_eff)
{
    if (z0 <= 0.0 || er_eff <= 0.0) return 0.0;
    return sqrt(er_eff) / (C0 * z0);
}

/**
 * Compute self-inductance from Z₀ and εeff.
 * L = Z₀ × √εeff / c₀  [H/m]
 */
static double self_l_from_z0(double z0, double er_eff)
{
    if (z0 <= 0.0 || er_eff <= 0.0) return 0.0;
    return z0 * sqrt(er_eff) / C0;
}

/**
 * Fill impedance result with derived quantities from Z₀ and εeff.
 */
static void fill_result(hs_impedance_result_t *r, double z0_single,
                         double er_eff)
{
    if (!r) return;
    r->z0_single = z0_single;
    r->epsilon_eff = er_eff;
    r->phase_velocity = C0 / sqrt(er_eff);
    r->delay_ps_per_mm = sqrt(er_eff) / (C0 * 1e-3) * 1e12;
    r->capacitance_pf_per_m = self_c_from_z0(z0_single, er_eff) * 1e12;
    r->inductance_nh_per_m = self_l_from_z0(z0_single, er_eff) * 1e9;
    r->z0_diff = 0.0;
    r->z0_odd = 0.0;
    r->z0_even = 0.0;
    r->z0_common = 0.0;
}

/* ================================================================
 * L4: Microstrip impedance — IPC-2141 formula
 *
 * The IPC-2141 formula is the industry-standard closed-form
 * approximation for microstrip characteristic impedance.
 *
 * Accuracy: ±5% for 0.1 < w/h < 3, εr < 15.
 *
 * Derivation: Based on Wheeler's conformal mapping with empirical
 * corrections derived from measurements.
 *
 * Reference: IPC-2141A, §4.2.2.1
 * Complexity: O(1)
 * ================================================================ */
int hs_microstrip_impedance(const hs_impedance_geometry_t *geo,
                             hs_impedance_result_t *result)
{
    if (!geo || !result) return -1;
    if (geo->dielectric_height <= 0.0 || geo->trace_width <= 0.0 ||
        geo->dielectric_constant <= 1.0) return -1;

    double w = geo->trace_width;
    double t = geo->trace_thickness;
    double h = geo->dielectric_height;
    double er = geo->dielectric_constant;

    /* Adjust effective width for t < h */
    double w_eff = w;
    if (t > 0.0 && t < h) {
        w_eff = w + (t / M_PI) * (1.0 + log(2.0 * h / t));
    }

    double w_over_h = w_eff / h;
    double er_eff;
    double z0;

    if (w_over_h < 0.1 || w_over_h > 10.0) {
        return -1; /* Outside IPC-2141 validity range */
    }

    /* Effective dielectric constant */
    if (w_over_h <= 1.0) {
        er_eff = (er + 1.0) / 2.0 +
                 (er - 1.0) / 2.0 *
                 (1.0 / sqrt(1.0 + 12.0 / w_over_h) +
                  0.04 * pow(1.0 - w_over_h, 2.0));
    } else {
        er_eff = (er + 1.0) / 2.0 +
                 (er - 1.0) / 2.0 *
                 (1.0 / sqrt(1.0 + 12.0 / w_over_h));
    }

    /* IPC-2141 impedance formula */
    if (w_over_h <= 1.0) {
        z0 = (87.0 / sqrt(er + 1.41)) *
             log(5.98 * h / (0.8 * w + t));
    } else {
        z0 = (120.0 * M_PI / sqrt(er_eff)) /
             (w_over_h + 1.393 + 0.667 * log(w_over_h + 1.444));
    }

    fill_result(result, z0, er_eff);
    return 0;
}

/* ================================================================
 * L4: Microstrip impedance — Hammerstad-Jensen model
 *
 * More accurate model than IPC-2141. Includes thickness correction
 * and improved width/h range.
 *
 * Accuracy: ±0.2% for 0.01 ≤ w/h ≤ 100, εr ≤ 128
 *
 * This model is widely used in commercial field solvers as the
 * analytical starting point.
 *
 * Reference: Hammerstad & Jensen, IEEE MTT-S, 1980
 * Complexity: O(1)
 * ================================================================ */
int hs_microstrip_impedance_hj(const hs_impedance_geometry_t *geo,
                                hs_impedance_result_t *result)
{
    if (!geo || !result) return -1;
    if (geo->dielectric_height <= 0.0 || geo->trace_width <= 0.0 ||
        geo->dielectric_constant <= 1.0) return -1;

    double w = geo->trace_width;
    double t = geo->trace_thickness;
    double h = geo->dielectric_height;
    double er = geo->dielectric_constant;
    double u = w / h;

    /* Thickness correction for effective width */
    double du1 = 0.0;
    if (t > 0.0) {
        du1 = (t / (M_PI * h)) * log(1.0 + 4.0 * exp(1.0) / (t * pow(coth(sqrt(6.517 * u)), 2.0)));
    }
    double u1 = u + du1;

    double du2 = du1 / 2.0;
    double ur = u + du2;

    /* Effective dielectric constant — Hammerstad-Jensen */
    double a = 1.0 + (1.0 / 49.0) * log((pow(u, 4.0) + pow(u / 52.0, 2.0)) /
                                         (pow(u, 4.0) + 0.432)) +
               (1.0 / 18.7) * log(1.0 + pow(u / 18.1, 3.0));

    double b = 0.564 * pow((er - 0.9) / (er + 3.0), 0.053);

    double er_eff = (er + 1.0) / 2.0 +
                    (er - 1.0) / 2.0 * pow(1.0 + 10.0 / ur, -a * b);

    /* Impedance */
    double f_term = 6.0 + (2.0 * M_PI - 6.0) *
                    exp(-pow(30.666 / u1, 0.7528));
    double z0 = (60.0 / sqrt(er_eff)) *
                log(f_term / u1 + sqrt(1.0 + pow(2.0 / u1, 2.0)));

    fill_result(result, z0, er_eff);
    return 0;
}

/* ================================================================
 * L4: Symmetric stripline — Cohn's model
 *
 * For a trace centered between two ground planes separated by 2h:
 *   Z₀ = (60/√εr) × ln(4h / (π × (0.8w + t)))
 *
 * Alternative (Wheeler):
 *   Z₀ = (60/√εr) × ln(1.9 × (2h + t) / (0.8w + t))
 *
 * For an exact solution with elliptic integrals:
 *   Z₀ = (η₀/4√εr) × K(k)/K'(k)
 *
 * Reference: S.B. Cohn, IRE Trans. MTT, 1955
 * Complexity: O(1)
 * ================================================================ */
int hs_stripline_impedance(const hs_impedance_geometry_t *geo,
                            hs_impedance_result_t *result)
{
    if (!geo || !result) return -1;
    if (geo->dielectric_height <= 0.0 || geo->trace_width <= 0.0 ||
        geo->dielectric_constant <= 1.0) return -1;

    double w = geo->trace_width;
    double t = geo->trace_thickness;
    double h = geo->dielectric_height;  /* Half-separation between ground planes */
    double er = geo->dielectric_constant;

    /* Cohn's formula with thickness correction */
    double w_eff = w;
    if (t > 0.0) {
        w_eff = w + (t / (M_PI)) * log(2.0 * h / t);
    }

    double z0;
    /* For thin strip (w/(2h-t) < 0.35): */
    double ratio = w / (2.0 * h - t);
    if (ratio < 0.35 && t > 0.0) {
        z0 = (60.0 / sqrt(er)) * log(4.0 * h / (M_PI * (0.8 * w + t)));
    } else {
        /* General formula using effective width */
        z0 = (60.0 / sqrt(er)) * log(1.0 + 4.0 * h / (M_PI * w_eff) *
                                      (2.0 * h / (M_PI * w_eff) +
                                       sqrt(1.0 + pow(2.0 * h / (M_PI * w_eff), 2.0))));
    }

    /* For symmetric stripline in homogeneous medium: εeff = εr */
    fill_result(result, z0, er);

    return 0;
}

/* ================================================================
 * L4: Asymmetric stripline impedance
 *
 * Trace is offset between two ground planes at distances h1 and h2.
 * The effective height is:
 *   h_eff = 2 × h1 × h2 / (h1 + h2)
 *
 * This accounts for the asymmetry in coupling to the nearer vs
 * farther reference plane.
 *
 * Complexity: O(1)
 * ================================================================ */
int hs_stripline_asymmetric_impedance(const hs_impedance_geometry_t *geo,
                                       hs_impedance_result_t *result)
{
    if (!geo || !result) return -1;
    double h1 = geo->dielectric_height;
    double h2 = geo->dielectric_height2;

    if (h1 <= 0.0 || h2 <= 0.0 || geo->trace_width <= 0.0 ||
        geo->dielectric_constant <= 1.0) return -1;

    /* Create a symmetric equivalent geometry with h_eff */
    hs_impedance_geometry_t sym_geo = *geo;
    sym_geo.dielectric_height = 2.0 * h1 * h2 / (h1 + h2);

    return hs_stripline_impedance(&sym_geo, result);
}

/* ================================================================
 * L5: Edge-coupled differential microstrip (IPC-2141)
 *
 * Two identical microstrip traces with edge-to-edge spacing s.
 *
 * Differential impedance:
 *   Z_diff = 2 × Z₀_single × (1 - 0.48 × exp(-0.96 × s/h))
 *
 * where Z₀_single is the impedance of one trace in isolation.
 * The exponential coupling factor accounts for the field interaction
 * between the two traces.
 *
 * Odd-mode impedance: Z_odd = Z_diff / 2
 * Even-mode impedance: Z_even ≈ Z₀_single × (1 + 0.48 × exp(-0.96 × s/h))
 *
 * Reference: IPC-2141A, §4.2.4
 * Complexity: O(1)
 * ================================================================ */
int hs_diff_microstrip_impedance(const hs_impedance_geometry_t *geo,
                                  hs_impedance_result_t *result)
{
    if (!geo || !result) return -1;
    if (geo->spacing <= 0.0) return -1;

    /* First, compute single-ended impedance of one trace */
    if (hs_microstrip_impedance_hj(geo, result) != 0) return -1;

    double z0_se = result->z0_single;
    double s_over_h = geo->spacing / geo->dielectric_height;

    /* Coupling factor */
    double coupling = 0.48 * exp(-0.96 * s_over_h);

    /* Odd-mode and differential impedance */
    double z_odd = z0_se * (1.0 - coupling);
    double z_even = z0_se * (1.0 + coupling);

    result->z0_odd = z_odd;
    result->z0_even = z_even;
    result->z0_diff = 2.0 * z_odd;
    result->z0_common = z_even / 2.0;

    return 0;
}

/* ================================================================
 * L5: Edge-coupled differential stripline
 *
 * For symmetric stripline, the differential impedance follows
 * a similar pattern but with homogeneous medium properties.
 *
 * Reference: IPC-2141A, §4.2.4.2
 * Complexity: O(1)
 * ================================================================ */
int hs_diff_stripline_impedance(const hs_impedance_geometry_t *geo,
                                 hs_impedance_result_t *result)
{
    if (!geo || !result) return -1;
    if (geo->spacing <= 0.0) return -1;

    /* First, compute single-ended stripline impedance */
    if (hs_stripline_impedance(geo, result) != 0) return -1;

    double z0_se = result->z0_single;
    double s = geo->spacing;
    double h = geo->dielectric_height;
    double s_over_b = s / (2.0 * h);  /* b = 2h is total separation */

    /* Coupling for stripline */
    double coupling = 0.374 * exp(-2.9 * s_over_b);

    double z_odd = z0_se * (1.0 - coupling);
    double z_even = z0_se * (1.0 + coupling);

    result->z0_odd = z_odd;
    result->z0_even = z_even;
    result->z0_diff = 2.0 * z_odd;
    result->z0_common = z_even / 2.0;

    return 0;
}

/* ================================================================
 * L5: Coplanar waveguide (CPW) impedance
 *
 * Signal trace with coplanar ground on both sides, no bottom ground.
 * Uses complete elliptic integrals K(k).
 *
 * We approximate K(k)/K'(k) using the logarithmic approximation:
 *   K(k)/K'(k) ≈ (1/π) × ln(2 × (1+√k')/(1-√k'))  for 0 < k < 0.707
 *   K(k)/K'(k) ≈ π / ln(2 × (1+√k)/(1-√k))         for 0.707 < k < 1
 *
 * Reference: Simons, "Coplanar Waveguide Circuits", 2001
 * Complexity: O(1)
 * ================================================================ */
int hs_cpw_impedance(const hs_impedance_geometry_t *geo,
                      hs_impedance_result_t *result)
{
    if (!geo || !result) return -1;
    if (geo->trace_width <= 0.0 || geo->spacing <= 0.0) return -1;

    double w = geo->trace_width;
    double s = geo->spacing;  /* Gap between signal and side ground */
    double h = geo->dielectric_height;
    double er = geo->dielectric_constant;

    /* k = signal_width / (signal_width + 2 × gap) */
    double a = w / 2.0;
    double b = w / 2.0 + s;
    double k = a / b;

    if (k <= 0.0 || k >= 1.0) return -1;

    double k_prime = sqrt(1.0 - k * k);

    /* Approximation for K(k)/K'(k) */
    double kk_ratio;
    if (k < 0.707) {
        kk_ratio = M_PI / log(2.0 * (1.0 + sqrt(k_prime)) / (1.0 - sqrt(k_prime)));
    } else {
        kk_ratio = (1.0 / M_PI) * log(2.0 * (1.0 + sqrt(k)) / (1.0 - sqrt(k)));
    }

    /* Effective εr for CPW:
     * εeff = 1 + (εr-1)/2 × K(k')K(k₁)/K(k)K(k₁')
     * Simplified for h >> w:
     * εeff ≈ (εr+1)/2
     */
    double er_eff;
    if (h > 10.0 * (w + 2.0 * s)) {
        er_eff = (er + 1.0) / 2.0;
    } else {
        /* With bottom ground effect (CPWG case), use full formula */
        double k1 = tanh(M_PI * a / (2.0 * h)) / tanh(M_PI * b / (2.0 * h));
        double k1_prime = sqrt(1.0 - k1 * k1);
        double kk1_ratio;
        if (k1 < 0.707) {
            kk1_ratio = M_PI / log(2.0 * (1.0 + sqrt(k1_prime)) /
                                      (1.0 - sqrt(k1_prime)));
        } else {
            kk1_ratio = (1.0 / M_PI) * log(2.0 * (1.0 + sqrt(k1)) /
                                              (1.0 - sqrt(k1)));
        }
        er_eff = 1.0 + (er - 1.0) / 2.0 * kk1_ratio / kk_ratio;
    }

    double z0 = (30.0 * M_PI / sqrt(er_eff)) / kk_ratio;

    fill_result(result, z0, er_eff);
    return 0;
}

/* ================================================================
 * L4: Reflection coefficient
 *
 * Γ = (Z_L - Z₀) / (Z_L + Z₀)
 *
 * This is a direct consequence of the boundary conditions at
 * the interface between two transmission lines.
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_reflection_coefficient(double z_load, double z0)
{
    if (z0 <= 0.0) return 1.0;
    if (z_load < 0.0) z_load = 0.0;
    return (z_load - z0) / (z_load + z0);
}

/**
 * VSWR from |Γ|
 * Complexity: O(1)
 */
double hs_vswr(double gamma_magnitude)
{
    if (gamma_magnitude < 0.0) gamma_magnitude = 0.0;
    if (gamma_magnitude >= 1.0) return INFINITY;
    return (1.0 + gamma_magnitude) / (1.0 - gamma_magnitude);
}

/**
 * Return loss from |Γ|
 * Complexity: O(1)
 */
double hs_return_loss_db(double gamma_magnitude)
{
    if (gamma_magnitude <= 0.0) return INFINITY;
    if (gamma_magnitude >= 1.0) return 0.0;
    return -20.0 * log10(gamma_magnitude);
}

/**
 * Mismatch loss from |Γ|
 * Complexity: O(1)
 */
double hs_mismatch_loss_db(double gamma_magnitude)
{
    if (gamma_magnitude >= 1.0) return INFINITY;
    if (gamma_magnitude <= 0.0) return 0.0;
    return -10.0 * log10(1.0 - gamma_magnitude * gamma_magnitude);
}

/* ================================================================
 * L5: Solve trace width for target impedance
 *
 * Uses Newton-Raphson iteration on the Hammerstad-Jensen model.
 *
 * Algorithm:
 *   1. Start with initial guess from IPC-2141 inversion
 *   2. Compute Z(w_n) using Hammerstad-Jensen
 *   3. Update: w_{n+1} = w_n - (Z(w_n) - Z_target) / Z'(w_n)
 *   4. Iterate until |Z(w_n) - Z_target| < tolerance
 *
 * Convergence: Quadratic near root. Typically 4-6 iterations
 * for 0.1 Ω tolerance.
 *
 * Complexity: O(N_iterations)
 * ================================================================ */
int hs_solve_trace_width(double target_z0, hs_impedance_geometry_t *geo,
                          hs_impedance_result_t *result)
{
    if (!geo || !result) return -1;
    if (target_z0 <= 0.0) return -1;

    double h = geo->dielectric_height;
    if (h <= 0.0) return -1;

    /* Initial guess from inverted IPC-2141 wide-line formula:
     * w/h ≈ (120π/(Z₀√εeff)) - 1.393
     * Solve iteratively since εeff depends on w/h
     */
    double er = geo->dielectric_constant;
    double w_over_h = (120.0 * M_PI) / (target_z0 * sqrt((er + 1.0) / 2.0)) - 1.393;
    if (w_over_h < 0.01) w_over_h = 0.01;
    if (w_over_h > 10.0) w_over_h = 10.0;

    geo->trace_width = w_over_h * h;

    /* Newton-Raphson iteration */
    double tolerance = 0.001 * target_z0; /* 0.1% tolerance */
    int max_iter = 50;
    hs_impedance_result_t temp;

    for (int iter = 0; iter < max_iter; iter++) {
        if (hs_microstrip_impedance_hj(geo, &temp) != 0) return -1;

        double z_current = temp.z0_single;
        double error = z_current - target_z0;

        if (fabs(error) < tolerance) {
            if (result) *result = temp;
            return 0;
        }

        /* Numerical derivative: perturb w by 0.1% */
        double dw = geo->trace_width * 0.001;
        if (dw < 1e-7) dw = 1e-7;

        double w_saved = geo->trace_width;
        geo->trace_width += dw;
        hs_impedance_result_t temp2;
        if (hs_microstrip_impedance_hj(geo, &temp2) != 0) {
            geo->trace_width = w_saved;
            return -1;
        }
        double z_plus = temp2.z0_single;
        geo->trace_width = w_saved;

        double deriv = (z_plus - z_current) / dw;
        if (fabs(deriv) < 1e-12) {
            /* Flat region; try stepping */
            if (error > 0) geo->trace_width *= 1.1;
            else geo->trace_width *= 0.9;
            continue;
        }

        double step = -error / deriv;
        /* Limit step to 50% of current width to avoid overshoot */
        if (fabs(step) > 0.5 * geo->trace_width) {
            step = copysign(0.5 * geo->trace_width, step);
        }

        geo->trace_width += step;
        if (geo->trace_width < h * 0.01) geo->trace_width = h * 0.01;
        if (geo->trace_width > h * 10.0) geo->trace_width = h * 10.0;
    }

    /* Return best effort after max iterations */
    hs_microstrip_impedance_hj(geo, &temp);
    if (result) *result = temp;
    return (fabs(temp.z0_single - target_z0) < 0.01 * target_z0) ? 0 : -1;
}

/* ================================================================
 * L5: Solder mask correction to effective εr
 *
 * Solder mask (LPI, εr ≈ 3.5-4.0, 0.5-1.5 mils thick)
 * loads the microstrip capacitively, lowering Z₀ by 0.5-2 Ω.
 *
 * The correction models this as an additional dielectric layer:
 *   Δεeff = (εr_coat - 1) × 2 × t_coat/h × exp(-1.5 × w/h)
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_solder_mask_er_correction(const hs_impedance_geometry_t *geo,
                                     double base_er_eff)
{
    if (!geo) return base_er_eff;
    if (geo->coating_thickness <= 0.0 || geo->coating_er <= 1.0) {
        return base_er_eff;
    }

    double w_over_h = geo->trace_width / geo->dielectric_height;
    double t_coat_over_h = geo->coating_thickness / geo->dielectric_height;
    double delta_er = (geo->coating_er - 1.0) * 2.0 * t_coat_over_h *
                       exp(-1.5 * w_over_h);

    return base_er_eff + delta_er;
}

/* ================================================================
 * L3: Coupling coefficient
 *
 * k = (Z_even - Z_odd) / (Z_even + Z_odd)
 *
 * This dimensionless coefficient quantifies the coupling between
 * two transmission lines. k ∈ [0, 1) where 0 means uncoupled
 * and values approaching 1 indicate very strong coupling.
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_coupling_coefficient(double z_even, double z_odd)
{
    if (z_even <= 0.0 || z_odd <= 0.0) return 0.0;
    return (z_even - z_odd) / (z_even + z_odd);
}

/* ================================================================
 * L5: Minimum spacing for target coupling
 *
 * Inverse model for estimating required trace spacing to
 * keep coupling below a target level.
 *
 * s_min ≈ h × ln(2 / (1 - exp(-k_base)))  (approximate)
 *
 * where k_base is calibrated for the geometry.
 *
 * Complexity: O(1)
 * ================================================================ */
double hs_min_spacing_for_coupling(double target_coupling, double height_m)
{
    if (target_coupling <= 0.0 || height_m <= 0.0) return 0.0;

    /* For target coupling k, the approximate spacing is:
     * s/h ≈ -ln(k / 0.48) / 0.96    (from IPC-2141 diff formula inversion)
     */
    if (target_coupling >= 0.48) target_coupling = 0.479;
    double s_over_h = -log(target_coupling / 0.48) / 0.96;
    if (s_over_h < 0.0) s_over_h = 0.0;

    return s_over_h * height_m;
}
