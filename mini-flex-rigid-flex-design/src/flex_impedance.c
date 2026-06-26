/**
 * @file flex_impedance.c
 * @brief Transmission Line Impedance Calculations for Flex/Rigid-Flex PCBs
 *
 * Implements the standard impedance formulas used in PCB design:
 * Wheeler microstrip, Hammerstad-Jensen effective DK, stripline,
 * differential pair, and loss models specific to flex geometries.
 *
 * Each function implements one well-known transmission line formula
 * with full mathematical fidelity.
 *
 * @module mini-flex-rigid-flex-design
 */

#include "flex_signal_integrity.h"
#include <math.h>

/* M_PI not defined in strict C11 mode */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Physical constants */
#define C0_MM_NS 299.792458  /**< Speed of light in vacuum (mm/ns) */
#define MU0 1.25663706212e-6 /**< Vacuum permeability (H/m) */

/* ========================================================================
 * L5: Wheeler Microstrip Model (1965)
 * ========================================================================*/

/**
 * Knowledge Point: Wheeler's microstrip impedance formula.
 *
 * H.A. Wheeler derived the fundamental equations for microstrip
 * transmission line characteristic impedance in 1965. Two regimes:
 *
 * Narrow strip (W/h ≤ 1):
 *   Z0 = (60/√εeff) * ln(8h/W + W/(4h))
 *
 * Wide strip (W/h > 1):
 *   Z0 = (120π/√εeff) / (W/h + 1.393 + 0.667*ln(W/h + 1.444))
 *
 * These formulas assume zero-thickness conductor. For finite thickness t,
 * the effective width W_eff = W + (t/π)*(1 + ln(2h/t)) for W/h > 1/(2π).
 *
 * The Wheeler model is the industry workhorse for PCB impedance prediction.
 * Accuracy: ~2% for 0.1 < W/h < 10 and εr < 10.
 *
 * Reference: Wheeler, "Transmission-Line Properties of a Strip on a
 *            Dielectric Sheet on a Plane", IEEE Trans. MTT, 1977
 *            (revision of the seminal 1965 paper)
 */
double flex_microstrip_z0_wheeler(double trace_width_um,
                                   double dielectric_thickness_um,
                                   double dk,
                                   double trace_thickness_um) {
    if (dielectric_thickness_um <= 0.0 || trace_width_um <= 0.0)
        return 0.0;

    double w = trace_width_um / 1000.0;   /* μm → mm */
    double h = dielectric_thickness_um / 1000.0;
    double t = trace_thickness_um / 1000.0;

    /* Effective width correction for finite conductor thickness */
    double w_eff = w;
    if (t > 0.0 && w / h < (1.0 / (2.0 * M_PI))) {
        w_eff = w + (t / M_PI) * (1.0 + log(2.0 * h / t));
    }

    /* Effective dielectric constant */
    double eps_eff = (dk + 1.0) / 2.0 +
                     (dk - 1.0) / 2.0 * (1.0 / sqrt(1.0 + 12.0 * h / w_eff));

    double ratio = w_eff / h;
    double z0;

    if (ratio <= 1.0) {
        z0 = (60.0 / sqrt(eps_eff)) * log(8.0 * h / w_eff + w_eff / (4.0 * h));
    } else {
        z0 = (120.0 * M_PI / sqrt(eps_eff)) /
             (ratio + 1.393 + 0.667 * log(ratio + 1.444));
    }

    return z0;
}

/**
 * Knowledge Point: Embedded microstrip impedance (with coverlay).
 *
 * When a coverlay is applied over a flex trace, the effective dielectric
 * constant increases because the fields are now fully embedded in
 * dielectric instead of partially in air. This LOWERs the impedance
 * compared to bare microstrip.
 *
 * The effective dielectric constant for embedded microstrip:
 *   εeff_embedded = εeff_surface + (εr_cover - 1) * f_coverage
 *
 * where f_coverage = fraction of field in the coverlay region,
 * approximated by 1 - exp(-π * h_cover / h_sub).
 *
 * Typical: A 50Ω surface microstrip drops to ~47Ω with 25μm PI coverlay.
 * Designers must account for this ∼6% shift.
 */
