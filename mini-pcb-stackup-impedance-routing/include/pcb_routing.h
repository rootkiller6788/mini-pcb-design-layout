#ifndef PCB_ROUTING_H
#define PCB_ROUTING_H

#include "pcb_transmission_line.h"
#include "pcb_stackup.h"

/* ============================================================================
 * L1: Core Definitions — PCB Routing Rules and Constraints
 *
 * PCB routing translates electrical requirements into physical geometry:
 * trace widths, spacings, via placements, length matching, and breakout
 * strategies. Every rule derives from SI, EMI, manufacturability constraints.
 *
 * Key routing principles:
 *  - Controlled impedance routing (width/spacing per target Z₀)
 *  - Differential pair routing (coupled, matched length, constant spacing)
 *  - Length matching for timing-critical buses
 *  - Via minimization and proper return path
 * ========================================================================= */

/* L1: Single-ended routing segment */
typedef struct {
    double start_x_mm, start_y_mm;  /* Start coordinates */
    double end_x_mm,   end_y_mm;    /* End coordinates */
    double width_um;                /* Trace width (µm) */
    int    layer;                   /* Layer index (1 = top) */
    double length_mm;               /* Computed segment length */
} RouteSegment;

/* L1: Differential pair routing segment */
typedef struct {
    RouteSegment p_seg;             /* Positive trace */
    RouteSegment n_seg;             /* Negative trace */
    double spacing_um;              /* Edge-to-edge spacing */
    double p_n_length_diff_mm;      /* P-N length mismatch */
    double coupling_length_mm;      /* Length over which coupling applies */
} DiffPairSegment;

/* L1: Via placement */
typedef struct {
    double x_mm, y_mm;
    double drill_diameter_mm;
    double pad_diameter_mm;
    double antipad_diameter_mm;     /* Clearance in planes */
    int    start_layer;
    int    end_layer;
} ViaPlacement;

/* L1: Complete route (ordered list of segments + vias) */
typedef struct {
    const char *net_name;
    RouteSegment *segments;
    int           num_segments;
    ViaPlacement *vias;
    int           num_vias;
    double        total_length_mm;
    double        total_delay_ps;
    int           is_differential;
} PcbRoute;

/* L1: Routing constraint */
typedef struct {
    int    layer;                    /* -1 = all layers */
    double min_width_um;
    double max_width_um;
    double min_spacing_um;
    double max_length_mm;            /* 0 = no limit */
    double max_vias;
    double max_skew_ps;              /* For differential pairs */
} RoutingConstraint;

/* ===================================================================
 * L2: Core Concepts — Basic routing calculations
 * =================================================================== */

/* L2: Compute segment length (Euclidean + Manhattan estimates) */
double routing_segment_length(const RouteSegment *seg);
double routing_manhattan_length(const RouteSegment *seg);

/* L2: Compute propagation delay for a route (sum of segments * t_pd) */
double routing_total_delay(const PcbRoute *route,
                            const TransmissionLine *tl_per_layer[]);

/* L2: Check if a route satisfies all constraints */
int routing_check_constraints(const PcbRoute *route,
                               const RoutingConstraint *constraints,
                               int num_constraints);

/* ===================================================================
 * L3: Mathematical Structures — Differential pair analysis
 * =================================================================== */

/* L3: Compute differential pair intra-pair skew
 *     skew = |t_pd_P - t_pd_N|
 *     Acceptable skew < 0.15 · t_rise (general guideline) */
double routing_diff_pair_skew(const DiffPairSegment *seg,
                               const TransmissionLine *tl);

/* L3: Compute coupling ratio over a differential segment
 *     Ratio of coupled length to total length */
double routing_diff_pair_coupling_ratio(const DiffPairSegment *seg);

/* L3: Compute odd-mode and even-mode impedances along a differential route */
void routing_diff_pair_impedance_profile(const DiffPairSegment *seg,
                                          const TransmissionLine *tl,
                                          double *z_odd, double *z_even,
                                          int num_points);

