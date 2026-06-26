/**
 * test_thermal.c - Comprehensive tests for PCB Thermal Management Library
 *
 * Tests cover: L1 definitions, L2 core concepts, L3 math structures,
 * L4 fundamental laws, L5 algorithms, L6 canonical problems.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>
#include <string.h>
#include "pcb_thermal_defs.h"
#include "pcb_thermal_analysis.h"
#include "pcb_thermal_via.h"
#include "pcb_thermal_material.h"
#include "pcb_thermal_design.h"
#include "pcb_thermal_simulation.h"

#define TOL 1e-6
#define NEAR(a,b,tol) (fabs((a)-(b)) < (tol))

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { printf("  TEST %s... ", #name); } while(0)
#define PASS() do { tests_passed++; printf("PASSED\n"); } while(0)
#define FAIL(msg) do { printf("FAILED: %s\n", msg); } while(0)
#define CHECK(cond, msg) do { tests_run++; if (cond) PASS(); else FAIL(msg); } while(0)

/* ================================================================
 * L1: Definitions - Data structure validation
 * ================================================================ */

void test_l1_inline_functions(void) {
    TEST(L1_inline_functions);
    thermal_point_t pt = thermal_point_init();
    int ok1 = (pt.x_mm == 0.0 && pt.y_mm == 0.0 && pt.z_mm == 0.0);

    ambient_conditions_t amb = ambient_default();
    int ok2 = NEAR(amb.ambient_temp_c, 25.0, TOL);

    thermal_resistance_t rt = {.r_jc=5.0, .r_cs=1.0, .r_sa=20.0, .r_jb=15.0, .r_ja=26.0};
    int ok3 = thermal_resistance_is_valid(&rt);
    int ok4 = !thermal_resistance_is_valid(NULL);

    heat_source_t src = {.power_w=1.0, .max_tj=150.0, .width_mm=5.0, .length_mm=5.0};
    int ok5 = heat_source_is_valid(&src);

    double hf = heat_flux_wm2(1.0, 100.0);
    int ok6 = NEAR(hf, 10000.0, 1.0);

    CHECK(ok1&&ok2&&ok3&&ok4&&ok5&&ok6, "L1 inline functions");
}

/* ================================================================
 * L2: Core Concepts - Steady-State Thermal Resistance
 * ================================================================ */

void test_l2_conduction(void) {
    TEST(L2_conduction);
    double r_cond = thermal_resistance_conduction(1.0, 385.0, 100.0);
    CHECK(NEAR(r_cond, 0.02597, 0.001), "conduction resistance");
}

void test_l2_convection(void) {
    TEST(L2_convection);
    double r_conv = thermal_resistance_convection(10.0, 2500.0);
    CHECK(NEAR(r_conv, 40.0, 0.01), "convection resistance");
}

void test_l2_series_parallel(void) {
    TEST(L2_series_parallel);
    double rs = thermal_resistance_series(10.0, 20.0);
    double rp = thermal_resistance_parallel(10.0, 20.0);
    double rn = thermal_resistance_n_parallel(100.0, 5);
    CHECK(NEAR(rs, 30.0, TOL) && NEAR(rp, 6.6667, 0.01) && NEAR(rn, 20.0, TOL),
          "series-parallel resistance");
}

void test_l2_spreading(void) {
    TEST(L2_spreading);
    double r_spread = thermal_resistance_spreading(385.0, 0.035, 25.0, 2500.0);
    CHECK(r_spread > 20.0 && r_spread < 35.0, "spreading resistance plausible");
}

void test_l2_junction_temperature(void) {
    TEST(L2_junction_temperature);
    heat_source_t src = {.power_w=1.0, .r_jc=5.0, .max_tj=150.0, .width_mm=5.0, .length_mm=5.0};
    tim_properties_t tim = {.r_cs=1.0};
    double tj, tc, tb;
    int ret = calculate_junction_temperature(&src, &tim, 20.0, 15.0, 10.0, 25.0, &tj, &tc, &tb);
    CHECK(ret==THERMAL_OK && NEAR(tj, 37.745, 0.1), "junction temperature calculation");
}

void test_l2_radiation(void) {
    TEST(L2_radiation);
    double r_rad = thermal_resistance_radiation(0.9, 80.0, 25.0, 2500.0);
    CHECK(r_rad > 50.0 && r_rad < 80.0, "radiation resistance plausible");
}

/* ================================================================
 * L3: Transient and Dimensionless Numbers
 * ================================================================ */

