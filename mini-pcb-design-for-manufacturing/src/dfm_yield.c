/**
 * @file    dfm_yield.c
 * @brief   PCB Manufacturing Yield Models - L3-L5
 *
 * @details Yield prediction models for PCB/VLSI manufacturing:
 *          - Poisson yield model (random defects, independent)
 *          - Murphy yield model (uniform defect density distribution)
 *          - Seeds yield model (exponential defect distribution)
 *          - Negative Binomial yield model (defect clustering)
 *          - Monte Carlo yield simulation
 *          - Critical area estimation from layout parameters
 *          - Panel yield from individual board yield
 *          - Required defect density for target yield
 *
 * Knowledge Mapping:
 *   L3 - Mathematical Structures: Poisson process, compound
 *        probability distributions, Monte Carlo methods
 *   L4 - Fundamental Laws:
 *     Poisson yield:  Y = exp(-A*D)
 *     Murphy yield:   Y = ((1-exp(-A*D))/(A*D))^2
 *     Seeds yield:    Y = exp(-sqrt(A*D))
 *     Neg Binomial:   Y = (1 + A*D/alpha)^(-alpha)
 *   L5 - Algorithms: Monte Carlo simulation, yield optimization
 *
 * Historical Reference:
 *   B.T. Murphy, "Cost-size optima of monolithic integrated circuits",
 *   Proceedings of the IEEE, vol. 52, no. 12, pp. 1537-1545, 1964.
 *
 *   R.B. Seeds, "Yield and cost analysis of bipolar LSI",
 *   IEEE International Electron Devices Meeting, 1967.
 *
 *   C.H. Stapper, "LSI yield modeling and process monitoring",
 *   IBM J. Res. Dev., vol. 20, no. 3, pp. 228-234, 1976.
 *
 *   J.A. Cunningham, "The use and evaluation of yield models in
 *   integrated circuit manufacturing", IEEE Trans. Semicond. Manuf.,
 *   vol. 3, no. 2, pp. 60-71, 1990.
 */

#include "dfm_yield.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/* ================================================================
   L4 - Poisson Yield Model
   ================================================================
   Formula: Y = exp(-A * D)

   Assumptions:
   - Defects occur randomly and independently
   - Defects follow a Poisson spatial distribution
   - Every defect in the critical area causes a failure
   - No clustering of defects

   This is the most pessimistic model (lowest yield), because it
   assumes a constant, uniform defect density with no clustering.

   Historical significance: First yield model developed at Bell Labs
   for transistor manufacturing in the 1960s.

   In practice, Poisson underestimates yields for larger die because:
   - Real defects cluster (lithography-related, wafer edge)
   - Not all defects in the critical area cause failures
   - Redundancy can recover some failures (memory repair)

   Parameters:
     A = critical area (cm^2) - the area where a defect would cause
         a failure. Typically 10-30% of total chip/board area.
     D = defect density (defects/cm^2) - average number of fatal
         defects per unit area.

   Example: A = 1 cm^2, D = 0.5 defects/cm^2
            Y = exp(-0.5) = 0.6065 = 60.7% yield
   ================================================================ */

double yield_poisson(double critical_area_cm2, double defect_density)
{
    if (critical_area_cm2 <= 0.0 || defect_density < 0.0) return 0.0;

    double AD = critical_area_cm2 * defect_density;
    return exp(-AD);
}

/* ================================================================
   L4 - Murphy Yield Model
   ================================================================
   Formula: Y = ((1 - exp(-A*D)) / (A*D))^2

   Assumptions:
   - Defect density varies uniformly from 0 to 2*D across the wafer
   - Each location has a fixed, but unknown, defect density
   - Integrates Poisson yield over the uniform distribution

   Derivation:
     Y_Murphy = integral_0^{2D}( exp(-A*x) * 1/(2D) dx )
              = (1/(2D)) * [ -exp(-A*x)/A ]_0^{2D}
              = (1 - exp(-2A*D)) / (2A*D)
     Then squared to account for two independent processing steps
     (front-end + back-end or photolith + etch).

   Murphy's insight: real manufacturing lines do not have uniform
   defect density across the wafer. The center may have more defects
   (photoresist spin issues) or the edges (handling damage). By
   assuming a uniform distribution of defect densities, the model
   becomes more realistic than Poisson.

   This model is intermediate between Poisson (pessimistic) and
   Seeds (optimistic).
   ================================================================ */