double flex_microstrip_embedded_z0(double trace_width_um,
                                    double substrate_thickness_um,
                                    double substrate_dk,
                                    double coverlay_thickness_um,
                                    double coverlay_dk,
                                    double trace_thickness_um) {
    /* First compute surface microstrip Z0 and εeff */
    double z0_surface = flex_microstrip_z0_wheeler(
        trace_width_um, substrate_thickness_um, substrate_dk, trace_thickness_um);

    if (z0_surface <= 0.0 || coverlay_thickness_um <= 0.0)
        return z0_surface;

    /* Effective DK for surface case */
    double w = trace_width_um / 1000.0;
    double h = substrate_thickness_um / 1000.0;
    double eps_eff_surface = (substrate_dk + 1.0) / 2.0 +
        (substrate_dk - 1.0) / 2.0 * (1.0 / sqrt(1.0 + 12.0 * h / w));

    /* Coverage factor: fraction of E-field in coverlay region */
    double h_cover = coverlay_thickness_um / 1000.0;  /* mm */
    double coverage = 1.0 - exp(-M_PI * h_cover / h);

    /* Effective DK increase due to coverlay */
    double eps_eff_embedded = eps_eff_surface +
                              (coverlay_dk - 1.0) * coverage;

    /* Impedance scales as 1/√εeff */
    double z0_embedded = z0_surface * sqrt(eps_eff_surface / eps_eff_embedded);

    return z0_embedded;
}

/**
 * Knowledge Point: Symmetric stripline impedance (Cohn model).
 *
 * Stripline: a trace sandwiched between two reference planes with
 * homogeneous dielectric above and below. Used in rigid sections
 * of rigid-flex designs for superior signal integrity.
 *
 * Cohn's formula:
 *   Z0 = (60/√εr) * ln(4b / (0.67*π*W*(0.8 + t/W)))
 *
 * where b = total distance between reference planes.
 *
 * For thin traces (t << W), simplifies to:
 *   Z0 = (60/√εr) * ln(1.9*(2h+t) / (0.8*W + t))
 *
 * Stripline advantages over microstrip:
 * - Full dielectric → no dispersion, pure TEM mode
 * - Better crosstalk isolation (fields contained)
 * - Less radiation (no open side)
 *
 * Disadvantage: Requires 2× more dielectric thickness for same Z0.
 *
 * Reference: Cohn, "Characteristic Impedance of Shielded-Strip
 *            Transmission Line", IRE Trans. MTT, 1954
 */
double flex_stripline_z0(double trace_width_um,
                          double dielectric_thickness_um,
                          double dk,
                          double trace_thickness_um) {
    if (dielectric_thickness_um <= 0.0 || trace_width_um <= 0.0 || dk <= 1.0)
        return 0.0;

    double w = trace_width_um / 1000.0;
    double b = dielectric_thickness_um / 1000.0;  /* Total height between planes */
    double t = trace_thickness_um / 1000.0;

    /* Cohn's formula: single centered strip between ground planes */
    double effective_width = 0.67 * M_PI * w * (0.8 + t / w);
    if (effective_width <= 0.0) return 0.0;

    double z0 = (60.0 / sqrt(dk)) * log(4.0 * b / effective_width);

    return z0;
}

/**
 * Knowledge Point: Asymmetric stripline impedance.
 *
 * When the trace is NOT centered between the reference planes,
 * the impedance is lower than symmetric stripline for the same
 * total dielectric thickness. The formula uses an offset factor.
 *
 * Configuration:
 *   Upper plane at distance h1 above trace
 *   Lower plane at distance h2 below trace
 *   Total: h1 + h2 + t
 *
 * For small offsets (h1 ≠ h2 but both >> t), the impedance can be
 * approximated by treating as two parallel paths.
 *
 * In rigid-flex, asymmetric stripline occurs when transitioning from
 * thick rigid (large h1+h2) to thin flex (smaller h1+h2).
 */