void test_l3_lumped_capacitance(void) {
    TEST(L3_lumped_capacitance);
    double t1 = transient_lumped_capacitance(100.0, 25.0, 10.0, 385.0, 10.0, 100.0, 3850.0);
    CHECK(NEAR(t1, 52.6, 0.2), "lumped capacitance at t=tau");
    double t0 = transient_lumped_capacitance(100.0, 25.0, 10.0, 385.0, 10.0, 100.0, 0.0);
    CHECK(NEAR(t0, 100.0, TOL), "lumped capacitance at t=0");
}

void test_l3_foster_model(void) {
    TEST(L3_foster_model);
    double r[] = {10.0};
    double c[] = {100.0};
    double tj = transient_foster_model(25.0, 1.0, r, c, 1, 1000.0);
    CHECK(NEAR(tj, 31.321, 0.01), "Foster single-stage");
    double tj_ss = transient_foster_model(25.0, 1.0, r, c, 1, 1.0e9);
    CHECK(NEAR(tj_ss, 35.0, 0.01), "Foster steady-state");
}

void test_l3_biot_number(void) {
    TEST(L3_biot_number);
    double bi = biot_number(10.0, 100.0, 100.0, 385.0);
    CHECK(NEAR(bi, 2.597e-5, 1e-6), "Biot number");
}

void test_l3_fourier_number(void) {
    TEST(L3_fourier_number);
    double fo = fourier_number(385.0, 8960.0, 385.0, 1.0, 1.0);
    CHECK(NEAR(fo, 111.7, 1.0), "Fourier number");
}

void test_l3_spreading_length(void) {
    TEST(L3_spreading_length);
    double lc = characteristic_spreading_length(10.0, 1.6, 10.0);
    CHECK(NEAR(lc, 40.0, 1.0), "spreading length");
}

void test_l3_thermal_impedance(void) {
    TEST(L3_thermal_impedance);
    double r[] = {5.0, 15.0};
    double c[] = {10.0, 100.0};
    double zth = transient_thermal_impedance(r, c, 2, 1000.0);
    CHECK(NEAR(zth, 12.3, 0.1), "thermal impedance");
}

/* ================================================================
 * L4: Network Solver & Mutual Heating
 * ================================================================ */

void test_l4_network_solver(void) {
    TEST(L4_network_solver);
    int n = 2;
    double **r_mat = (double **)malloc(n * sizeof(double *));
    for (int i = 0; i < n; i++) r_mat[i] = (double *)calloc(n, sizeof(double));
    r_mat[0][1] = r_mat[1][0] = 5.0;
    double r_amb[] = {10.0, 20.0};
    double power[] = {2.0, 0.0};
    double t_out[2];
    int ret = thermal_network_solve(n, (const double **)r_mat, r_amb, power, 25.0, t_out, 1000, 1e-8);
    CHECK(ret==THERMAL_OK && t_out[0] > 30.0 && t_out[0] < 60.0, "network solver");
    for (int i = 0; i < n; i++) free(r_mat[i]);
    free(r_mat);
}

void test_l4_mutual_heating(void) {
    TEST(L4_mutual_heating);
    heat_source_t src1 = {.power_w=1.0, .width_mm=5.0, .length_mm=5.0, .max_tj=150.0};
    heat_source_t src2 = {.power_w=0.5, .width_mm=5.0, .length_mm=5.0, .max_tj=150.0};
    double tj1, tj2;
    int ret = calculate_mutual_heating(25.0, &src1, &src2, 20.0, 10.0, 1.6, 10.0, &tj1, &tj2);
    CHECK(ret==THERMAL_OK, "mutual heating calculation");
}

/* ================================================================
 * L5: Via Analysis and Material Database
 * ================================================================ */

void test_l5_via_single(void) {
    TEST(L5_via_single);
    thermal_via_geometry_t via = {
        .drill_diameter_mm = 0.3, .plating_thickness_mm = 0.025,
        .pitch_mm = 0.8, .num_vias = 1, .is_filled = 0
    };
    double r = thermal_via_single_resistance(&via, 1.6, 385.0);
    CHECK(r > 150.0 && r < 250.0, "single via resistance plausible");
    via.num_vias = 25;
    double r_array = thermal_via_array_resistance(&via, 1.6, 385.0);
    CHECK(r_array < r && r_array > 0.0, "via array resistance reduction");
}

void test_l5_via_count(void) {
    TEST(L5_via_count);
    int n_vias; double r_achieved;
    /* 2W, 30C max rise => R_target = 15 C/W. With 0.5mm drill, lower R. */
    int ret = thermal_via_calculate_count(2.0, 30.0, 1.6, 385.0, 0.5, 0.025, 1.25, 100, &n_vias, &r_achieved);
    CHECK(ret==THERMAL_OK && n_vias > 0 && r_achieved <= 15.0, "via count calculation");
}

