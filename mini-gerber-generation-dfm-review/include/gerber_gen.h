/**
 * gerber_gen.h — Gerber File Generation API
 *
 * Produces RS-274X compliant Gerber files from PCB geometry descriptions.
 * Supports standard photoplot operations: flashes, draws, arcs, and
 * region fills with full aperture table emission.
 *
 * Knowledge map:
 *   L2: Photoplotter operation — how Gerber commands translate to physical plotting
 *   L5: Gerber generation algorithm — assembling RS-274X output
 *   L6: Complete layer generation — producing all files for fabrication
 *
 * Course mapping:
 *   Berkeley EE117 EM -> fabrication file formats
 *   TU Munich High-Frequency Eng -> precision PCB manufacturing
 */

#ifndef GERBER_GEN_H
#define GERBER_GEN_H

#include "gerber.h"
#include "pcb_geometry.h"
#include <stdio.h>

/* ─── Gerber Writer ─────────────────────────────────────────────── */

/**
 * Gerber file generator state.
 * Maintains output context: current file handle, graphics state,
 * aperture table, and format parameters.
 */
typedef struct {
    FILE           *output;          /** Output file stream */
    GerberState     state;           /** Current graphics state */
    ApertureTable   apertures;       /** Aperture table for this file */
    ApertureMacroTable macros;       /** Macro table for this file */
    X2FileAttribute x2_attr;        /** X2 file attributes */
    int             x2_enabled;      /** 1 if generating X2 format */
    int             region_active;   /** 1 if inside G36..G37 region */
    int             op_count;        /** Total operations emitted */
} GerberWriter;

/**
 * Initialize a Gerber writer.
 *
 * @param writer   Writer state to initialize
 * @param output   Output FILE* (must be opened for writing, e.g., fopen("layer.gbr","w"))
 * @param unit     GERBER_UNIT_MM or GERBER_UNIT_INCH
 * @param int_digits Integer digits in coordinate format (e.g., 3 for mm)
 * @param dec_digits Decimal digits (e.g., 3 for 3.3 format)
 * @param zero_supp Zero suppression mode
 *
 * Complexity: O(1). Writes the RS-274X header immediately.
 */
void gerber_writer_init(GerberWriter *writer, FILE *output,
                        GerberUnit unit, int int_digits, int dec_digits,
                        GerberZeroSuppression zero_supp);

/**
 * Set X2 file attributes for enhanced metadata.
 * Must be called before any geometry commands if X2 is desired.
 */
void gerber_writer_set_x2(GerberWriter *writer, const X2FileAttribute *attr);

/**
 * Define an aperture and assign it a D-code.
 * Returns D-code number (>=10) or -1 on failure.
 *
 * This function writes the %ADD aperture definition directive
 * to the output file AND registers the aperture in the table.
 */
int gerber_writer_add_aperture(GerberWriter *writer, ApertureShape shape,
                               double p1, double p2, double p3,
                               double hole_dia);

/**
 * Select an aperture for subsequent draw/flash operations.
 * Writes "D<code>*" to output.
 *
 * Complexity: O(1)
 */
void gerber_writer_select_aperture(GerberWriter *writer, int d_code);

/**
 * Move to position without drawing (D02).
 * Converts physical coordinates to Gerber integers using current format.
 */
void gerber_writer_move_to(GerberWriter *writer, double x, double y);

/**
 * Flash aperture at position (D03).
 * Places a single aperture instance at (x,y).
 * Common for pads, vias, and discrete features.
 */
void gerber_writer_flash_at(GerberWriter *writer, double x, double y);

/**
 * Draw a line to position (D01).
 * Interpolates from current position to (x,y) using current aperture.
 *
 * Incremental mode: offset is emitted
 * Absolute mode: coordinate is emitted
 */
void gerber_writer_draw_to(GerberWriter *writer, double x, double y);

/**
 * Draw a circular arc to endpoint with given center offset.
 *
 * @param x     Arc endpoint X
 * @param y     Arc endpoint Y
 * @param cx    X offset from start to arc center
 * @param cy    Y offset from start to arc center
 * @param cw    1 for clockwise (G02), 0 for counter-clockwise (G03)
 *
 * Theorem (Arc center computation):
 *   Given start point S, end point E, and center C, the arc radius is
 *   R = |C - S| = |C - E|, and the arc angle θ satisfies:
 *   cos(θ/2) = ((S-C)·(E-C)) / (R²)
 */