double yield_murphy(double critical_area_cm2, double defect_density)
{
    if (critical_area_cm2 <= 0.0 || defect_density <= 0.0) {
        return 1.0; /* no defects = perfect yield */
    }

    double AD = critical_area_cm2 * defect_density;

    /* Guard against numerical issues when AD is very small */
    if (AD < 1e-9) return 1.0;

    /* (1 - exp(-A*D)) / (A*D) */
    double term = (1.0 - exp(-AD)) / AD;

    return term * term;
}

/* ================================================================
   L4 - Seeds Yield Model
   ================================================================
   Formula: Y = exp(-sqrt(A * D))

   Assumptions:
   - Defect density follows an exponential distribution:
     f(D) = (1/D_avg) * exp(-D/D_avg)
   - High defect density regions are rare but have large impact
   - Integrates Poisson yield over the exponential distribution

   Derivation:
     Y_Seeds = integral_0^inf( exp(-A*x) * (1/D) * exp(-x/D) dx )
             = 1 / (1 + A*D)
     Then empirically modified to: exp(-sqrt(A*D))

   Seeds observed that the simple exponential integral gave
   Y = 1/(1+A*D), but real data showed Y = exp(-sqrt(A*D)) fit
   much better. This empirical modification has been validated
   by decades of manufacturing data.

   This is the most optimistic model (highest yield) for large
   critical areas, because defects are assumed to be heavily
   concentrated in a few areas, leaving most of the chip/board
   defect-free.
   ================================================================ */

double yield_seeds(double critical_area_cm2, double defect_density)
{
    if (critical_area_cm2 <= 0.0 || defect_density < 0.0) return 0.0;

    return exp(-sqrt(critical_area_cm2 * defect_density));
}

/* ================================================================
   L4 - Negative Binomial Yield Model (Clustering)
   ================================================================
   Formula: Y = (1 + A*D/alpha)^(-alpha)

   This is the most widely used yield model in semiconductor
   manufacturing because it can capture defect clustering.

   Parameters:
     A     = Critical area (cm^2)
     D     = Average defect density (defects/cm^2)
     alpha = Clustering parameter (>0)

   Alpha interpretation:
   - alpha -> infinity  : Approaches Poisson (no clustering)
   - alpha -> 1         : Moderate clustering
   - alpha = 0.5-1      : Strong clustering (typical for logic)
   - alpha < 0.5        : Very strong clustering (memory, large die)
   - alpha -> 0         : Approaches Y = 1 (all defects in one spot)

   The clustered model gives higher yields than Poisson because
   defects are concentrated in a small fraction of the area.
   Only the boards/dice in those cluster areas fail.

   Historical context: Stapper at IBM (1976) introduced this model
   for LSI memory yield prediction. It effectively explained why
   observed yields were higher than the Poisson model predicted
   and why yield did not degrade as rapidly with area as Poisson
   suggested.

   The negative binomial arises naturally from a Poisson process
   with a gamma-distributed rate parameter (compound distribution).
   ================================================================ */

double yield_neg_binomial(double critical_area_cm2, double defect_density,
                           double alpha)
{
    if (critical_area_cm2 <= 0.0 || defect_density < 0.0
        || alpha <= 0.0) {
        return 0.0;
    }

    double AD_over_alpha = critical_area_cm2 * defect_density / alpha;

    /* Y = (1 + AD/alpha)^(-alpha) */
    return pow(1.0 + AD_over_alpha, -alpha);
}

