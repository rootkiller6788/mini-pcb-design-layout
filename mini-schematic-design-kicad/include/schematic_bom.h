/**
 * @file schematic_bom.h
 * @brief Bill of Materials (BOM) generation from schematic
 *
 * Generates BOM reports from schematic component data, supporting
 * multiple output formats and aggregation strategies. Handles
 * component grouping, quantity summation, part number matching,
 * and procurement-ready CSV/Excel/HTML output.
 *
 * BOM Aggregation Strategies (L5 Algorithms):
 *   1. Same value + same footprint → group
 *   2. Same manufacturer part number (MPN) → group
 *   3. Same value only → group (loose)
 *   4. Per-sheet → separate groups
 *   5. By component type → group
 *
 * Course Mapping:
 *   - Stanford EE272 — Design for Manufacturing
 *   - Michigan EECS 411 — Microwave component selection
 *   - Georgia Tech ECE 6350 — System integration BOM
 *   - TU Munich — Automotive BOM management
 *
 * Reference: IPC-2581, ISO 10303-210 (STEP AP210)
 */

#ifndef SCHEMATIC_BOM_H
#define SCHEMATIC_BOM_H

#include "schematic_core.h"
#include <stdio.h>

/* ──────────── L1: BOM Structures ──────────── */

/**
 * @brief Single BOM line item — one component group
 */
typedef struct {
    char     value[128];         /* Component value (e.g., "10k 1% 0805") */
    char     footprint[64];      /* PCB footprint (e.g., "Resistor_SMD:R_0805") */
    char     manufacturer[64];   /* Manufacturer name */
    char     manufacturer_pn[64]; /* Manufacturer part number (MPN) */
    char     supplier[64];       /* Distributor (e.g., DigiKey, Mouser) */
    char     supplier_pn[64];    /* Supplier orderable part number */
    char     description[256];   /* Human-readable description */
    int      quantity;           /* Number of units needed */
    char     designators[1024];  /* Comma-separated reference designators */
    double   unit_cost;          /* Per-unit cost estimate */
    bool     is_critical;        /* Long-lead or safety-critical flag */
    bool     is_DNP;             /* Do Not Populate flag */
    int      sheet_number;       /* Originating schematic sheet */
} bom_line_item_t;

/**
 * @brief Complete BOM report
 */
typedef struct {
    char     project_name[128];
    char     revision[16];
    char     date[32];
    char     prepared_by[64];
    char     approved_by[64];
    int      num_items;
    bom_line_item_t *items;
    double   total_cost;         /* Sum of unit_cost * quantity */
    int      total_components;   /* Sum of all quantities */
    int      unique_parts;       /* Number of distinct part types */
} bom_report_t;

/* ──────────── BOM Grouping Strategies ──────────── */

typedef enum {
    BOM_GROUP_VALUE_FOOTPRINT = 0,  /* Same value + footprint → group */
    BOM_GROUP_MPN             = 1,  /* Same manufacturer PN → group */
    BOM_GROUP_VALUE_ONLY      = 2,  /* Same value only → group */
    BOM_GROUP_SHEET           = 3,  /* Group by schematic sheet */
    BOM_GROUP_TYPE            = 4,  /* Group by component type prefix */
    BOM_GROUP_NO_GROUP        = 5   /* One line per component */
} bom_grouping_strategy_t;

/* ──────────── BOM Output Formats ──────────── */

typedef enum {
    BOM_FMT_CSV     = 0,  /* Comma-separated values */
    BOM_FMT_TSV     = 1,  /* Tab-separated values */
    BOM_FMT_HTML    = 2,  /* HTML table */
    BOM_FMT_MARKDOWN = 3, /* Markdown table */
    BOM_FMT_KICAD   = 4,  /* KiCad native XML BOM */
    BOM_FMT_JSON    = 5   /* JSON format */
} bom_output_format_t;

/* ──────────── L1: Component Classification ──────────── */