void test_l5_via_fill_analysis(void) {
    TEST(L5_via_fill_analysis);
    double r_unfilled, r_filled, improvement;
    thermal_via_fill_analysis(0.3, 0.025, 58.0, &r_unfilled, &r_filled, &improvement);
    CHECK(r_filled < r_unfilled && improvement > 1.0, "filled via improvement");
}

void test_l5_material_db(void) {
    TEST(L5_material_db);
    const material_property_t *fr4 = pcb_thermal_material_get(PCB_MAT_FR4);
    int ok1 = (fr4 != NULL && strcmp(fr4->name, "FR4 Standard") == 0 && NEAR(fr4->k_xy, 0.35, 0.01));
    double k_cu = pcb_thermal_material_k_xy(PCB_MAT_COPPER);
    int ok2 = NEAR(k_cu, 385.0, 1.0);
    int ok3 = pcb_thermal_material_cte_compatible(PCB_MAT_COPPER, PCB_MAT_ALUMINUM, 10.0);
    pcb_material_type_t rec = pcb_thermal_material_recommend(30.0, 10.0, 0.0, 0.0, 0.0, 0);
    int ok4 = (rec == PCB_MAT_CERAMIC || rec == PCB_MAT_COPPER || rec == PCB_MAT_ALUMINUM);
    CHECK(ok1&&ok2&&ok3&&ok4, "material database functions");
}

void test_l5_copper_layer_k(void) {
    TEST(L5_copper_layer_k);
    double k_full = pcb_thermal_copper_layer_k(CU_1OZ, 1.0, 0.30);
    CHECK(NEAR(k_full, 385.0, 1.0), "copper layer full coverage");
    double k_half = pcb_thermal_copper_layer_k(CU_1OZ, 0.5, 0.30);
    CHECK(NEAR(k_half, 192.65, 0.1), "copper layer half coverage");
}

void test_l5_copper_pour_sizing(void) {
    TEST(L5_copper_pour_sizing);
    /* 0.5W chip with Rjc=5, forced convection h=50 W/m^2-K, 4-layer board k=10 */
    double area, r_pour;
    int ret = copper_pour_sizing(0.5, 45.0, 125.0, 5.0, 10.0, 1.6, 50.0, CU_2OZ, &area, &r_pour);
    CHECK(ret==THERMAL_OK && area > 0.0, "copper pour sizing");
}

void test_l5_derating(void) {
    TEST(L5_derating);
    double df = thermal_derating_factor(100.0, 150.0, 25.0);
    CHECK(NEAR(df, 0.4, 0.01), "thermal derating factor");
}

void test_l5_lifetime(void) {
    TEST(L5_lifetime);
    double life = thermal_lifetime_estimate(115.0, 105.0, 10000.0, 0.7);
    CHECK(life < 10000.0 && life > 1000.0, "lifetime estimate decreased");
}

/* ================================================================
 * L6: FDM Simulation
 * ================================================================ */

void test_l6_fdm_basic(void) {
    TEST(L6_fdm_basic);
    ambient_conditions_t amb = ambient_default();
    thermal_field_t *field = thermal_field_create(50.0, 50.0, 1.6, 1.0, 1.0, &amb);
    assert(field != NULL);
    thermal_field_set_uniform_k(field, 0.35);
    heat_source_t src = {
        .center = {25.0, 25.0, 0.0}, .width_mm = 5.0, .length_mm = 5.0,
        .height_mm = 1.0, .power_w = 1.0, .r_jc = 5.0, .max_tj = 150.0
    };
    thermal_field_add_heat_source(field, &src);
    int ret = thermal_fdm_solve_steady(field, 10.0, 10.0, 1e-4, 50000);
    CHECK(ret==THERMAL_OK, "FDM steady-state convergence");
    double t_max = thermal_fdm_max_temperature(field);
    CHECK(t_max > amb.ambient_temp_c, "FDM max temperature above ambient");
    double tj_arr[1];
    thermal_fdm_extract_junction_temps(field, tj_arr);
    CHECK(tj_arr[0] > amb.ambient_temp_c, "FDM junction temperature");
    thermal_field_free(field);
}

