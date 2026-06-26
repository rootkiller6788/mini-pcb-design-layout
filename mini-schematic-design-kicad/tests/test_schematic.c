/**
 * @file test_schematic.c
 * @brief Comprehensive test suite for mini-schematic-design-kicad
 *
 * Tests: core data structures, netlist extraction, BOM generation,
 * ERC, connectivity analysis, S-expression parsing, netlist formats,
 * SPICE parse, MNA matrix construction/solving.
 *
 * Run: make test
 */

#include "schematic_core.h"
#include "schematic_netlist.h"
#include "schematic_bom.h"
#include "schematic_erc.h"
#include "schematic_connectivity.h"
#include "schematic_sexpr.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)

/* ─── Core Data Structure Tests ─── */

static void test_design_create_free(void)
{
    TEST("design_create_free");
    schematic_design_t *sch = schematic_design_create("Test Design");
    assert(sch != NULL);
    assert(strcmp(sch->title, "Test Design") == 0);
    assert(sch->num_components == 0);
    assert(sch->num_nets == 0);
    schematic_design_free(sch);
    PASS();
}

static void test_add_component_pin(void)
{
    TEST("add_component_pin");
    schematic_design_t *sch = schematic_design_create("Test");
    int idx = schematic_add_component(sch, "R1", "10k", "R_0805", "device:R");
    assert(idx == 0);
    assert(sch->num_components == 1);
    assert(strcmp(sch->components[0].reference, "R1") == 0);
    assert(strcmp(sch->components[0].value, "10k") == 0);

    int pidx = schematic_component_add_pin(sch->components, "1", "1", PIN_TYPE_PASSIVE);
    assert(pidx == 0);
    assert(sch->components[0].num_pins == 1);
    assert(strcmp(sch->components[0].pins[0].name, "1") == 0);

    pidx = schematic_component_add_pin(sch->components, "2", "2", PIN_TYPE_PASSIVE);
    assert(pidx == 1);
    assert(sch->components[0].num_pins == 2);
    schematic_design_free(sch);
    PASS();
}

static void test_net_creation(void)
{
    TEST("net_creation");
    schematic_design_t *sch = schematic_design_create("NetTest");
    int ni = schematic_add_net(sch, "VCC", 1);
    assert(ni == 0);
    assert(sch->num_nets == 1);
    assert(strcmp(sch->nets[0].name, "VCC") == 0);
    assert(sch->nets[0].is_power_net == true);

    ni = schematic_add_net(sch, "Net-(R1-Pad1)", -1);
    assert(ni == 1);
    assert(sch->nets[1].net_code == 2);
    assert(sch->nets[1].is_power_net == false);
    schematic_design_free(sch);
    PASS();
}

static void test_connect_pin(void)
{
    TEST("connect_pin");
    schematic_design_t *sch = schematic_design_create("ConnTest");
    schematic_add_component(sch, "R1", "10k", "R_0805", "device:R");
    schematic_component_add_pin(sch->components, "1", "1", PIN_TYPE_PASSIVE);
    schematic_component_add_pin(sch->components, "2", "2", PIN_TYPE_PASSIVE);
    bool ok = schematic_connect_pin(sch, "R1", "1", "Net-R1-1");
    assert(ok == true);
    assert(sch->num_nets == 1);
    assert(sch->nets[0].num_connections == 1);
    ok = schematic_connect_pin(sch, "R1", "2", "Net-R1-1");
    assert(ok == true);
    assert(sch->nets[0].num_connections == 2);
    assert(schematic_total_connections(sch) == 2);
    assert(schematic_total_pins(sch) == 2);
    schematic_design_free(sch);
    PASS();
}

