/**
 * @file    dfm_core.c
 * @brief   DFM Core Utilities - L1-L3 implementations
 *
 * @details Implements fundamental PCB DFM data structures:
 *          - IPC classification system
 *          - Substrate material database (FR4, Rogers, Polyimide, etc.)
 *          - Surface finish database (HASL, ENIG, OSP, etc.)
 *          - Copper weight conversion
 *          - Statistical process capability (Cp, Cpk)
 *          - Design rule checking result management
 *          - Normal CDF approximation for DPMO estimation
 *          - Overall Equipment Effectiveness (OEE)
 *
 * Knowledge Mapping:
 *   L1 - Definitions: IPC classes, substrate types, surface finishes,
 *        copper weight, via types, fiducial marks, Gerber layers
 *   L2 - Core Concepts: Process capability indices, DRC violation
 *        reporting, quality control metrics, OEE
 *   L3 - Math Structures: Statistical Cp/Cpk computation,
 *        Normal CDF rational approximation
 *
 * Reference: IPC-6012, IPC-4101, IPC-4562, IPC-2221
 *            Montgomery, "Statistical Quality Control" (2013)
 *            Nakajima, "Introduction to TPM" (1988)
 */

#include "dfm_core.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ================================================================
   L1 - IPC Class Names
   ================================================================

   IPC-6012 defines three performance classes for electronic products:
   - Class 1: General Electronic Products (consumer toys, low-cost)
   - Class 2: Dedicated Service Electronic Products (computers, telecom)
   - Class 3: High Reliability Electronic Products (medical, aerospace)

   Each class imposes progressively tighter manufacturing tolerances
   and inspection requirements. Cost increases ~30-50% per class step.
   ================================================================ */

const char* ipc_class_name(ipc_class_t cls)
{
    switch (cls) {
    case IPC_CLASS_1:
        return "Class 1 - General Electronic Products";
    case IPC_CLASS_2:
        return "Class 2 - Dedicated Service Electronic Products";
    case IPC_CLASS_3:
        return "Class 3 - High Reliability Electronic Products";
    default:
        return "Unknown IPC Class";
    }
}

/* ================================================================
   L1 - Substrate Material Database
   ================================================================

   IPC-4101 defines specification sheets for base materials.
   Each substrate has distinct electrical, thermal, and mechanical
   properties that determine its suitability for different applications.

   Key properties explained:
   - Dk (Dielectric Constant / Relative Permittivity):
     Affects signal propagation velocity: v = c / sqrt(Dk).
     Lower Dk = faster signals = higher impedance for same geometry.
     Rogers 4350B Dk=3.48 vs standard FR4 Dk=4.50

   - Df (Loss Tangent / Dissipation Factor):
     Determines dielectric losses at high frequency.
     Power loss per unit length: alpha_d = (pi * f * sqrt(Dk) * Df) / c
     At 10 GHz: FR4 loss ~0.047 dB/mm, Rogers 4350B ~0.009 dB/mm
     Critical for high-speed digital >1 Gbps and RF >1 GHz

   - Tg (Glass Transition Temperature):
     Temperature where material transitions from rigid glassy state
     to soft rubbery state. Above Tg, CTE increases ~10x in Z-axis.
     Reflow soldering peak temperature ~260C must not exceed Tg by
     too large a margin or delamination occurs.

   - CTE (Coefficient of Thermal Expansion):
     X/Y CTE: In-plane expansion, should match copper (~17 ppm/C)
     Z CTE: Through-thickness expansion, causes PTH barrel cracking
     IPC-4101 limits: CTE_Z < 50 ppm/C for FR4 below Tg

   - Halogen-free: Br + Cl < 900 ppm total (IEC 61249-2-21)
     Required for RoHS-compliant and WEEE-directive products
   ================================================================ */

