/**
 * dfm.h — Design for Manufacturing (DFM) Rule Checking
 *
 * Implements comprehensive DFM analysis for PCB layouts according to
 * IPC-2221 (Generic Standard on Printed Board Design), IPC-6012
 * (Qualification and Performance Specification for Rigid PCBs), and
 * IPC-7351 (Land Pattern Standard).
 *
 * Each rule is independently checkable and maps to a specific
 * manufacturing process constraint.
 *
 * Knowledge map:
 *   L1: DFM rule definitions — trace width, spacing, annular ring
 *   L2: Manufacturing process constraints — etching, drilling, plating
 *   L3: Geometric proximity analysis — distance computation, polygon offset
 *   L4: IPC standards — IPC-2221, IPC-6012, IPC-7351
 *   L5: DRC algorithms — pair-wise check, sweep-line, grid-based
 *   L6: DFM violation detection and reporting
 *   L7: PCB fabrication submission readiness
 *
 * Course mapping:
 *   Michigan EECS 411 Microwave -> fabrication tolerances for RF
 *   Georgia Tech ECE 6350 EM -> PCB manufacturing processes
 *   TU Munich High-Frequency Eng -> precision PCB fabrication
 */

#ifndef DFM_H
#define DFM_H

#include "gerber.h"
#include "pcb_geometry.h"
#include <stdint.h>

/* ─── DFM Rule Definitions ──────────────────────────────────────── */

/**
 * IPC classification for PCB quality/reliability.
 * Class 1: Consumer electronics (lowest requirements)
 * Class 2: Industrial/telecom (standard)
 * Class 3: High-reliability / aerospace / medical
 */
typedef enum {
    IPC_CLASS_1 = 1,
    IPC_CLASS_2 = 2,
    IPC_CLASS_3 = 3
} IPCClass;

/**
 * Copper weight specification.
 * Encoded as ounces per square foot (oz/ft²) or microns (µm).
 */
typedef enum {
    COPPER_0_5OZ = 0,   /** 0.5 oz/ft² ≈ 17.5 µm */
    COPPER_1OZ   = 1,   /** 1 oz/ft² ≈ 35 µm */
    COPPER_2OZ   = 2,   /** 2 oz/ft² ≈ 70 µm */
    COPPER_3OZ   = 3    /** 3 oz/ft² ≈ 105 µm */
} CopperWeight;

/** Convert copper weight in oz to thickness in millimeters */
#define COPPER_OZ_TO_MM(oz) ((oz) * 0.0348)

/**
 * DFM rule — minimum trace width.
 * Minimum width of a copper trace that the fab can reliably etch.
 *
 * IPC-2221 Table 6-1: For 1 oz copper, internal layers:
 *   Class 1: 0.150 mm, Class 2: 0.150 mm, Class 3: 0.200 mm
 * External layers are typically 0.025-0.050 mm larger due to plating.
 */
typedef struct {
    double  min_width_mm;       /** Minimum trace width in mm */
    int     is_external;        /** 1 for external layers (plated) */
    IPCClass ipc_class;         /** IPC classification */
} DFMRuleTraceWidth;

/**
 * DFM rule — minimum spacing (clearance).
 * Minimum air gap between copper features.
 *
 * IPC-2221 Table 6-1 specifies clearance based on voltage and altitude.
 * For low-voltage (<15V) designs:
 *   Class 1: 0.150 mm, Class 2: 0.150 mm, Class 3: 0.200 mm
 */
typedef struct {
    double  min_spacing_mm;     /** Minimum spacing in mm */
    double  max_voltage_v;      /** Maximum voltage between features */
    int     is_external;        /** 1 for external layers */
    IPCClass ipc_class;
} DFMRuleSpacing;

/**
 * DFM rule — annular ring.
 * Copper ring around a plated through-hole.
 * IPC-6012: Minimum annular ring = pad diameter - hole diameter.
 *
 * Class 2: 0.050 mm min external, 0.025 mm min internal
 * Class 3: 0.100 mm min external, 0.050 mm min internal
 */
typedef struct {
    double  min_annular_mm;     /** Minimum annular ring in mm */
    int     is_external;        /** 1 for external layers */
    IPCClass ipc_class;
} DFMRuleAnnularRing;