static void test_validation(void)
{
    TEST("validation");
    schematic_design_t *sch = schematic_design_create("ValTest");
    schematic_add_component(sch, "U1", "MCU", "QFP-100", "mcu:STM32");
    schematic_add_component(sch, "U1", "MCU2", "QFP-100", "mcu:STM32");
    char errors[1024] = {0};
    int nerr = schematic_validate_references(sch, errors, sizeof(errors));
    assert(nerr > 0);
    assert(strstr(errors, "Duplicate") != NULL);
    assert(strstr(errors, "U1") != NULL);
    schematic_design_free(sch);
    PASS();
}

/* ─── Netlist Tests ─── */

static void test_netlist_extract(void)
{
    TEST("netlist_extract");
    schematic_design_t *sch = schematic_design_create("NLExtract");
    schematic_add_component(sch, "R1", "1k", "R_0805", "device:R");
    schematic_component_add_pin(sch->components, "1", "1", PIN_TYPE_PASSIVE);
    schematic_component_add_pin(sch->components, "2", "2", PIN_TYPE_PASSIVE);
    schematic_connect_pin(sch, "R1", "1", "N1");
    schematic_connect_pin(sch, "R1", "2", "GND");
    netlist_data_t *nl = netlist_extract(sch);
    assert(nl != NULL);
    assert(nl->num_nets == 2);
    assert(netlist_total_connections(nl) == 2);
    int gi = netlist_find_net(nl, "GND");
    assert(gi >= 0);
    netlist_free(nl);
    schematic_design_free(sch);
    PASS();
}

static void test_netlist_spice_write(void)
{
    TEST("netlist_spice_write");
    schematic_design_t *sch = schematic_design_create("SpiceTest");
    schematic_add_component(sch, "R1", "10k", "R_0805", "device:R");
    schematic_component_add_pin(sch->components, "1", "1", PIN_TYPE_PASSIVE);
    schematic_component_add_pin(sch->components, "2", "2", PIN_TYPE_PASSIVE);
    schematic_connect_pin(sch, "R1", "1", "VCC");
    schematic_connect_pin(sch, "R1", "2", "GND");
    netlist_data_t *nl = netlist_extract(sch);
    int ret = netlist_write(nl, NETLIST_FMT_SPICE, "test_output.cir");
    assert(ret == 0);
    FILE *fp = fopen("test_output.cir", "r");
    assert(fp != NULL);
    char line[256];
    bool found_end = false;
    while (fgets(line, sizeof(line), fp))
        if (strstr(line, ".END")) found_end = true;
    fclose(fp);
    assert(found_end);
    remove("test_output.cir");
    netlist_free(nl);
    schematic_design_free(sch);
    PASS();
}

static void test_netlist_formats(void)
{
    TEST("netlist_formats");
    schematic_design_t *sch = schematic_design_create("FmtTest");
    schematic_add_component(sch, "C1", "100n", "C_0805", "device:C");
    schematic_component_add_pin(sch->components, "1", "1", PIN_TYPE_PASSIVE);
    schematic_component_add_pin(sch->components, "2", "2", PIN_TYPE_PASSIVE);
    schematic_connect_pin(sch, "C1", "1", "VCC");
    schematic_connect_pin(sch, "C1", "2", "GND");
    netlist_data_t *nl = netlist_extract(sch);
    int ret = netlist_write(nl, NETLIST_FMT_KICAD, "test_fmt.net");
    assert(ret == 0);
    remove("test_fmt.net");
    ret = netlist_write(nl, NETLIST_FMT_PADS, "test_fmt.asc");
    assert(ret == 0);
    remove("test_fmt.asc");
    netlist_free(nl);
    schematic_design_free(sch);
    PASS();
}

/* ─── BOM Tests ─── */

