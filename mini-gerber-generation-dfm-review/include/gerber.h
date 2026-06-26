/**
 * gerber.h — Gerber Format Core Definitions (RS-274X and X2)
 *
 * Defines the fundamental data structures for the Gerber photoplot format,
 * the de-facto standard for PCB fabrication data exchange.
 *
 * Standards referenced:
 *   - Gerber Format Specification (RS-274X), Ucamco
 *   - Gerber X2 Format Specification, Ucamco
 *   - IPC-2581 Generic Requirements for PCB Assembly
 *
 * Knowledge map:
 *   L1: Aperture types, D-code, G-code, M-code definitions
 *   L2: Photoplotting concept, flash/draw operation modes
 *   L3: Coordinate geometry, arc parameter representation
 *
 * Course mapping:
 *   MIT 6.630 EM Waves -> Fabrication tolerances
 *   Georgia Tech ECE 6350 EM -> PCB manufacturing processes
 */

#ifndef GERBER_H
#define GERBER_H

#include <stdint.h>
#include <stdio.h>

/* ─── Coordinate System ─────────────────────────────────────────── */

/** Unit of measurement for Gerber coordinates */
typedef enum {
    GERBER_UNIT_MM,
    GERBER_UNIT_INCH
} GerberUnit;

/**
 * Zero-suppression mode for coordinate formatting.
 * Leading: suppress leading zeros (e.g., 005000 -> 05000)
 * Trailing: suppress trailing zeros (e.g., 005000 -> 0050)
 * None: no suppression (fixed-width)
 */
typedef enum {
    GERBER_ZERO_LEADING,
    GERBER_ZERO_TRAILING,
    GERBER_ZERO_NONE
} GerberZeroSuppression;

/**
 * Coordinate notation mode.
 * ABSOLUTE: coordinates relative to origin
 * INCREMENTAL: coordinates relative to last point
 */
typedef enum {
    GERBER_ABS,
    GERBER_INC
} GerberCoordinateMode;

/** Point in 2D PCB coordinate space (integer in Gerber units) */
typedef struct {
    int64_t x;
    int64_t y;
} GerberPoint;

/** Floating-point representation for computational precision */
typedef struct {
    double x;
    double y;
} GerberPointD;

/* ─── Aperture System ───────────────────────────────────────────── */

/** Aperture shape types defined in RS-274X */
typedef enum {
    APERTURE_CIRCLE      = 'C',
    APERTURE_RECTANGLE   = 'R',
    APERTURE_OBROUND     = 'O',
    APERTURE_POLYGON     = 'P',
    APERTURE_MACRO       = 'M',
    APERTURE_NONE        = 0
} ApertureShape;

/**
 * Aperture definition (D-code).
 * Each D-code D10-D999+ defines a physical shape for the photoplotter.
 * D01 = draw (interpolate line), D02 = move, D03 = flash.
 */
typedef struct {
    int           d_code;         /** D-code number (>=10 for apertures) */
    ApertureShape shape;          /** Shape type */
    double        param1;         /** Circle: diameter, Rect: width */
    double        param2;         /** Rect: height, Obround: width */
    double        param3;         /** Obround: height, Polygon: outer diameter */
    double        hole_diameter;  /** Hole diameter (0 = solid) */
    int           n_vertices;     /** Polygon: number of vertices (3-12) */
    double        rotation;       /** Rotation angle in degrees */
    char          name[32];       /** Aperture macro name if APERTURE_MACRO */
} ApertureDef;

/** Maximum number of user-defined apertures per file */
#define GERBER_MAX_APERTURES 1024

/** Aperture table (list of all D-codes in a Gerber file) */
typedef struct {
    ApertureDef apertures[GERBER_MAX_APERTURES];
    int         count;
} ApertureTable;

/* ─── Aperture Macros ───────────────────────────────────────────── */

/** Primitive types for aperture macro construction */
typedef enum {
    AM_PRIMITIVE_COMMENT   = 0,   /** %AM comment */
    AM_PRIMITIVE_CIRCLE    = 1,   /** Circle primitive */
    AM_PRIMITIVE_LINE20    = 2,   /** Vector line with 2 endpoints */
    AM_PRIMITIVE_LINE21    = 20,  /** Center-line with length+width+angle */
    AM_PRIMITIVE_LINE22    = 21,  /** Lower-left-line with width+height+center */
    AM_PRIMITIVE_OUTLINE   = 4,   /** Polygon outline */
    AM_PRIMITIVE_POLYGON   = 5,   /** Filled regular polygon */
    AM_PRIMITIVE_MOIRE     = 6,   /** Moire pattern */
    AM_PRIMITIVE_THERMAL   = 7    /** Thermal relief */
} AMMacroPrimitive;

