/**
 * excellon.h — Excellon Drill/Rout File Format
 *
 * The Excellon format (originally from Excellon Automation) is the
 * de-facto industry standard for PCB drilling and routing data.
 * It specifies tool definitions, drill coordinates, and optional
 * slot/rout paths.
 *
 * Standards referenced:
 *   - Excellon Format Specification (NC Drill)
 *   - IPC-NC-349 Computer Numerical Control Formatting
 *
 * Knowledge map:
 *   L1: Tool codes, drill/hit definitions, slot routing
 *   L2: CNC drilling process — spindle, feed rate, peck drilling
 *   L3: Coordinate transformations for drill positioning
 *   L5: Excellon parsing/generation algorithm
 *   L6: Complete drill file generation with tool sorting
 *
 * Course mapping:
 *   MIT 6.630 -> precision machining for electronics
 *   TU Munich High-Frequency Eng -> via drilling for RF boards
 */

#ifndef EXCELLON_H
#define EXCELLON_H

#include "gerber.h"
#include <stdint.h>
#include <stdio.h>

/* ─── Excellon Tool Definitions ─────────────────────────────────── */

/** Tool type for drill/rout operations */
typedef enum {
    EX_TOOL_DRILL = 0,   /** Standard twist drill bit */
    EX_TOOL_ROUT  = 1,   /** Router/mill bit for slots and outlines */
    EX_TOOL_SLOT  = 2    /** Slot drill (elongated holes) */
} ExcellonToolType;

/** Tool definition (T-code) */
typedef struct {
    int             tool_number;    /** T-code number (T01-T99) */
    double          diameter_mm;    /** Tool diameter in mm */
    ExcellonToolType type;          /** Drill, rout, or slot */
    double          feed_rate;      /** Feed rate (in/min or mm/min) */
    double          spindle_rpm;    /** Spindle speed (RPM) */
    int             peck_enabled;   /** 1 if peck drilling enabled */
    double          peck_depth_mm;  /** Depth per peck cycle */
} ExcellonTool;

/** Maximum number of tools per file */
#define EX_MAX_TOOLS 128

/** Tool table */
typedef struct {
    ExcellonTool tools[EX_MAX_TOOLS];
    int          count;
} ExcellonToolTable;

/* ─── Drill/Rout Operations ─────────────────────────────────────── */

/** Drill hit — a single hole at (x, y) */
typedef struct {
    double x_mm;
    double y_mm;
    int    tool_number;       /** Tool to use (T-code) */
} ExcellonHit;

/** Slot/rout path — series of connected line segments */
typedef struct {
    double   *x_points;        /** Array of X coordinates */
    double   *y_points;        /** Array of Y coordinates */
    int       n_points;        /** Number of points */
    int       tool_number;     /** Tool number */
    double    width_mm;        /** Slot width (for slots) */
    int       is_closed;       /** 1 if path is closed polygon */
} ExcellonSlot;

/** Slot list */
typedef struct {
    ExcellonSlot *slots;
    int           count;
    int           capacity;
} ExcellonSlotList;

/* ─── Complete Drill File ───────────────────────────────────────── */

/** Layer pair for PTH (plated through-hole) vs NPTH (non-plated) */
typedef enum {
    EX_LAYER_PTH  = 0,   /** Plated through-hole (all layers) */
    EX_LAYER_NPTH = 1    /** Non-plated (mechanical hole) */
} ExcellonLayerPair;

/**
 * Complete Excellon drill file data.
 * Contains all information needed to generate a valid NC drill file.
 */
typedef struct {
    ExcellonToolTable    tool_table;
    ExcellonHit         *hits;
    int                  n_hits;
    int                  hits_capacity;
    ExcellonSlotList     slot_list;
    ExcellonLayerPair    layer_pair;
    GerberUnit           unit;
    GerberZeroSuppression zero_supp;
    int                  int_digits;
    int                  dec_digits;
    int                  suppress_trailing;  /** 1 = suppress trailing zeros */
    char                 header_comment[256];
} ExcellonFile;

/* ─── Excellon Format API ───────────────────────────────────────── */

/**
 * Initialize an empty Excellon drill file structure.
 * Defaults: mm units, 3.3 format, trailing zero suppression,
 * plated through-hole.
 */
void excellon_file_init(ExcellonFile *ef);

/**
 * Initialize an empty tool table.
 */
void excellon_tool_table_init(ExcellonToolTable *table);