static const substrate_properties_t substrate_database[] = {
    /* FR4 Standard - Most widely used, cost-effective.
     * Tg 140C is standard; high-Tg FR4 (170-180C) also common.
     * Suitable for consumer electronics, computing, automotive. */
    { SUBSTRATE_FR4,             "FR-4 (Standard)",
      4.50, 0.020, 140.0, 14.0, 50.0, 0.30, false },

    /* Rogers 4350B - Hydrocarbon ceramic laminate.
     * Low loss, stable Dk over frequency and temperature.
     * Used for: military radar, satellite communication,
     * 5G mmWave (28/39 GHz), automotive radar (77 GHz).
     * ~5-10x more expensive than FR4. */
    { SUBSTRATE_ROGERS_4350,     "Rogers 4350B",
      3.48, 0.004, 280.0, 10.0, 32.0, 0.69, false },

    /* Polyimide - High-temperature capable.
     * Tg 250C survives multiple lead-free reflow cycles.
     * Used for: flex/rigid-flex PCBs, aerospace, downhole.
     * Higher moisture absorption than FR4 (1.5% vs 0.2%). */
    { SUBSTRATE_POLYIMIDE,       "Polyimide",
      3.80, 0.005, 250.0, 12.0, 45.0, 0.12, false },

    /* Ceramic Alumina (Al2O3 96%) - Excellent thermal conductor.
     * High Dk=9.8 allows compact microwave circuits (smaller
     * wavelength at same frequency). Used for: high-power RF
     * amplifiers, LED heat-spreading substrates, hermetic packages.
     * Brittle, expensive, limited to small panels. */
    { SUBSTRATE_CERAMIC_ALUMINA, "Ceramic Alumina (Al2O3 96%)",
      9.80, 0.0003, 1600.0, 6.7, 7.0, 24.0, true },

    /* PTFE (Teflon) - Lowest loss tangent available commercially.
     * Dk=2.2 lowest, Df=0.0009 excellent for mmWave.
     * Used for: 77 GHz automotive radar, satellite transponders,
     * microwave test equipment. Mechanically soft, difficult to
     * process (cold flow), requires sodium etch for copper adhesion. */
    { SUBSTRATE_PTFE,            "PTFE (Teflon)",
      2.20, 0.0009, 250.0, 24.0, 100.0, 0.25, false },

    /* BT-Epoxy Blend - Bismaleimide Triazine + epoxy.
     * Popular in high-speed digital: DDR memory modules, PCIe
     * motherboard layers, networking switches.
     * Good balance of performance, processability, and cost. */
    { SUBSTRATE_BT_EPOXY,        "BT-Epoxy Blend",
      4.10, 0.010, 180.0, 13.0, 45.0, 0.35, false },

    /* CEM-1 - Composite Epoxy Material, paper core + woven glass surface.
     * Single-sided only (copper foil on one side). Lowest cost option.
     * Consumer electronics: toys, calculators, LED lighting strips,
     * power supplies (single-sided CEM-1 is standard). */
    { SUBSTRATE_CEM1,            "CEM-1 (Composite Epoxy)",
      4.80, 0.025, 110.0, 18.0, 55.0, 0.25, false },

    /* CEM-3 - Non-woven glass core + woven glass surface.
     * Double-sided capable, FR4 alternative at ~80% of cost.
     * Slightly lower mechanical strength and moisture resistance.
     * Consumer white goods: washing machines, microwaves, AC units. */
    { SUBSTRATE_CEM3,            "CEM-3 (Composite Epoxy)",
      4.60, 0.023, 130.0, 16.0, 52.0, 0.28, false },
};

static const int num_substrates =
    (int)(sizeof(substrate_database) / sizeof(substrate_database[0]));

const substrate_properties_t* substrate_lookup(substrate_material_t mat)
{
    if (mat < 0 || mat >= num_substrates) return NULL;
    /* Direct index access with safety check */
    if (substrate_database[mat].material != mat) {
        for (int i = 0; i < num_substrates; i++) {
            if (substrate_database[i].material == mat)
                return &substrate_database[i];
        }
        return NULL;
    }
    return &substrate_database[mat];
}

