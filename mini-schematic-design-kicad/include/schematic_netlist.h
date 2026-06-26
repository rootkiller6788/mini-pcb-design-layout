/**
 * @file schematic_netlist.h
 * @brief Netlist extraction and interchange format support
 *
 * Implements netlist generation from schematic designs supporting
 * multiple industry-standard netlist formats:
 *   - KiCad native netlist (S-expression)
 *   - SPICE netlist (.cir)
 *   - IPC-D-356 (bare-board electrical test)
 *   - EDIF 2.0.0 (Electronic Design Interchange Format)
 *   - PADS ASCII netlist
 *   - Cadence Allegro (telesis) netlist
 *
 * Course Mapping:
 *   - MIT 6.002 — nodal analysis from netlist
 *   - Berkeley EE105 — SPICE netlist circuit description
 *   - Stanford EE313 — digital system connectivity
 *
 * Reference:
 *   - SPICE User Guide, Nagel & Pederson (1973)
 *   - IPC-D-356A Bare Board Electrical Test Data Format
 *   - EDIF Version 2.0.0, ANSI/EIA-548
 */

#ifndef SCHEMATIC_NETLIST_H
#define SCHEMATIC_NETLIST_H

#include "schematic_core.h"
#include <stdio.h>

/* ──────────── Netlist Format Enumeration ──────────── */

typedef enum {
    NETLIST_FMT_KICAD   = 0,  /* KiCad S-expression netlist */
    NETLIST_FMT_SPICE   = 1,  /* SPICE circuit netlist */
    NETLIST_FMT_IPC_D_356 = 2, /* IPC-D-356 netlist */
    NETLIST_FMT_PADS    = 3,  /* Mentor PADS ASCII */
    NETLIST_FMT_ALLEGRO = 4,  /* Cadence Allegro */
    NETLIST_FMT_EDIF    = 5   /* EDIF 2.0.0 */
} netlist_format_t;

/* ──────────── L1: Netlist Core Structures ──────────── */

/**
 * @brief Single netlist entry — one electrical node
 *
 * In netlist terminology, a "net" is an equipotential node
 * connecting component terminals. This structure corresponds
 * to Kirchhoff's Current Law: the sum of currents at each
 * net is zero.
 */
typedef struct {
    char     net_name[128];      /* Net name (e.g., "Net-(R1-Pad1)", "VCC") */
    int      net_number;         /* Sequential net number */
    int      num_pins;           /* Number of connected pins */
    struct netlist_pin {
        char ref_des[16];        /* Component reference designator */
        char pin_number[8];      /* Pin/terminal number */
        char pin_name[32];       /* Pin function name */
        char pin_type[16];       /* Pin electrical type */
    } *pins;                     /* Connected pin list */
} netlist_net_t;

/**
 * @brief Complete netlist — all nets in the design
 */
typedef struct {
    int      num_nets;           /* Total net count */
    netlist_net_t *nets;         /* Net array */
    int      num_components;     /* Component count in netlist */
    char   **component_refs;     /* Component reference list */
    char     design_name[128];   /* Design name */
    char     timestamp[32];      /* Generation timestamp */
    char     tool_info[64];      /* Tool name and version */
} netlist_data_t;

/* ──────────── SPICE-specific Types ──────────── */

/** SPICE component types */
typedef enum {
    SPICE_RESISTOR    = 'R',
    SPICE_CAPACITOR   = 'C',
    SPICE_INDUCTOR    = 'L',
    SPICE_DIODE       = 'D',
    SPICE_BJT_NPN      = 'Q',
    SPICE_MOSFET_NMOS  = 'M',
    SPICE_JFET_NCH     = 'J',
    SPICE_VSOURCE     = 'V',
    SPICE_ISOURCE     = 'I',
    SPICE_SUBCKT      = 'X',
    SPICE_TRANSMISSION = 'T'
} spice_component_type_t;

/** SPICE analysis types */
typedef enum {
    SPICE_ANALYSIS_OP    = 0,  /* .OP — DC operating point */
    SPICE_ANALYSIS_DC    = 1,  /* .DC — DC sweep */
    SPICE_ANALYSIS_AC    = 2,  /* .AC — AC frequency sweep */
    SPICE_ANALYSIS_TRAN  = 3,  /* .TRAN — transient analysis */
    SPICE_ANALYSIS_NOISE = 4,  /* .NOISE — noise analysis */
    SPICE_ANALYSIS_TF    = 5,  /* .TF — transfer function */
    SPICE_ANALYSIS_PZ    = 6   /* .PZ — pole-zero analysis */
} spice_analysis_type_t;

/**
 * @brief SPICE simulation control card
 */
typedef struct {
    spice_analysis_type_t analysis;
    union {
        struct { double start, stop, step; } dc;
        struct { int num_pts; double fstart, fstop; char scale[8]; } ac;
        struct { double tstep, tstop, tstart; double tmax; } tran;
        struct { char output[32]; char src[16]; } noise;
        struct { char out_var[32]; char in_src[16]; } tf;
    } params;
} spice_simulation_t;

/* ──────────── Netlist API ──────────── */

/** Extract netlist from schematic design */
netlist_data_t* netlist_extract(const schematic_design_t *sch);
void netlist_free(netlist_data_t *nl);

/** Count total connections across all nets */
int netlist_total_connections(const netlist_data_t *nl);

/** Get net by name, returns index or -1 */
int netlist_find_net(const netlist_data_t *nl, const char *name);

/** Get all pins connected to a given net */
int netlist_get_net_pins(const netlist_data_t *nl, int net_idx,
                          struct netlist_pin *pins, int max_pins);

/* ──────────── L5: Netlist Format Writers ──────────── */

/** Write netlist to file in specified format */
int netlist_write(const netlist_data_t *nl, netlist_format_t fmt,
                  const char *filename);

/** Write SPICE format netlist
 *  Produces standard SPICE 3f5-compatible netlist with .SUBCKT/.ENDS
 *  for hierarchical designs */
int netlist_write_spice(const netlist_data_t *nl, FILE *fp);

/** Write IPC-D-356 format netlist */
int netlist_write_ipc_d_356(const netlist_data_t *nl, FILE *fp);

/** Write KiCad native S-expression netlist */
int netlist_write_kicad(const netlist_data_t *nl, FILE *fp);

/** Write Mentor PADS ASCII netlist */
int netlist_write_pads(const netlist_data_t *nl, FILE *fp);

/** Write Cadence Allegro netlist */
int netlist_write_allegro(const netlist_data_t *nl, FILE *fp);

/** Write EDIF 2.0.0 netlist */
int netlist_write_edif(const netlist_data_t *nl, FILE *fp);

/** Generate net name from reference + pin */
void netlist_generate_net_name(char *buf, size_t bufsz,
                                const char *ref, const char *pin);

/* ──────────── L3: SPICE Netlist Algebra ──────────── */

/**
 * @brief Parse SPICE netlist line into component structure
 *
 * Matches patterns like: R1 n1 n2 10k
 *                         C1 n3 n4 100n
 *                         Q1 nc nb ne 2N3904
 *
 * @return Component type character, or 0 on failure
 */
char spice_parse_component(const char *line,
                           char *ref, size_t refsz,
                           char *nodes, int max_nodes,
                           char *value, size_t valsz);

/** Build a modified nodal analysis (MNA) matrix from SPICE netlist */
int spice_build_mna_matrix(const netlist_data_t *nl,
                            double **A, double **b, int *n);

/** Solve MNA linear system for DC operating point (Gaussian elimination) */
int spice_solve_dc_op(double *A, double *b, double *x, int n);

#endif /* SCHEMATIC_NETLIST_H */