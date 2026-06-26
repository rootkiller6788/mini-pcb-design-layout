#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <complex.h>
#include <assert.h>
#include "pcb_stackup.h"
#include "pcb_impedance.h"
#include "pcb_transmission_line.h"
#include "pcb_material.h"
#include "pcb_via.h"
#include "pcb_routing.h"
#include "pcb_signal_integrity.h"

#define EPS 1e-6

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s... ", #name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define CHECK(cond) do { if (!(cond)) { printf("FAIL at line %d: %s\n", __LINE__, #cond); return; } } while(0)
#define CHECK_NEAR(a,b,tol) do { if (fabs((a)-(b)) > (tol)) { printf("FAIL at line %d: |%g-%g| > %g\n", __LINE__, (double)(a), (double)(b), (double)(tol)); return; } } while(0)

void test_stackup_create(void) {
    TEST(stackup_create);
    PcbStackup s = stackup_create("Test", 4);
    CHECK(s.num_layers == 4);
    CHECK(s.total_thickness_mm > 0);
    PASS();
}

void test_stackup_4layer(void) {
    TEST(stackup_4layer);
    PcbStackup s = stackup_standard_4layer();
    CHECK(s.num_layers == 4);
    CHECK(s.num_signal_layers == 2);
    CHECK(s.num_plane_layers == 2);
    CHECK(s.copper_weight_outer_oz == 1.0);
    PASS();
}

void test_stackup_build(void) {
    TEST(stackup_build);
    PcbStackup s = stackup_standard_4layer();
    int r = stackup_build(&s);
    CHECK(r == 0);
    CHECK(s.total_thickness_mm > 1.0);
    PASS();
}

void test_stackup_symmetry(void) {
    TEST(stackup_symmetry);
    PcbStackup s = stackup_standard_4layer();
    double asym = stackup_symmetry_check(&s);
    CHECK(asym < 5.0);
    PASS();
}

void test_stackup_min_trace(void) {
    TEST(min_trace_width);
    double w1 = stackup_min_trace_width(1, 1.0);
    double w2 = stackup_min_trace_width(2, 1.0);
    double w3 = stackup_min_trace_width(3, 1.0);
    CHECK(w1 < w2 && w2 < w3);
    PASS();
}

void test_microstrip_ipc(void) {
    TEST(microstrip_ipc);
    MicrostripGeometry geo;
    memset(&geo, 0, sizeof(geo));
    geo.trace.trace_width_m = 350e-6;
    geo.trace.dielectric_height_m = 200e-6;
    geo.trace.dielectric_er = 4.2;
    geo.trace.trace_thickness_m = 35e-6;
    geo.trace.conductor_sigma = 5.8e7;
    ImpedanceResult r = impedance_microstrip_ipc(&geo);
    CHECK(r.z0_ohm > 30 && r.z0_ohm < 80);
    CHECK(r.er_eff > 1.0);
    PASS();
}

void test_microstrip_hammerstad(void) {
    TEST(hammerstad);
    MicrostripGeometry geo;
    memset(&geo, 0, sizeof(geo));
    geo.trace.trace_width_m = 350e-6;
    geo.trace.dielectric_height_m = 200e-6;
    geo.trace.dielectric_er = 4.2;
    geo.trace.trace_thickness_m = 35e-6;
    geo.trace.conductor_sigma = 5.8e7;
    ImpedanceResult r = impedance_microstrip_hammerstad(&geo);
    CHECK(r.z0_ohm > 35 && r.z0_ohm < 75);
    CHECK(strcmp(r.formula_used, "Hammerstad-Jensen") == 0);
    PASS();
}

void test_stripline(void) {
    TEST(stripline);
    StriplineGeometry geo;
    memset(&geo, 0, sizeof(geo));
    geo.trace.trace_width_m = 200e-6;
    geo.trace.dielectric_er = 4.2;
    geo.trace.trace_thickness_m = 35e-6;
    geo.trace.conductor_sigma = 5.8e7;
    geo.upper_height_m = 200e-6;
    geo.lower_height_m = 200e-6;
    ImpedanceResult r = impedance_stripline_symmetric(&geo);
    CHECK(r.z0_ohm > 30 && r.z0_ohm < 80);
    PASS();
}