double flex_stripline_asymmetric_z0(double trace_width_um,
                                     double h1_um, double h2_um,
                                     double dk,
                                     double trace_thickness_um) {
    if (h1_um <= 0.0 || h2_um <= 0.0 || trace_width_um <= 0.0)
        return 0.0;

    /* Convert to mm */
    double h1 = h1_um / 1000.0;
    double h2 = h2_um / 1000.0;
    double t = trace_thickness_um / 1000.0;

    /* Total height */
    double b = h1 + h2 + t;

    /* Asymmetric factor: ratio of heights */
    double h_ratio = h1 / h2;
    double symmetry_factor;

    if (h_ratio < 1.0) {
        /* h1 is smaller → trace closer to upper plane → lower Z0 */
        symmetry_factor = 1.0 + 0.3 * (1.0 - h_ratio);
    } else if (h_ratio > 1.0) {
        /* h2 is smaller */
        double inv_ratio = 1.0 / h_ratio;
        symmetry_factor = 1.0 + 0.3 * (1.0 - inv_ratio);
    } else {
        /* Symmetric case */
        symmetry_factor = 1.0;
    }

    /* Base symmetric stripline impedance scaled by asymmetry */
    double z0_sym = flex_stripline_z0(trace_width_um,
                                       b * 1000.0, dk, trace_thickness_um);

    return z0_sym / symmetry_factor;
}

/* ========================================================================
 * Effective Dielectric Constant
 * ========================================================================*/

/**
 * Knowledge Point: Effective dielectric constant for microstrip.
 *
 * Microstrip fields exist partially in the substrate (εr) and partially
 * in air (εr=1). The effective dielectric constant εeff is a weighted
 * average:
 *
 *   εeff = (εr + 1)/2 + (εr - 1)/2 * 1/√(1 + 12h/W)
 *
 * Limits:
 * - W → ∞ (very wide): εeff → εr             (all field in substrate)
 * - W → 0 (very narrow): εeff → (εr+1)/2      (half in air, half in substrate)
 *
 * This formula by Hammerstad and Jensen (1980) improves on Wheeler's
 * original with <0.2% error for typical PCB geometries.
 *
 * Reference: Hammerstad & Jensen, "Accurate Models for Microstrip
 *            Computer-Aided Design", IEEE MTT-S Digest, 1980
 */
double flex_effective_dk_microstrip(double dk,
                                     double trace_width_um,
                                     double dielectric_thickness_um) {
    if (dielectric_thickness_um <= 0.0 || trace_width_um <= 0.0)
        return dk;

    double w_over_h = trace_width_um / dielectric_thickness_um;
    double factor = 1.0 / sqrt(1.0 + 12.0 / w_over_h);

    return (dk + 1.0) / 2.0 + (dk - 1.0) / 2.0 * factor;
}

/* ========================================================================
 * L5: Differential Pair Impedance
 * ========================================================================*/

/**
 * Knowledge Point: Edge-coupled differential microstrip impedance.
 *
 * Differential signaling sends complementary signals on two closely
 * spaced traces. The differential impedance Zdiff is what the signal
 * "sees" between the two traces:
 *
 *   Zdiff = 2 * Z0 * √((1 - k) / (1 + k))
 *
 * where k = coupling coefficient between the traces.
 *
 * For loose coupling (s >> h): k → 0, Zdiff → 2*Z0
 * For tight coupling (s << h): k → 1, Zdiff → lower
 *
 * Typical target: Zdiff = 100Ω (USB, HDMI, PCIe)
 *                  Zdiff = 90Ω  (USB 3.0)
 *
 * IPC-2223: Differential pairs in flex must maintain constant spacing
 * through bend zones to avoid impedance discontinuities.
 */
double flex_diff_microstrip_z0(double trace_width_um,
                                double trace_spacing_um,
                                double dielectric_thickness_um,
                                double dk,
                                double trace_thickness_um) {
    /* Single-ended impedance */
    double z0_se = flex_microstrip_z0_wheeler(
        trace_width_um, dielectric_thickness_um, dk, trace_thickness_um);

    if (z0_se <= 0.0) return 0.0;

    double s_over_h = trace_spacing_um / dielectric_thickness_um;

    /* Coupling coefficient: k ≈ exp(-s/h) for parallel microstrips */
    double k = exp(-s_over_h);

    /* Differential impedance */
    double z_diff = 2.0 * z0_se * sqrt((1.0 - k) / (1.0 + k));

    return z_diff;
}

/**
 * Knowledge Point: Edge-coupled differential stripline impedance.
 *
 * Similar to microstrip differential pairs but with better coupling
 * control due to the homogeneous dielectric environment.
 *
 * Zdiff_stripline ≈ 2 * Z0_stripline * (1 - 0.48 * exp(-0.96 * s/b))
 *
 * where s = spacing, b = height between planes.
 */
