/**
 * @file schematic_netlist.c
 * @brief Netlist extraction, transformation, and multi-format export
 *
 * Converts schematic_design_t into industry-standard netlist formats:
 * SPICE, IPC-D-356, PADS ASCII, Cadence Allegro, EDIF 2.0.0, and
 * KiCad native S-expression format.
 *
 * Kirchhoff's Current Law (L4): Each net enforces sum(I_j) = 0.
 * The netlist encodes this constraint topologically.
 *
 * MNA Matrix Construction (L5): Modified Nodal Analysis formulates
 * circuit equations as [G B; C D][v; w] = [i; e] for SPICE simulation.
 */

#include "schematic_netlist.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/* ----------- Netlist Extraction ----------- */

netlist_data_t* netlist_extract(const schematic_design_t *sch)
{
    if (!sch) return NULL;
    netlist_data_t *nl = calloc(1, sizeof(netlist_data_t));
    if (!nl) return NULL;
    nl->num_nets = sch->num_nets;
    strncpy(nl->design_name, sch->title, sizeof(nl->design_name) - 1);
    strncpy(nl->tool_info, "mini-schematic-kicad v1.0", sizeof(nl->tool_info) - 1);
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);
    strftime(nl->timestamp, sizeof(nl->timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

    if (nl->num_nets > 0) {
        nl->nets = calloc(nl->num_nets, sizeof(netlist_net_t));
        if (!nl->nets) { free(nl); return NULL; }
        for (int i = 0; i < sch->num_nets; i++) {
            schematic_net_t *snet = &sch->nets[i];
            netlist_net_t *nnet = &nl->nets[i];
            strncpy(nnet->net_name, snet->name, sizeof(nnet->net_name) - 1);
            nnet->net_number = snet->net_code;
            nnet->num_pins = snet->num_connections;
            if (snet->num_connections > 0) {
                nnet->pins = calloc(snet->num_connections, sizeof(struct netlist_pin));
                if (!nnet->pins) { netlist_free(nl); return NULL; }
                for (int j = 0; j < snet->num_connections; j++) {
                    struct netlist_pin *np = &nnet->pins[j];
                    strncpy(np->ref_des, snet->connections[j].ref, 15);
                    np->ref_des[15] = 0;
                    strncpy(np->pin_number, snet->connections[j].pin, 7);
                    np->pin_number[7] = 0;
                    int cidx = schematic_find_component(sch, snet->connections[j].ref);
                    if (cidx >= 0) {
                        schematic_component_t *comp = &sch->components[cidx];
                        for (int k = 0; k < comp->num_pins; k++) {
                            if (strcmp(comp->pins[k].number, snet->connections[j].pin) == 0) {
                                strncpy(np->pin_name, comp->pins[k].name, sizeof(np->pin_name) - 1);
                                np->pin_name[sizeof(np->pin_name) - 1] = 0;
                                switch (comp->pins[k].electrical_type) {
                                case PIN_TYPE_INPUT:  strcpy(np->pin_type, "input"); break;
                                case PIN_TYPE_OUTPUT: strcpy(np->pin_type, "output"); break;
                                case PIN_TYPE_BIDI:   strcpy(np->pin_type, "bidirectional"); break;
                                case PIN_TYPE_PASSIVE: strcpy(np->pin_type, "passive"); break;
                                case PIN_TYPE_POWER_IN: strcpy(np->pin_type, "power_in"); break;
                                case PIN_TYPE_POWER_OUT: strcpy(np->pin_type, "power_out"); break;
                                case PIN_TYPE_OPEN_COLLECTOR: strcpy(np->pin_type, "open_collector"); break;
                                default: strcpy(np->pin_type, "unspecified"); break;
                                }
                                break;
                            }
                        }
                    }
                }
            }
        }
    }
    nl->num_components = sch->num_components;
    if (nl->num_components > 0) {
        nl->component_refs = calloc(nl->num_components, sizeof(char*));
        if (nl->component_refs) {
            for (int i = 0; i < sch->num_components; i++)
                nl->component_refs[i] = strdup(sch->components[i].reference);
        }
    }
    return nl;
}

void netlist_free(netlist_data_t *nl)
{
    if (!nl) return;
    if (nl->nets) {
        for (int i = 0; i < nl->num_nets; i++) free(nl->nets[i].pins);
        free(nl->nets);
    }
    if (nl->component_refs) {
        for (int i = 0; i < nl->num_components; i++) free(nl->component_refs[i]);
        free(nl->component_refs);
    }
    free(nl);
}

int netlist_total_connections(const netlist_data_t *nl)
{
    if (!nl) return 0;
    int total = 0;
    for (int i = 0; i < nl->num_nets; i++) total += nl->nets[i].num_pins;
    return total;
}

int netlist_find_net(const netlist_data_t *nl, const char *name)
{
    if (!nl || !name) return -1;
    for (int i = 0; i < nl->num_nets; i++) {
        if (strcmp(nl->nets[i].net_name, name) == 0) return i;
    }
    return -1;
}

int netlist_get_net_pins(const netlist_data_t *nl, int net_idx,
                          struct netlist_pin *pins, int max_pins)
{
    if (!nl || !pins || net_idx < 0 || net_idx >= nl->num_nets) return -1;
    int count = nl->nets[net_idx].num_pins;
    if (count > max_pins) count = max_pins;
    for (int i = 0; i < count; i++) pins[i] = nl->nets[net_idx].pins[i];
    return count;
}

void netlist_generate_net_name(char *buf, size_t bufsz,
                                const char *ref, const char *pin)
{
    if (!buf || bufsz == 0) return;
    snprintf(buf, bufsz, "Net-(%s-Pad%s)", ref ? ref : "?", pin ? pin : "?");
}

/* ----------- Format Dispatcher ----------- */

int netlist_write(const netlist_data_t *nl, netlist_format_t fmt,
                  const char *filename)
{
    if (!nl || !filename) return -1;
    FILE *fp = fopen(filename, "w");
    if (!fp) return -1;
    int ret = 0;
    switch (fmt) {
    case NETLIST_FMT_KICAD:   ret = netlist_write_kicad(nl, fp); break;
    case NETLIST_FMT_SPICE:   ret = netlist_write_spice(nl, fp); break;
    case NETLIST_FMT_IPC_D_356: ret = netlist_write_ipc_d_356(nl, fp); break;
    case NETLIST_FMT_PADS:    ret = netlist_write_pads(nl, fp); break;
    case NETLIST_FMT_ALLEGRO: ret = netlist_write_allegro(nl, fp); break;
    case NETLIST_FMT_EDIF:    ret = netlist_write_edif(nl, fp); break;
    default: ret = -1; break;
    }
    fclose(fp);
    return ret;
}

/* ----------- SPICE Netlist Writer (L5) ----------- */

int netlist_write_spice(const netlist_data_t *nl, FILE *fp)
{
    if (!nl || !fp) return -1;
    fprintf(fp, "* SPICE netlist generated by mini-schematic-kicad\n");
    fprintf(fp, "* Design: %s\n", nl->design_name);
    fprintf(fp, "* Date: %s\n\n", nl->timestamp);

    /* Write component connectivity for traceability */
    fprintf(fp, "* --- Component Connectivity ---\n");
    for (int i = 0; i < nl->num_nets; i++) {
        netlist_net_t *nnet = &nl->nets[i];
        if (nnet->num_pins > 1) {
            fprintf(fp, "* Net %s: ", nnet->net_name);
            for (int j = 0; j < nnet->num_pins; j++)
                fprintf(fp, "%s.%s ", nnet->pins[j].ref_des, nnet->pins[j].pin_number);
            fprintf(fp, "\n");
        }
    }

    /* Write default power supplies */
    fprintf(fp, "\n* --- Power Supplies (default) ---\n");
    fprintf(fp, "V_VCC VCC 0 DC 5.0\n");
    fprintf(fp, "V_VDD VDD 0 DC 3.3\n");
    fprintf(fp, "V_GND 0 GND DC 0.0\n");

    /* Net aliases for simulation reference */
    fprintf(fp, "\n* --- Net Aliases ---\n");
    int node_counter = 1;
    for (int i = 0; i < nl->num_nets; i++) {
        netlist_net_t *nnet = &nl->nets[i];
        if (strcmp(nnet->net_name, "VCC") == 0 ||
            strcmp(nnet->net_name, "VDD") == 0 ||
            strcmp(nnet->net_name, "GND") == 0) continue;
        fprintf(fp, "* .alias N%03d %s\n", node_counter++, nnet->net_name);
    }

    /* Write component instances based on reference designator prefixes */
    fprintf(fp, "\n* --- Component Instances ---\n");
    for (int i = 0; i < nl->num_components; i++) {
        const char *ref = nl->component_refs[i];
        if (!ref) continue;
        char prefix = ref[0];
        char net1[128] = "0", net2[128] = "0", net3[128] = "0";
        int net_count = 0;
        for (int j = 0; j < nl->num_nets && net_count < 3; j++) {
            for (int k = 0; k < nl->nets[j].num_pins; k++) {
                if (strcmp(nl->nets[j].pins[k].ref_des, ref) == 0) {
                    switch (net_count) {
                        case 0: snprintf(net1, sizeof(net1), "%s", nl->nets[j].net_name); break;
                        case 1: snprintf(net2, sizeof(net2), "%s", nl->nets[j].net_name); break;
                        case 2: snprintf(net3, sizeof(net3), "%s", nl->nets[j].net_name); break;
                    }
                    net_count++; break;
                }
            }
        }
        switch (prefix) {
        case 'R': fprintf(fp, "%s %s %s 10k\n", ref, net1, net2); break;
        case 'C': fprintf(fp, "%s %s %s 100n\n", ref, net1, net2); break;
        case 'L': fprintf(fp, "%s %s %s 10u\n", ref, net1, net2); break;
        case 'D': fprintf(fp, "%s %s %s 1N4148\n", ref, net1, net2); break;
        case 'Q': fprintf(fp, "%s %s %s %s 2N3904\n", ref, net1, net2, net3); break;
        case 'U':
            fprintf(fp, "X%s ", ref);
            for (int j = 0; j < nl->num_nets; j++)
                for (int k = 0; k < nl->nets[j].num_pins; k++)
                    if (strcmp(nl->nets[j].pins[k].ref_des, ref) == 0)
                        fprintf(fp, "%s ", nl->nets[j].net_name);
            fprintf(fp, "GENERIC_IC\n"); break;
        default: fprintf(fp, "* %s (unrecognized type '%c')\n", ref, prefix); break;
        }
    }
    fprintf(fp, "\n* --- Simulation Control ---\n");
    fprintf(fp, ".OPTIONS NOMOD NOPAGE NOECHO\n");
    fprintf(fp, ".TEMP 27\n");
    fprintf(fp, ".OP\n");
    fprintf(fp, ".END\n");
    return 0;
}

/* ----------- IPC-D-356 Netlist Writer (L5) ----------- */

int netlist_write_ipc_d_356(const netlist_data_t *nl, FILE *fp)
{
    if (!nl || !fp) return -1;

    /* IPC-D-356A Header Record */
    fprintf(fp, "C  IPC-D-356A Netlist\n");
    fprintf(fp, "C  Design: %s\n", nl->design_name);
    fprintf(fp, "C  Date: %s\n", nl->timestamp);
    fprintf(fp, "C  Units: MILS\n");

    /* P - Parameter record: board description */
    fprintf(fp, "P  JOB %s\n", nl->design_name);

    /* Component records (317 - Component definition)
     * Format: 317 comp_type comp_name x y rot side */
    fprintf(fp, "C  --- Component Definitions ---\n");
    for (int i = 0; i < nl->num_components; i++) {
        fprintf(fp, "317IC  %-20s 0 0 0 TOP\n", nl->component_refs[i]);
    }

    /* Net records (327 - Netpad, 317 - Net definition)
     * Format: 327 net_name node_name ref_des pin_num location */
    fprintf(fp, "C  --- Net Definitions ---\n");
    for (int i = 0; i < nl->num_nets; i++) {
        netlist_net_t *nnet = &nl->nets[i];
        if (nnet->num_pins < 2) {
            fprintf(fp, "S  %-20s %d\n", nnet->net_name, nnet->num_pins);
        }
        for (int j = 0; j < nnet->num_pins; j++) {
            fprintf(fp, "327%-20s %-8s %-4s 0 0 0\n",
                    nnet->net_name,
                    nnet->pins[j].ref_des,
                    nnet->pins[j].pin_number);
        }
    }

    /* End-of-file marker */
    fprintf(fp, "999\n");
    return 0;
}

/* ----------- KiCad Native S-Expression Netlist Writer (L5) ----------- */

int netlist_write_kicad(const netlist_data_t *nl, FILE *fp)
{
    if (!nl || !fp) return -1;

    fprintf(fp, "(export (version D)\n");
    fprintf(fp, "  (design\n");
    fprintf(fp, "    (source \"%s\")\n", nl->tool_info);
    fprintf(fp, "    (date \"%s\")\n", nl->timestamp);
    fprintf(fp, "    (tool \"mini-schematic-kicad\")\n");
    fprintf(fp, "  )\n");

    /* Components section */
    fprintf(fp, "  (components\n");
    for (int i = 0; i < nl->num_components; i++) {
        fprintf(fp, "    (comp (ref %s)\n", nl->component_refs[i]);
        fprintf(fp, "      (value \"UNKNOWN\")\n");
        fprintf(fp, "      (footprint \"\")\n");
        fprintf(fp, "    )\n");
    }
    fprintf(fp, "  )\n");

    /* Nets section */
    fprintf(fp, "  (nets\n");
    for (int i = 0; i < nl->num_nets; i++) {
        netlist_net_t *nnet = &nl->nets[i];
        fprintf(fp, "    (net (code %d) (name \"%s\")\n",
                nnet->net_number, nnet->net_name);
        for (int j = 0; j < nnet->num_pins; j++) {
            fprintf(fp, "      (node (ref %s) (pin \"%s\"))\n",
                    nnet->pins[j].ref_des,
                    nnet->pins[j].pin_number);
        }
        fprintf(fp, "    )\n");
    }
    fprintf(fp, "  )\n");
    fprintf(fp, ")\n");
    return 0;
}

/* ----------- PADS ASCII Netlist Writer (L5) ----------- */

int netlist_write_pads(const netlist_data_t *nl, FILE *fp)
{
    if (!nl || !fp) return -1;

    /* PADS ASCII header: starts with *PADS* magic */
    fprintf(fp, "*PADS*\n");
    fprintf(fp, "*PCB* *NETLIST*\n\n");

    /* Net definitions: *NET* netname */
    for (int i = 0; i < nl->num_nets; i++) {
        netlist_net_t *nnet = &nl->nets[i];
        fprintf(fp, "*NET* %s\n", nnet->net_name);
        for (int j = 0; j < nnet->num_pins; j++) {
            fprintf(fp, " %s.%s\n",
                    nnet->pins[j].ref_des,
                    nnet->pins[j].pin_number);
        }
        fprintf(fp, "\n");
    }
    fprintf(fp, "*END*\n");
    return 0;
}

/* ----------- Cadence Allegro Netlist Writer (L5) ----------- */

int netlist_write_allegro(const netlist_data_t *nl, FILE *fp)
{
    if (!nl || !fp) return -1;

    fprintf(fp, "$PACKAGES\n");
    for (int i = 0; i < nl->num_components; i++) {
        fprintf(fp, "%s ! UNKNOWN ! ; No value\n", nl->component_refs[i]);
    }

    fprintf(fp, "$NETS\n");
    for (int i = 0; i < nl->num_nets; i++) {
        netlist_net_t *nnet = &nl->nets[i];
        fprintf(fp, "%s\n", nnet->net_name);
        for (int j = 0; j < nnet->num_pins; j++) {
            fprintf(fp, " %s.%s\n",
                    nnet->pins[j].ref_des,
                    nnet->pins[j].pin_number);
        }
    }

    fprintf(fp, "$END\n");
    return 0;
}

/* ----------- EDIF 2.0.0 Netlist Writer (L5) ----------- */

int netlist_write_edif(const netlist_data_t *nl, FILE *fp)
{
    if (!nl || !fp) return -1;

    /* EDIF 2.0.0 is Lisp-like format used in EDA interchange */
    fprintf(fp, "(edif \"mini-schematic-kicad\"\n");
    fprintf(fp, "  (edifVersion 2 0 0)\n");
    fprintf(fp, "  (edifLevel 0)\n");
    fprintf(fp, "  (keywordMap (keywordLevel 0))\n");
    fprintf(fp, "  (status\n");
    fprintf(fp, "    (written\n");
    fprintf(fp, "      (timeStamp %s)\n", nl->timestamp);
    fprintf(fp, "    )\n");
    fprintf(fp, "  )\n");
    fprintf(fp, "  (library \"%s\"\n", nl->design_name);
    fprintf(fp, "    (edifLevel 0)\n");
    fprintf(fp, "    (technology (numberDefinition))\n");
    fprintf(fp, "    (cell \"TOP\"\n");
    fprintf(fp, "      (cellType GENERIC)\n");
    fprintf(fp, "      (view \"schematic\"\n");
    fprintf(fp, "        (viewType NETLIST)\n");

    /* Instantiate all components */
    fprintf(fp, "        (interface\n");
    for (int i = 0; i < nl->num_nets; i++) {
        fprintf(fp, "          (port \"%s\" (direction INOUT))\n",
                nl->nets[i].net_name);
    }
    fprintf(fp, "        )\n");

    /* Net connectivity using joined pins */
    fprintf(fp, "        (contents\n");
    for (int i = 0; i < nl->num_nets; i++) {
        netlist_net_t *nnet = &nl->nets[i];
        fprintf(fp, "          (net \"%s\"\n", nnet->net_name);
        fprintf(fp, "            (joined\n");
        for (int j = 0; j < nnet->num_pins; j++) {
            fprintf(fp, "              (portRef \"%s_%s\")\n",
                    nnet->pins[j].ref_des,
                    nnet->pins[j].pin_number);
        }
        fprintf(fp, "            )\n");
        fprintf(fp, "          )\n");
    }
    fprintf(fp, "        )\n");
    fprintf(fp, "      )\n");
    fprintf(fp, "    )\n");
    fprintf(fp, "  )\n");
    fprintf(fp, ")\n");
    return 0;
}

/* ----------- SPICE Parse / MNA Matrix (L3-L4-L5) ----------- */

/**
 * @brief Parse single SPICE component line
 *
 * Recognizes SPICE 3f5 format lines:
 *   Rname n+ n- value    Cname n+ n- value    Lname n+ n- value
 *   Dname n+ n- model    Qname nc nb ne model Mname nd ng ns model
 *   Vname n+ n- value    Iname n+ n- value
 *
 * @return Component type letter, or 0 on parse failure.
 */
char spice_parse_component(const char *line,
                           char *ref, size_t refsz,
                           char *nodes, int max_nodes,
                           char *value, size_t valsz)
{
    if (!line || !ref || !nodes || !value) return 0;

    /* Skip leading whitespace */
    while (*line == ' ' || *line == '\t') line++;
    if (*line == '\0' || *line == '*' || *line == '\n') return 0;

    /* First character is component type */
    char ctype = *line;
    if (ctype >= 'a' && ctype <= 'z') ctype = ctype - 'a' + 'A';

    /* Must be a known SPICE component type */
    const char *valid = "RCVDIQMJLXT";
    bool found = false;
    for (const char *p = valid; *p; p++) {
        if (*p == ctype) { found = true; break; }
    }
    if (!found) return 0;

    /* Tokenize: component_name node1 node2 [node3...] value */
    const char *p = line;
    while (*p && *p != ' ' && *p != '\t') { p++; }

    /* Extract reference name between line+1 and p */
    size_t reflen = (size_t)(p - line);
    if (reflen >= refsz) reflen = refsz - 1;
    if (refsz > 0) {
        memcpy(ref, line, reflen);
        ref[reflen] = '\0';
    }

    /* Skip whitespace, then read node names */
    int ncount = 0;
    while (*p && ncount < max_nodes) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '\n') break;

        const char *nstart = p;
        while (*p && *p != ' ' && *p != '\t' && *p != '\n') p++;
        size_t nlen = (size_t)(p - nstart);
        if (nlen > 0) {
            size_t copylen = (nlen < 31U) ? nlen : 31U;
            memcpy(&nodes[ncount * 32], nstart, copylen);
            nodes[ncount * 32 + copylen] = '\0';
            ncount++;
        }
        if (*p == '\n' || *p == '\0') break;
    }

    /* Last token is the value/model */
    while (*p == ' ' || *p == '\t') p++;
    if (*p && *p != '\n') {
        const char *vstart = p;
        while (*p && *p != '\n' && *p != '\r') p++;
        size_t vlen = (size_t)(p - vstart);
        if (vlen >= valsz) vlen = valsz - 1;
        if (valsz > 0) {
            memcpy(value, vstart, vlen);
            value[vlen] = '\0';
        }
    } else {
        if (valsz > 0) value[0] = '\0';
    }

    return ctype;
}