/* ================================================================
   L5 - Monte Carlo Yield Simulation
   ================================================================

   Monte Carlo simulation generates many random instances of the
   manufacturing process and counts the fraction that yield good
   boards. This approach can model complex defect distributions
   that don't have closed-form solutions.

   Algorithm:
   1. For each simulation run i:
      a. Generate random defect count n_i from the chosen distribution
      b. For defect-dependent models, generate n_i random densities
      c. Compute board yield for this run
      d. Accumulate: total_yield += board_yield_i
   2. Yield = total_yield / num_simulations
   3. Compute 95% confidence interval:
      CI = 1.96 * sigma_yield / sqrt(N)

   Pseudo-random number generation uses a simple Linear Congruential
   Generator (Park & Miller "Minimal Standard"):
     X_{n+1} = (16807 * X_n) mod (2^31 - 1)

   For uniform random numbers:
     U = X / (2^31 - 1)

   For exponential random numbers (inverse CDF method):
     E = -ln(U) / lambda

   For Poisson random numbers (Knuth's method):
     Generate exponentials until sum > 1, count = n-1
   ================================================================ */

/* Park & Miller "Minimal Standard" LCG */
static unsigned int lcg_state = 1;

static void set_lcg_seed(unsigned int seed)
{
    lcg_state = (seed == 0) ? 1 : seed;
}

static double lcg_uniform(void)
{
    lcg_state = (unsigned int)(((unsigned long)lcg_state * 16807UL)
                               % 2147483647UL);
    return (double)lcg_state / 2147483647.0;
}

/* Generate Poisson distributed random number with mean lambda */
static int poisson_random(double lambda)
{
    if (lambda < 1e-9) return 0;
    if (lambda > 30.0) {
        /* Normal approximation for large lambda */
        double z = sqrt(lambda) * 2.0 * (lcg_uniform() - 0.5);
        int k = (int)(lambda + z + 0.5);
        return (k < 0) ? 0 : k;
    }
    /* Knuth's algorithm for small lambda */
    double L = exp(-lambda);
    int k = 0;
    double p = 1.0;
    do {
        k++;
        p *= lcg_uniform();
    } while (p > L && k < 1000);
    return k - 1;
}

