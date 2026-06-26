/**
 * pcb_geometry.h — PCB Geometry Primitives and Operations
 *
 * Defines computational geometry types needed for PCB design:
 * points, lines, arcs, polygons, pads, traces, vias, and board outlines.
 * Implements fundamental geometric operations used by Gerber generation,
 * DFM checking, and Excellon drill processing.
 *
 * Knowledge map:
 *   L1: Point, line, arc, polygon, pad, via definitions
 *   L2: PCB layer stackup concept
 *   L3: Euclidean geometry, polygon area, point-in-polygon, winding number
 *   L5: Polygon operations — offset, Boolean, triangulation
 *
 * Course mapping:
 *   MIT 6.630 EM -> planar geometry for PCB
 *   Stanford EE359 -> RF transmission line geometry
 *   Berkeley EE117 -> EM geometry for PCB simulation
 */

#ifndef PCB_GEOMETRY_H
#define PCB_GEOMETRY_H

#include "gerber.h"
#include <stdint.h>
#include <math.h>

/* ─── Basic Geometry Types ──────────────────────────────────────── */

/** 2D line segment */
typedef struct {
    GerberPointD start;
    GerberPointD end;
    double       width_mm;     /** Line width (for PCB traces) */
} PCBLineSegment;

/** Circle (center + radius) */
typedef struct {
    GerberPointD center;
    double       radius_mm;
} PCBCircle;

/** Axis-aligned bounding box (AABB) */
typedef struct {
    double x_min, x_max;
    double y_min, y_max;
} PCBAABB;

/* ─── PCB-Specific Geometry ─────────────────────────────────────── */

/** Pad shape enumeration */
typedef enum {
    PAD_ROUND     = 0,  /** Circular pad */
    PAD_RECT      = 1,  /** Rectangular pad */
    PAD_OVAL      = 2,  /** Oval/obround pad */
    PAD_ROUNDRECT = 3,  /** Rounded rectangle */
    PAD_OCTAGON   = 4,  /** Octagonal pad */
    PAD_CUSTOM    = 5   /** Custom polygon pad */
} PadShape;

/** Pad technology */
typedef enum {
    PAD_TECH_SMD  = 0,  /** Surface-mount device pad */
    PAD_TECH_PTH  = 1,  /** Plated through-hole pad */
    PAD_TECH_NPTH = 2   /** Non-plated through-hole */
} PadTech;

/** 
 * PCB Pad definition.
 * A pad is a conductive feature where a component lead is soldered.
 */
typedef struct {
    GerberPointD position;      /** Center position */
    PadShape     shape;         /** Pad geometry */
    double       width_mm;      /** X dimension */
    double       height_mm;     /** Y dimension (same as width for round) */
    double       hole_diameter; /** 0 for SMD, >0 for PTH */
    double       rotation_deg;  /** Rotation angle */
    PadTech      tech;          /** SMD / PTH / NPTH */
    double       corner_radius; /** For rounded rectangle (0 = square corners) */
    int          layer;         /** PCB layer (1=top, N=bottom) */
    char         designator[16];/** e.g., "1" for pad 1 of component */
    char         net_name[64];  /** Net name (e.g., "GND", "VCC") */
} PCBPad;

/**
 * PCB Trace segment.
 * A trace is a copper connection between two points on one layer.
 */
typedef struct {
    GerberPointD start;        /** Start point */
    GerberPointD end;          /** End point */
    double       width_mm;     /** Trace width */
    int          layer;        /** PCB layer */
    char         net_name[64]; /** Net name */
} PCBTrace;

/**
 * PCB Via.
 * A via is a vertical electrical connection between layers.
 */
typedef enum {
    VIA_THROUGH  = 0,  /** Through-hole via (all layers) */
    VIA_BLIND    = 1,  /** Blind via (surface to inner layer) */
    VIA_BURIED   = 2,  /** Buried via (between inner layers) */
    VIA_MICROVIA = 3   /** Microvia (laser-drilled, ≤0.15mm, single layer pair) */
} ViaType;

typedef struct {
    GerberPointD position;     /** Via center */
    double       pad_diameter; /** Outer pad diameter on each layer */
    double       hole_diameter;/** Drill hole diameter */
    ViaType      type;
    int          from_layer;   /** Start layer (1=top) */
    int          to_layer;     /** End layer (N=bottom) */
    char         net_name[64]; /** Net name */
} PCBVia;

/**
 * PCB Hole (used for DFM checking and Excellon).
 * Generic hole definition that covers both PTH and NPTH.
 */
typedef struct {
    GerberPointD position;
    double       hole_diameter;
    double       pad_diameter;     /** 0 for NPTH */
    int          is_plated;        /** 1 = PTH, 0 = NPTH */
    int          from_layer;
    int          to_layer;
    char         type_desc[32];    /** e.g., "via", "mounting hole", "slot" */
} PCBHole;

/**
 * PCB Copper pour / fill region.
 * A polygon defining a solid copper area (generally connected to
 * a power or ground net).
 */