/**
 * @brief Build Modified Nodal Analysis (MNA) matrix from netlist
 *
 * L4: Kirchhoff's Current Law �� sum of currents at each node = 0
 *     MNA formulates circuit as [G B; C D][v; w] = [i; e]
 *     G = conductance matrix, B = voltage source incidence,
 *     v = node voltages, w = branch currents.
 *
 * For a topology-only netlist, builds placeholder matrix with
 * unit conductance per net and strong ground reference.
 */
int spice_build_mna_matrix(const netlist_data_t *nl,
                            double **A, double **b, int *n)
{
    if (!nl || !A || !b || !n) return -1;

    /* Collect unique net names as nodes */
    int node_count = 0;
    char (*node_names)[128] = NULL;

    for (int i = 0; i < nl->num_nets; i++) {
        netlist_net_t *nnet = &nl->nets[i];
        if (nnet->num_pins > 0) {
            bool exists = false;
            for (int j = 0; j < node_count; j++) {
                if (strcmp(node_names[j], nnet->net_name) == 0) {
                    exists = true; break;
                }
            }
            if (!exists) {
                void *tmp = realloc(node_names,
                    (node_count + 1) * sizeof(char[128]));
                if (!tmp) { free(node_names); return -1; }
                node_names = tmp;
                strncpy(node_names[node_count], nnet->net_name, 127);
                node_names[node_count][127] = '\0';
                node_count++;
            }
        }
    }

    if (node_count == 0) { *n = 0; return 0; }

    /* Build net-to-node index map */
    int *net_to_node = calloc(nl->num_nets, sizeof(int));
    if (!net_to_node) { free(node_names); return -1; }
    for (int i = 0; i < nl->num_nets; i++) {
        net_to_node[i] = -1;
        for (int j = 0; j < node_count; j++) {
            if (strcmp(nl->nets[i].net_name, node_names[j]) == 0) {
                net_to_node[i] = j; break;
            }
        }
    }

    /* Allocate MNA matrices: A(n��n), b(n) */
    *n = node_count;
    *A = calloc(node_count * node_count, sizeof(double));
    *b = calloc(node_count, sizeof(double));
    if (!*A || !*b) {
        free(*A); free(*b); *A = NULL; *b = NULL;
        free(net_to_node); free(node_names);
        *n = -1; return -1;
    }

    /* Fill conductance matrix: each net contributes unit conductance */
    for (int i = 0; i < nl->num_nets; i++) {
        (void)nl->nets[i]; /* referenced for clarity */
        int node = net_to_node[i];
        if (node >= 0 && node < node_count) {
            /* Self-conductance to ground */
            (*A)[node * node_count + node] += 1.0;
        }
    }

    /* Strong ground reference at node 0 */
    if (node_count > 0) {
        (*A)[0 * node_count + 0] += 1e12;
        (*b)[0] = 0.0;
    }

    free(node_names);
    free(net_to_node);
    return 0;
}