static void test_bom_generate(void)
{
    TEST("bom_generate");
    schematic_design_t *sch = schematic_design_create("BOMTest");
    schematic_add_component(sch, "R1", "10k", "R_0805", "device:R");
    schematic_add_component(sch, "R2", "10k", "R_0805", "device:R");
    schematic_add_component(sch, "C1", "100n", "C_0805", "device:C");
    schematic_add_component(sch, "R3", "1k", "R_0805", "device:R");
    bom_report_t *bom = bom_generate(sch, BOM_GROUP_VALUE_FOOTPRINT);
    assert(bom != NULL);
    assert(bom->total_components == 4);
    assert(bom->unique_parts == 3);
    bool found_10k = false;
    for (int i = 0; i < bom->num_items; i++) {
        if (strcmp(bom->items[i].value, "10k") == 0) {
            assert(bom->items[i].quantity == 2);
            assert(strstr(bom->items[i].designators, "R1") != NULL);
            assert(strstr(bom->items[i].designators, "R2") != NULL);
            found_10k = true;
        }
    }
    assert(found_10k);
    bom_report_free(bom);
    schematic_design_free(sch);
    PASS();
}

static void test_bom_export(void)
{
    TEST("bom_export");
    schematic_design_t *sch = schematic_design_create("BOMExp");
    schematic_add_component(sch, "R1", "10k", "R_0805", "device:R");
    bom_report_t *bom = bom_generate(sch, BOM_GROUP_NO_GROUP);
    assert(bom != NULL);
    assert(bom->num_items == 1);
    int ret = bom_export(bom, BOM_FMT_CSV, "test_bom.csv");
    assert(ret == 0);
    remove("test_bom.csv");
    ret = bom_export(bom, BOM_FMT_JSON, "test_bom.json");
    assert(ret == 0);
    remove("test_bom.json");
    ret = bom_export(bom, BOM_FMT_MARKDOWN, "test_bom.md");
    assert(ret == 0);
    remove("test_bom.md");
    bom_report_free(bom);
    schematic_design_free(sch);
    PASS();
}

static void test_bom_classify(void)
{
    TEST("bom_classify");
    assert(bom_classify_component("R1") == CAT_RESISTOR);
    assert(bom_classify_component("C10") == CAT_CAPACITOR);
    assert(bom_classify_component("L2") == CAT_INDUCTOR);
    assert(bom_classify_component("D1") == CAT_DIODE);
    assert(bom_classify_component("Q3") == CAT_TRANSISTOR);
    assert(bom_classify_component("U1") == CAT_IC_DIGITAL);
    assert(bom_classify_component("J2") == CAT_CONNECTOR);
    assert(bom_classify_component("Y1") == CAT_CRYSTAL);
    assert(strcmp(bom_category_name(CAT_RESISTOR), "Resistor") == 0);
    PASS();
}

static void test_bom_assembly_cost(void)
{
    TEST("bom_assembly_cost");
    schematic_design_t *sch = schematic_design_create("CostTest");
    schematic_add_component(sch, "R1", "10k", "Resistor_SMD:R_0805", "device:R");
    schematic_add_component(sch, "J1", "CONN", "Connector_PinHeader_2x05", "conn:PinHeader");
    bom_report_t *bom = bom_generate(sch, BOM_GROUP_NO_GROUP);
    double cost = bom_estimate_assembly_cost(bom, 0.005, 0.03, 50.0);
    assert(cost > 50.0);
    assert(cost < 100.0);
    bom_report_free(bom);
    schematic_design_free(sch);
    PASS();
}

/* ─── ERC Tests ─── */

static void test_erc_run(void)
{
    TEST("erc_run");
    schematic_design_t *sch = schematic_design_create("ERCTest");
    schematic_add_component(sch, "U1", "MCU", "QFP-100", "mcu:STM32");
    schematic_component_add_pin(sch->components, "VCC", "1", PIN_TYPE_POWER_IN);
    schematic_component_add_pin(sch->components, "GND", "2", PIN_TYPE_POWER_IN);
    erc_report_t *report = erc_run(sch);
    assert(report != NULL);
    assert(report->num_violations > 0);
    int up = erc_count_by_code(report, ERC_VIOL_UNCONNECTED_POWER);
    assert(up > 0);
    erc_report_free(report);
    schematic_design_free(sch);
    PASS();
}

