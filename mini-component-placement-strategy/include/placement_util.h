/**
 * @file placement_util.h
 * @brief Utility functions for PCB component placement
 *
 * Provides geometric computation, spatial indexing, random number
 * generation, statistics, and I/O utilities used across the
 * placement system.
 *
 * Knowledge Mapping:
 *   L3 (Math Structures): 2D geometry, quadtree spatial indexing,
 *                         matrix operations, statistics
 *   L5 (Algorithms): Quadtree construction/query, random sampling
 *
 * Course Alignment:
 *   - CMU 15-462: Computational geometry
 *   - Berkeley EE16A: Matrix operations
 *   - ETH 227-0427: Numeric methods
 */

#ifndef PLACEMENT_UTIL_H
#define PLACEMENT_UTIL_H

#include "placement_core.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * 2D Geometry Utilities
 * ============================================================================ */

/**
 * Compute Euclidean distance between two points.
 */
double placement_util_distance(const Point2D* a, const Point2D* b);

/**
 * Compute Manhattan distance between two points.
 */
double placement_util_manhattan_distance(const Point2D* a, const Point2D* b);

/**
 * Check if two rectangles overlap.
 *
 * @param a  First rectangle
 * @param b  Second rectangle
 * @return   True if rectangles intersect (including edge contact)
 */
bool placement_util_rect_overlap(const Rect2D* a, const Rect2D* b);

/**
 * Compute the overlap area between two rectangles.
 *
 * @param a  First rectangle
 * @param b  Second rectangle
 * @return   Overlap area in mm² (0 if no overlap)
 */
double placement_util_rect_overlap_area(const Rect2D* a, const Rect2D* b);

/**
 * Compute the intersection of two rectangles.
 *
 * @param a      First rectangle
 * @param b      Second rectangle
 * @param result Output intersection rectangle (empty rect if no overlap)
 * @return       True if intersection exists
 */
bool placement_util_rect_intersection(const Rect2D* a, const Rect2D* b,
                                       Rect2D* result);

/**
 * Rotate a point around an origin by a given angle.
 *
 * @param point   Point to rotate (modified in place)
 * @param origin  Rotation origin
 * @param degrees Rotation angle in degrees (positive = CCW)
 */
void placement_util_rotate_point(Point2D* point, const Point2D* origin,
                                  double degrees);

/**
 * Compute the bounding box of a rotated rectangle.
 *
 * @param rect     Axis-aligned rectangle
 * @param origin   Center of rotation
 * @param degrees  Rotation in degrees
 * @return         Bounding box of the rotated rectangle
 */
Rect2D placement_util_bounding_box_rotated(const Rect2D* rect,
                                            const Point2D* origin,
                                            double degrees);

/**
 * Compute the convex hull of a set of points using Graham scan.
 *
 * Complexity: O(N log N).
 * Reference: R. Graham, "An Efficient Algorithm for Determining the
 *            Convex Hull of a Finite Planar Set", Info. Proc. Lett., 1972.
 *
 * @param points     Input points
 * @param n          Number of input points
 * @param hull       Output hull points (must be at least n)
 * @param hull_count Output: number of hull points
 */
void placement_util_convex_hull(const Point2D* points, uint32_t n,
                                 Point2D* hull, uint32_t* hull_count);

/* ============================================================================
 * Quadtree Spatial Index
 * ============================================================================ */

/** Quadtree node for efficient spatial queries */
typedef struct QuadTreeNode {
    Rect2D              region;
    uint32_t*           comp_ids;      /* Component IDs in this node */
    uint32_t            comp_count;
    uint32_t            comp_capacity;
    bool                is_leaf;
    struct QuadTreeNode* children[4];  /* NW, NE, SW, SE */
} QuadTreeNode;

/** Quadtree structure */
typedef struct {
    QuadTreeNode* root;
    uint32_t      max_depth;
    uint32_t      max_per_node;   /* Split when > this many components */
} QuadTree;

/**
 * Build a quadtree from a set of placed components.
 *
 * Complexity: O(C * D) where C = components, D = max_depth.
 * Reference: Finkel & Bentley, "Quad Trees: A Data Structure for
 *            Retrieval on Composite Keys", Acta Informatica, 1974.
 *
 * @param tree     Quadtree to build
 * @param result   Placement result (components must be placed)
 * @param max_depth Maximum tree depth
 * @return         True on success
 */
bool placement_util_quadtree_build(QuadTree* tree,
                                    const PlacementResult* result,
                                    uint32_t max_depth);