/* ================================================================
   L1 - Surface Finish Database
   ================================================================

   Surface finishes protect exposed copper pads from oxidation and
   provide a solderable surface for component assembly. Each finish
   involves specific trade-offs between flatness, shelf life, cost,
   wire bondability, and RoHS compliance.

   Key selection criteria:
   - Fine-pitch SMT (<0.5mm pitch): Requires flat finish (ENIG/OSP/ImAg)
   - Wire bonding: Requires ENIG, ENEPIG, or Hard Gold with Ni barrier
   - Press-fit connectors: Immersion Sn recommended (good lubricity)
   - Long shelf life: ENIG (12mo) or Hard Gold (24mo)
   - Lowest cost: OSP (0.7x) or HASL (0.9-1.0x)
   - RF performance: Immersion Ag (lowest insertion loss at GHz)

   Thickness notes:
   - ENIG: Ni 3-7um + Au 0.05-0.15um (IPC-4552)
   - ENEPIG: Ni 3-7um + Pd 0.05-0.15um + Au 0.03-0.08um (IPC-4556)
   - Hard Gold: Ni 2.5-5um + Au 0.5-1.5um (connector grade)
   - HASL: SnPb or SAC alloy, 1-25um depending on leveling

   References: IPC-4552 (ENIG), IPC-4553 (ImAg), IPC-4554 (ImSn),
               IPC-4556 (ENEPIG), IPC-6012 (qualification)
   ================================================================ */

static const finish_properties_t finish_database[] = {
    /* HASL with SnPb - Traditional finish, uneven surface.
     * Not RoHS compliant. Being phased out but still used in
     * military/aerospace where Pb exemption applies. */
    { FINISH_HASL_SNPB,    "HASL SnPb",
      20.0, 12.0, false, true,  0.90 },

    /* HASL Lead-Free (SAC305: Sn96.5/Ag3.0/Cu0.5) - RoHS replacement.
     * Higher process temperature (260C vs 230C) causes more
     * thermal shock. Uneven surface problematic for fine-pitch. */
    { FINISH_HASL_LF,      "HASL LF (SAC305)",
      15.0, 12.0, true,  true,  1.00 },

    /* ENIG (Electroless Nickel Immersion Gold) - Industry workhorse.
     * Flat surface (<0.5um flatness), excellent solderability.
     * Risk: "Black pad" syndrome - excessive Ni corrosion during
     * immersion Au step creates weak Ni-Sn intermetallic. */
    { FINISH_ENIG,         "ENIG (Ni/Au)",
      5.0,  12.0, true,  true,  1.40 },

    /* ENEPIG (Electroless Ni, Electroless Pd, Immersion Au).
     * Pd layer prevents Ni corrosion (eliminates black pad).
     * Best wire bonding performance. Most expensive. */
    { FINISH_ENEPIG,       "ENEPIG (Ni/Pd/Au)",
      3.0,  18.0, true,  true,  2.00 },

    /* OSP (Organic Solderability Preservative) - Simple organic coat.
     * Benzotriazole or imidazole-based. Thinnest finish (~0.2-0.5um).
     * Short shelf life, degrades after 2-3 reflow cycles.
     * Cannot be probed (insulating), not wire-bondable. */
    { FINISH_OSP,          "OSP (Organic)",
      0.5,   6.0, true,  false, 0.70 },

    /* Immersion Silver - Excellent for high-frequency.
     * Thin (0.1-0.6um Ag), low contact resistance.
     * Tarnishes in sulfur-rich environments (rubber, pollution).
     * Silver migration risk under DC bias + humidity. */
    { FINISH_IMMERSION_AG, "Immersion Silver",
      6.0,   9.0, true,  true,  1.10 },

    /* Immersion Tin - Good for press-fit applications.
     * Pure tin coating, excellent solderability.
     * Risk: Tin whisker growth (conductive filaments causing shorts).
     * Not recommended for fine-pitch due to whisker risk. */
    { FINISH_IMMERSION_SN, "Immersion Tin",
      5.0,   6.0, true,  false, 1.00 },

    /* Hard Gold (electrolytic Ni/Au) - Card-edge connectors.
     * Thick gold (0.5-1.5um) over nickel. Wear-resistant.
     * Not solderable (Au-Sn intermetallic embrittlement).
     * Used for: PCIe edge fingers, SIM card contacts, test points. */
    { FINISH_HARD_GOLD,    "Hard Gold (Ni/Au)",
      10.0, 24.0, true,  true,  2.50 },
};