/** Standard EIA component categories */
typedef enum {
    CAT_RESISTOR        = 0,
    CAT_CAPACITOR       = 1,
    CAT_INDUCTOR        = 2,
    CAT_DIODE           = 3,
    CAT_TRANSISTOR      = 4,
    CAT_IC_ANALOG       = 5,
    CAT_IC_DIGITAL      = 6,
    CAT_IC_MIXED        = 7,
    CAT_CONNECTOR       = 8,
    CAT_SWITCH          = 9,
    CAT_CRYSTAL         = 10,
    CAT_FUSE            = 11,
    CAT_TRANSFORMER     = 12,
    CAT_RELAY           = 13,
    CAT_OPTOCOUPLER     = 14,
    CAT_SENSOR          = 15,
    CAT_MECHANICAL      = 16,
    CAT_TEST_POINT      = 17,
    CAT_JUMPER          = 18,
    CAT_UNKNOWN         = 19
} component_category_t;

/** Classify component by reference prefix */
component_category_t bom_classify_component(const char *reference);

/** Get category name string */
const char* bom_category_name(component_category_t cat);

/* ──────────── BOM Core API ──────────── */

/** Generate BOM from schematic design */
bom_report_t* bom_generate(const schematic_design_t *sch,
                            bom_grouping_strategy_t strategy);
void bom_report_free(bom_report_t *bom);

/** Sort BOM items by reference designator */
void bom_sort_by_designator(bom_report_t *bom);

/** Sort BOM items by value alphabetically */
void bom_sort_by_value(bom_report_t *bom);

/** Sort BOM items by quantity descending */
void bom_sort_by_quantity(bom_report_t *bom);

/** Merge multiple BOM reports (e.g., from multi-sheet flat hierarchy) */
bom_report_t* bom_merge(bom_report_t **boms, int num_boms);

/** Compare two BOM reports for equivalence (same items, quantities) */
bool bom_compare(const bom_report_t *a, const bom_report_t *b,
                  double tolerance);

/** Calculate total cost from unit costs and quantities */
double bom_calculate_total_cost(const bom_report_t *bom);

/** Export BOM to file in specified format */
int bom_export(const bom_report_t *bom, bom_output_format_t fmt,
               const char *filename);

/** Export BOM as CSV */
int bom_export_csv(const bom_report_t *bom, FILE *fp);

/** Export BOM as HTML table */
int bom_export_html(const bom_report_t *bom, FILE *fp);

/** Export BOM as Markdown table */
int bom_export_markdown(const bom_report_t *bom, FILE *fp);

/** Export BOM as JSON */
int bom_export_json(const bom_report_t *bom, FILE *fp);

/** Export KiCad native XML BOM format */
int bom_export_kicad_xml(const bom_report_t *bom, FILE *fp);

/* ──────────── L5: Advanced BOM Operations ──────────── */

/**
 * @brief Find cheapest supplier for each BOM item
 *
 * Compares prices across a database of suppliers and selects
 * the optimal combination minimizing total procurement cost.
 * Uses greedy heuristic with fallback for O(n log n) performance.
 */
int bom_optimize_suppliers(bom_report_t *bom,
                            const char *supplier_list[],
                            const double *price_matrix[],
                            int num_suppliers);

/**
 * @brief Estimate assembly cost based on component count and type
 *
 * Uses IPC-2221 derived cost estimation model:
 *   C = N * (c_placement + c_solder) + C_setup
 */
double bom_estimate_assembly_cost(const bom_report_t *bom,
                                   double cost_per_smd,
                                   double cost_per_tht,
                                   double setup_cost);

/** Identify single-source components (supply chain risk) */
int bom_identify_risk_items(const bom_report_t *bom,
                             bom_line_item_t **risk_items);

/** Check for obsolete or end-of-life parts from a known list */
int bom_check_obsolescence(const bom_report_t *bom,
                            const char *eol_parts[],
                            int num_eol_parts);

#endif /* SCHEMATIC_BOM_H */