typedef struct {
    GerberPointD *vertices;    /** Array of polygon vertices (closed) */
    int           n_vertices;  /** Number of vertices (≥3) */
    int           layer;       /** PCB layer */
    char          net_name[64];/** Net name */
    double        min_width_mm;/** Minimum bridge/neck width constraint */
} PCBRegion;

/**
 * Generic PCB feature union for DFM checking.
 * Covers all conductive and non-conductive features.
 */
typedef enum {
    FEAT_TRACE  = 0,
    FEAT_PAD    = 1,
    FEAT_VIA    = 2,
    FEAT_REGION = 3,
    FEAT_HOLE   = 4
} PCBFeatureType;

typedef struct {
    PCBFeatureType type;
    union {
        PCBTrace  trace;
        PCBPad    pad;
        PCBVia    via;
        PCBRegion region;
        PCBHole   hole;
    } data;
    int layer;
    char net_name[64];
} PCBFeature;

/* ─── Board Layer Stackup ───────────────────────────────────────── */

/** Single PCB layer definition */
typedef struct {
    char    name[32];            /** Layer name (e.g., "Top Copper") */
    int     is_copper;           /** 1 = copper layer, 0 = dielectric */
    int     is_external;         /** 1 = top/bottom layer */
    double  copper_thickness_mm; /** Copper thickness */
    double  dielectric_thickness_mm; /** Dielectric thickness to next layer */
    double  dielectric_constant; /** εr of dielectric material */
    char    material[64];        /** Material name (e.g., "FR-4", "Rogers 4350B") */
} PCBLayer;

/**
 * Complete PCB layer stackup.
 * Defines the physical structure of the printed circuit board.
 *
 * Standard 4-layer stackup example:
 *   Layer 1: Top Copper (signal)
 *   Layer 2: Inner 1 (GND plane)
 *   Layer 3: Inner 2 (PWR plane)
 *   Layer 4: Bottom Copper (signal)
 */
typedef struct {
    PCBLayer *layers;
    int       n_layers;
    double    total_thickness_mm;
} PCBLayerStackup;

/* ─── Complete PCB Board ────────────────────────────────────────── */

/**
 * Complete PCB board definition.
 * Contains all geometric and electrical information needed for
 * Gerber generation, DFM analysis, and drill file creation.
 */
typedef struct {
    char              board_name[64];
    char              revision[16];
    PCBLayerStackup   stackup;
    GerberPointD      *outline;           /** Board outline polygon */
    int                n_outline_vertices;

    /* Layer-specific geometry */
    PCBTrace         **traces;            /** traces[layer] = array of traces */
    int               *n_traces;          /** n_traces[layer] */
    PCBPad           **pads;              /** pads[layer] */
    int               *n_pads;            /** n_pads[layer] */
    PCBRegion        **regions;           /** regions[layer] */
    int               *n_regions;         /** n_regions[layer] */
    PCBVia            *vias;
    int                n_vias;
    PCBHole           *holes;
    int                n_holes;

    /* Silkscreen */
    PCBLineSegment    *silkscreen_top;
    int                n_silkscreen_top;
    PCBLineSegment    *silkscreen_bottom;
    int                n_silkscreen_bottom;

    /* Feature arrays for DFM (flat, all layers) */
    PCBFeature        *all_features;
    int                n_all_features;

    /* Layer count */
    int                n_layers;
} PCBBoard;

/* ─── Geometry Operations ───────────────────────────────────────── */

/**
 * Compute Euclidean distance between two points.
 * d = √((x₁-x₂)² + (y₁-y₂)²)
 *
 * Complexity: O(1)
 */
double pcb_distance(const GerberPointD *a, const GerberPointD *b);

/**
 * Compute squared distance (avoids sqrt for comparison).
 */
double pcb_distance_sq(const GerberPointD *a, const GerberPointD *b);

/**
 * Check if point P lies inside polygon defined by vertices.
 * Uses the ray-casting algorithm (even-odd rule).
 *
 * A ray from P to +infinity is cast; if it crosses the polygon
 * boundary an odd number of times, P is inside.
 *
 * Complexity: O(n) where n = number of vertices.
 * Reference: Sutherland-Hodgman (1974), Foley et al. §3.4.1
 */
int pcb_point_in_polygon(const GerberPointD *point,
                         const GerberPointD *polygon, int n_vertices);

/**
 * Compute the signed area of a polygon.
 * A = ½ Σ(x_i·y_{i+1} - x_{i+1}·y_i)
 *
 * Positive area = counter-clockwise winding.
 * Negative area = clockwise winding.
 *
 * Complexity: O(n).
 * Reference: Shoelace formula (Gauss area formula).
 */
double pcb_polygon_area(const GerberPointD *polygon, int n_vertices);

/**
 * Compute winding number of point P relative to polygon.
 * Winding number = total signed angle traversal / (2π).
 *
 * A non-zero winding number means P is strictly inside the polygon
 * (unlike even-odd, winding number accounts for self-intersecting polygons).
 *
 * Complexity: O(n).
 */
int pcb_winding_number(const GerberPointD *point,
                       const GerberPointD *polygon, int n_vertices);