static const int num_finishes =
    (int)(sizeof(finish_database) / sizeof(finish_database[0]));

const finish_properties_t* finish_lookup(surface_finish_t fin)
{
    if (fin < 0 || fin >= num_finishes) return NULL;
    if (finish_database[fin].finish != fin) {
        for (int i = 0; i < num_finishes; i++) {
            if (finish_database[i].finish == fin)
                return &finish_database[i];
        }
        return NULL;
    }
    return &finish_database[fin];
}

/* ================================================================
   L1 - Copper Weight Conversion
   ================================================================

   Copper weight in PCB industry is measured in ounces per square foot
   (oz/ft^2). This originates from the era when copper foil was
   manufactured by electrodeposition.

   Conversion factors:
     1 oz/ft^2 copper = 1.37 mils = 34.79 micrometers

   Standard copper weights and their applications:
   +---------+-----------+----------------------------------+
   | Weight  | Thickness | Typical Application              |
   +---------+-----------+----------------------------------+
   | 0.5 oz  | 17.4 um   | Inner layers, fine-pitch SMT     |
   | 1.0 oz  | 34.8 um   | Standard signal layers           |
   | 1.5 oz  | 52.2 um   | Power+signal mixed, automotive   |
   | 2.0 oz  | 69.6 um   | Power planes, high-current traces|
   | 3.0 oz  | 104.4 um  | High current (>5A), motor control|
   | 4.0 oz  | 139.2 um  | Power distribution, bus bars     |
   | 6.0 oz  | 208.8 um  | Extreme current, welding equipment|
   +---------+-----------+----------------------------------+

   External layers receive additional electroplating during the
   through-hole process (~20-25 um additional copper), making them
   thicker than the base foil. Internal layers are not plated.
   ================================================================ */

static const double copper_um_per_oz     = 34.79;
static const double plating_thickness_um = 22.5;

double copper_weight_to_um(copper_weight_t cw, bool plated)
{
    double base_um;
    switch (cw) {
    case CU_WEIGHT_0_5_OZ: base_um = 0.5 * copper_um_per_oz; break;
    case CU_WEIGHT_1_0_OZ: base_um = 1.0 * copper_um_per_oz; break;
    case CU_WEIGHT_2_0_OZ: base_um = 2.0 * copper_um_per_oz; break;
    case CU_WEIGHT_3_0_OZ: base_um = 3.0 * copper_um_per_oz; break;
    case CU_WEIGHT_4_0_OZ: base_um = 4.0 * copper_um_per_oz; break;
    case CU_WEIGHT_6_0_OZ: base_um = 6.0 * copper_um_per_oz; break;
    default:               return 0.0;
    }
    return plated ? (base_um + plating_thickness_um) : base_um;
}

/* ================================================================
   L3 - Process Capability (Cp, Cpk)
   ================================================================

   Process capability indices are statistical measures that quantify
   how well a manufacturing process produces output within the
   specification limits (tolerance band).

   Mathematical definitions:

     Cp = (USL - LSL) / (6 * sigma)
        = Specification Width / Process Spread
        = Measures process POTENTIAL (ignores centering)

     Cpk_lower = (mu - LSL) / (3 * sigma)
     Cpk_upper = (USL - mu) / (3 * sigma)
     Cpk       = min(Cpk_lower, Cpk_upper)
        = Measures process PERFORMANCE (accounts for centering)

   The factor of 6 in Cp represents +/-3 sigma, covering 99.73% of
   a normal distribution. Cpk uses 3 sigma to measure the distance
   from mean to nearest specification limit.

   Industry interpretation:
     Cpk >= 1.67 : 6-Sigma process (<3.4 DPMO), world-class
     Cpk >= 1.33 : 4-Sigma process (~63 DPMO), fully capable
     Cpk >= 1.00 : 3-Sigma process (~2700 DPMO), marginally capable
     Cpk >= 0.67 : 2-Sigma process (~45500 DPMO), needs improvement
     Cpk <  0.67 : Not capable, process redesign required

   In PCB manufacturing, typical capability targets:
     - Trace width/space: Cpk >= 1.33 (tightest requirement)
     - Drill registration: Cpk >= 1.0
     - Solder mask registration: Cpk >= 1.0
     - Board outline routing: Cpk >= 0.67

   Reference: Kane, "Process Capability Indices", J. Quality Tech (1986)
              Montgomery, "Statistical Quality Control" 8th ed., Wiley
   ================================================================ */