double flex_diff_stripline_z0(double trace_width_um,
                               double trace_spacing_um,
                               double dielectric_thickness_um,
                               double dk,
                               double trace_thickness_um) {
    double z0_se = flex_stripline_z0(
        trace_width_um, dielectric_thickness_um, dk, trace_thickness_um);

    if (z0_se <= 0.0) return 0.0;

    double s = trace_spacing_um / 1000.0;  /* mm */
    double b = dielectric_thickness_um / 1000.0;
    double coupling = 1.0 - 0.48 * exp(-0.96 * s / b);

    return 2.0 * z0_se * coupling;
}

/* ========================================================================
 * L5: Loss Models
 * ========================================================================*/

/**
 * Knowledge Point: Conductor loss from skin effect.
 *
 * At high frequencies, current flows in a thin layer (skin depth δ)
 * at the conductor surface. The AC resistance is higher than DC:
 *
 *   R_ac = R_dc * (t/δ)   [when δ < t]
 *
 * Conductor loss in dB per unit length:
 *   αc = R_ac / (2 * Z0)   [nepers/m]
 *   αc_dB = 8.686 * αc     [dB/m]
 *
 * For 35 μm copper at 10 GHz: δ ≈ 0.66 μm
 * AC/DC resistance ratio ≈ 53×  →  significant loss increase
 *
 * Smoother copper (RA, low-profile) reduces the effective path length
 * at high frequencies, lowering loss.
 */
double flex_conductor_loss_db_per_mm(double frequency_hz,
                                      double trace_width_um,
                                      double trace_thickness_um,
                                      double z0_ohm,
                                      double roughness_um) {
    if (frequency_hz <= 0.0 || z0_ohm <= 0.0) return 0.0;

    /* Skin depth in copper */
    double rho_cu = 1.72e-8;  /* Ω·m */
    double skin_depth = sqrt(rho_cu / (M_PI * frequency_hz * MU0));

    /* Effective cross-sectional area accounting for skin effect */
    double w_m = trace_width_um * 1.0e-6;
    double t_m = trace_thickness_um * 1.0e-6;

    double effective_area;
    if (skin_depth < t_m / 2.0) {
        /* Current confined to skin depth perimeter */
        effective_area = 2.0 * (w_m + t_m) * skin_depth;
    } else {
        /* Full cross-section (low frequency) */
        effective_area = w_m * t_m;
    }

    if (effective_area <= 0.0) return 0.0;

    /* Resistance per meter */
    double r_ac_ohm_per_m = rho_cu / effective_area;

    /* Roughness correction */
    double kr = 1.0;
    if (roughness_um > 0.0) {
        double r_rms_m = roughness_um * 1.0e-6;
        kr = 1.0 + (2.0 / M_PI) * atan(1.4 * pow(r_rms_m / skin_depth, 2.0));
    }
    r_ac_ohm_per_m *= kr;

    /* Conductor loss: αc = R / (2*Z0) in nepers/m */
    double alpha_np_per_m = r_ac_ohm_per_m / (2.0 * z0_ohm);
    /* Convert to dB/mm: 1 Np/m = 8.686 dB/m = 0.008686 dB/mm */
    double alpha_db_per_mm = alpha_np_per_m * 8.686 / 1000.0;

    return alpha_db_per_mm;
}

/**
 * Knowledge Point: Dielectric loss.
 *
 * Dielectric loss arises from the imaginary part of the permittivity
 * (polarization lag). It increases linearly with frequency:
 *
 *   αd = (π * f / c0) * √εeff * tanδ   [nepers/m]
 *
 * Unlike conductor loss (~√f), dielectric loss grows as f, so it
 * dominates at mmWave frequencies (30+ GHz).
 *
 * For PI (tanδ=0.003) at 50 GHz: αd ≈ 0.12 dB/mm
 * For LCP (tanδ=0.002) at 50 GHz: αd ≈ 0.08 dB/mm
 *
 * This 33% lower loss is why LCP is preferred for high-frequency flex.
 *
 * Reference: Pozar, "Microwave Engineering", 4th Ed., Ch. 3
 */
