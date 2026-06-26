/**
 * @file    dfm_yield.h
 * @brief   PCB Manufacturing Yield Models - L3 Structures, L4 Theorems
 *
 * @details Yield prediction models for PCB manufacturing: Poisson,
 *          Murphy, Seeds, and Negative Binomial (clustering) models.
 *
 * Knowledge Mapping:
 *   L3 - Mathematical Structures:
 *     - Poisson process for random defect distribution
 *     - Compound probability distributions
 *   L4 - Fundamental Laws:
 *     - Poisson yield: Y = exp(-A*D)
 *     - Murphy yield: Y = ((1-exp(-A*D))/(A*D))^2
 *     - Seeds yield: Y = exp(-sqrt(A*D))
 *     - Negative Binomial: Y = (1+A*D/alpha)^(-alpha)
 *   L5 - Algorithms: Monte Carlo yield simulation
 *
 * Reference: Murphy (1964), Seeds (1967), Stapper VLSI yield models
 */

#ifndef DFM_YIELD_H
#define DFM_YIELD_H

#include "dfm_core.h"
#include <stddef.h>
#include <stdbool.h>

/* ---- Defect Density Statistics ---- */

typedef struct {
    double defect_density;
    double defect_density_stddev;
    double critical_area_cm2;
    double clustering_factor;
    size_t sample_count;
    double avg_interconnect_length;
} defect_density_stats_t;

/* ---- Yield Model Types ---- */

typedef enum {
    YIELD_POISSON      = 0,
    YIELD_MURPHY       = 1,
    YIELD_SEEDS        = 2,
    YIELD_NEG_BINOMIAL = 3,
    YIELD_UNIFORM      = 4,
    YIELD_EXPONENTIAL  = 5
} yield_model_type_t;

/* ---- Yield Result ---- */

typedef struct {
    yield_model_type_t model;
    double predicted_yield;
    double yield_percentage;
    double defect_rate_per_million;
    double critical_area_cm2;
    double defect_density;
    double clustering_alpha;
    double confidence_interval_95;
    bool   is_economical;
} yield_result_t;

/* ---- L4: Poisson Yield Model ---- */

/**
 * Poisson model: Y = exp(-A*D)
 * Assumes random, independent defect distribution.
 * Most pessimistic (lowest yield) model.
 *
 * @param critical_area_cm2  Critical area (cm^2)
 * @param defect_density     Defect density (defects/cm^2)
 * @return                   Predicted yield (0.0 - 1.0)
 */
double yield_poisson(double critical_area_cm2, double defect_density);

/* ---- L4: Murphy Yield Law ---- */

/**
 * Murphy model: Y = ((1-exp(-A*D))/(A*D))^2
 * Assumes uniform distribution of defect densities.
 * More optimistic than Poisson.
 *
 * Reference: Murphy, Proc. IEEE, vol. 52, 1964.
 *
 * @param critical_area_cm2  Critical area (cm^2)
 * @param defect_density     Peak defect density (defects/cm^2)
 * @return                   Predicted yield (0.0 - 1.0)
 */
double yield_murphy(double critical_area_cm2, double defect_density);

/* ---- L4: Seeds Yield Model ---- */

/**
 * Seeds model: Y = exp(-sqrt(A*D))
 * Exponential defect distribution gives intermediate results.
 *
 * Reference: Seeds, IEEE IEDM, 1967.
 *
 * @param critical_area_cm2  Critical area (cm^2)
 * @param defect_density     Defect density (defects/cm^2)
 * @return                   Predicted yield (0.0 - 1.0)
 */
double yield_seeds(double critical_area_cm2, double defect_density);

/* ---- L4: Negative Binomial (Clustering) ---- */

/**
 * Neg Binomial: Y = (1 + A*D/alpha)^(-alpha)
 * Models defect clustering. alpha controls clustering strength.
 * alpha -> infinity approaches Poisson.
 * alpha < 1 indicates strong clustering (higher yield).
 *
 * @param critical_area_cm2  Critical area (cm^2)
 * @param defect_density     Defect density (defects/cm^2)
 * @param alpha              Clustering factor (must be > 0)
 * @return                   Predicted yield (0.0 - 1.0)
 */
double yield_neg_binomial(double critical_area_cm2, double defect_density,
                          double alpha);

/* ---- L5: Monte Carlo Simulation ---- */

yield_result_t yield_monte_carlo(double critical_area_cm2,
                                  double defect_density,
                                  yield_model_type_t model,
                                  double alpha,
                                  size_t num_simulations,
                                  unsigned int seed);

/* ---- L5: Full Analysis ---- */

yield_result_t yield_compute_full(double critical_area_cm2,
                                   double defect_density,
                                   double alpha);

/* ---- L5: Critical Area ---- */

/**
 * Estimate critical area from layout parameters.
 *
 * Simplified model: A_crit = L_total * W_crit
 * where W_crit = (min_spacing + min_trace_width)
 *
 * @param total_trace_length_cm  Total trace length (cm)
 * @param min_spacing_um         Min conductor spacing (um)
 * @param min_trace_width_um     Min trace width (um)
 * @param board_area_cm2         Total board area (cm^2)
 * @return                       Critical area estimate (cm^2)
 */
double compute_critical_area(double total_trace_length_cm,
                              double min_spacing_um,
                              double min_trace_width_um,
                              double board_area_cm2);

/* ---- L4: Panel Yield ---- */

double compute_panel_yield(double board_yield, int num_boards);

double estimate_required_defect_density(double target_yield,
                                         double critical_area_cm2);

#endif /* DFM_YIELD_H */