/**
 * DFM rule — solder mask clearance.
 * Gap between copper pad edge and solder mask opening.
 * IPC-7351 recommends: 0.05-0.10 mm expansion.
 *
 * Too small: mask encroaches on pad (solderability issues)
 * Too large: mask slivers / dam breakage between fine-pitch pads
 */
typedef struct {
    double  min_clearance_mm;   /** Minimum solder mask clearance */
    double  max_clearance_mm;   /** Maximum clearance (to prevent slivers) */
} DFMRuleSolderMask;

/**
 * DFM rule — aspect ratio (for vias).
 * Ratio of board thickness to minimum hole diameter.
 *
 * IPC-2221 §9.1.3: Aspect ratio ≤ 8:1 for standard plating,
 * ≤ 10:1 for advanced processes, ≤ 1:1 for laser microvias.
 */
typedef struct {
    double  max_aspect_ratio;   /** Maximum allowed aspect ratio */
    double  board_thickness_mm; /** Total board thickness */
    double  min_hole_mm;        /** Minimum drill hole diameter */
} DFMRuleAspectRatio;

/**
 * DFM rule — copper-to-edge clearance.
 * Minimum distance from copper feature to board edge.
 * Prevents copper exposure during depaneling.
 *
 * Typical: 0.25-0.50 mm, depending on routing tolerance.
 */
typedef struct {
    double  min_clearance_mm;
} DFMRuleEdgeClearance;

/**
 * DFM rule — silkscreen minimum line width.
 * Minimum stroke width for readable silkscreen.
 * Typical: 0.15 mm (standard), 0.10 mm (fine).
 */
typedef struct {
    double  min_line_width_mm;
} DFMRuleSilkscreen;

/**
 * DFM rule — via-in-pad.
 * Determines if vias are allowed inside SMD pads.
 * Generally discouraged (solder wicking) but allowed with
 * plugged/capped vias for fine-pitch BGA.
 */
typedef enum {
    VIA_IN_PAD_DISALLOWED     = 0,  /** Not allowed */
    VIA_IN_PAD_PLUGGED        = 1,  /** Allowed if plugged */
    VIA_IN_PAD_CAPPED_FILLED  = 2   /** Allowed if capped and filled */
} ViaInPadPolicy;

/**
 * Complete DFM rule set for a PCB design.
 * All rules are evaluated independently; violations are accumulated.
 */
typedef struct {
    DFMRuleTraceWidth    trace_width;
    DFMRuleSpacing       spacing;
    DFMRuleAnnularRing   annular_ring;
    DFMRuleSolderMask    solder_mask;
    DFMRuleAspectRatio   aspect_ratio;
    DFMRuleEdgeClearance edge_clearance;
    DFMRuleSilkscreen    silkscreen;
    ViaInPadPolicy       via_in_pad;
    IPCClass             ipc_class;
    CopperWeight         copper_weight;
    int                  layer_count;
    double               finished_thickness_mm;
} DFMRuleSet;

/**
 * Initialize a DFM rule set with IPC-2221 Class 2 defaults.
 * Class 2 is the most commonly specified for industrial electronics.
 *
 * Reference: IPC-2221A Generic Standard on Printed Board Design, Table 6-1
 */
void dfm_ruleset_init(DFMRuleSet *rules, IPCClass ipc_class,
                      int layer_count, double thickness_mm,
                      CopperWeight copper_wt);

/* ─── DFM Violation Types ───────────────────────────────────────── */

/** Violation severity */
typedef enum {
    DFM_SEV_INFO     = 0,  /** Informational — no action required */
    DFM_SEV_WARNING  = 1,  /** Warning — may cause yield issues */
    DFM_SEV_ERROR    = 2,  /** Error — likely manufacturing failure */
    DFM_SEV_CRITICAL = 3   /** Critical — guaranteed failure */
} DFMSeverity;

/** Violation category */
typedef enum {
    DFM_CAT_TRACE_WIDTH    = 0,
    DFM_CAT_SPACING        = 1,
    DFM_CAT_ANNULAR_RING   = 2,
    DFM_CAT_SOLDER_MASK    = 3,
    DFM_CAT_ASPECT_RATIO   = 4,
    DFM_CAT_EDGE_CLEARANCE = 5,
    DFM_CAT_SILKSCREEN     = 6,
    DFM_CAT_VIA_IN_PAD     = 7,
    DFM_CAT_ACID_TRAP      = 8,
    DFM_CAT_SLIVER         = 9,
    DFM_CAT_STARVED_THERMAL= 10,
    DFM_CAT_CUSTOM         = 99
} DFMCategory;