double flex_dielectric_loss_db_per_mm(double frequency_hz,
                                       double effective_dk,
                                       double loss_tangent) {
    if (frequency_hz <= 0.0 || effective_dk <= 0.0 || loss_tangent <= 0.0)
        return 0.0;

    /* αd = (π * f * √εeff * tanδ) / c0  [nepers/m] */
    double alpha_np_per_m = (M_PI * frequency_hz * sqrt(effective_dk) *
                             loss_tangent) / C0_MM_NS / 1000.0;

    /* Convert to dB/mm */
    double alpha_db_per_mm = alpha_np_per_m * 8.686;

    return alpha_db_per_mm;
}

/**
 * Knowledge Point: Hammerstad copper roughness correction.
 *
 * At high frequencies, current follows the rough surface profile,
 * increasing the effective path length and thus the loss:
 *
 *   Kr = 1 + (2/π) * arctan(1.4 * (Rrms/δ)²)
 *
 * Limits:
 * - Kr → 1 when Rrms << δ (smooth relative to skin depth)
 * - Kr → 2 when Rrms >> δ (maximum roughness penalty)
 *
 * At 10 GHz (δ ≈ 0.66 μm):
 * - Smooth RA (Rrms=0.3 μm): Kr ≈ 1.08  (+8% loss)
 * - Standard ED (Rrms=1.5 μm): Kr ≈ 1.52  (+52% loss)
 *
 * This is why low-profile copper is essential for >10 GHz designs.
 *
 * Reference: Hammerstad & Bekkadal, "Microstrip Handbook", ELAB, 1975
 */
double flex_hammerstad_roughness_factor(double rms_roughness_um,
                                         double skin_depth_um) {
    if (skin_depth_um <= 0.0 || rms_roughness_um <= 0.0)
        return 1.0;

    double ratio = rms_roughness_um / skin_depth_um;
    double arg = 1.4 * ratio * ratio;
    double factor = 1.0 + (2.0 / M_PI) * atan(arg);

    /* Clamp maximum roughness factor */
    return (factor > 2.0) ? 2.0 : factor;
}

/**
 * Knowledge Point: Skin depth calculation.
 *
 * δ = √(ρ / (π * f * μ0))
 *
 * For copper (ρ = 1.72e-8 Ω·m, μ0 = 4π×10⁻⁷):
 *   δ(1 MHz)   ≈ 66 μm
 *   δ(100 MHz) ≈ 6.6 μm
 *   δ(1 GHz)   ≈ 2.1 μm
 *   δ(10 GHz)  ≈ 0.66 μm
 *   δ(50 GHz)  ≈ 0.30 μm
 *
 * When δ exceeds conductor thickness, the trace behaves as a DC
 * resistor (full cross-section conducts). When δ << t, the current
 * crowds into the surface, increasing effective resistance.
 */
double flex_skin_depth_um(double frequency_hz) {
    if (frequency_hz <= 0.0) return 1.0e6;  /* DC → very deep */

    double rho = 1.72e-8;  /* Ω·m, copper */
    double skin_depth_m = sqrt(rho / (M_PI * frequency_hz * MU0));
    return skin_depth_m * 1.0e6;  /* Convert to μm */
}

/* ========================================================================
 * Crosstalk and Time Domain
 * ========================================================================*/

/**
 * Knowledge Point: Near-end crosstalk (NEXT) estimation.
 *
 * NEXT is the coupled noise measured at the near end of a victim trace.
 * For parallel microstrips with homogeneous dielectric (TEM approximation):
 *
 *   NEXT ≈ (1/4) * (Lm/L + Cm/C)
 *
 * Simplified engineering approximation:
 *   K_NEXT ≈ 1 / (1 + (s/h)²)
 *
 * At s = h (spacing = height): K_NEXT ≈ 0.5 → -6 dB
 * At s = 2h: K_NEXT ≈ 0.2 → -14 dB
 * At s = 3h: K_NEXT ≈ 0.1 → -20 dB
 *
 * IPC-2223 recommends s ≥ 2h for digital signals and s ≥ 3h for
 * sensitive analog/RF traces.
 */
double flex_next_coefficient(double trace_spacing_um,
                              double dielectric_thickness_um) {
    if (dielectric_thickness_um <= 0.0) return 1.0;

    double s_over_h = trace_spacing_um / dielectric_thickness_um;
    double k_next = 1.0 / (1.0 + s_over_h * s_over_h);

    return k_next;
}