/**
 * Add a tool definition to the table.
 * Returns tool number on success, -1 if table full.
 *
 * Complexity: O(1)
 */
int excellon_tool_add(ExcellonToolTable *table, double diameter_mm,
                      ExcellonToolType type);

/**
 * Find a tool by diameter (within 0.001mm tolerance).
 * Returns tool number or -1 if not found.
 * Uses linear search — O(n).
 */
int excellon_tool_find_by_diameter(const ExcellonToolTable *table,
                                   double diameter_mm);

/**
 * Set advanced tool parameters (feed, speed, peck).
 */
void excellon_tool_set_params(ExcellonToolTable *table, int tool_num,
                              double feed_rate, double spindle_rpm,
                              int peck_enabled, double peck_depth);

/**
 * Add a drill hit to the file.
 * Automatically resizes the hits array as needed.
 *
 * Returns 0 on success, -1 on allocation failure.
 *
 * Complexity: O(1) amortized.
 */
int excellon_add_hit(ExcellonFile *ef, double x_mm, double y_mm,
                     int tool_number);

/**
 * Initialize an empty slot list.
 */
void excellon_slot_list_init(ExcellonSlotList *list);

/**
 * Add a slot/rout path to the list.
 * Ownership of the coordinate arrays is transferred to the slot.
 *
 * Returns 0 on success, -1 on allocation failure.
 */
int excellon_slot_add(ExcellonSlotList *list, const double *x_pts,
                      const double *y_pts, int n_pts,
                      int tool_number, double width_mm, int is_closed);

/**
 * Free all memory held by an Excellon file structure.
 */
void excellon_file_free(ExcellonFile *ef);

/**
 * Free all memory in a slot list.
 */
void excellon_slot_list_free(ExcellonSlotList *list);

/* ─── Excellon Generation ───────────────────────────────────────── */

/**
 * Generate a complete NC drill file.
 *
 * @param filename  Output file path (e.g., "board.drl" or "board.txt")
 * @param ef        Populated Excellon file data
 *
 * The generated file follows standard format:
 *   M48 — Header begin
 *   Tool definitions (T01C0.350, etc.)
 *   % — Tool definition end
 *   T01 — Select first tool
 *   X...Y... — Drill coordinates
 *   T02 — Select next tool
 *   ...
 *   M30 — End of program
 *
 * Tools are automatically sorted by diameter for optimal manufacturing.
 *
 * Returns 0 on success, -1 on file I/O error.
 */
int excellon_generate(const char *filename, const ExcellonFile *ef);

/**
 * Generate a separate NPTH (non-plated) drill file.
 * Filters only NPTH holes from the combined file.
 */
int excellon_gen_npth(const char *filename, const ExcellonFile *ef);

/**
 * Optimize drill hit ordering to minimize total travel distance.
 *
 * Uses a nearest-neighbor greedy algorithm:
 *   Start at origin (0,0). At each step, select the closest
 *   unvisited hit as the next target.
 *
 * Complexity: O(n²) greedy. Optimal TSP is NP-hard.
 * For n ≤ 5000, greedy is within ~25% of optimal.
 *
 * Reference: Christofides algorithm provides 1.5-approximation
 * for metric TSP but is omitted here for simplicity.
 *
 * @param ef  Excellon file (modified in-place — hits reordered)
 */
void excellon_optimize_path(ExcellonFile *ef);

/**
 * Merge holes with identical position and same tool.
 * Useful for cleaning up duplicate hits from multi-layer drill data.
 *
 * Returns the number of duplicates removed.
 */
int excellon_deduplicate(ExcellonFile *ef);

/**
 * Sort hits by tool number for efficient manufacturing
 * (minimizes tool changes).
 */
void excellon_sort_by_tool(ExcellonFile *ef);

/**
 * Validate an Excellon file for consistency.
 * Checks: all hits reference valid tools, no overlapping slots,
 * coordinates within machine limits.
 *
 * Returns 0 if valid, or an error code:
 *   1 = invalid tool reference in hit
 *   2 = coordinate out of range
 *   3 = slot self-intersection detected
 */
int excellon_validate(const ExcellonFile *ef);

/**
 * Report statistics about the drill file.
 * Prints total hits, tools used, min/max hole size, etc.
 */
void excellon_report_stats(const ExcellonFile *ef, FILE *fp);

#endif /* EXCELLON_H */