/** Single DFM violation record */
typedef struct {
    DFMCategory  category;         /** Violation category */
    DFMSeverity  severity;         /** Severity level */
    double       position_x_mm;    /** Location X in mm */
    double       position_y_mm;    /** Location Y in mm */
    double       measured_value;   /** Actual measured value (e.g., trace width) */
    double       threshold_value;  /** Rule threshold */
    int          layer;            /** PCB layer where violation occurs */
    char         description[256]; /** Human-readable description */
} DFMViolation;

/** Maximum violations buffered */
#define DFM_MAX_VIOLATIONS 4096

/** Violation report container */
typedef struct {
    DFMViolation violations[DFM_MAX_VIOLATIONS];
    int          count;
    int          by_category[16];  /** Count per category */
    int          by_severity[4];   /** Count per severity level */
} DFMViolationReport;

/* ─── DFM Checking API ──────────────────────────────────────────── */

/**
 * Initialize an empty violation report.
 */
void dfm_report_init(DFMViolationReport *report);

/**
 * Add a violation to the report.
 * Returns 0 on success, -1 if report is full.
 */
int dfm_report_add(DFMViolationReport *report, const DFMViolation *violation);

/**
 * Get the number of violations of a given severity or higher.
 */
int dfm_report_count_ge(const DFMViolationReport *report, DFMSeverity min_sev);

/**
 * Print a formatted DFM violation report to a file stream.
 */
void dfm_report_print(const DFMViolationReport *report, FILE *fp);

/* ─── DFM Rule Check Functions ──────────────────────────────────── */

/**
 * Check minimum trace width for a set of PCB traces.
 *
 * For each trace, if the narrowest point is below the threshold,
 * a violation is recorded.
 *
 * Algorithm: For each trace, iterate all segments and check width.
 * Complexity: O(N * S) where N = number of traces, S = avg segments.
 *
 * @param traces       Array of PCB traces
 * @param n_traces     Number of traces
 * @param rule         Trace width rule
 * @param layer        PCB layer number
 * @param report       Violation report (output)
 */
void dfm_check_trace_width(const PCBTrace *traces, int n_traces,
                           const DFMRuleTraceWidth *rule,
                           int layer, DFMViolationReport *report);

/**
 * Check minimum spacing between copper features.
 *
 * Implements pair-wise distance comparison. For large designs,
 * a sweep-line algorithm would be more efficient, but the brute-force
 * approach is correct and adequate for moderate designs.
 *
 * Algorithm: For all pairs of features, compute minimum distance
 * between their bounding geometries.
 * Complexity: O(N²) brute force. Reference: Bentley-Ottmann sweep-line O(N log N).
 *
 * @param features     Array of PCB features (traces, pads, pours)
 * @param n_features   Number of features
 * @param rule         Spacing rule
 * @param layer        PCB layer number
 * @param report       Violation report (output)
 */
void dfm_check_spacing(const PCBFeature *features, int n_features,
                       const DFMRuleSpacing *rule,
                       int layer, DFMViolationReport *report);

/**
 * Check annular ring for plated through-holes.
 *
 * Annular ring = (pad_diameter - hole_diameter) / 2
 * Must be >= min_annular_mm on each side.
 *
 * @param holes        Array of PTH definitions
 * @param n_holes      Number of holes
 * @param rule         Annular ring rule
 * @param layer        PCB layer number
 * @param report       Violation report (output)
 */
void dfm_check_annular_ring(const PCBHole *holes, int n_holes,
                            const DFMRuleAnnularRing *rule,
                            int layer, DFMViolationReport *report);

/**
 * Check solder mask clearance around pads.
 *
 * Mask opening = pad_size + 2 * expansion
 * Clearance must be within [min_clearance, max_clearance].
 *
 * @param pads         Array of pad definitions
 * @param n_pads       Number of pads
 * @param expansion_mm Solder mask expansion value
 * @param rule         Solder mask rule
 * @param layer        PCB layer
 * @param report       Violation report (output)
 */
void dfm_check_soldermask(const PCBPad *pads, int n_pads,
                          double expansion_mm,
                          const DFMRuleSolderMask *rule,
                          int layer, DFMViolationReport *report);