process_capability_t compute_process_capability(
    const double *measurements, size_t N,
    double usl_um, double lsl_um)
{
    process_capability_t cap;
    memset(&cap, 0, sizeof(cap));

    /* Input validation */
    if (!measurements || N < 2 || usl_um <= lsl_um) {
        cap.capable = false;
        return cap;
    }

    /* Step 1: Compute sample mean.
       mu_hat = (1/N) * sum(x_i)   for i=1..N */
    double sum = 0.0;
    for (size_t i = 0; i < N; i++) {
        sum += measurements[i];
    }
    double mean = sum / (double)N;

    /* Step 2: Compute sample standard deviation.
       s = sqrt( (1/(N-1)) * sum((x_i - mu_hat)^2) )
       Using Bessel's correction (N-1) for unbiased estimate. */
    double sum_sq_diff = 0.0;
    for (size_t i = 0; i < N; i++) {
        double diff = measurements[i] - mean;
        sum_sq_diff += diff * diff;
    }
    double stddev = sqrt(sum_sq_diff / (double)(N - 1));

    /* Edge case: zero variance (all measurements identical) */
    if (stddev < 1e-12) {
        cap.cp        = 100.0;
        cap.cpk       = 100.0;
        cap.cpk_upper = 100.0;
        cap.cpk_lower = 100.0;
        cap.mean_um   = mean;
        cap.stddev_um = 0.0;
        cap.usl_um    = usl_um;
        cap.lsl_um    = lsl_um;
        cap.capable   = true;
        return cap;
    }

    /* Step 3: Compute capability indices */
    cap.cp        = (usl_um - lsl_um) / (6.0 * stddev);
    cap.cpk_lower = (mean - lsl_um) / (3.0 * stddev);
    cap.cpk_upper = (usl_um - mean) / (3.0 * stddev);
    cap.cpk       = (cap.cpk_lower < cap.cpk_upper)
                    ? cap.cpk_lower : cap.cpk_upper;
    cap.mean_um   = mean;
    cap.stddev_um = stddev;
    cap.usl_um    = usl_um;
    cap.lsl_um    = lsl_um;
    cap.capable   = (cap.cpk >= 1.0);
    return cap;
}

/* ================================================================
   L2 - DRC Result Management
   ================================================================

   Design Rule Checking (DRC) is the automated verification that a
   PCB layout conforms to manufacturing constraints. This is a
   critical DFM step that prevents costly re-spins.

   Severity classification:
   - FATAL: Design will certainly fail manufacturing. Examples:
            board outline too large for panel, wrong layer stackup
   - ERROR: Violates IPC rule at checked class level. Examples:
            trace width below minimum, annular ring insufficient
   - WARNING: Marginal condition that may fail under process variation.
            Examples: trace spacing near minimum, via aspect ratio >6:1
   - INFO: Advisory note for designer consideration.
            Examples: silkscreen near pad, copper pour isolated

   The DRC result aggregator collects all violations, tracks pass/fail
   status, and supports formatted reporting for design review.
   ================================================================ */

void drc_result_init(drc_result_t *result, ipc_class_t ipc_class,
                     size_t capacity)
{
    if (!result) return;

    memset(result, 0, sizeof(drc_result_t));
    result->checked_class = ipc_class;
    result->passed        = true;

    if (capacity > 0) {
        result->violations = (drc_violation_t*)
            malloc(capacity * sizeof(drc_violation_t));
        if (result->violations) {
            result->capacity = capacity;
        }
    }
}