/* L3: Compute differential to common-mode conversion due to asymmetry
 *     S_cd21 ≈ ΔZ₀/(2·Z₀) · sin(β·ΔL/2)
 *     where ΔZ₀ is impedance mismatch and ΔL is length mismatch */
double routing_diff_to_common_conversion(const DiffPairSegment *seg,
                                          const TransmissionLine *tl,
                                          double freq_hz);

/* ===================================================================
 * L4: Fundamental Laws — Length matching and timing
 * =================================================================== */

/* L4: Compute maximum allowable length mismatch for a timing budget
 *     ΔL_max = v_p · Δt_budget
 *     where Δt_budget is typically 10% of bit period for parallel buses */
double routing_max_length_mismatch(double vp_m_per_s, double timing_budget_ps);

/* L4: Compute required serpentine length for delay matching
 *     Given target delay mismatch → additional trace length needed */
double routing_serpentine_length(double delay_mismatch_ps, double t_pd_ps_per_mm);

/* L4: Compute serpentine geometry parameters
 *     For a given added length, computes amplitude and pitch
 *     that minimize coupling between serpentine segments */
typedef struct {
    double amplitude_mm;     /* Serpentine wave amplitude */
    double pitch_mm;         /* Distance between meander peaks */
    double segment_length_mm; /* Length of straight section in meander */
    int    num_meanders;     /* Number of meander cycles */
} SerpentineGeometry;

SerpentineGeometry routing_serpentine_design(double added_length_mm,
                                              double min_spacing_3x_mm);

/* L4: Trombone (accordion) delay matching structure design
 *     Alternative to serpentine for shorter delays.
 *     Uses a single fold-back loop with rounded corners. */
typedef struct {
    double trombone_width_mm;
    double trombone_extension_mm;
    double added_delay_ps;
} TromboneGeometry;

TromboneGeometry routing_trombone_design(double added_length_mm,
                                          double trace_width_mm,
                                          double trace_spacing_mm);

/* ===================================================================
 * L5: Algorithms — BGA breakout and routing optimization
 * =================================================================== */

/* L5: BGA breakout pattern design
 *     Given BGA pitch, generates escape routing parameters.
 *     Uses dog-bone fanout for 1.0mm pitch, via-in-pad for 0.8mm and below. */
typedef struct {
    double bga_pitch_mm;
    int    num_rows;
    int    num_columns;
    int    num_io_signals;
    int    num_pwr_gnd_pins;
} BgaPackage;

typedef struct {
    double min_trace_width_um;
    double min_spacing_um;
    double via_drill_mm;
    double via_pad_mm;
    int    num_routing_layers_needed;
    int    is_via_in_pad;         /* Via-in-pad required? */
    int    escape_layers_per_row;  /* Layers needed per row of escape */
} BgaBreakoutDesign;

BgaBreakoutDesign routing_bga_breakout(const BgaPackage *bga, int ipc_class);

/* L5: Optimize via count for a multi-segment route
 *     Finds layer transitions that minimize total via count */
int routing_minimize_vias(PcbRoute *route,
                           const RoutingConstraint *constraints,
                           int num_layers_available);

/* L5: Compute crosstalk between two parallel route segments
 *     Uses analytical coupled-line theory.
 *     NEXT (near-end) and FEXT (far-end) crosstalk. */
typedef struct {
    double NEXT_db;     /* Near-end crosstalk */
    double FEXT_db;     /* Far-end crosstalk */
    double coupling_length_mm;
    double max_saturation_db;
} CrosstalkResult;

CrosstalkResult routing_crosstalk_parallel_segments(
    const RouteSegment *aggressor, const RouteSegment *victim,
    const TransmissionLine *tl, double freq_hz, double rise_time_ps);

/* L5: Compute optimal trace separation for crosstalk budget
 *     Given maximum allowed NEXT/FEXT, finds minimum spacing */
double routing_min_spacing_for_crosstalk(double max_crosstalk_db,
                                          double coupling_length_mm,
                                          const TransmissionLine *tl,
                                          double freq_hz);

/* ===================================================================
 * L6: Canonical Problems — Standard routing scenarios
 * =================================================================== */