/**
 * Check via aspect ratio.
 *
 * Aspect ratio = board_thickness / hole_diameter
 * Violates if ratio > max_aspect_ratio.
 *
 * This is critical for reliability: high aspect ratio vias
 * are prone to plating voids and barrel cracking.
 *
 * @param vias         Array of via definitions
 * @param n_vias       Number of vias
 * @param rule         Aspect ratio rule
 * @param report       Violation report (output)
 */
void dfm_check_aspect_ratio(const PCBHole *vias, int n_vias,
                            const DFMRuleAspectRatio *rule,
                            DFMViolationReport *report);

/**
 * Check copper-to-board-edge clearance.
 *
 * Any copper feature closer than min_clearance to the board outline
 * is flagged.
 *
 * @param features     Array of features
 * @param n_features   Feature count
 * @param outline      Board outline polygon (closed)
 * @param n_outline    Number of outline vertices
 * @param rule         Edge clearance rule
 * @param layer        PCB layer
 * @param report       Violation report (output)
 */
void dfm_check_edge_clearance(const PCBFeature *features, int n_features,
                              const GerberPointD *outline, int n_outline,
                              const DFMRuleEdgeClearance *rule,
                              int layer, DFMViolationReport *report);

/**
 * Check for acid traps.
 *
 * An acid trap is a sharp inside corner (< 90°) where etchant
 * can pool, causing over-etching and trace necking.
 *
 * Algorithm: For each copper polygon, check interior angles.
 * Interior angle < 90° at a concave vertex = acid trap.
 *
 * @param polygons     Array of polygon features
 * @param n_polygons   Number of polygons
 * @param angle_thresh_deg  Maximum allowed interior angle (default 90°)
 * @param report       Violation report (output)
 */
void dfm_check_acid_traps(const PCBRegion *polygons, int n_polygons,
                          double angle_thresh_deg,
                          DFMViolationReport *report);

/**
 * Check for solder mask slivers.
 *
 * A sliver is a thin strip of solder mask between two closely-spaced pads.
 * Slivers break off during assembly, causing shorts.
 *
 * Detection: find mask bridge sections narrower than min_width.
 *
 * @param pads         Array of SMD pads
 * @param n_pads       Number of pads
 * @param expansion_mm Solder mask expansion
 * @param min_width_mm Minimum allowed mask bridge width
 * @param report       Violation report (output)
 */
void dfm_check_mask_slivers(const PCBPad *pads, int n_pads,
                            double expansion_mm, double min_width_mm,
                            DFMViolationReport *report);

/**
 * Check for starved thermal reliefs.
 *
 * Thermal relief pads connect to copper pours via narrow spokes.
 * Spoke width must be sufficient to carry current without fusing.
 *
 * IPC-2221 recommends spoke width ≥ trace width for the net.
 *
 * @param pads         Array of pad definitions
 * @param n_pads       Number of pads
 * @param pours        Array of copper pour regions
 * @param n_pours      Number of pours
 * @param min_spoke_mm Minimum spoke width
 * @param report       Violation report (output)
 */
void dfm_check_starved_thermals(const PCBPad *pads, int n_pads,
                                const PCBRegion *pours, int n_pours,
                                double min_spoke_mm,
                                DFMViolationReport *report);

/**
 * Perform a comprehensive DFM review on a complete PCB design.
 *
 * This runs all applicable checks and aggregates results.
 *
 * @param board        Complete PCB board definition
 * @param rules        DFM rule set
 * @param report       Output: violation report
 *
 * Returns: total number of ERROR and CRITICAL violations (0 = clean).
 *
 * Complexity: O(T² + P + V + A + S) where:
 *   T = number of features (traces + pads + pours)
 *   P = number of pads, V = vias, A = polygon vertices, S = silkscreen segments
 */
int dfm_review_complete(const PCBBoard *board, const DFMRuleSet *rules,
                        DFMViolationReport *report);

/* ─── Utility: DFM Readiness Score ──────────────────────────────── */

/**
 * Compute a DFM readiness score (0.0 to 1.0).
 *
 * Score = 1.0 - weighted_penalties
 *   CRITICAL: -0.25 per violation
 *   ERROR:    -0.10 per violation
 *   WARNING:  -0.02 per violation
 *   INFO:     -0.01 per violation
 *
 * Clamped to [0.0, 1.0]. Score ≥ 0.90 is considered "factory ready".
 */
double dfm_readiness_score(const DFMViolationReport *report);

#endif /* DFM_H */