/**
 * Single aperture macro primitive.
 * Macros are built from a sequence of primitives with exposure on/off.
 */
typedef struct {
    AMMacroPrimitive code;
    double           exposure;    /** 1=on (draw), 0=off (erase) */
    int              n_params;
    double           params[12];  /** Up to 12 parameters per primitive */
} AMMacroPrim;

/** Maximum primitives per macro */
#define AM_MAX_PRIMITIVES 32

/** Aperture macro definition */
typedef struct {
    char          name[32];
    int           n_primitives;
    AMMacroPrim  primitives[AM_MAX_PRIMITIVES];
} ApertureMacro;

/** Macro table */
typedef struct {
    ApertureMacro macros[256];
    int           count;
} ApertureMacroTable;

/* ─── Graphical State ───────────────────────────────────────────── */

/**
 * Polarity of the image (dark/clear).
 * DARK (positive): plotted areas represent copper (or mask, etc.)
 * CLEAR (negative): plotted areas remove from the layer.
 */
typedef enum {
    GERBER_POLARITY_DARK  = 'D',   /** %LPD — Layer Polarity Dark */
    GERBER_POLARITY_CLEAR = 'C'    /** %LPC — Layer Polarity Clear */
} GerberPolarity;

/**
 * Interpolation mode for draw/fill operations.
 * LINEAR: straight line between points (G01)
 * CW_CIRCULAR: clockwise arc (G02)
 * CCW_CIRCULAR: counter-clockwise arc (G03)
 */
typedef enum {
    GERBER_INTERP_LINEAR       = 1,
    GERBER_INTERP_CW_CIRCULAR  = 2,
    GERBER_INTERP_CCW_CIRCULAR = 3
} GerberInterpolation;

/**
 * Arc quadrant mode.
 * SINGLE: arc spans exactly one quadrant
 * MULTI: arc can span multiple quadrants
 */
typedef enum {
    GERBER_QUAD_SINGLE = 0,
    GERBER_QUAD_MULTI  = 1
} GerberQuadrantMode;

/**
 * Graphics state — mutable state during Gerber parsing/generation.
 * Maintains the current tool context: position, aperture, mode.
 */
typedef struct {
    GerberPoint          current_pos;     /** Current (X,Y) position */
    int                  current_dcode;   /** Active D-code (>=10) */
    GerberInterpolation  interpolation;   /** G01/G02/G03 */
    GerberQuadrantMode   quadrant;        /** G74 single / G75 multi */
    GerberCoordinateMode coord_mode;      /** G90 absolute / G91 incremental */
    GerberPolarity       polarity;        /** LPD / LPC */
    GerberUnit           unit;            /** MOMM / MOIN */
    GerberZeroSuppression zero_suppress;  /** Leading / trailing / none */
    int                  int_digits;      /** Integer digits in coordinate */
    int                  dec_digits;      /** Decimal digits in coordinate */
    int                  current_aperture;/** Active D-code aperture index */
} GerberState;

/* ─── G-Codes and M-Codes ───────────────────────────────────────── */

/** Standard Gerber G-codes */
typedef enum {
    G_CODE_INTERP_LINEAR   = 1,   /** G01 Linear interpolation */
    G_CODE_INTERP_CW       = 2,   /** G02 Clockwise circular */
    G_CODE_INTERP_CCW      = 3,   /** G03 Counter-clockwise circular */
    G_CODE_COMMENT         = 4,   /** G04 Comment */
    G_CODE_APERTURE_SELECT = 54,  /** G54 Select aperture (obsolete, use D-code) */
    G_CODE_QUAD_SINGLE     = 74,  /** G74 Single-quadrant arc */
    G_CODE_QUAD_MULTI      = 75,  /** G75 Multi-quadrant arc */
    G_CODE_UNIT_MM         = 71,  /** G71 Set units to mm */
    G_CODE_UNIT_INCH       = 70,  /** G70 Set units to inches */
    G_CODE_ABS             = 90,  /** G90 Absolute coordinate mode */
    G_CODE_INC             = 91   /** G91 Incremental coordinate mode */
} GerberGCode;

/** Standard Gerber M-codes */
typedef enum {
    M_CODE_PROGRAM_STOP       = 0,   /** M00 Program stop */
    M_CODE_OPTIONAL_STOP      = 1,   /** M01 Optional stop */
    M_CODE_END_OF_PROGRAM     = 2    /** M02 End of program */
} GerberMCode;