void test_l6_fdm_sor(void) {
    TEST(L6_fdm_sor);
    ambient_conditions_t amb = ambient_default();
    thermal_field_t *field = thermal_field_create(30.0, 30.0, 1.6, 0.5, 0.5, &amb);
    assert(field != NULL);
    thermal_field_set_uniform_k(field, 8.7);
    heat_source_t src = {
        .center = {15.0, 15.0, 0.0}, .width_mm = 3.0, .length_mm = 3.0,
        .power_w = 0.5, .r_jc = 10.0, .max_tj = 150.0
    };
    thermal_field_add_heat_source(field, &src);
    double omega = thermal_sor_optimal_omega(field->rows, field->cols, 0.96);
    int ret = thermal_fdm_solve_sor(field, 10.0, 10.0, omega, 1e-6, 50000);
    CHECK(ret==THERMAL_OK, "FDM SOR convergence");
    double res = thermal_fdm_compute_residual(field, 10.0, 10.0);
    CHECK(!isinf(res), "FDM residual check");  /* Residual finite after convergence */
    thermal_field_free(field);
}

/* ================================================================
 * L7: Applications
 * ================================================================ */

void test_l7_design_cooling(void) {
    TEST(L7_design_cooling);
    heat_source_t ldo = {
        .center = {20.0, 20.0, 0.0}, .width_mm = 6.0, .length_mm = 5.0,
        .height_mm = 1.5, .power_w = 0.85, .package = PKG_SOT23,
        .r_jc = 80.0, .max_tj = 125.0
    };
    cooling_type_t cooling; double final_tj;
    int ret = design_cooling_solution(&ldo, 45.0, 2500.0, 0.35, 1.6, CU_1OZ, 0, 1.25, &cooling, &final_tj);
    CHECK(ret==THERMAL_OK || ret==THERMAL_ERR_TJ_EXCEEDED, "design cooling solution");
}

void test_l7_heatsink_select(void) {
    TEST(L7_heatsink_select);
    heat_sink_model_t selected;
    int ret = heatsink_select_from_catalog(20.0, 50.0, 30.0, 50.0, 0.0, &selected);
    CHECK(ret==THERMAL_OK && selected.base_width_mm > 0.0, "heatsink selection");
}

/* ================================================================
 * L8: Advanced Topics
 * ================================================================ */

void test_l8_thermal_runaway(void) {
    TEST(L8_thermal_runaway);
    heat_source_t mosfets[2] = {
        {.power_w=2.0, .max_tj=175.0, .width_mm=6.0, .length_mm=5.0},
        {.power_w=2.0, .max_tj=175.0, .width_mm=6.0, .length_mm=5.0}
    };
    int ret = thermal_runaway_check(mosfets, 2, 20.0, 40.0);
    CHECK(ret==THERMAL_OK, "thermal runaway check balanced");
}

void test_l8_cauer_model(void) {
    TEST(L8_cauer_model);
    double r_vals[] = {2.0, 1.0, 5.0, 20.0};
    double c_vals[] = {0.01, 0.1, 1.0};
    double tj;
    int ret = transient_cauer_model(25.0, 10.0, r_vals, c_vals, 3, 10.0, 0.001, &tj);
    CHECK(ret==THERMAL_OK && tj > 25.0, "Cauer transient model");
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("\n============================================================\n");
    printf("PCB Thermal Management Library - Test Suite\n");
    printf("============================================================\n\n");

    printf("--- L1: Definitions ---\n");
    test_l1_inline_functions();

    printf("\n--- L2: Core Concepts ---\n");
    test_l2_conduction();
    test_l2_convection();
    test_l2_series_parallel();
    test_l2_spreading();
    test_l2_junction_temperature();
    test_l2_radiation();

    printf("\n--- L3: Math Structures ---\n");
    test_l3_lumped_capacitance();
    test_l3_foster_model();
    test_l3_biot_number();
    test_l3_fourier_number();
    test_l3_spreading_length();
    test_l3_thermal_impedance();

    printf("\n--- L4: Fundamental Laws ---\n");
    test_l4_network_solver();
    test_l4_mutual_heating();

    printf("\n--- L5: Algorithms ---\n");
    test_l5_via_single();
    test_l5_via_count();
    test_l5_via_fill_analysis();
    test_l5_material_db();
    test_l5_copper_layer_k();
    test_l5_copper_pour_sizing();
    test_l5_derating();
    test_l5_lifetime();

    printf("\n--- L6: Canonical Problems ---\n");
    test_l6_fdm_basic();
    test_l6_fdm_sor();

    printf("\n--- L7: Applications ---\n");
    test_l7_design_cooling();
    test_l7_heatsink_select();

    printf("\n--- L8: Advanced Topics ---\n");
    test_l8_thermal_runaway();
    test_l8_cauer_model();

    printf("\n============================================================\n");
    printf("RESULTS: %d/%d tests passed\n", tests_passed, tests_run);
    printf("============================================================\n");

    return (tests_passed == tests_run) ? 0 : 1;
}