/**
 * @brief Solve MNA linear system Ax = b via Gaussian elimination
 *
 * L5: Gaussian Elimination with partial pivoting.
 * Time: O(n^3), Space: O(n^2) in-place on augmented matrix.
 *
 * Reference: Golub & Van Loan, "Matrix Computations", 4th ed. (2013)
 *            Algorithm 3.4.1 (GEPP)
 */
int spice_solve_dc_op(double *A, double *b, double *x, int n)
{
    if (!A || !b || !x || n <= 0) return -1;

    /* Build augmented matrix [A|b] in dense storage */
    double *aug = calloc((size_t)n * (n + 1), sizeof(double));
    if (!aug) return -1;

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            aug[i * (n + 1) + j] = A[i * n + j];
        }
        aug[i * (n + 1) + n] = b[i];
    }

    /* Forward elimination with partial pivoting */
    for (int col = 0; col < n; col++) {
        /* Find pivot row */
        int pivot_row = col;
        double max_val = fabs(aug[col * (n + 1) + col]);
        for (int row = col + 1; row < n; row++) {
            double val = fabs(aug[row * (n + 1) + col]);
            if (val > max_val) { max_val = val; pivot_row = row; }
        }

        /* Singularity check */
        if (max_val < 1e-15) { free(aug); return -1; }

        /* Swap rows */
        if (pivot_row != col) {
            for (int j = 0; j <= n; j++) {
                double tmp = aug[col * (n + 1) + j];
                aug[col * (n + 1) + j] = aug[pivot_row * (n + 1) + j];
                aug[pivot_row * (n + 1) + j] = tmp;
            }
        }

        /* Eliminate rows below */
        double pivot = aug[col * (n + 1) + col];
        for (int row = col + 1; row < n; row++) {
            double factor = aug[row * (n + 1) + col] / pivot;
            for (int j = col; j <= n; j++) {
                aug[row * (n + 1) + j] -= factor * aug[col * (n + 1) + j];
            }
        }
    }

    /* Back substitution */
    for (int i = n - 1; i >= 0; i--) {
        double sum = aug[i * (n + 1) + n];
        for (int j = i + 1; j < n; j++) {
            sum -= aug[i * (n + 1) + j] * x[j];
        }
        double pivot = aug[i * (n + 1) + i];
        if (fabs(pivot) < 1e-15) { free(aug); return -1; }
        x[i] = sum / pivot;
    }

    free(aug);
    return 0;
}