void test_inverse_design(void) {
    TEST(inverse_design);
    double w = impedance_microstrip_w_for_z0(50.0, 200.0, 4.2, 35.0, NULL);
    CHECK(w > 50 && w < 500);
    CHECK_NEAR(w, 350, 100);
    PASS();
}

void test_diff_pair(void) {
    TEST(diff_pair);
    DiffPairEdgeGeometry dg;
    memset(&dg, 0, sizeof(dg));
    dg.trace.trace_width_m = 150e-6;
    dg.trace.dielectric_height_m = 200e-6;
    dg.trace.dielectric_er = 4.2;
    dg.trace.trace_thickness_m = 35e-6;
    dg.trace.conductor_sigma = 5.8e7;
    dg.spacing_m = 200e-6;
    DiffImpedanceResult r = impedance_diff_pair_edge(&dg);
    CHECK(r.z_diff_ohm > 60 && r.z_diff_ohm < 150);
    CHECK(r.coupling_coeff > 0 && r.coupling_coeff < 1);
    PASS();
}

void test_transmission_line(void) {
    TEST(transmission_line);
    TraceGeometry geo;
    memset(&geo, 0, sizeof(geo));
    geo.trace_width_m = 350e-6;
    geo.trace_thickness_m = 35e-6;
    geo.dielectric_height_m = 200e-6;
    geo.dielectric_er = 4.2;
    geo.loss_tangent = 0.02;
    geo.conductor_sigma = 5.8e7;
    TransmissionLine tl = tl_from_geometry(&geo, 1e9);
    CHECK(tl.z0_real > 20 && tl.z0_real < 150);
    CHECK(tl.vp > 0);
    double complex gamma = tl_reflection_coefficient(25.0, tl.z0_real);
    double vswr = tl_vswr_from_gamma(gamma);
    CHECK(vswr >= 1.0);
    PASS();
}

void test_via_inductance(void) {
    TEST(via_inductance);
    ViaDimensions vd;
    memset(&vd, 0, sizeof(vd));
    vd.drill_diameter_mm = 0.3;
    vd.pad_diameter_mm = 0.55;
    vd.antipad_diameter_mm = 0.85;
    vd.plating_thickness_um = 25.0;
    vd.total_length_mm = 1.6;
    double L = via_inductance(&vd);
    CHECK(L > 1e-4 && L < 10.0);
    PASS();
}

void test_via_capacitance(void) {
    TEST(via_capacitance);
    ViaDimensions vd;
    memset(&vd, 0, sizeof(vd));
    vd.drill_diameter_mm = 0.3;
    vd.pad_diameter_mm = 0.55;
    vd.antipad_diameter_mm = 0.85;
    double C = via_capacitance(&vd, 4.2, 0.035);
    CHECK(C >= 0);
    PASS();
}

void test_material_skin_depth(void) {
    TEST(skin_depth);
    PcbMaterialSystem mat = material_create("Test", 4.2, 0.02, 5.8e7, 140.0);
    double delta = material_skin_depth(&mat, 1e9);
    CHECK(delta > 0 && delta < 1e-3);
    PASS();
}

void test_er_at_freq(void) {
    TEST(er_at_freq);
    DielectricMaterial dm;
    memset(&dm, 0, sizeof(dm));
    dm.epsilon_r = 4.2;
    dm.loss_tangent = 0.02;
    double er = material_epsilon_r_at_freq(&dm, 10e9);
    CHECK(er < 4.2 && er > 1.0);
    PASS();
}

void test_current_capacity(void) {
    TEST(current_capacity);
    double cur = stackup_trace_current_capacity(0.2, 35.0, 10.0, 0);
    CHECK(cur > 0 && cur < 10);
    PASS();
}

int main(void) {
    printf("=== PCB Stackup & Impedance Tests ===\n");
    test_stackup_create();
    test_stackup_4layer();
    test_stackup_build();
    test_stackup_symmetry();
    test_stackup_min_trace();
    test_microstrip_ipc();
    test_microstrip_hammerstad();
    test_stripline();
    test_inverse_design();
    test_diff_pair();
    test_transmission_line();
    test_via_inductance();
    test_via_capacitance();
    test_material_skin_depth();
    test_er_at_freq();
    test_current_capacity();
    printf("\n=== %d/%d tests passed ===\n", tests_passed, tests_run);
    return tests_passed == tests_run ? 0 : 1;
}