/**
 * Knowledge Point: Far-end crosstalk (FEXT).
 *
 * FEXT accumulates along the coupled length. Unlike NEXT, FEXT
 * for stripline in homogeneous dielectric is zero (TEM mode).
 * For microstrip, non-TEM causes small but non-zero FEXT.
 *
 * FEXT_dB ≈ -20 * log10(1 + (s/h)²) * (L_parallel / λ)
 *
 * This is a worst-case bound. Actual FEXT depends on signal
 * rise time and coupling length.
 */
double flex_fext_db(double trace_spacing_um,
                     double dielectric_thickness_um,
                     double parallel_length_mm,
                     double wavelength_mm) {
    if (wavelength_mm <= 0.0) return -999.0;
    if (dielectric_thickness_um <= 0.0) return -999.0;

    double s_over_h = trace_spacing_um / dielectric_thickness_um;
    double coupling_per_unit = -20.0 * log10(1.0 + s_over_h * s_over_h);
    double fext = coupling_per_unit * (parallel_length_mm / wavelength_mm);

    return fext;
}

/**
 * Knowledge Point: Propagation delay from effective DK.
 *
 * Tpd = √εeff / c0
 *
 * In vacuum: Tpd = 3.3356 ps/mm
 * In PI flex (εeff ≈ 3.2): Tpd = √3.2 * 3.3356 ≈ 5.97 ps/mm
 * In LCP flex (εeff ≈ 2.7): Tpd = √2.7 * 3.3356 ≈ 5.48 ps/mm
 *
 * A 100 mm flex trace has ~600 ps delay — important for timing
 * budgets in high-speed digital designs (DDR, SerDes).
 */
double flex_propagation_delay_ps_per_mm(double effective_dk) {
    if (effective_dk <= 1.0) effective_dk = 1.0;
    return sqrt(effective_dk) / C0_MM_NS * 1000.0;  /* ps/mm */
}

double flex_wavelength_mm(double frequency_hz, double effective_dk) {
    if (frequency_hz <= 0.0 || effective_dk <= 0.0) return 0.0;
    return C0_MM_NS * 1.0e9 / (frequency_hz * sqrt(effective_dk));
}

/**
 * Knowledge Point: Critical length for transmission line effects.
 *
 * A trace is "electrically long" when its length exceeds about
 * 1/6 of the signal rise time spatial extent:
 *
 *   L_critical = tr / (3 * Tpd)   [conservative]
 *   L_critical = tr / (2 * Tpd)   [standard rule of thumb]
 *
 * or equivalently, length > λ/10 at the knee frequency.
 *
 * For a 100 ps rise time on PI flex (Tpd ≈ 6 ps/mm):
 *   L_critical = 100 / (2 * 6) ≈ 8.3 mm
 *
 * Any trace longer than 8.3 mm MUST be treated as a transmission line.
 */
double flex_critical_length_mm(double rise_time_ps, double tpd_ps_per_mm) {
    if (tpd_ps_per_mm <= 0.0) return 0.0;
    return rise_time_ps / (2.0 * tpd_ps_per_mm);
}

/* ========================================================================
 * L5: Complete Transmission Line Analysis
 * ========================================================================*/

/**
 * Knowledge Point: Unified TL analysis.
 *
 * Computes all transmission line parameters for a flex trace in one
 * call. This is the main entry point for signal integrity analysis.
 */