static void test_erc_duplicate_refs(void)
{
    TEST("erc_duplicate_refs");
    schematic_design_t *sch = schematic_design_create("DupTest");
    schematic_add_component(sch, "R1", "10k", "R_0805", "device:R");
    schematic_add_component(sch, "R1", "1k", "R_0805", "device:R");
    erc_report_t *report = erc_run(sch);
    assert(report != NULL);
    int dup = erc_count_by_code(report, ERC_VIOL_DUPLICATE_REF);
    assert(dup > 0);
    erc_report_free(report);
    schematic_design_free(sch);
    PASS();
}

static void test_erc_pin_types(void)
{
    TEST("erc_pin_types");
    assert(erc_is_driver_type(PIN_TYPE_OUTPUT) == true);
    assert(erc_is_driver_type(PIN_TYPE_INPUT) == false);
    assert(erc_is_receiver_type(PIN_TYPE_INPUT) == true);
    assert(erc_is_receiver_type(PIN_TYPE_OUTPUT) == false);
    assert(erc_is_power_type(PIN_TYPE_POWER_IN) == true);
    assert(erc_is_power_type(PIN_TYPE_PASSIVE) == false);
    assert(strcmp(erc_pin_type_name(PIN_TYPE_INPUT), "input") == 0);
    assert(strcmp(erc_severity_name(ERC_ERROR), "ERROR") == 0);
    assert(strcmp(erc_violation_name(ERC_VIOL_SINGLE_PIN_NET), "SinglePinNet") == 0);
    PASS();
}

static void test_erc_filter(void)
{
    TEST("erc_filter");
    schematic_design_t *sch = schematic_design_create("FilterTest");
    schematic_add_component(sch, "U1", "MCU", "QFP-100", "mcu:STM32");
    schematic_component_add_pin(sch->components, "VCC", "1", PIN_TYPE_POWER_IN);
    erc_report_t *full = erc_run(sch);
    erc_report_t *err_only = erc_filter_by_severity(full, ERC_ERROR);
    assert(err_only != NULL);
    assert(err_only->num_violations <= full->num_violations);
    char json_buf[4096] = {0};
    int len = erc_report_to_json(full, json_buf, sizeof(json_buf));
    assert(len > 0);
    assert(strstr(json_buf, "violations") != NULL);
    erc_report_free(err_only);
    erc_report_free(full);
    schematic_design_free(sch);
    PASS();
}

/* ─── Connectivity Tests ─── */

static void test_connectivity_metrics(void)
{
    TEST("connectivity_metrics");
    schematic_design_t *sch = schematic_design_create("ConnMetrics");
    schematic_add_component(sch, "R1", "10k", "R_0805", "device:R");
    schematic_component_add_pin(sch->components, "1", "1", PIN_TYPE_PASSIVE);
    schematic_component_add_pin(sch->components, "2", "2", PIN_TYPE_PASSIVE);
    schematic_connect_pin(sch, "R1", "1", "N1");
    schematic_connect_pin(sch, "R1", "2", "N2");
    connectivity_metrics_t m = connectivity_compute_metrics(sch);
    assert(m.num_vertices == 2);
    assert(m.num_edges == 2);
    connectivity_metrics_print(stdout, &m);
    PASS();
}

static void test_shortest_path(void)
{
    TEST("shortest_path");
    schematic_design_t *sch = schematic_design_create("PathTest");
    schematic_add_component(sch, "R1", "10k", "R_0805", "device:R");
    schematic_component_add_pin(sch->components, "1", "1", PIN_TYPE_PASSIVE);
    schematic_component_add_pin(sch->components, "2", "2", PIN_TYPE_PASSIVE);
    schematic_add_component(sch, "R2", "1k", "R_0805", "device:R");
    schematic_component_add_pin(sch->components + 1, "1", "1", PIN_TYPE_PASSIVE);
    schematic_component_add_pin(sch->components + 1, "2", "2", PIN_TYPE_PASSIVE);
    /* R1.1 and R2.2 share a net — directly connected */
    schematic_connect_pin(sch, "R1", "1", "SharedNet");
    schematic_connect_pin(sch, "R2", "2", "SharedNet");
    /* R1.2 on separate net */
    schematic_connect_pin(sch, "R1", "2", "NetA");
    schematic_connect_pin(sch, "R2", "1", "NetB");
    connectivity_path_t *path = connectivity_shortest_path(sch, "R1", "1", "R2", "2");
    assert(path != NULL);
    assert(path->path_length >= 2);
    connectivity_path_free(path);
    PASS();
}