/* ─── Gerber X2 Attributes ───────────────────────────────────────── */

/**
 * Gerber X2 file function identification.
 * Each Gerber file declares its function (e.g., copper layer L1, solder mask).
 */
typedef enum {
    X2_FUNC_COPPER         = 0,  /** Copper layer */
    X2_FUNC_SOLDERMASK_TOP = 1,  /** Top solder mask */
    X2_FUNC_SOLDERMASK_BOT = 2,  /** Bottom solder mask */
    X2_FUNC_SILKSCREEN_TOP = 3,  /** Top silkscreen */
    X2_FUNC_SILKSCREEN_BOT = 4,  /** Bottom silkscreen */
    X2_FUNC_SOLDERPASTE_TOP= 5,  /** Top solder paste */
    X2_FUNC_SOLDERPASTE_BOT= 6,  /** Bottom solder paste */
    X2_FUNC_PROFILE        = 7,  /** Board profile / outline */
    X2_FUNC_DRILL          = 8,  /** Drill layer (non-plated) */
    X2_FUNC_ROUT           = 9,  /** Rout / milling layer */
    X2_FUNC_LEGEND         = 10, /** Legend / silkscreen */
    X2_FUNC_GLUE           = 11, /** Glue dots */
    X2_FUNC_CARBON         = 12, /** Carbon conductive layer */
    X2_FUNC_GOLD           = 13, /** Gold plating */
    X2_FUNC_PEELABLE       = 14  /** Peelable solder mask */
} X2FileFunction;

/**
 * X2 part attribute for component identification.
 * Associates a pad/flash with specific component info.
 */
typedef struct {
    char designator[16];   /** e.g., "R1", "C5", "U3" */
    char footprint[64];    /** e.g., "0805", "SOIC-8" */
    char value[32];        /** e.g., "10k", "100nF" */
    int  pin;              /** Pin number (1-indexed) */
    char mount_type;       /** 'S' = SMD, 'T' = through-hole */
} X2PartAttribute;

/** File-level X2 attributes */
typedef struct {
    X2FileFunction function;       /** File layer function */
    char           part_name[64];  /** e.g., "MainPCB" */
    char           part_number[32];/** Manufacturer part number */
    char           revision[16];   /** Revision string */
    int            layer_number;   /** Physical layer number (1=top, etc.) */
    int            n_part_attrs;   /** Number of pad-level attributes */
    X2PartAttribute *part_attrs;   /** Array of pad attributes */
} X2FileAttribute;

/* ─── Block-Level Data ──────────────────────────────────────────── */

/** A single Gerber command/operation record */
typedef enum {
    GERBER_OP_FLASH     = 0,  /** D03 — Flash aperture at current position */
    GERBER_OP_DRAW      = 1,  /** D01 — Draw line to point */
    GERBER_OP_MOVE      = 2,  /** D02 — Move to point (no draw) */
    GERBER_OP_ARC       = 3,  /** G02/G03 — Arc to point with center offset */
    GERBER_OP_REGION    = 4,  /** G36/G37 — Region fill begin/end */
    GERBER_OP_APERTURE  = 5   /** D-code selection */
} GerberOpType;

/** Arc specification for circular interpolation */
typedef struct {
    int64_t cx_offset;   /** X offset from start to arc center */
    int64_t cy_offset;   /** Y offset from start to arc center */
} GerberArc;

/** Single Gerber operation record */
typedef struct {
    GerberOpType type;
    GerberPoint  endpoint;    /** Target point for MOVE/DRAW/FLASH */
    GerberArc    arc;         /** Arc center offset (valid for OP_ARC only) */
    int          d_code;      /** D-code for OP_APERTURE/OP_FLASH */
    int          region_begin;/** 1 = begin region, 0 = end region */
} GerberOperation;

/** Dynamic array of operations */
typedef struct {
    GerberOperation *ops;
    int              count;
    int              capacity;
} GerberOpList;

/* ─── Core Gerber API ───────────────────────────────────────────── */

/**
 * Initialize a Gerber state machine to default values.
 * Defaults: mm units, absolute coordinates, linear interpolation,
 * dark polarity, 3.6 format, leading zero suppression.
 *
 * Theorem (Gerber coordinate representation):
 *   Given integer X and format N.M, the physical coordinate is
 *   coordinate = X * 10^{-M} units.
 */
void gerber_state_init(GerberState *state);