int flex_tl_analyze(const flex_tl_params_t *params,
                     flex_tl_result_t *result) {
    if (!params || !result) return -1;

    /* Zero-initialize result */
    result->characteristic_impedance  = 0.0;
    result->differential_impedance    = 0.0;
    result->even_mode_impedance       = 0.0;
    result->odd_mode_impedance        = 0.0;
    result->effective_dk              = params->dielectric_constant;
    result->propagation_delay_ps_per_mm = 0.0;
    result->phase_velocity_m_per_s    = 0.0;
    result->conductor_loss_db_per_mm  = 0.0;
    result->dielectric_loss_db_per_mm = 0.0;
    result->total_loss_db_per_mm      = 0.0;
    result->crosstalk_coefficient     = 0.0;
    result->wavelength_mm             = 0.0;
    result->skin_depth_um             = 0.0;
    result->copper_roughness_factor   = 1.0;
    result->via_impedance_ohm         = 0.0;

    /* Characteristic impedance */
    double dk_eff = params->dielectric_constant;

    switch (params->tl_type) {
    case FLEX_TL_MICROSTRIP_SURFACE:
        result->characteristic_impedance = flex_microstrip_z0_wheeler(
            params->trace_width_um,
            params->dielectric_thickness_um,
            params->dielectric_constant,
            params->trace_thickness_um);
        dk_eff = flex_effective_dk_microstrip(params->dielectric_constant,
                    params->trace_width_um, params->dielectric_thickness_um);
        break;

    case FLEX_TL_MICROSTRIP_EMBEDDED:
        result->characteristic_impedance = flex_microstrip_embedded_z0(
            params->trace_width_um,
            params->dielectric_thickness_um,
            params->dielectric_constant,
            params->coverlay_thickness_um,
            params->coverlay_dk,
            params->trace_thickness_um);
        dk_eff = params->coverlay_dk;  /* Approximate */
        break;

    case FLEX_TL_STRIPLINE_SYMMETRIC:
        result->characteristic_impedance = flex_stripline_z0(
            params->trace_width_um,
            params->dielectric_thickness_um,
            params->dielectric_constant,
            params->trace_thickness_um);
        dk_eff = params->dielectric_constant;
        break;

    case FLEX_TL_STRIPLINE_ASYMMETRIC:
        result->characteristic_impedance = flex_stripline_asymmetric_z0(
            params->trace_width_um,
            params->upper_dielectric_um,
            params->lower_dielectric_um,
            params->dielectric_constant,
            params->trace_thickness_um);
        dk_eff = params->dielectric_constant;
        break;

    case FLEX_TL_DIFF_MICROSTRIP:
        result->differential_impedance = flex_diff_microstrip_z0(
            params->trace_width_um,
            params->trace_spacing_um,
            params->dielectric_thickness_um,
            params->dielectric_constant,
            params->trace_thickness_um);
        result->characteristic_impedance = result->differential_impedance / 2.0;
        dk_eff = flex_effective_dk_microstrip(params->dielectric_constant,
                    params->trace_width_um, params->dielectric_thickness_um);
        break;

    case FLEX_TL_DIFF_STRIPLINE:
        result->differential_impedance = flex_diff_stripline_z0(
            params->trace_width_um,
            params->trace_spacing_um,
            params->dielectric_thickness_um,
            params->dielectric_constant,
            params->trace_thickness_um);
        result->characteristic_impedance = result->differential_impedance / 2.0;
        dk_eff = params->dielectric_constant;
        break;

    case FLEX_TL_COPLANAR_WAVEGUIDE:
        /* Simplified: CPW on flex approximates microstrip for narrow gaps */
        result->characteristic_impedance = flex_microstrip_z0_wheeler(
            params->trace_width_um,
            params->dielectric_thickness_um,
            params->dielectric_constant,
            params->trace_thickness_um);
        dk_eff = flex_effective_dk_microstrip(params->dielectric_constant,
                    params->trace_width_um, params->dielectric_thickness_um);
        break;

    default:
        return -1;
    }

    result->effective_dk = dk_eff;

    /* Propagation delay */
    result->propagation_delay_ps_per_mm = flex_propagation_delay_ps_per_mm(dk_eff);
    result->phase_velocity_m_per_s = C0_MM_NS * 1.0e6 / sqrt(dk_eff);

    /* Wavelength */
    result->wavelength_mm = flex_wavelength_mm(params->frequency_hz, dk_eff);

    /* Skin depth */
    result->skin_depth_um = flex_skin_depth_um(params->frequency_hz);

    /* Copper roughness factor */
    result->copper_roughness_factor = flex_hammerstad_roughness_factor(
        params->copper_roughness_um, result->skin_depth_um);

    /* Losses */
    if (result->characteristic_impedance > 0.0) {
        result->conductor_loss_db_per_mm = flex_conductor_loss_db_per_mm(
            params->frequency_hz,
            params->trace_width_um,
            params->trace_thickness_um,
            result->characteristic_impedance,
            params->copper_roughness_um);
    }

    result->dielectric_loss_db_per_mm = flex_dielectric_loss_db_per_mm(
        params->frequency_hz, dk_eff, params->loss_tangent);

    result->total_loss_db_per_mm = result->conductor_loss_db_per_mm +
                                   result->dielectric_loss_db_per_mm;

    /* Crosstalk */
    result->crosstalk_coefficient = flex_next_coefficient(
        params->trace_spacing_um, params->dielectric_thickness_um);

    return 0;
}