void drc_result_add(drc_result_t *result, const drc_violation_t *v)
{
    if (!result || !v) return;

    /* Dynamic array growth using geometric expansion (factor 2).
     * This gives amortized O(1) insertion time. */
    if (result->num_violations >= result->capacity) {
        size_t new_cap = result->capacity > 0
                         ? result->capacity * 2 : 16;
        drc_violation_t *new_arr = (drc_violation_t*)
            realloc(result->violations,
                    new_cap * sizeof(drc_violation_t));
        if (!new_arr) return; /* allocation failure - silent */
        result->violations = new_arr;
        result->capacity   = new_cap;
    }

    /* Append violation and update statistics */
    result->violations[result->num_violations++] = *v;

    switch (v->severity) {
    case DRC_FATAL:
        result->num_fatal++;
        result->passed = false;
        break;
    case DRC_ERROR:
        result->num_errors++;
        result->passed = false;
        break;
    case DRC_WARN:
        result->num_warnings++;
        break;
    case DRC_INFO:
        result->num_info++;
        break;
    }
}

void drc_result_free(drc_result_t *result)
{
    if (!result) return;
    free(result->violations);
    result->violations     = NULL;
    result->num_violations = 0;
    result->capacity       = 0;
}

void drc_result_report(const drc_result_t *result, bool show_all)
{
    if (!result) {
        printf("DRC Report: NULL result\n");
        return;
    }

    printf("\n");
    printf("========================================\n");
    printf("  DRC Analysis Report\n");
    printf("========================================\n");
    printf("IPC Class:       %s\n",
           ipc_class_name(result->checked_class));
    printf("Runtime:         %.3f sec\n",
           result->total_runtime_sec);
    printf("Overall Status:  %s\n",
           result->passed ? "PASSED" : "FAILED");
    printf("----------------------------------------\n");
    printf("Violation Summary:\n");
    printf("  Total:  %zu\n", result->num_violations);
    printf("  Fatal:  %zu\n", result->num_fatal);
    printf("  Errors: %zu\n", result->num_errors);
    printf("  Warn:   %zu\n", result->num_warnings);
    printf("  Info:   %zu\n", result->num_info);
    printf("========================================\n");

    if (!show_all || result->num_violations == 0) return;

    printf("\nViolation Details:\n");
    printf("--------------------\n");
    const char *sev_names[] = {"FATAL", "ERROR", "WARN", "INFO"};
    for (size_t i = 0; i < result->num_violations; i++) {
        const drc_violation_t *v = &result->violations[i];
        printf("\n  %zu. [%s] %s\n",
               i + 1, sev_names[v->severity], v->rule_name);
        printf("  %s\n", v->message);
        printf("  Measured: %6.1f um   Required: %6.1f um   "
               "Margin: %+5.1f um\n",
               v->measured_value_um, v->required_value_um,
               v->margin_um);
        printf("  Location: (%6.2f, %6.2f) mm  Layer %d  "
               "Net: %s  Component: %s\n",
               v->location.x_mm, v->location.y_mm,
               v->location.layer,
               v->location.net_name ? v->location.net_name : "(none)",
               v->location.component ? v->location.component : "(none)");
    }
}

/* ================================================================
   L2 - Via Geometry Checks
   ================================================================

   Via annular ring is the copper pad remaining around a drilled
   hole after the drill process. It is critical for:
   1. Reliable solder joint formation (enough copper for fillet)
   2. Electrical connection to inner layers
   3. Mechanical strength of the plated barrel

   Annular ring calculation:
     ring = (pad_diameter - drill_diameter) / 2

   IPC-2221 / IPC-6012 requirements:
     Class 1: Min 50 um external (breakout permitted)
     Class 2: Min 50 um external, no breakout (90 deg tangent OK)
     Class 3: Min 50 um external + 25 um internal, zero breakout

   Aspect Ratio (board_thickness / drill_diameter):
     Limits the maximum board thickness for a given hole size.
     - Standard mechanical drill: max 8:1
     - Advanced mechanical drill: max 10:1 (with special processes)
     - Laser microvia: max 1:1 (depth <= diameter for plating)
     - Stacked microvia: max 1.5:1

   High aspect ratio risks:
     - Drill wander (bit deflection increases with depth)
     - Poor plating uniformity (acid flow, current distribution)
     - Barrel cracking during thermal cycling (CTE mismatch stress)
   ================================================================ */