/**
 * Compute the centroid of a polygon.
 * Uses the area-weighted average of triangle centroids.
 *
 * Complexity: O(n).
 */
GerberPointD pcb_polygon_centroid(const GerberPointD *polygon, int n_vertices);

/**
 * Compute the bounding box of a set of points.
 *
 * Complexity: O(n).
 */
PCBAABB pcb_compute_aabb(const GerberPointD *points, int n_points);

/**
 * Compute the bounding box of a polygon region.
 */
PCBAABB pcb_region_aabb(const PCBRegion *region);

/**
 * Check if two AABBs overlap.
 * Used for broad-phase collision detection.
 *
 * Complexity: O(1).
 */
int pcb_aabb_overlap(const PCBAABB *a, const PCBAABB *b);

/**
 * Compute minimum distance between a point and a line segment.
 * Returns the perpendicular distance if projection falls on segment,
 * otherwise the distance to the nearest endpoint.
 *
 * Complexity: O(1).
 */
double pcb_point_to_segment_distance(const GerberPointD *p,
                                     const GerberPointD *a,
                                     const GerberPointD *b);

/**
 * Compute minimum distance between two line segments.
 * Considers all 4 endpoint-to-segment distances and
 * checks for intersection.
 *
 * Complexity: O(1).
 */
double pcb_segment_to_segment_distance(const GerberPointD *a1,
                                       const GerberPointD *a2,
                                       const GerberPointD *b1,
                                       const GerberPointD *b2);

/**
 * Compute minimum distance from point to polygon edge.
 * Returns 0 if point is inside polygon.
 *
 * Complexity: O(n).
 */
double pcb_point_to_polygon_distance(const GerberPointD *p,
                                     const GerberPointD *polygon,
                                     int n_vertices);

/**
 * Offset a polygon inward/outward by a given distance.
 * Negative offset = inward (shrinking), positive = outward (expanding).
 *
 * Uses the straight skeleton method with mitered corners.
 * For simple convex polygons this is exact; for complex concave
 * polygons, self-intersection must be handled by the caller.
 *
 * Complexity: O(n).
 *
 * @param polygon    Input polygon (closed, n_vertices points)
 * @param n_vertices Number of vertices
 * @param offset_mm  Offset distance (positive=outward, negative=inward)
 * @param out_count   Output: number of vertices in result
 *
 * @return Dynamically allocated result polygon (caller must free),
 *         or NULL on degenerate result (offset inward too far).
 */
GerberPointD* pcb_polygon_offset(const GerberPointD *polygon,
                                 int n_vertices, double offset_mm,
                                 int *out_count);

/**
 * Compute minimum distance between two polygons.
 */
double pcb_polygon_to_polygon_distance(const GerberPointD *poly1,
                                       int n1,
                                       const GerberPointD *poly2,
                                       int n2);

/**
 * Initialize a PCB region from an array of points.
 * Makes an internal copy of the vertex data.
 *
 * Returns 0 on success, -1 on allocation failure.
 */
int pcb_region_init(PCBRegion *region, const GerberPointD *vertices,
                    int n_vertices, int layer, const char *net_name);

/**
 * Free memory held by a PCB region.
 */
void pcb_region_free(PCBRegion *region);

/**
 * Initialize a PCB board structure with a given number of layers.
 * Allocates per-layer arrays. Caller must populate them.
 *
 * Returns 0 on success, -1 on allocation failure.
 */
int pcb_board_init(PCBBoard *board, int n_layers, const char *name,
                   const char *revision);

/**
 * Free all memory held by a PCB board.
 */
void pcb_board_free(PCBBoard *board);

/**
 * Add a trace to a specific layer of the board.
 * Performs dynamic array management.
 *
 * Returns 0 on success, -1 on allocation failure.
 */
int pcb_board_add_trace(PCBBoard *board, int layer,
                        const PCBTrace *trace);

/**
 * Add a pad to a specific layer.
 */
int pcb_board_add_pad(PCBBoard *board, int layer,
                      const PCBPad *pad);

/**
 * Add a region (copper pour) to a specific layer.
 */
int pcb_board_add_region(PCBBoard *board, int layer,
                         const PCBRegion *region);

/**
 * Add a via to the board.
 */
int pcb_board_add_via(PCBBoard *board, const PCBVia *via);

/**
 * Add a hole to the board.
 */
int pcb_board_add_hole(PCBBoard *board, const PCBHole *hole);

/**
 * Build the flat all_features array from per-layer data.
 * Must be called before DFM checking.
 */
int pcb_board_build_features(PCBBoard *board);

/**
 * Set the board outline polygon.
 * Makes a copy of the vertex array.
 */
int pcb_board_set_outline(PCBBoard *board, const GerberPointD *vertices,
                          int n_vertices);

/**
 * Compute total board area (of outline polygon) in mm².
 */
double pcb_board_area(const PCBBoard *board);

/**
 * Compute copper fill percentage for a given layer.
 */
double pcb_board_copper_fill(const PCBBoard *board, int layer);

#endif /* PCB_GEOMETRY_H */