/**
 * Free a quadtree.
 */
void placement_util_quadtree_free(QuadTree* tree);

/**
 * Query components within a rectangular region.
 *
 * @param tree       Quadtree to query
 * @param region     Query region
 * @param comp_ids   Output array of matching component IDs
 * @param max_ids    Maximum number of IDs to return
 * @return           Number of components found
 */
uint32_t placement_util_quadtree_query(const QuadTree* tree,
                                        const Rect2D* region,
                                        uint32_t* comp_ids,
                                        uint32_t max_ids);

/**
 * Find the nearest placed component to a given point.
 *
 * @param tree    Quadtree to query
 * @param result  Placement result (for position lookups)
 * @param point   Query point
 * @param comp_id Output: nearest component ID (-1 if none)
 * @return        Distance to nearest component (INFINITY if none)
 */
double placement_util_quadtree_nearest(const QuadTree* tree,
                                        const PlacementResult* result,
                                        const Point2D* point,
                                        int32_t* comp_id);

/* ============================================================================
 * Random Number Generation
 * ============================================================================ */

/** Linear congruential generator (LCG) state for reproducible randomness */
typedef struct {
    uint64_t state;
    uint64_t multiplier;
    uint64_t increment;
    uint64_t modulus;
} RandomState;

/**
 * Initialize a random state with a given seed.
 *
 * Uses the Numerical Recipes LCG parameters:
 *   multiplier = 6364136223846793005
 *   increment  = 1442695040888963407
 *   modulus    = 2^64
 *
 * @param state  Random state to initialize
 * @param seed   Seed value
 */
void placement_util_random_init(RandomState* state, uint64_t seed);

/**
 * Generate a uniform random double in [0, 1).
 */
double placement_util_random_uniform(RandomState* state);

/**
 * Generate a uniform random integer in [min_val, max_val].
 */
int32_t placement_util_random_int(RandomState* state,
                                   int32_t min_val, int32_t max_val);

/**
 * Generate a Gaussian random variable using Box-Muller transform.
 *
 * Reference: Box & Muller, "A Note on the Generation of Random
 *            Normal Deviates", Ann. Math. Stat., 1958.
 *
 * @param state  Random state
 * @param mean   Mean of the distribution
 * @param sigma  Standard deviation
 * @return       Gaussian random variable
 */
double placement_util_random_gaussian(RandomState* state,
                                       double mean, double sigma);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * Compute mean of an array of doubles.
 */
double placement_util_mean(const double* data, uint32_t n);

/**
 * Compute standard deviation of an array of doubles.
 */
double placement_util_stddev(const double* data, uint32_t n);

/**
 * Compute Pearson correlation coefficient between two arrays.
 *
 * @param x  First array
 * @param y  Second array
 * @param n  Number of elements
 * @return   Correlation coefficient in [-1, 1]
 */
double placement_util_correlation(const double* x, const double* y, uint32_t n);

/**
 * Compute linear regression (y = a * x + b).
 *
 * @param x     Independent variable
 * @param y     Dependent variable
 * @param n     Number of data points
 * @param slope Output slope (a)
 * @param intercept Output intercept (b)
 * @param r2    Output R² goodness-of-fit
 */
void placement_util_linear_regression(const double* x, const double* y,
                                       uint32_t n, double* slope,
                                       double* intercept, double* r2);

/* ============================================================================
 * I/O Utilities
 * ============================================================================ */

/**
 * Export placement to CSV (pick-and-place format).
 *
 * Format: designator, x_mm, y_mm, rotation_deg, side, package, part_number
 *
 * Compatible with standard PCB assembly pick-and-place file format.
 *
 * @param result    Placement result
 * @param filepath  Output file path
 * @return          True on success
 */
bool placement_util_export_csv(const PlacementResult* result,
                                const char* filepath);

/**
 * Import placement from CSV (pick-and-place format).
 *
 * Reads component positions from a standard pick-and-place file.
 * Components must already exist in the result; only positions are updated.
 *
 * @param result    Placement result (components must be initialized)
 * @param filepath  Input file path
 * @return          Number of components placed from file
 */
uint32_t placement_util_import_csv(PlacementResult* result,
                                    const char* filepath);

/**
 * Print a human-readable placement summary to stdout.
 *
 * @param result  Placement result to summarize
 */
void placement_util_print_summary(const PlacementResult* result);

#ifdef __cplusplus
}
#endif

#endif /* PLACEMENT_UTIL_H */
