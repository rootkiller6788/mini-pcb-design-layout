/**
 * @file schematic_erc.h
 * @brief Electrical Rules Check (ERC) for schematic verification
 *
 * Implements DRC-style electrical rule checking for schematics.
 * ERC detects common design errors before PCB layout:
 *   - Single-pin nets (floating nodes)
 *   - Conflicting output drivers on one net
 *   - Power pins left unconnected
 *   - Input pins with no driving source
 *   - Bi-directional pin conflicts
 *   - Missing power supply connections
 *   - Unconnected hierarchical pins
 *   - Duplicate reference designators
 *   - Pin type mismatches (e.g., output driving output)
 *
 * Pin drive conflict matrix (L6: Classic Problem):
 *   Based on IEEE Std 1164-1993 (VHDL std_logic resolution).
 *   Drive strength and conflict resolution follows the 12-value
 *   logic system: U, X, 0, 1, Z, W, L, H, - with resolution
 *   function modeling real electrical conflicts.
 *
 * Course Mapping:
 *   - MIT 6.004 Computation Structures — digital conflict detection
 *   - Berkeley EE141 — ERC in design flow
 *   - ETH 227-0455 — EMC-aware ERC rules
 *   - TU Munich — automotive ERC standards
 *
 * Reference: KiCad ERC rule set, IEEE 1164, ISO 26262 (automotive)
 */

#ifndef SCHEMATIC_ERC_H
#define SCHEMATIC_ERC_H

#include "schematic_core.h"
#include <stdio.h>

/* ──────────── ERC Severity Levels ──────────── */

typedef enum {
    ERC_INFO     = 0,  /* Informational note */
    ERC_WARNING  = 1,  /* Potential issue, review recommended */
    ERC_ERROR    = 2,  /* Definite error, must fix */
    ERC_FATAL    = 3   /* Fatal error, design will fail */
} erc_severity_t;

/* ──────────── ERC Violation Types ──────────── */

typedef enum {
    ERC_OK                       = 0,
    ERC_VIOL_SINGLE_PIN_NET      = 1,   /* Net has only one pin */
    ERC_VIOL_DRIVER_CONFLICT     = 2,   /* Multiple drivers on one net */
    ERC_VIOL_UNCONNECTED_INPUT   = 3,   /* Input pin floating */
    ERC_VIOL_UNCONNECTED_POWER   = 4,   /* Power pin not connected */
    ERC_VIOL_DUPLICATE_REF       = 5,   /* Duplicate reference */
    ERC_VIOL_PIN_TYPE_MISMATCH   = 6,   /* Incompatible pin types */
    ERC_VIOL_INPUT_POWER_CONFLICT = 7,  /* Input connected only to power */
    ERC_VIOL_OUTPUT_POWER_CONFLICT = 8, /* Output connected to power net */
    ERC_VIOL_NO_DRIVING_SOURCE   = 9,   /* Net has no driver */
    ERC_VIOL_GLOBAL_LABEL_MISSING = 10, /* Global label not found */
    ERC_VIOL_HIER_PIN_UNMATCHED  = 11,  /* Hierarchical pin not connected */
    ERC_VIOL_SHORTED_POWER       = 12,  /* Different power nets shorted */
    ERC_VIOL_UNCONNECTED_SHEET_PIN = 13,/* Sheet pin floating */
    ERC_VIOL_BUS_LABEL_MISMATCH  = 14,  /* Bus width mismatch */
    ERC_VIOL_NOCONNECT_INVALID   = 15   /* No-connect on mandatory pin */
} erc_violation_code_t;

/**
 * @brief Single ERC violation report entry
 */
typedef struct {
    erc_violation_code_t code;
    erc_severity_t       severity;
    char     message[256];       /* Human-readable description */
    char     location_ref[16];   /* Component reference, if applicable */
    char     location_pin[8];    /* Pin number, if applicable */
    char     net_name[128];      /* Affected net name */
    int      line_number;        /* Source file line (if from S-expr parse) */
    double   pos_x, pos_y;       /* Schematic location */
} erc_violation_t;

/**
 * @brief ERC report — collection of all violations
 */
typedef struct {
    int      num_violations;
    erc_violation_t *violations;
    int      num_errors;         /* Count of ERROR and FATAL */
    int      num_warnings;       /* Count of WARNING */
    int      num_info;           /* Count of INFO */
    char     design_name[128];
} erc_report_t;

/* ──────────── L2: Pin Conflict Resolution Matrix ──────────── */

/**
 * @brief 7×7 pin type compatibility matrix
 *
 * Determines whether connecting two pins of given types on a
 * single net is electrically safe. Based on IEEE 1164 resolution
 * function extended for analog/mixed-signal designs.
 *
 * Matrix legend:
 *   0 = OK, no conflict
 *   1 = WARNING, potential issue
 *   2 = ERROR, definite conflict
 */
#define ERC_PIN_OK      0
#define ERC_PIN_WARNING 1
#define ERC_PIN_ERROR   2

/** Initialize the conflict matrix from IEC 60617 pin types */
void erc_pin_conflict_matrix_init(int matrix[12][12]);

/** Check compatibility between two pin types */
int erc_check_pin_compatibility(pin_type_t type_a, pin_type_t type_b);

/** Get human-readable name for pin type */
const char* erc_pin_type_name(pin_type_t pt);

/** Get human-readable severity string */
const char* erc_severity_name(erc_severity_t s);

/** Get human-readable violation name */
const char* erc_violation_name(erc_violation_code_t v);

/* ──────────── ERC Core API ──────────── */

/** Run full ERC on a schematic design */
erc_report_t* erc_run(const schematic_design_t *sch);
void erc_report_free(erc_report_t *report);

/** Check for single-pin nets (floating connections) */
int erc_check_single_pin_nets(const schematic_design_t *sch,
                               erc_report_t *report);

/** Check for driver conflicts on shared nets */
int erc_check_driver_conflicts(const schematic_design_t *sch,
                                erc_report_t *report);

/** Check for unconnected input pins */
int erc_check_unconnected_inputs(const schematic_design_t *sch,
                                  erc_report_t *report);

/** Check for unconnected power pins */
int erc_check_unconnected_power(const schematic_design_t *sch,
                                 erc_report_t *report);

/** Check for duplicate reference designators */
int erc_check_duplicate_refs(const schematic_design_t *sch,
                              erc_report_t *report);

/** Check hierarchical sheet pin connectivity */
int erc_check_hierarchical_pins(const schematic_design_t *sch,
                                 erc_report_t *report);

/** Check for shorted power nets (VCC shorted to GND) */
int erc_check_shorted_power(const schematic_design_t *sch,
                             erc_report_t *report);

/** Check bus width consistency */
int erc_check_bus_widths(const schematic_design_t *sch,
                          erc_report_t *report);

/** Print ERC report in human-readable format */
void erc_report_print(FILE *fp, const erc_report_t *report);

/** Filter ERC report by severity threshold */
erc_report_t* erc_filter_by_severity(const erc_report_t *report,
                                      erc_severity_t min_level);

/** Count violations by type */
int erc_count_by_code(const erc_report_t *report,
                       erc_violation_code_t code);

/** Export ERC report to JSON string */
int erc_report_to_json(const erc_report_t *report, char *buf, size_t bufsz);

/** Check if a pin type is a "driver" (can source current) */
bool erc_is_driver_type(pin_type_t pt);

/** Check if a pin type is a "receiver" (only sinks current) */
bool erc_is_receiver_type(pin_type_t pt);

/** Check if a pin type is a power pin */
bool erc_is_power_type(pin_type_t pt);

#endif /* SCHEMATIC_ERC_H */