/* L6: DDR4 fly-by routing topology verification
 *     DDR4 uses fly-by topology for address/command/control bus.
 *     Checks timing at each SDRAM in the chain. */
typedef struct {
    int    num_ranks;
    double clock_freq_mhz;        /* DDR4: 1600-3200 MHz */
    double trace_lengths_mm[4];   /* Length from controller to each SDRAM */
    double segment_lengths_mm[4]; /* Length between SDRAMs */
    double skew_per_rank_ps[4];   /* Computed skew at each rank */
    int    timing_ok;
} Ddr4FlyByBus;

Ddr4FlyByBus routing_ddr4_flyby_check(double clock_freq_mhz,
                                       const RouteSegment *segments,
                                       int num_segments,
                                       const TransmissionLine *tl);

/* L6: PCIe lane-to-lane skew budget analysis
 *     PCIe spec requires max 1.6ns (Gen1/2) or 1.0ns (Gen3+) lane skew */
typedef struct {
    double lane_lengths_mm[16];   /* Length of each lane */
    int    num_lanes;
    double max_skew_ps;
    double worst_pair_skew_ps;
    double skew_budget_used_pct;
    int    compliant;
} PcieSkewBudget;

PcieSkewBudget routing_pcie_skew_check(const double *lane_lengths_mm,
                                        int num_lanes, int pcie_generation,
                                        const TransmissionLine *tl);

/* L6: DDR memory address bus length matching group optimization
 *     Finds optimal target length for a group of N nets
 *     that minimizes total serpentine added length. */
double routing_bus_matching_target(const double *trace_lengths_mm,
                                    int num_traces, double *serpentine_per_trace);

/* ===================================================================
 * L7: Applications — Industry protocol routing
 * =================================================================== */

/* L7: HDMI 2.1 routing requirements
 *     4 differential pairs + clock, 48 Gbps, tight skew control */
typedef struct {
    double max_length_mm;
    double max_intra_pair_skew_ps;
    double max_inter_pair_skew_ps;
    int    impedance_controlled;
} Hdmi21RoutingSpec;

int routing_hdmi21_validate(const PcbRoute *tmds_pairs[4],
                             const PcbRoute *clock_pair,
                             const Hdmi21RoutingSpec *spec,
                             const TransmissionLine *tl);

/* L7: USB-C routing requirements
 *     Multiple high-speed pairs (SSRX1/2, SSTX1/2) + SBU + CC */
int routing_usb_c_validate(const PcbRoute *ss_pairs[4],
                            const DiffPairSegment *diff_segs[4],
                            const TransmissionLine *tl);

/* L7: Ethernet 10GBASE-T (IEEE 802.3an) routing validation */
int routing_10gbase_t_validate(const PcbRoute *pairs[4],
                                const TransmissionLine *tl,
                                double total_length_m);

/* ===================================================================
 * L8: Advanced — High-speed routing techniques
 * =================================================================== */

/* L8: Fiber-weave effect mitigation through routing angle
 *     Routing at 10°-15° angle to glass weave prevents periodic εr variation.
 *     Computes effective skew reduction from angled routing. */
double routing_fiber_weave_skew_reduction(double angle_degrees,
                                           double trace_length_mm,
                                           double glass_pitch_mm);

/* L8: Via stub resonance frequency
 *     f_res = c / (4 · √εr · L_stub) for a quarter-wave stub
 *     Used to determine if backdrilling is needed. */
double routing_via_stub_resonance_freq(double stub_length_mm, double er_eff);

/* L8: When to backdrill: compute stub length budget for given bit rate */
double routing_max_stub_length(double bit_rate_gbps, double er_eff,
                                double margin_db);

/* L8: T-coil / peaking inductance for capacitive load compensation
 *     Used at receiver inputs to extend bandwidth.
 *     L_peak = R²·C_load / (4·ζ²) for maximally flat response */
double routing_tcoil_inductance(double termination_ohm, double load_cap_pf,
                                 double damping_factor);

#endif /* PCB_ROUTING_H */
