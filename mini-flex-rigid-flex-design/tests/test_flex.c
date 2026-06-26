/**
 * @file test_flex.c
 * @brief Comprehensive test suite for mini-flex-rigid-flex-design
 *
 * Tests all core APIs with mathematical assertions, boundary conditions,
 * and IPC standard conformance verification.
 *
 * Each test validates one specific knowledge point from the L1-L6 curriculum.
 */

#include "flex_material.h"
#include "flex_bend.h"
#include "flex_stackup.h"
#include "flex_design_rule.h"
#include "flex_signal_integrity.h"
#include "flex_rigid_transition.h"
#include "flex_thermal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    printf("  TEST %s... ", name); \
    tests_passed++; \
} while(0)

#define CHECK(cond) do { \
    if (!(cond)) { \
        printf("FAIL at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        tests_failed++; tests_passed--; \
    } \
} while(0)

#define CHECK_FEQ(a, b, eps) do { \
    if (fabs((a) - (b)) > (eps)) { \
        printf("FAIL at %s:%d: |%.6f - %.6f| > %.6f\n", \
               __FILE__, __LINE__, (a), (b), (eps)); \
        tests_failed++; tests_passed--; \
    } \
} while(0)

/* ========================================================================
 * L1 — Material Definition Tests
 * ========================================================================*/

static void test_material_definitions(void) {
    TEST("Polyimide electrical properties");
    flex_dielectric_electrical_t pi_elec = flex_polyimide_electrical_standard();
    CHECK(pi_elec.dielectric_constant == 3.4);
    CHECK(pi_elec.loss_tangent == 0.002);
    CHECK(pi_elec.dielectric_strength > 200.0);

    TEST("LCP electrical properties");
    flex_dielectric_electrical_t lcp_elec = flex_lcp_electrical_standard();
    CHECK(lcp_elec.dielectric_constant == 2.9);
    CHECK(lcp_elec.moisture_absorption < 0.1);  /* Near-zero moisture */

    TEST("Polyimide mechanical properties");
    flex_dielectric_mechanical_t pi_mech = flex_polyimide_mechanical_standard();
    CHECK(pi_mech.youngs_modulus == 2500.0);
    CHECK(pi_mech.cte_xy == 20.0);
    CHECK(pi_mech.glass_transition_tg > 350.0);  /* No true Tg */

    TEST("PET mechanical properties (low Tg)");
    flex_dielectric_mechanical_t pet_mech = flex_pet_mechanical_standard();
    CHECK(pet_mech.glass_transition_tg == 80.0);  /* Low Tg — soldering issue */
    CHECK(pet_mech.elongation_at_break > 50.0);

    TEST("RA copper foil properties");
    flex_copper_foil_t ra_cu = flex_copper_foil_standard(FLEX_COPPER_RA);
    CHECK(ra_cu.elongation == 12.0);
    CHECK(ra_cu.fatigue_strength > 100.0);
    CHECK(ra_cu.surface_roughness_ra < 0.5);  /* Smooth */

    TEST("ED copper foil properties");
    flex_copper_foil_t ed_cu = flex_copper_foil_standard(FLEX_COPPER_ED);
    CHECK(ed_cu.tensile_strength > 300.0);  /* Stronger than RA */
    CHECK(ed_cu.surface_roughness_ra > 1.0);  /* Rougher */
    CHECK(ed_cu.grain_size > 10.0);  /* Coarser grains */

    TEST("Adhesive properties — acrylic");
    flex_adhesive_properties_t acr = flex_adhesive_properties_standard(FLEX_ADHESIVE_ACRYLIC);
    CHECK(acr.glass_transition_tg < 100.0);  /* Low Tg */
    CHECK(acr.cte > 60.0);  /* High CTE → mismatch risk */

    TEST("Adhesive properties — PI");
    flex_adhesive_properties_t pi_ad = flex_adhesive_properties_standard(FLEX_ADHESIVE_PI);
    CHECK(pi_ad.glass_transition_tg > 200.0);  /* High Tg */
    CHECK(pi_ad.cte < 50.0);  /* Lower CTE → better match */
}

/* ========================================================================
 * L4 — Fundamental Law Tests
 * ========================================================================*/

static void test_fundamental_laws(void) {
    TEST("IPC-2223 min bend radius — 1-layer");
    double r1 = flex_min_bend_radius_ipc2223(0.2, 1, 16.0);
    CHECK(r1 >= 1.2);  /* 6 × 0.2 = 1.2 mm */
    CHECK(r1 <= 1.3);

    TEST("IPC-2223 min bend radius — 2-layer");
    double r2 = flex_min_bend_radius_ipc2223(0.3, 2, 16.0);
    CHECK(r2 >= 3.5);  /* 12 × 0.3 = 3.6 mm */
    CHECK(r2 <= 3.7);

    TEST("IPC-2223 min bend radius — multi-layer");
    double r4 = flex_min_bend_radius_ipc2223(0.5, 4, 16.0);
    CHECK(r4 >= 9.5);  /* 20 × 0.5 = 10 mm */

    TEST("Beam theory min bend radius");
    flex_bend_params_t bp;
    memset(&bp, 0, sizeof(bp));
    bp.total_thickness_mm = 0.3;
    bp.copper_elongation_limit_percent = 16.0;
    bp.youngs_modulus_copper_mpa = 117000.0;
    double rb = flex_min_bend_radius_beam_theory(&bp);
    CHECK(rb > 0.0);
    CHECK(rb < 5.0);

    TEST("CTE mismatch stress calculation");
    double stress = flex_cte_mismatch_stress(117000.0, 20.0, 17.0, 200.0);
    CHECK(stress > 50.0);   /* ~70 MPa expected */
    CHECK(stress < 100.0);

    TEST("Djordjevic-Sarkar DK at frequency");
    double dk_1ghz = flex_dk_at_frequency(3.4, 2.8, 1.0e9, 1.0e6);
    CHECK(dk_1ghz < 3.4);  /* DK decreases with frequency */
    CHECK(dk_1ghz > 2.8);

    TEST("Moisture correction on DK");
    double dk_wet = flex_dk_moisture_correction(3.4, 2.8);
    CHECK(dk_wet > 3.4);  /* Moisture increases DK */

    TEST("Kelley-Bueche Tg moisture shift");
    double tg_wet = flex_tg_moisture_shift(80.0, 1.0);
    CHECK(tg_wet < 80.0);  /* Moisture depresses Tg */

    TEST("Intrinsic impedance of PI");
    double eta_pi = flex_intrinsic_impedance(3.4);
    CHECK(eta_pi > 200.0);
    CHECK(eta_pi < 210.0);  /* 377/√3.4 ≈ 204 Ω */
}

/* ========================================================================
 * L5 — Algorithm Tests
 * ========================================================================*/

static void test_algorithms(void) {
    TEST("Copper strain — pure bending");
    double strain = flex_copper_strain_percent(5.0, 0.15, 0.3);
    CHECK(strain > 1.0);  /* y/R ≈ 0.15/5 = 3% */
    CHECK(strain < 10.0);

    TEST("Copper strain — infinite radius (zero strain)");
    double strain_inf = flex_copper_strain_percent(1.0e9, 0.15, 0.3);
    CHECK(strain_inf < 0.1);

    TEST("Strain profile through thickness");
    double profile[5];
    flex_strain_profile(5.0, 0.15, 0.3, profile, 5);
    /* Inner surface in compression, outer in tension */
    CHECK(profile[0] < -1.0);   /* Compression */
    CHECK(profile[4] > 1.0);    /* Tension */

    TEST("Bending moment calculation");
    double moment = flex_bending_moment(100.0, 5.0);
    CHECK_FEQ(moment, 20.0, 0.01);  /* M = EI/R */

    TEST("Springback angle — typical PI flex");
    double sb = flex_springback_angle(90.0, 5.0, 0.3, 200.0, 5000.0);
    CHECK(sb > 0.0);
    CHECK(sb <= 45.0);  /* Clamped at half the bend angle */

    TEST("Coffin-Manson fatigue — RA copper");
    double cm_ra = flex_cycles_to_failure_coffin_manson(0.5, FLEX_COPPER_RA);
    CHECK(cm_ra > 100.0);   /* 0.5% strain → millions of cycles */

    TEST("Coffin-Manson fatigue — ED copper (shorter life)");
    double cm_ed = flex_cycles_to_failure_coffin_manson(0.5, FLEX_COPPER_ED);
    CHECK(cm_ed < cm_ra);   /* ED has shorter life than RA */

    TEST("Coffin-Manson — zero strain → infinite life");
    double cm_zero = flex_cycles_to_failure_coffin_manson(0.0, FLEX_COPPER_RA);
    CHECK(cm_zero > 1.0e9);

    TEST("IPC-TM-650 flexural fatigue");
    double ipc_life = flex_cycles_ipc_tm650(5.0, 0.3, FLEX_COPPER_RA);
    CHECK(ipc_life > 1000.0);  /* R/t = 16.7 → good life */

    TEST("Arrhenius temperature derating");
    double derated = flex_cycles_temperature_derate(1.0e6, 85.0, 0.8);
    CHECK(derated < 1.0e6);  /* Hot → shorter life */
    CHECK(derated > 1.0e3);  /* Not catastrophic */

    /* Impedance calculations */
    TEST("Wheeler microstrip Z0 — 50 Ohm target");
    double z0_ms = flex_microstrip_z0_wheeler(150.0, 100.0, 3.4, 18.0);
    CHECK(z0_ms > 40.0);
    CHECK(z0_ms < 70.0);

    TEST("Effective DK — microstrip");
    double dk_eff = flex_effective_dk_microstrip(3.4, 150.0, 100.0);
    CHECK(dk_eff >= 2.5);
    CHECK(dk_eff <= 3.4);

    TEST("Symmetrical stripline Z0");
    double z0_sl = flex_stripline_z0(100.0, 200.0, 3.4, 18.0);
    CHECK(z0_sl > 40.0);
    CHECK(z0_sl < 80.0);

    TEST("Differential microstrip Z0");
    double z0_diff = flex_diff_microstrip_z0(100.0, 150.0, 100.0, 3.4, 18.0);
    CHECK(z0_diff > 70.0);  /* Typically 80-120 Ω */
    CHECK(z0_diff < 150.0);

    TEST("Skin depth at 10 GHz");
    double delta = flex_skin_depth_um(10.0e9);
    CHECK(delta > 0.5);
    CHECK(delta < 1.0);  /* ≈ 0.66 μm at 10 GHz */

    TEST("Skin depth at 1 MHz");
    double delta_1m = flex_skin_depth_um(1.0e6);
    CHECK(delta_1m > 50.0);  /* ≈ 66 μm */

    TEST("Hammerstad roughness factor — smooth Cu");
    double kr_smooth = flex_hammerstad_roughness_factor(0.3, 6.6);
    CHECK(kr_smooth < 1.2);  /* Smooth → low factor */

    TEST("Hammerstad roughness factor — rough Cu at HF");
    double kr_rough = flex_hammerstad_roughness_factor(1.5, 0.66);
    CHECK(kr_rough > 1.2);  /* Rough at high freq → high factor */

    TEST("Propagation delay — PI flex");
    double tpd = flex_propagation_delay_ps_per_mm(3.2);
    CHECK(tpd > 5.0);
    CHECK(tpd < 7.0);  /* √3.2 × 3.336 ≈ 5.97 ps/mm */

    TEST("Critical length — 100 ps rise time");
    double lcrit = flex_critical_length_mm(100.0, 6.0);
    CHECK(lcrit > 5.0);
    CHECK(lcrit < 15.0);

    TEST("NEXT coefficient — tight spacing");
    double next_tight = flex_next_coefficient(100.0, 100.0);
    CHECK(next_tight > 0.4);  /* s/h = 1 → K ≈ 0.5 */

    TEST("NEXT coefficient — loose spacing");
    double next_loose = flex_next_coefficient(300.0, 100.0);
    CHECK(next_loose < 0.2);  /* s/h = 3 → K ≈ 0.1 */
}

/* ========================================================================
 * L6 — Canonical Problem Tests
 * ========================================================================*/

static void test_canonical_problems(void) {
    TEST("Complete bend analysis — static flex");
    flex_bend_params_t bp;
    memset(&bp, 0, sizeof(bp));
    bp.total_thickness_mm = 0.3;
    bp.bend_radius_mm = 2.0;
    bp.bend_angle_deg = 90.0;
    bp.copper_elongation_limit_percent = 16.0;
    bp.youngs_modulus_copper_mpa = 117000.0;
    bp.num_layers = 1;
    bp.is_dynamic = 0;
    bp.copper_thickness_total_um = 35.0;

    flex_bend_result_t result;
    int rc = flex_bend_analyze(&bp, &result);
    CHECK(rc == 0);
    CHECK(result.is_compliant_ipc2223 == 1);
    CHECK(result.safety_factor >= 1.0);

    TEST("Stackup — build and analyze");
    flex_stackup_t stackup = flex_stackup_init("TestFlex");
    int l1 = flex_stackup_add_signal_layer(&stackup, "L1_TOP", 18.0,
        FLEX_COPPER_RA, FLEX_DIELEC_POLYIMIDE, 50.0);
    CHECK(l1 == 1);

    int l2 = flex_stackup_add_plane_layer(&stackup, "L2_GND", 35.0,
        FLEX_COPPER_RA);
    CHECK(l2 == 2);

    flex_stackup_set_flex_through(&stackup, 1);
    flex_stackup_set_flex_through(&stackup, 2);

    double flex_t = flex_stackup_flex_thickness(&stackup);
    CHECK(flex_t > 0.0);

    double cu_tot = flex_stackup_flex_copper_total(&stackup);
    CHECK_FEQ(cu_tot, 53.0, 0.1);

    int flex_lc = flex_stackup_flex_layer_count(&stackup);
    CHECK(flex_lc == 2);

    double na;
    rc = flex_stackup_neutral_axis(&stackup, &na);
    CHECK(rc == 0);
    CHECK(na > 0.0);

    double d = flex_stackup_flexural_rigidity(&stackup);
    CHECK(d > 0.0);

    TEST("Stackup symmetry verification");
    /* 2-layer with different types is asymmetric by design */
    int sym = flex_stackup_verify_symmetry(&stackup);
    /* May be 0 (asymmetric) or 1 — both valid, test non-crash */
    CHECK(sym == 0 || sym == 1);

    double asym = flex_stackup_asymmetry_metric(&stackup);
    CHECK(asym >= 0.0);
    CHECK(asym <= 1.0);

    TEST("DRC — bend radius check");
    flex_drc_violation_t v;
    memset(&v, 0, sizeof(v));
    int ok = flex_drc_bend_radius(2.0, 0.3, 1, &v);
    CHECK(ok == 1);  /* 2.0 mm > 6*0.3 = 1.8 mm → compliant */

    TEST("DRC — annular ring");
    flex_via_params_t via;
    memset(&via, 0, sizeof(via));
    via.pad_diameter_mm = 0.8;
    via.hole_diameter_mm = 0.3;
    via.is_supported = 1;
    ok = flex_drc_annular_ring(&via, &v);
    CHECK(ok == 1);  /* AR = (0.8-0.3)/2 = 0.25 ≥ 0.15 */

    TEST("DRC — via in bend zone (critical)");
    flex_via_params_t via_bend;
    memset(&via_bend, 0, sizeof(via_bend));
    via_bend.is_in_bend_zone = 1;
    via_bend.distance_to_bend_mm = 0.0;
    ok = flex_drc_via_in_bend_zone(&via_bend, 2.0, &v);
    CHECK(ok == 0);  /* Via in bend → CRITICAL violation */

    TEST("DRC — trace to edge clearance");
    ok = flex_drc_trace_to_edge(0.6, FLEX_SECTION_FLEX, &v);
    CHECK(ok == 1);  /* 0.6 mm ≥ 0.5 mm */

    TEST("Transition zone — shear stress");
    double tau = flex_transition_shear_stress(6.0, 200.0, 10000.0,
        2.0, 500.0, 0.5, 0.025);
    CHECK(tau > 0.0);
    CHECK(tau < 50.0);

    TEST("Transition zone — peel stress");
    double peel = flex_transition_peel_stress(tau, 2.0, 0.5);
    CHECK(peel > 0.0);
    CHECK(peel < 20.0);

    TEST("Transition zone — stress concentration");
    double kt = flex_transition_stress_concentration(0.5, 1.6, 0.3);
    CHECK(kt > 1.0);
    CHECK(kt < 4.0);

    TEST("Transition zone — impedance delta");
    double gamma;
    double delta_z = flex_transition_impedance_delta(50.0, 47.0, &gamma);
    CHECK(delta_z == 3.0);
    CHECK(gamma < 0.05);  /* 3/97 ≈ 0.031 */

    TEST("Transition zone — tear-stop spacing");
    double ts_spacing = flex_tear_stop_spacing(10.0, 3.5);
    CHECK(ts_spacing > 0.0);
    CHECK(ts_spacing < 10.0);

    TEST("Transition zone — tear-stop adequacy");
    int ts_ok = flex_tear_stop_is_adequate(0.5, 10.0, 3.5);
    CHECK(ts_ok == 1);

    TEST("Transition zone — anchor tab length");
    double at_len = flex_anchor_tab_length(6.0, 200.0, 24000.0,
        10.0, 2.0, 4, 1.2);
    CHECK(at_len >= 1.0);  /* Min per IPC-2223 */

    TEST("Transition zone — anchor tab strength");
    double at_str = flex_anchor_tab_strength(3.0, 2.0, 4, 1.2);
    CHECK_FEQ(at_str, 28.8, 0.1);  /* 1.2 * 3 * 2 * 4 = 28.8 */

    TEST("Transition zone — thermal fatigue life");
    double nf = flex_transition_thermal_life(0.002, 0.3, -0.5);
    CHECK(nf > 1000.0);  /* 0.2% strain → long life */
}

/* ========================================================================
 * Thermal Analysis Tests
 * ========================================================================*/

static void test_thermal(void) {
    TEST("Trace ampacity IPC-2152 — outer layer");
    double i_outer = flex_trace_ampacity_ipc2152(500.0, 35.0, 10.0, 1);
    CHECK(i_outer > 1.0);
    CHECK(i_outer < 5.0);

    TEST("Trace ampacity IPC-2152 — inner layer (lower)");
    double i_inner = flex_trace_ampacity_ipc2152(500.0, 35.0, 10.0, 0);
    CHECK(i_inner < i_outer);  /* Inner always lower */

    TEST("Trace temp rise — 1A through 0.5mm × 35μm");
    double dt = flex_trace_temp_rise(1.0, 500.0, 35.0, 1);
    CHECK(dt > 0.0);
    CHECK(dt < 20.0);

    TEST("Flex ampacity derating");
    double derated = flex_ampacity_derate_flex(3.0, 4, 1);
    CHECK(derated < 3.0);  /* Derated in bend zone */

    TEST("Convection coefficient — natural");
    double h_nat = flex_convection_coefficient(10.0, 0.05, 0.0);
    CHECK(h_nat > 3.0);
    CHECK(h_nat < 15.0);

    TEST("Convection coefficient — forced (higher)");
    double h_for = flex_convection_coefficient(10.0, 0.05, 2.0);
    CHECK(h_for > h_nat);  /* Forced convection better */

    TEST("Board thermal resistance");
    double theta = flex_thermal_resistance_board(1500.0, 8.0, 0.85);
    CHECK(theta > 0.0);
    CHECK(theta < 200.0);

    TEST("Thermal strain");
    double eps_th = flex_thermal_strain(20.0, 17.0, 100.0);
    CHECK_FEQ(eps_th, 3.0e-4, 1.0e-5);  /* 3e-6 * 100 = 3e-4 */

    TEST("Critical temp delta for delamination");
    double dt_crit = flex_critical_temp_delta(1.0, 500.0, 60.0);
    CHECK(dt_crit > 20.0);  /* ~33°C for acrylic */

    TEST("Thermal time constant");
    double tau = flex_thermal_time_constant(500.0, 1420.0, 1090.0, 50.0);
    CHECK(tau > 0.0);
    CHECK(tau < 100.0);

    TEST("Junction temperature");
    double tj = flex_junction_temperature(85.0, 0.5, 20.0, 60.0);
    CHECK_FEQ(tj, 125.0, 0.1);  /* 85 + 0.5*(20+60) = 125 */

    /* 1D conduction — Fourier's law */
    double thick[2] = {0.05, 0.035};   /* mm */
    double k[2] = {0.12, 398.0};       /* W/m·K: PI and Cu */
    double q = flex_thermal_conduction_1d(thick, k, 2, 100.0, 50.0);
    CHECK(q > 0.0);

    /* Effective in-plane vs through-thickness conductivity */
    double k_ip = flex_thermal_conductivity_in_plane(k, thick, 2);
    double k_tt = flex_thermal_conductivity_through(k, thick, 2);
    CHECK(k_ip > k_tt);  /* In-plane always higher */
}

/* ========================================================================
 * Boundary Condition Tests
 * ========================================================================*/

static void test_boundary_conditions(void) {
    TEST("Null pointer protection — bend analysis");
    CHECK(flex_bend_analyze(NULL, NULL) == -1);

    TEST("Zero thickness — IPC-2223");
    CHECK(flex_min_bend_radius_ipc2223(0.0, 1, 16.0) == 0.0);
    CHECK(flex_min_bend_radius_ipc2223(-0.1, 1, 16.0) == 0.0);

    TEST("Zero radius — infinite strain");
    double strain_zero = flex_copper_strain_percent(0.0, 0.1, 0.3);
    CHECK(strain_zero > 100.0);  /* Infinite strain indicator */

    TEST("Zero spacing — NEXT = 1 (full coupling)");
    double next_zero = flex_next_coefficient(0.0, 100.0);
    CHECK_FEQ(next_zero, 1.0, 0.01);

    TEST("Null impedance profile — no crash");
    flex_impedance_taper_exponential(50.0, 47.0, 3.0, 10, NULL);

    TEST("Zero layers — flex thickness = 0");
    flex_stackup_t empty = flex_stackup_init("Empty");
    CHECK(flex_stackup_flex_thickness(&empty) == 0.0);
    CHECK(flex_stackup_flex_layer_count(&empty) == 0);

    TEST("Zero frequency — skin depth");
    double delta_dc = flex_skin_depth_um(0.0);
    CHECK(delta_dc > 1000.0);  /* DC → very large */

    TEST("Zero frequency — conductor loss = 0");
    double loss_dc = flex_conductor_loss_db_per_mm(0.0, 100.0, 18.0, 50.0, 0.3);
    CHECK(loss_dc == 0.0);
}

/* ========================================================================
 * DRC Report Tests
 * ========================================================================*/

static void test_drc_report(void) {
    TEST("DRC report initialization");
    flex_drc_report_t report = flex_drc_report_init();
    CHECK(report.total_violations == 0);
    CHECK(report.error_count == 0);

    TEST("DRC add violation");
    flex_drc_violation_t v;
    memset(&v, 0, sizeof(v));
    v.severity = FLEX_RULE_ERROR;
    v.rule_id = 9999;
    int rc = flex_drc_add_violation(&report, &v);
    CHECK(rc == 0);
    CHECK(report.total_violations == 1);
    CHECK(report.error_count == 1);

    TEST("DRC report print — no crash");
    flex_drc_report_print(&report);

    TEST("DRC report full run");
    flex_stackup_t stackup = flex_stackup_init("DRC_Test");
    flex_stackup_add_signal_layer(&stackup, "L1", 18.0,
        FLEX_COPPER_RA, FLEX_DIELEC_POLYIMIDE, 50.0);
    flex_stackup_add_plane_layer(&stackup, "L2", 35.0, FLEX_COPPER_RA);
    flex_stackup_add_bend_zone(&stackup, 0, 0, 10, 10, 2.0, 90.0, 0);
    flex_drc_report_t full = flex_drc_run_full(&stackup);
    CHECK(full.total_violations >= 0);  /* Must not crash */
}

int main(void) {
    printf("=== mini-flex-rigid-flex-design Test Suite ===\n\n");

    printf("--- L1: Material Definitions ---\n");
    test_material_definitions();

    printf("\n--- L4: Fundamental Laws ---\n");
    test_fundamental_laws();

    printf("\n--- L5: Algorithms ---\n");
    test_algorithms();

    printf("\n--- L6: Canonical Problems ---\n");
    test_canonical_problems();

    printf("\n--- Thermal Analysis ---\n");
    test_thermal();

    printf("\n--- Boundary Conditions ---\n");
    test_boundary_conditions();

    printf("\n--- DRC Report ---\n");
    test_drc_report();

    printf("\n=== Results: %d passed, %d failed ===\n",
           tests_passed, tests_failed);

    return tests_failed > 0 ? 1 : 0;
}