yield_result_t yield_monte_carlo(double critical_area_cm2,
                                   double defect_density,
                                   yield_model_type_t model,
                                   double alpha,
                                   size_t num_simulations,
                                   unsigned int seed)
{
    yield_result_t result;
    memset(&result, 0, sizeof(result));

    if (critical_area_cm2 <= 0.0 || defect_density < 0.0
        || num_simulations < 100) {
        result.is_economical = false;
        return result;
    }

    set_lcg_seed(seed);

    result.model = model;
    result.critical_area_cm2 = critical_area_cm2;
    result.defect_density = defect_density;
    result.clustering_alpha = alpha;

    /* Storage for individual run yields (for CI calculation) */
    double *run_yields = NULL;
    if (num_simulations > 100000) {
        /* For very large N, approximate CI without storing all values */
        run_yields = NULL;
    } else {
        run_yields = (double*)malloc(num_simulations * sizeof(double));
    }

    double sum_yield = 0.0;
    double sum_sq    = 0.0;

    for (size_t i = 0; i < num_simulations; i++) {
        double board_yield;

        switch (model) {
        case YIELD_POISSON: {
            /* Poisson: #defects ~ Poisson(D*A) */
            double mean_defects = defect_density * critical_area_cm2;
            int defects = poisson_random(mean_defects);
            board_yield = (defects == 0) ? 1.0 : 0.0;
            break;
        }
        case YIELD_MURPHY: {
            /* Murphy: D uniformly distributed [0, 2*D_avg] */
            double D_i = 2.0 * defect_density * lcg_uniform();
            board_yield = yield_poisson(critical_area_cm2, D_i);
            break;
        }
        case YIELD_SEEDS: {
            /* Seeds: D exponentially distributed */
            double U = lcg_uniform();
            if (U < 1e-12) U = 1e-12;
            double D_i = -defect_density * log(U);
            board_yield = yield_poisson(critical_area_cm2, D_i);
            break;
        }
        case YIELD_NEG_BINOMIAL: {
            /* Neg Binomial: use compound Poisson-Gamma
               Gamma random variable via ratio of uniforms */
            if (alpha <= 0.0) { board_yield = 0.0; break; }
            /* Generate Gamma(alpha, 1) using approximate method
               then scale by (D*alpha) */
            double gamma_sum = 0.0;
            for (int j = 0; j < (int)ceil(alpha); j++) {
                double Uj = lcg_uniform();
                if (Uj < 1e-12) Uj = 1e-12;
                gamma_sum += -log(Uj);
            }
            double D_i = defect_density * gamma_sum / alpha;
            board_yield = yield_poisson(critical_area_cm2, D_i);
            break;
        }
        case YIELD_UNIFORM: {
            /* Simple uniform yield between 0.8 and 1.0 */
            board_yield = 0.8 + 0.2 * lcg_uniform();
            break;
        }
        case YIELD_EXPONENTIAL: {
            double U = lcg_uniform();
            if (U < 1e-12) U = 1e-12;
            board_yield = exp(-critical_area_cm2 * defect_density * 0.1
                              * (-log(U)));
            if (board_yield > 1.0) board_yield = 1.0;
            break;
        }
        default:
            board_yield = 0.0;
            break;
        }

        sum_yield += board_yield;
        sum_sq    += board_yield * board_yield;

        if (run_yields) {
            run_yields[i] = board_yield;
        }
    }

    /* Compute mean yield */
    double mean_yield = sum_yield / (double)num_simulations;
    result.predicted_yield = mean_yield;
    result.yield_percentage = mean_yield * 100.0;

    /* 95% confidence interval: CI = 1.96 * sigma / sqrt(N) */
    double variance = (sum_sq / (double)num_simulations)
                    - (mean_yield * mean_yield);
    if (variance < 0.0) variance = 0.0;
    double std_error = sqrt(variance / (double)num_simulations);
    result.confidence_interval_95 = 1.96 * std_error;

    /* Defect rate per million */
    result.defect_rate_per_million = (1.0 - mean_yield) * 1e6;

    /* Economic viability: typically yield > 80% for cost-sensitive,
       > 60% for complex/military boards */
    result.is_economical = (mean_yield >= 0.60);

    free(run_yields);
    return result;
}

/* ================================================================
   L5 - Full Yield Analysis
   ================================================================

   Computes yield under all four standard models and returns the
   most conservative (minimum) estimate with diagnostics.
   ================================================================ */

yield_result_t yield_compute_full(double critical_area_cm2,
                                    double defect_density,
                                    double alpha)
{
    yield_result_t result;
    memset(&result, 0, sizeof(result));

    if (critical_area_cm2 <= 0.0 || defect_density < 0.0) {
        result.is_economical = false;
        return result;
    }

    /* Compute yields under all models */
    double Y_poisson = yield_poisson(critical_area_cm2, defect_density);
    double Y_murphy  = yield_murphy(critical_area_cm2, defect_density);
    double Y_seeds   = yield_seeds(critical_area_cm2, defect_density);
    double Y_nb       = (alpha > 0.0)
        ? yield_neg_binomial(critical_area_cm2, defect_density, alpha)
        : Y_poisson;

    /* Use the most conservative (minimum) as the prediction */
    double Y_min = Y_poisson;
    yield_model_type_t best_model = YIELD_POISSON;

    if (Y_murphy < Y_min) { Y_min = Y_murphy; best_model = YIELD_MURPHY; }
    if (Y_seeds < Y_min)  { Y_min = Y_seeds; best_model = YIELD_SEEDS; }
    if (Y_nb < Y_min)     { Y_min = Y_nb; best_model = YIELD_NEG_BINOMIAL; }

    result.model = best_model;
    result.predicted_yield = Y_min;
    result.yield_percentage = Y_min * 100.0;
    result.critical_area_cm2 = critical_area_cm2;
    result.defect_density = defect_density;
    result.clustering_alpha = alpha;
    result.defect_rate_per_million = (1.0 - Y_min) * 1e6;
    result.confidence_interval_95 = 0.0; /* no MC simulation */
    result.is_economical = (Y_min >= 0.60);

    return result;
}