/**
 * Set coordinate format: integer digits and decimal digits.
 * Common formats: 2.4 (inch), 3.3 (mm), 3.4 (inch), 4.3 (mm).
 */
void gerber_set_format(GerberState *state, int int_digits, int dec_digits);

/**
 * Set the zero suppression mode for coordinate output.
 * O(1) — simple flag assignment.
 */
void gerber_set_zero_suppression(GerberState *state, GerberZeroSuppression mode);

/**
 * Convert a physical coordinate (double) to Gerber integer format.
 *
 * Given format N.M and value V:
 *   int_val = round(V * 10^M)
 *
 * Complexity: O(1). Reference: Ucamco Gerber Format Spec §4.5.
 */
int64_t gerber_phys_to_int(double value, int dec_digits);

/**
 * Convert a Gerber integer coordinate back to physical value.
 * phys_val = int_val * 10^{-dec_digits}
 *
 * Complexity: O(1).
 */
double gerber_int_to_phys(int64_t int_val, int dec_digits);

/**
 * Initialize an empty aperture table.
 * Table starts with only the standard D01-D03 pseudo-codes.
 */
void aperture_table_init(ApertureTable *table);

/**
 * Add an aperture definition to the table.
 * Returns the D-code number assigned (10 + index).
 * Returns -1 if table is full (GERBER_MAX_APERTURES exceeded).
 *
 * Complexity: O(1) amortized.
 */
int aperture_table_add(ApertureTable *table, ApertureShape shape,
                       double p1, double p2, double p3,
                       double hole_dia, double rotation);

/**
 * Find an aperture by D-code number.
 * Returns index in table, or -1 if not found.
 * Uses linear search — O(n) where n = table->count.
 */
int aperture_table_find(const ApertureTable *table, int d_code);

/**
 * Initialize an aperture macro table.
 */
void macro_table_init(ApertureMacroTable *table);

/**
 * Add an aperture macro definition.
 * Returns 0 on success, -1 on failure (table full or name conflict).
 */
int macro_table_add(ApertureMacroTable *table, const char *name,
                    const AMMacroPrim *primitives, int n_prims);

/**
 * Find a macro by name.
 * Returns index or -1.
 */
int macro_table_find(const ApertureMacroTable *table, const char *name);

/**
 * Initialize an operation list.
 * Pre-allocates initial capacity.
 */
void gerber_oplist_init(GerberOpList *list, int initial_capacity);

/**
 * Append an operation to the list.
 * Returns 0 on success, -1 on allocation failure.
 *
 * Complexity: O(1) amortized (geometric growth).
 */
int gerber_oplist_append(GerberOpList *list, const GerberOperation *op);

/**
 * Free all memory associated with an operation list.
 * Sets count=0, capacity=0, ops=NULL.
 */
void gerber_oplist_free(GerberOpList *list);

/**
 * Create an X2 file attribute structure with defaults.
 */
void x2_file_attr_init(X2FileAttribute *attr);

/**
 * Add a part-level attribute to the X2 structure.
 * Allocates memory — caller must call x2_file_attr_free() to release.
 *
 * Returns 0 on success, -1 on allocation failure.
 */
int x2_add_part_attr(X2FileAttribute *attr, const char *designator,
                     const char *footprint, const char *value,
                     int pin, char mount_type);

/**
 * Release memory held by X2 attributes.
 */
void x2_file_attr_free(X2FileAttribute *attr);

/**
 * Format a Gerber coordinate integer as a string with zero suppression.
 *
 * @param value   Integer coordinate value
 * @param int_d   Integer digits (N)
 * @param dec_d   Decimal digits (M)
 * @param zs      Zero suppression mode
 * @param buf     Output buffer (must be >= 16 chars)
 * @return        Number of characters written
 */
int format_coordinate(int64_t value, int int_d, int dec_d,
                      GerberZeroSuppression zs, char *buf);

/**
 * Validate Gerber state for consistency (no contradictory settings).
 * Returns 0 if valid, or an error code:
 *   1 = invalid format (int_digits or dec_digits <= 0)
 *   2 = zero_suppress mode incompatible with format
 */
int gerber_state_validate(const GerberState *state);

/* ─── Utility Macros ────────────────────────────────────────────── */

/** Gerber precision factor: 10^dec_digits */
#define GERBER_PRECISION(dd) ((int64_t)1e##dd)

/** Maximum coordinate value in Gerber format */
#define GERBER_MAX_COORD(dd) ((int64_t)(1e6))

#endif /* GERBER_H */