bool via_annular_ring_ok(const via_geometry_t *via, ipc_class_t ipc_class)
{
    if (!via || via->drill_diameter_mm <= 0.0) return false;

    /* Annular ring = (pad_diameter - drill_diameter) / 2
       Multiply by 1000 to convert mm to um */
    double ring_um = (via->pad_diameter_mm - via->drill_diameter_mm)
                     * 500.0; /* /2 * 1000 */

    double min_ring_um = 50.0; /* IPC minimum for all classes */

    /* For buried vias (internal only), Class 3 allows 25um */
    if (via->type == VIA_BURIED && ipc_class == IPC_CLASS_3) {
        min_ring_um = 25.0;
    }

    if (ring_um < (min_ring_um - 1e-9)) return false;

    /* Class 3 requires zero breakout. Tangential contact
       (where drill edge exactly touches pad edge) is not acceptable.
       Need at least 5um margin for process variation. */
    if (ipc_class == IPC_CLASS_3 && ring_um < 5.0) {
        return false;
    }

    return true;
}

bool via_aspect_ratio_ok(const via_geometry_t *via)
{
    if (!via || via->drill_diameter_mm <= 0.0) return false;

    double depth_mm;

    switch (via->type) {
    case VIA_THROUGH_HOLE:
        /* Standard 1.6mm PCB thickness for through-hole */
        depth_mm = 1.6;
        break;
    case VIA_BLIND:
        /* Blind via goes from outer layer to first inner layer.
           Typically ~100um dielectric thickness (1 prepreg). */
        depth_mm = 0.1;
        break;
    case VIA_BURIED:
        /* Buried via connects internal layers.
           Each dielectric layer is typically 0.1-0.2mm.
           Minimum 1 dielectric layer. */
        depth_mm = (double)abs(via->end_layer - via->start_layer) * 0.2;
        if (depth_mm < 0.1) depth_mm = 0.1;
        break;
    case VIA_MICROVIA:
        /* Laser-drilled microvia, typically 100um deep */
        depth_mm = 0.1;
        break;
    case VIA_STACKED:
        /* Stacked microvias: two layers of microvias stacked.
           Total depth ~200um */
        depth_mm = 0.2;
        break;
    case VIA_STAGGERED:
        /* Staggered microvias: offset, each ~100um */
        depth_mm = 0.1;
        break;
    default:
        depth_mm = 1.6;
        break;
    }

    double ar = depth_mm / via->drill_diameter_mm;

    switch (via->type) {
    case VIA_THROUGH_HOLE: return ar <= 8.0;
    case VIA_BLIND:        return ar <= 6.0;
    case VIA_BURIED:       return ar <= 8.0;
    case VIA_MICROVIA:     return ar <= 1.0;
    case VIA_STACKED:      return ar <= 1.5;
    case VIA_STAGGERED:    return ar <= 1.0;
    default:               return ar <= 8.0;
    }
}

/* ================================================================
   L3 - Normal CDF Approximation
   ================================================================

   For DPMO estimation and statistical tolerance analysis, we need
   the standard normal cumulative distribution function Phi(z).

   We use the rational approximation from:
     Abramowitz & Stegun, Handbook of Mathematical Functions,
     Formula 26.2.17 (1964)

   Maximum absolute error < 7.5e-8 for all real z.

   Phi(z) = 1 - phi(z) * (b1*t + b2*t^2 + b3*t^3 + b4*t^4 + b5*t^5)
   where:
     phi(z) = (1/sqrt(2*pi)) * exp(-z^2/2)     [standard normal PDF]
     t      = 1 / (1 + p * |z|)
     p      = 0.2316419
     b1..b5 = {0.319381530, -0.356563782, 1.781477937,
               -1.821255978, 1.330274429}
   ================================================================ */