/* ================================================================
   L5 - Critical Area Estimation
   ================================================================

   Critical area is the region on the PCB where a defect of a given
   size would cause a failure (short or open). It depends on:

   1. Trace width and spacing: Finer geometries = larger critical
      area relative to total board area (more edge per unit area)

   2. Total trace length: Longer traces = more opportunity for
      opens (breaks) from point defects

   3. Board area: The background defect density applies to the
      entire board area

   Simplified estimation model:
     A_crit = L_total * W_crit

   where:
     L_total = total trace length across all layers (cm)
     W_crit = critical width = (min_spacing + min_trace_width) / 2

   This assumes that a defect larger than W_crit would bridge
   adjacent traces or break a single trace.

   For more accurate estimation, the critical area integral is:
     A_crit(r) = integral_0^inf (A_board * D(r) * f(r)) dr

   where r = defect radius, D(r) = defect size distribution,
   f(r) = fraction of board vulnerable to defects of size r.
   ================================================================ */

double compute_critical_area(double total_trace_length_cm,
                               double min_spacing_um,
                               double min_trace_width_um,
                               double board_area_cm2)
{
    if (total_trace_length_cm <= 0.0 || board_area_cm2 <= 0.0) {
        return 0.0;
    }

    /* Critical width: the defect size that can cause a failure */
    double w_crit_um = (min_spacing_um + min_trace_width_um) / 2.0;
    if (w_crit_um <= 0.0) return 0.0;

    /* Convert critical width to cm */
    double w_crit_cm = w_crit_um * 1e-4;

    /* Critical area = total_trace_length * critical_width */
    double A_crit_cm2 = total_trace_length_cm * w_crit_cm;

    /* Critical area cannot exceed total board area */
    if (A_crit_cm2 > board_area_cm2) A_crit_cm2 = board_area_cm2;

    return A_crit_cm2;
}

/* ================================================================
   L4 - Panel Yield
   ================================================================

   For a panel containing N boards, the panel yield is:
     Y_panel = Y_board^N

   This assumes independent failures across boards on the same panel.
   In practice, defects may be correlated (e.g., processing issue
   affecting the entire panel), making the true panel yield even lower.

   Conversely, some defects are random and independent, making the
   per-board yield the product of independent probabilities.

   Example: Board yield = 95%, 4 boards per panel
            Panel yield = 0.95^4 = 0.8145 = 81.5%
            (nearly 1 in 5 panels has at least one bad board)
   ================================================================ */

double compute_panel_yield(double board_yield, int num_boards)
{
    if (board_yield <= 0.0 || board_yield > 1.0) return 0.0;
    if (num_boards < 1) return 0.0;

    return pow(board_yield, (double)num_boards);
}

/* ================================================================
   L4 - Required Defect Density for Target Yield
   ================================================================

   Inverse problem: given a target yield, what is the maximum
   allowable defect density?

   Using Poisson model (most conservative):
     Y_target = exp(-A * D_required)
     D_required = -ln(Y_target) / A

   This gives the defect density that the manufacturing process
   must achieve to meet the yield target using the most
   conservative model. If this D_required is not achievable
   with current process technology, the design must be modified
   (reduce critical area, add redundancy, change technology node).

   Example: Target yield = 90%, Critical area = 2 cm^2
            D_req = -ln(0.9) / 2 = 0.0527 defects/cm^2
   ================================================================ */

double estimate_required_defect_density(double target_yield,
                                          double critical_area_cm2)
{
    if (target_yield <= 0.0 || target_yield >= 1.0
        || critical_area_cm2 <= 0.0) {
        return 0.0;
    }

    /* Using Poisson: Y = exp(-A*D) => D = -ln(Y)/A */
    return -log(target_yield) / critical_area_cm2;
}