static void test_reachable_components(void)
{
    TEST("reachable_components");
    schematic_design_t *sch = schematic_design_create("ReachTest");
    schematic_add_component(sch, "U1", "MCU", "QFP", "mcu:STM32");
    schematic_component_add_pin(sch->components, "VCC", "1", PIN_TYPE_POWER_IN);
    schematic_connect_pin(sch, "U1", "VCC", "VCC");
    int reachable = connectivity_reachable_components(sch, "U1", "1");
    assert(reachable >= 1);
    PASS();
}

/* ─── S-Expression Tests ─── */

static void test_sexpr_tokenizer(void)
{
    TEST("sexpr_tokenizer");
    const char *input = "(symbol (lib_id \"device:R\") (value \"10k\"))";
    sexpr_tokenizer_t t;
    sexpr_tokenizer_init(&t, input, strlen(input));
    sexpr_token_type_t tt = sexpr_next_token(&t);
    assert(tt == SEXPR_TOK_LPAREN);
    tt = sexpr_next_token(&t);
    assert(tt == SEXPR_TOK_SYMBOL);
    assert(strcmp(t.current.text, "symbol") == 0);
    tt = sexpr_next_token(&t);
    assert(tt == SEXPR_TOK_LPAREN);
    tt = sexpr_next_token(&t);
    assert(tt == SEXPR_TOK_SYMBOL);
    assert(strcmp(t.current.text, "lib_id") == 0);
    tt = sexpr_next_token(&t);
    assert(tt == SEXPR_TOK_STRING);
    assert(strcmp(t.current.text, "device:R") == 0);
    PASS();
}

static void test_sexpr_parse(void)
{
    TEST("sexpr_parse");
    const char *input = "(kicad_sch (version 20230121) (symbol (lib_id \"device:R\") (property \"Reference\" \"R1\")))";
    sexpr_tokenizer_t t;
    sexpr_tokenizer_init(&t, input, strlen(input));
    sexpr_node_t *root = sexpr_parse(&t);
    assert(root != NULL);
    assert(root->node_type == SEXPR_LIST);
    assert(sexpr_is_list_with(root, "kicad_sch"));
    assert(sexpr_num_children(root) == 3);
    sexpr_node_t *sym = sexpr_find_child(root, "symbol");
    assert(sym != NULL);
    const char *ref = sexpr_get_property(sym, "Reference");
    assert(ref != NULL);
    assert(strcmp(ref, "R1") == 0);
    sexpr_free(root);
    PASS();
}

static void test_kicad_sch_write(void)
{
    TEST("kicad_sch_write");
    schematic_design_t *sch = schematic_design_create("KiCadTest");
    schematic_add_component(sch, "R1", "10k", "R_0805", "device:R");
    strncpy(sch->company, "TestCorp", sizeof(sch->company) - 1);
    int ret = kicad_sch_write("test_output.kicad_sch", sch);
    assert(ret == 0);
    FILE *fp = fopen("test_output.kicad_sch", "r");
    assert(fp != NULL);
    char buf[4096] = {0};
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);
    assert(n > 0);
    assert(strstr(buf, "kicad_sch") != NULL);
    assert(strstr(buf, "R1") != NULL);
    assert(strstr(buf, "10k") != NULL);
    remove("test_output.kicad_sch");
    schematic_design_free(sch);
    PASS();
}

/* ─── SPICE/MNA Math Tests (L4) ─── */