static double normal_cdf_approx(double z)
{
    const double p  = 0.2316419;
    const double b1 = 0.319381530;
    const double b2 = -0.356563782;
    const double b3 = 1.781477937;
    const double b4 = -1.821255978;
    const double b5 = 1.330274429;

    double abs_z = fabs(z);
    double t     = 1.0 / (1.0 + p * abs_z);

    /* Standard normal PDF at z */
    double phi_z = (1.0 / sqrt(2.0 * M_PI)) * exp(-0.5 * z * z);

    /* Horner's method for polynomial evaluation */
    double poly = ((((b5 * t + b4) * t + b3) * t + b2) * t + b1) * t;

    double P = 1.0 - phi_z * poly;

    /* Symmetry: Phi(-z) = 1 - Phi(z) */
    return (z >= 0.0) ? P : (1.0 - P);
}

/**
 * Compute Defects Per Million Opportunities (DPMO) from Cpk.
 *
 * Assuming a centered normal distribution:
 *   DPMO = 2 * [1 - Phi(3 * Cpk)] * 10^6
 *
 * The factor 3*Cpk maps capability index to sigma distance.
 * Multiplication by 2 accounts for both tails.
 *
 * For reference:
 *   Cpk=0.67 -> DPMO ~45,500 (2 sigma)
 *   Cpk=1.00 -> DPMO ~2,700  (3 sigma)
 *   Cpk=1.33 -> DPMO ~63     (4 sigma)
 *   Cpk=1.67 -> DPMO ~0.57   (5 sigma)
 *   Cpk=2.00 -> DPMO ~0.002  (6 sigma)
 */
double compute_dpmo_from_cpk(double cpk)
{
    if (cpk <= 0.0) return 1e6;  /* 100% defective */
    if (cpk >= 5.0) return 0.0;  /* essentially zero defects */

    double z_score   = 3.0 * cpk;
    double tail_prob = 1.0 - normal_cdf_approx(z_score);
    return 2.0 * tail_prob * 1e6;
}

/* ================================================================
   L2 - Overall Equipment Effectiveness (OEE)
   ================================================================

   OEE is the gold standard for measuring manufacturing productivity.
   It combines three factors into a single metric:

   OEE = Availability * Performance * Quality

   Availability = Operating_Time / Planned_Production_Time
     (Was the machine available when needed?)

   Performance = (Ideal_Cycle_Time * Total_Units_Produced) / Operating_Time
     (Was the machine running at its designed speed?)

   Quality = Good_Units / Total_Units_Produced
     (What percentage of output was defect-free?)

   World-class manufacturing benchmarks:
     OEE >= 85% : World class
     OEE 60-85% : Typical, room for improvement
     OEE < 60%  : Significant improvement opportunities

   For PCB/SMT manufacturing:
     - Typical SMT line: 65-75% OEE
     - Best-in-class (Industry 4.0): 85-92% OEE
     - Major losses: changeover time, solder paste issues,
       component feeder jams, reflow defects

   Reference: Nakajima, "Introduction to Total Productive
              Maintenance (TPM)", Productivity Press (1988)
   ================================================================ */

double compute_pcb_oee(double planned_time_hr, double downtime_hr,
                       double ideal_cycle_sec, double total_units,
                       double good_units)
{
    /* Input validation */
    if (planned_time_hr <= 0.0 ||
        ideal_cycle_sec <= 0.0 ||
        total_units     <= 0.0) {
        return 0.0;
    }

    double operating_time_hr = planned_time_hr - downtime_hr;
    if (operating_time_hr <= 0.0) return 0.0;

    /* Availability factor */
    double availability = operating_time_hr / planned_time_hr;

    /* Performance factor:
       Ideal output = operating_time / ideal_cycle_time
       Performance = actual_output / ideal_output */
    double operating_time_sec = operating_time_hr * 3600.0;
    double ideal_output = operating_time_sec / ideal_cycle_sec;
    double performance  = (ideal_output > 0.0)
                          ? total_units / ideal_output : 0.0;

    /* Quality factor */
    double quality = (total_units > 0.0)
                     ? good_units / total_units : 0.0;

    /* Clamp each factor to [0, 1] */
    if (availability > 1.0) availability = 1.0;
    if (performance  > 1.0) performance  = 1.0;
    if (quality      > 1.0) quality      = 1.0;

    return availability * performance * quality;
}