void gerber_writer_draw_arc_to(GerberWriter *writer, double x, double y,
                               double cx, double cy, int cw);

/**
 * Begin a region (G36).
 * A region is a filled polygon defined by a sequence of draw operations
 * terminated by a G37 command.
 *
 * The region boundary is self-closing: Gerber automatically connects
 * the last point back to the first.
 */
void gerber_writer_begin_region(GerberWriter *writer);

/**
 * End a region (G37).
 */
void gerber_writer_end_region(GerberWriter *writer);

/**
 * Set layer polarity.
 * DARK = positive image (copper), CLEAR = negative.
 */
void gerber_writer_set_polarity(GerberWriter *writer, GerberPolarity polarity);

/**
 * Write a comment to the Gerber file.
 * G04 comment — ignored by photoplotters, useful for human readability.
 */
void gerber_writer_comment(GerberWriter *writer, const char *comment);

/**
 * Add a step-and-repeat block marker.
 * Required for SR (step-and-repeat) directives in RS-274X.
 *
 * @param name     Block name (e.g., "SR1")
 * @param nx       Repeat count in X
 * @param ny       Repeat count in Y
 * @param dx       Step distance X
 * @param dy       Step distance Y
 */
void gerber_writer_step_repeat(GerberWriter *writer, const char *name,
                               int nx, int ny, double dx, double dy);

/**
 * Emit file trailer and close the Gerber file.
 * Writes M02* (end of program) and flushes.
 * Does NOT close the FILE* (caller must fclose).
 *
 * Complexity: O(1)
 */
void gerber_writer_finish(GerberWriter *writer);

/* ─── Utility: Generate Complete Gerber Layer ─────────────────────── */

/**
 * Generate a complete copper layer Gerber file from polygon data.
 *
 * @param filename     Output file path
 * @param regions      Array of PCB regions (traces, pours, pads)
 * @param n_regions    Number of regions
 * @param layer_num    Layer number (1 = top copper, 2 = inner1, etc.)
 * @param board_name   Board name for X2 metadata
 *
 * This handles the full pipeline: header, aperture definitions,
 * region fills, and trailer. It selects appropriate apertures
 * based on the geometry content.
 *
 * Complexity: O(n_regions * max_trace_points).
 *
 * Returns 0 on success, -1 on file I/O error.
 */
int gerber_gen_copper_layer(const char *filename,
                            const PCBRegion *regions, int n_regions,
                            int layer_num, const char *board_name);

/**
 * Generate a solder mask Gerber file.
 * Solder mask is typically a negative image: openings are created
 * by drawing clearance pads around component pads.
 *
 * @param filename      Output file path
 * @param pads          Array of pad definitions
 * @param n_pads        Number of pads
 * @param expansion_mm  Solder mask expansion (typically 0.05-0.1mm)
 * @param is_top        1 for top side, 0 for bottom
 *
 * Returns 0 on success, -1 on error.
 */
int gerber_gen_soldermask(const char *filename,
                          const PCBPad *pads, int n_pads,
                          double expansion_mm, int is_top);

/**
 * Generate a silkscreen Gerber file.
 * Silkscreen contains reference designators, board outline, polarity marks.
 *
 * @param filename      Output file path
 * @param outlines      List of 2D line segments
 * @param n_outlines    Number of outline segments
 * @param is_top        1 for top side
 *
 * Returns 0 on success, -1 on error.
 */
int gerber_gen_silkscreen(const char *filename,
                          const PCBLineSegment *outlines, int n_outlines,
                          int is_top);

/**
 * Generate the board outline (profile) Gerber file.
 * This defines the physical board shape for routing/milling.
 *
 * @param filename    Output file path
 * @param outline     Array of points forming closed polygon
 * @param n_points    Number of points in outline (≥3)
 *
 * Returns 0 on success, -1 on error.
 */
int gerber_gen_profile(const char *filename,
                       const GerberPointD *outline, int n_points);

#endif /* GERBER_GEN_H */