static void test_spice_parse_component(void)
{
    TEST("spice_parse_component");
    char ref[32], value[32];
    char nodes[3][32] = {{0}};
    char ctype = spice_parse_component("R1 n1 n2 10k", ref, sizeof(ref),
                                        (char*)nodes, 2, value, sizeof(value));
    assert(ctype == 'R');
    assert(strcmp(ref, "R1") == 0);
    assert(strcmp(nodes[0], "n1") == 0);
    assert(strcmp(nodes[1], "n2") == 0);
    assert(strcmp(value, "10k") == 0);
    PASS();
}

static void test_mna_matrix(void)
{
    TEST("mna_matrix");
    schematic_design_t *sch = schematic_design_create("MNATest");
    schematic_add_component(sch, "R1", "1k", "R_0805", "device:R");
    schematic_component_add_pin(sch->components, "1", "1", PIN_TYPE_PASSIVE);
    schematic_component_add_pin(sch->components, "2", "2", PIN_TYPE_PASSIVE);
    schematic_connect_pin(sch, "R1", "1", "VCC");
    schematic_connect_pin(sch, "R1", "2", "GND");
    netlist_data_t *nl = netlist_extract(sch);
    double *A = NULL, *b = NULL;
    int n = 0;
    int ret = spice_build_mna_matrix(nl, &A, &b, &n);
    assert(ret == 0);
    assert(n > 0);
    assert(A != NULL);
    assert(b != NULL);
    if (n > 0) {
        double *x = calloc(n, sizeof(double));
        ret = spice_solve_dc_op(A, b, x, n);
        assert(ret == 0);
        assert(fabs(x[0]) < 1e-9);
        free(x);
    }
    free(A); free(b);
    netlist_free(nl);
    schematic_design_free(sch);
    PASS();
}

/* ─── BOM Merge Test ─── */

static void test_bom_merge(void)
{
    TEST("bom_merge");
    schematic_design_t *s1 = schematic_design_create("Sheet1");
    schematic_add_component(s1, "R1", "10k", "R_0805", "device:R");
    schematic_design_t *s2 = schematic_design_create("Sheet2");
    schematic_add_component(s2, "R2", "10k", "R_0805", "device:R");
    bom_report_t *b1 = bom_generate(s1, BOM_GROUP_VALUE_FOOTPRINT);
    bom_report_t *b2 = bom_generate(s2, BOM_GROUP_VALUE_FOOTPRINT);
    bom_report_t *boms[2] = {b1, b2};
    bom_report_t *merged = bom_merge(boms, 2);
    assert(merged != NULL);
    assert(merged->unique_parts == 1);
    assert(merged->items[0].quantity == 2);
    bom_report_free(merged);
    bom_report_free(b2); bom_report_free(b1);
    schematic_design_free(s2); schematic_design_free(s1);
    PASS();
}

/* ─── Main ─── */

int main(void)
{
    printf("=== mini-schematic-design-kicad Test Suite ===\n\n");

    printf("[Core]\n");
    test_design_create_free();
    test_add_component_pin();
    test_net_creation();
    test_connect_pin();
    test_validation();

    printf("\n[Netlist]\n");
    test_netlist_extract();
    test_netlist_spice_write();
    test_netlist_formats();

    printf("\n[BOM]\n");
    test_bom_generate();
    test_bom_export();
    test_bom_classify();
    test_bom_assembly_cost();
    test_bom_merge();

    printf("\n[ERC]\n");
    test_erc_run();
    test_erc_duplicate_refs();
    test_erc_pin_types();
    test_erc_filter();

    printf("\n[Connectivity]\n");
    test_connectivity_metrics();
    test_shortest_path();
    test_reachable_components();

    printf("\n[S-Expression]\n");
    test_sexpr_tokenizer();
    test_sexpr_parse();
    test_kicad_sch_write();

    printf("\n[Math/L4]\n");
    test_spice_parse_component();
    test_mna_matrix();

    printf("\n=== Results: %d/%d tests passed ===\n",
           tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}