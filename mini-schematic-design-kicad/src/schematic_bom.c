/**
 * @file schematic_bom.c
 * @brief Bill of Materials generation, aggregation, and export
 *
 * Implements BOM generation from schematic component data with
 * multiple grouping strategies, multi-format export (CSV, HTML,
 * JSON, Markdown, KiCad XML), supplier optimization, cost estimation,
 * and risk/obsolescence analysis.
 *
 * Knowledge Coverage:
 *   L1: BOM data structures (bom_line_item_t, bom_report_t)
 *   L2: Component classification, grouping strategies
 *   L3: Aggregation algebra (partition by equivalence relations)
 *   L4: Economic order quantity / total cost theory
 *   L5: Greedy supplier optimization, IPC-2221 cost estimation
 *   L6: Multi-format industrial BOM export, risk identification
 *   L7: Supply chain risk (Toyota, ISO 9001), automotive traceability
 *
 * Course Mapping:
 *   Stanford EE272 �� Design for Manufacturing BOM
 *   Michigan EECS 411 / TU Munich �� Automotive BOM management
 *   Georgia Tech ECE 6350 �� System integration procurement
 *
 * Reference: IPC-2581, ISO 10303-210 (STEP AP210), ISO 9001
 */

#include "schematic_bom.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <ctype.h>

/* ������������������������ Component Classification (L2) ������������������������ */

component_category_t bom_classify_component(const char *reference)
{
    if (!reference || reference[0] == '\0') return CAT_UNKNOWN;

    char prefix = reference[0];
    switch (prefix) {
    case 'R': return CAT_RESISTOR;
    case 'C': return CAT_CAPACITOR;
    case 'L': return CAT_INDUCTOR;
    case 'D':
        if (reference[1] == 'I' || reference[1] == 'A') return CAT_DIODE;
        return CAT_DIODE;
    case 'Q': return CAT_TRANSISTOR;
    case 'U':
        if (reference[1] == 'A') return CAT_IC_ANALOG;
        if (reference[1] == 'D') return CAT_IC_DIGITAL;
        if (reference[1] == 'M') return CAT_IC_MIXED;
        return CAT_IC_DIGITAL;
    case 'J': return CAT_CONNECTOR;
    case 'P': return CAT_CONNECTOR;
    case 'S':
        if (reference[1] == 'W') return CAT_SWITCH;
        return CAT_SWITCH;
    case 'Y': return CAT_CRYSTAL;
    case 'F': return CAT_FUSE;
    case 'T':
        if (reference[1] == 'P') return CAT_TEST_POINT;
        return CAT_TRANSFORMER;
    case 'K': return CAT_RELAY;
    case 'O':
        if (reference[1] == 'K') return CAT_OPTOCOUPLER;
        return CAT_OPTOCOUPLER;
    case 'X': return CAT_SENSOR;
    case 'H': return CAT_MECHANICAL;
    case 'M': return CAT_MECHANICAL;
    case 'E': return CAT_MECHANICAL;
    case 'B':
        if (reference[1] == 'T') return CAT_MECHANICAL;
        return CAT_MECHANICAL;
    case 'W': return CAT_JUMPER;
    default:
        /* Try category based on first two chars for composite prefixes */
        if (reference[0] == 'I' && reference[1] == 'C') return CAT_IC_DIGITAL;
        if (reference[0] == 'A' && reference[1] == 'M') return CAT_IC_ANALOG;
        return CAT_UNKNOWN;
    }
}

const char* bom_category_name(component_category_t cat)
{
    static const char *names[] = {
        "Resistor", "Capacitor", "Inductor", "Diode", "Transistor",
        "IC-Analog", "IC-Digital", "IC-Mixed", "Connector", "Switch",
        "Crystal", "Fuse", "Transformer", "Relay", "Optocoupler",
        "Sensor", "Mechanical", "Test Point", "Jumper", "Unknown"
    };
    if (cat < 0 || cat > CAT_UNKNOWN) cat = CAT_UNKNOWN;
    return names[cat];
}

/* ������������������������ BOM Generation Core (L5) ������������������������ */

/**
 * Compare two component instances for grouping equivalence.
 * The grouping relation determines which components are merged
 * into a single BOM line item.
 *
 * L3: Equivalence relation on components:
 *   a ~ b iff (value, footprint) are the same for VALUE_FOOTPRINT strategy
 */
static bool bom_components_equivalent(const schematic_component_t *a,
                                       const schematic_component_t *b,
                                       bom_grouping_strategy_t strategy)
{
    switch (strategy) {
    case BOM_GROUP_VALUE_FOOTPRINT:
        return (strcmp(a->value, b->value) == 0 &&
                strcmp(a->footprint, b->footprint) == 0);
    case BOM_GROUP_MPN:
        /* MPN stored in library_id for simplicity */
        return (strcmp(a->library_id, b->library_id) == 0);
    case BOM_GROUP_VALUE_ONLY:
        return (strcmp(a->value, b->value) == 0);
    case BOM_GROUP_SHEET:
        return false; /* Sheet grouping handled separately */
    case BOM_GROUP_TYPE:
        return (bom_classify_component(a->reference) ==
                bom_classify_component(b->reference));
    case BOM_GROUP_NO_GROUP:
        return false; /* Never merge */
    default:
        return false;
    }
}

/**
 * Generate BOM from schematic using specified grouping strategy.
 *
 * Algorithm (L5): Partition components by equivalence relation,
 * then aggregate each equivalence class into a BOM line item.
 *
 * Time: O(N^2) for N components (pairwise comparison for grouping).
 *        Can be improved to O(N log N) with sort-and-scan.
 * Space: O(N + M) where M = number of unique part types.
 */
bom_report_t* bom_generate(const schematic_design_t *sch,
                            bom_grouping_strategy_t strategy)
{
    if (!sch) return NULL;

    bom_report_t *bom = calloc(1, sizeof(bom_report_t));
    if (!bom) return NULL;

    strncpy(bom->project_name, sch->title, sizeof(bom->project_name) - 1);
    strncpy(bom->revision, sch->rev, sizeof(bom->revision) - 1);
    strncpy(bom->date, sch->date, sizeof(bom->date) - 1);

    if (sch->num_components == 0) {
        bom->num_items = 0;
        bom->items = NULL;
        bom->total_cost = 0.0;
        bom->total_components = 0;
        bom->unique_parts = 0;
        return bom;
    }

    /* Track which components have been assigned to a group */
    bool *assigned = calloc(sch->num_components, sizeof(bool));
    if (!assigned) { bom_report_free(bom); return NULL; }

    int capacity = 16;
    bom->items = calloc(capacity, sizeof(bom_line_item_t));
    if (!bom->items) { free(assigned); bom_report_free(bom); return NULL; }

    int num_groups = 0;

    /* Build groups via greedy aggregation */
    for (int i = 0; i < sch->num_components; i++) {
        if (assigned[i]) continue;
        if (sch->components[i].exclude_from_bom) {
            assigned[i] = true;
            continue;
        }

        /* Start a new group */
        int g = num_groups;
        if (g >= capacity) {
            capacity *= 2;
            void *tmp = realloc(bom->items,
                (size_t)capacity * sizeof(bom_line_item_t));
            if (!tmp) { free(assigned); bom_report_free(bom); return NULL; }
            bom->items = tmp;
        }

        memset(&bom->items[g], 0, sizeof(bom_line_item_t));
        schematic_component_t *rep = &sch->components[i];
        strncpy(bom->items[g].value, rep->value,
                sizeof(bom->items[g].value) - 1);
        strncpy(bom->items[g].footprint, rep->footprint,
                sizeof(bom->items[g].footprint) - 1);

        /* Collect all components matching this group */
        bom->items[g].quantity = 0;
        char designator_buf[1024] = "";
        size_t dpos = 0;

        for (int j = i; j < sch->num_components; j++) {
            if (assigned[j]) continue;
            if (sch->components[j].exclude_from_bom) { assigned[j] = true; continue; }

            if (strategy == BOM_GROUP_NO_GROUP) {
                /* One item per component */
                if (j > i) break;
                assigned[j] = true;
            } else if (bom_components_equivalent(rep, &sch->components[j],
                                                  strategy)) {
                assigned[j] = true;
            } else {
                continue;
            }

            bom->items[g].quantity++;

            /* Append reference designator */
            if (dpos > 0 && dpos < sizeof(designator_buf) - 4) {
                designator_buf[dpos++] = ',';
                designator_buf[dpos++] = ' ';
            }
            size_t rlen = strlen(sch->components[j].reference);
            if (dpos + rlen < sizeof(designator_buf) - 1) {
                memcpy(designator_buf + dpos,
                       sch->components[j].reference, rlen);
                dpos += rlen;
            }
        }
        designator_buf[dpos] = '\0';
        strncpy(bom->items[g].designators, designator_buf,
                sizeof(bom->items[g].designators) - 1);

        /* Compute category for the group */
        component_category_t cat = bom_classify_component(rep->reference);
        bom->items[g].sheet_number = 1; /* simplified; real impl would track sheets */

        /* Check for DNP (Do Not Populate) by value keyword */
        bom->items[g].is_DNP = (strstr(rep->value, "DNP") != NULL ||
                                 strstr(rep->value, "DNI") != NULL ||
                                 strstr(rep->value, "NM") != NULL);
        bom->items[g].is_critical = (cat == CAT_IC_DIGITAL ||
                                      cat == CAT_IC_ANALOG ||
                                      cat == CAT_TRANSFORMER);

        num_groups++;
    }

    free(assigned);
    bom->num_items = num_groups;
    bom->total_components = sch->num_components;
    bom->unique_parts = num_groups;
    bom->total_cost = bom_calculate_total_cost(bom);

    return bom;
}

void bom_report_free(bom_report_t *bom)
{
    if (!bom) return;
    free(bom->items);
    free(bom);
}

/* ������������������������ BOM Sorting (L5) ������������������������ */

static int bom_cmp_designator(const void *a, const void *b)
{
    const bom_line_item_t *ia = (const bom_line_item_t *)a;
    const bom_line_item_t *ib = (const bom_line_item_t *)b;
    return strcmp(ia->designators, ib->designators);
}

static int bom_cmp_value(const void *a, const void *b)
{
    const bom_line_item_t *ia = (const bom_line_item_t *)a;
    const bom_line_item_t *ib = (const bom_line_item_t *)b;
    return strcmp(ia->value, ib->value);
}

static int bom_cmp_quantity(const void *a, const void *b)
{
    const bom_line_item_t *ia = (const bom_line_item_t *)a;
    const bom_line_item_t *ib = (const bom_line_item_t *)b;
    return ib->quantity - ia->quantity; /* descending */
}

void bom_sort_by_designator(bom_report_t *bom)
{
    if (!bom || bom->num_items < 2) return;
    qsort(bom->items, (size_t)bom->num_items,
          sizeof(bom_line_item_t), bom_cmp_designator);
}

void bom_sort_by_value(bom_report_t *bom)
{
    if (!bom || bom->num_items < 2) return;
    qsort(bom->items, (size_t)bom->num_items,
          sizeof(bom_line_item_t), bom_cmp_value);
}

void bom_sort_by_quantity(bom_report_t *bom)
{
    if (!bom || bom->num_items < 2) return;
    qsort(bom->items, (size_t)bom->num_items,
          sizeof(bom_line_item_t), bom_cmp_quantity);
}

/* ������������������������ BOM Merging (L5) ������������������������ */

/**
 * Merge multiple BOM reports (e.g., from multi-sheet designs).
 *
 * Each input BOM represents one sheet. Merging concatenates items
 * and then re-aggregates duplicates. Useful for flat hierarchy BOM
 * where each sheet was generated independently.
 *
 * L3: Set union with equivalence-class merging.
 * L4: Economic order quantity (EOQ) �� merging enables volume discounts.
 */
bom_report_t* bom_merge(bom_report_t **boms, int num_boms)
{
    if (!boms || num_boms <= 0) return NULL;

    bom_report_t *merged = calloc(1, sizeof(bom_report_t));
    if (!merged) return NULL;

    if (num_boms >= 1 && boms[0]) {
        strncpy(merged->project_name, boms[0]->project_name,
                sizeof(merged->project_name) - 1);
        strncpy(merged->date, boms[0]->date,
                sizeof(merged->date) - 1);
    }

    /* Count total items */
    int total_items = 0;
    for (int i = 0; i < num_boms; i++) {
        if (boms[i]) total_items += boms[i]->num_items;
    }

    /* Gather all items */
    bom_line_item_t *all_items = calloc((size_t)total_items,
                                         sizeof(bom_line_item_t));
    if (!all_items) { bom_report_free(merged); return NULL; }

    int idx = 0;
    for (int i = 0; i < num_boms; i++) {
        if (!boms[i]) continue;
        for (int j = 0; j < boms[i]->num_items; j++) {
            all_items[idx++] = boms[i]->items[j];
        }
        merged->total_components += boms[i]->total_components;
    }

    /* Re-aggregate: merge items with same value+footprint */
    int capacity = (total_items > 0) ? total_items : 1;
    bom_line_item_t *agg_items = calloc((size_t)capacity,
                                         sizeof(bom_line_item_t));
    if (!agg_items) { free(all_items); bom_report_free(merged); return NULL; }
    bool *merged_flag = calloc((size_t)total_items, sizeof(bool));
    if (!merged_flag) {
        free(agg_items); free(all_items); bom_report_free(merged); return NULL;
    }

    int agg_count = 0;
    for (int i = 0; i < total_items; i++) {
        if (merged_flag[i]) continue;

        bom_line_item_t *g = &agg_items[agg_count];
        *g = all_items[i];
        g->quantity = 0;
        g->designators[0] = '\0';
        size_t dpos = 0;

        for (int j = i; j < total_items; j++) {
            if (merged_flag[j]) continue;
            if (strcmp(all_items[i].value, all_items[j].value) == 0 &&
                strcmp(all_items[i].footprint, all_items[j].footprint) == 0) {
                merged_flag[j] = true;
                g->quantity += all_items[j].quantity;

                /* Append designators */
                if (all_items[j].designators[0]) {
                    if (dpos > 0 && dpos < sizeof(g->designators) - 4) {
                        g->designators[dpos++] = ',';
                        g->designators[dpos++] = ' ';
                    }
                    size_t len = strlen(all_items[j].designators);
                    if (dpos + len < sizeof(g->designators) - 1) {
                        memcpy(g->designators + dpos,
                               all_items[j].designators, len);
                        dpos += len;
                    }
                }
            }
        }
        g->designators[dpos] = '\0';
        agg_count++;
    }

    free(merged_flag);
    free(all_items);

    merged->num_items = agg_count;
    merged->items = agg_items;
    merged->unique_parts = agg_count;
    merged->total_cost = bom_calculate_total_cost(merged);

    return merged;
}

bool bom_compare(const bom_report_t *a, const bom_report_t *b,
                  double tolerance)
{
    if (!a || !b) return false;
    if (a->num_items != b->num_items) return false;
    if (a->total_components != b->total_components) return false;

    double cost_diff = fabs(a->total_cost - b->total_cost);
    if (cost_diff > tolerance) return false;

    /* Compare items individually */
    for (int i = 0; i < a->num_items; i++) {
        if (strcmp(a->items[i].value, b->items[i].value) != 0) return false;
        if (strcmp(a->items[i].footprint, b->items[i].footprint) != 0) return false;
        if (a->items[i].quantity != b->items[i].quantity) return false;
    }
    return true;
}

double bom_calculate_total_cost(const bom_report_t *bom)
{
    if (!bom) return 0.0;
    double total = 0.0;
    for (int i = 0; i < bom->num_items; i++) {
        total += bom->items[i].unit_cost * (double)bom->items[i].quantity;
    }
    return total;
}

/* ������������������������ BOM Export Functions (L6) ������������������������ */

int bom_export(const bom_report_t *bom, bom_output_format_t fmt,
               const char *filename)
{
    if (!bom || !filename) return -1;
    FILE *fp = fopen(filename, "w");
    if (!fp) return -1;
    int ret = 0;

    switch (fmt) {
    case BOM_FMT_CSV:      ret = bom_export_csv(bom, fp); break;
    case BOM_FMT_HTML:     ret = bom_export_html(bom, fp); break;
    case BOM_FMT_MARKDOWN: ret = bom_export_markdown(bom, fp); break;
    case BOM_FMT_JSON:     ret = bom_export_json(bom, fp); break;
    case BOM_FMT_KICAD:    ret = bom_export_kicad_xml(bom, fp); break;
    case BOM_FMT_TSV: {
        /* TSV is same as CSV but with tab separator; reuse CSV logic */
        if (bom->num_items == 0) { ret = 0; break; }
        fprintf(fp, "Ref\tValue\tFootprint\tQty\tUnitCost\tDescription\n");
        for (int i = 0; i < bom->num_items; i++) {
            bom_line_item_t *item = &bom->items[i];
            fprintf(fp, "%s\t%s\t%s\t%d\t%.4f\t%s\n",
                    item->designators, item->value, item->footprint,
                    item->quantity, item->unit_cost, item->description);
        }
        ret = 0;
        break;
    }
    default: ret = -1; break;
    }

    fclose(fp);
    return ret;
}

int bom_export_csv(const bom_report_t *bom, FILE *fp)
{
    if (!bom || !fp) return -1;

    fprintf(fp, "Reference,Value,Footprint,Quantity,UnitCost,Category,Supplier,MPN,DNP,Critical\n");
    for (int i = 0; i < bom->num_items; i++) {
        bom_line_item_t *item = &bom->items[i];
        /* Quote fields that may contain commas */
        fprintf(fp, "\"%s\",\"%s\",\"%s\",%d,%.4f,\"%s\",\"%s\",\"%s\",%s,%s\n",
                item->designators,
                item->value,
                item->footprint,
                item->quantity,
                item->unit_cost,
                bom_category_name(bom_classify_component(item->designators)),
                item->supplier,
                item->manufacturer_pn,
                item->is_DNP ? "Yes" : "No",
                item->is_critical ? "Yes" : "No");
    }
    return 0;
}

int bom_export_html(const bom_report_t *bom, FILE *fp)
{
    if (!bom || !fp) return -1;

    fprintf(fp, "<!DOCTYPE html>\n<html>\n<head>\n");
    fprintf(fp, "<title>BOM: %s</title>\n", bom->project_name);
    fprintf(fp, "<style>table { border-collapse: collapse; } "
                "th, td { border: 1px solid #333; padding: 4px 8px; } "
                "th { background: #4472C4; color: white; } "
                "tr:nth-child(even) { background: #f2f2f2; }</style>\n");
    fprintf(fp, "</head>\n<body>\n");
    fprintf(fp, "<h1>Bill of Materials: %s</h1>\n", bom->project_name);
    fprintf(fp, "<p>Revision: %s | Date: %s | Prepared: %s | Approved: %s</p>\n",
            bom->revision, bom->date, bom->prepared_by, bom->approved_by);
    fprintf(fp, "<p>Total Parts: %d | Unique: %d | Cost: $%.2f</p>\n",
            bom->total_components, bom->unique_parts, bom->total_cost);

    fprintf(fp, "<table>\n<tr><th>#</th><th>Reference</th><th>Value</th>"
                "<th>Footprint</th><th>Qty</th><th>Unit Cost</th>"
                "<th>Supplier</th><th>MPN</th><th>DNP</th></tr>\n");

    for (int i = 0; i < bom->num_items; i++) {
        bom_line_item_t *item = &bom->items[i];
        fprintf(fp, "<tr><td>%d</td><td>%s</td><td>%s</td><td>%s</td>"
                    "<td>%d</td><td>$%.4f</td><td>%s</td><td>%s</td>"
                    "<td>%s</td></tr>\n",
                i + 1,
                item->designators,
                item->value,
                item->footprint,
                item->quantity,
                item->unit_cost,
                item->supplier[0] ? item->supplier : "-",
                item->manufacturer_pn[0] ? item->manufacturer_pn : "-",
                item->is_DNP ? "DNP" : "Populate");
    }

    fprintf(fp, "</table>\n");
    fprintf(fp, "<p><em>Generated by mini-schematic-kicad BOM tool</em></p>\n");
    fprintf(fp, "</body>\n</html>\n");
    return 0;
}

int bom_export_markdown(const bom_report_t *bom, FILE *fp)
{
    if (!bom || !fp) return -1;

    fprintf(fp, "# Bill of Materials: %s\n\n", bom->project_name);
    fprintf(fp, "| **Rev** | %s | **Date** | %s |\n", bom->revision, bom->date);
    fprintf(fp, "| **Total** | %d | **Unique** | %d | **Cost** | $%.2f |\n\n",
            bom->total_components, bom->unique_parts, bom->total_cost);

    fprintf(fp, "| # | Reference | Value | Footprint | Qty | Unit Cost |\n");
    fprintf(fp, "|---|-----------|-------|-----------|-----|----------|\n");

    for (int i = 0; i < bom->num_items; i++) {
        bom_line_item_t *item = &bom->items[i];
        fprintf(fp, "| %d | %s | %s | %s | %d | $%.4f |\n",
                i + 1, item->designators, item->value,
                item->footprint, item->quantity, item->unit_cost);
    }

    fprintf(fp, "\n*Generated by mini-schematic-kicad BOM tool*\n");
    return 0;
}

int bom_export_json(const bom_report_t *bom, FILE *fp)
{
    if (!bom || !fp) return -1;

    fprintf(fp, "{\n");
    fprintf(fp, "  \"project\": \"%s\",\n", bom->project_name);
    fprintf(fp, "  \"revision\": \"%s\",\n", bom->revision);
    fprintf(fp, "  \"date\": \"%s\",\n", bom->date);
    fprintf(fp, "  \"prepared_by\": \"%s\",\n", bom->prepared_by);
    fprintf(fp, "  \"total_cost\": %.4f,\n", bom->total_cost);
    fprintf(fp, "  \"total_components\": %d,\n", bom->total_components);
    fprintf(fp, "  \"unique_parts\": %d,\n", bom->unique_parts);
    fprintf(fp, "  \"items\": [\n");

    for (int i = 0; i < bom->num_items; i++) {
        bom_line_item_t *item = &bom->items[i];
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"reference\": \"%s\",\n", item->designators);
        fprintf(fp, "      \"value\": \"%s\",\n", item->value);
        fprintf(fp, "      \"footprint\": \"%s\",\n", item->footprint);
        fprintf(fp, "      \"quantity\": %d,\n", item->quantity);
        fprintf(fp, "      \"unit_cost\": %.4f\n", item->unit_cost);
        fprintf(fp, "    }%s\n", (i < bom->num_items - 1) ? "," : "");
    }

    fprintf(fp, "  ]\n");
    fprintf(fp, "}\n");
    return 0;
}

int bom_export_kicad_xml(const bom_report_t *bom, FILE *fp)
{
    if (!bom || !fp) return -1;

    /* KiCad BOM XML format (v5 style) */
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(fp, "<bom version=\"1.1\" generator=\"mini-schematic-kicad\">\n");
    fprintf(fp, "  <project>%s</project>\n", bom->project_name);
    fprintf(fp, "  <revision>%s</revision>\n", bom->revision);
    fprintf(fp, "  <date>%s</date>\n", bom->date);
    fprintf(fp, "  <generated>%s</generated>\n", bom->date);

    fprintf(fp, "  <components>\n");
    for (int i = 0; i < bom->num_items; i++) {
        bom_line_item_t *item = &bom->items[i];
        fprintf(fp, "    <component>\n");
        fprintf(fp, "      <ref>%s</ref>\n", item->designators);
        fprintf(fp, "      <value>%s</value>\n", item->value);
        fprintf(fp, "      <footprint>%s</footprint>\n", item->footprint);
        fprintf(fp, "      <qty>%d</qty>\n", item->quantity);
        fprintf(fp, "      <dnp>%s</dnp>\n", item->is_DNP ? "true" : "false");
        fprintf(fp, "    </component>\n");
    }
    fprintf(fp, "  </components>\n");
    fprintf(fp, "</bom>\n");
    return 0;
}

/* ������������������������ Supplier Optimization (L7: Supply Chain) ������������������������ */

/**
 * @brief Find cheapest supplier for each BOM item
 *
 * L5: Greedy supplier selection with competitive sourcing.
 * For each BOM item, selects the supplier with the lowest unit price
 * from a price matrix [item][supplier].
 *
 * L7 Application: Automotive supply chain (Toyota Production System
 *   principle: multiple qualified suppliers to prevent single-source risk).
 *   ISO 9001:2015 ��8.4 �� Control of externally provided processes.
 *
 * Time: O(I �� S) for I items, S suppliers
 * Space: O(1)
 */
int bom_optimize_suppliers(bom_report_t *bom,
                            const char *supplier_list[],
                            const double *price_matrix[],
                            int num_suppliers)
{
    if (!bom || !supplier_list || !price_matrix || num_suppliers <= 0)
        return -1;

    double total_savings = 0.0;

    for (int i = 0; i < bom->num_items; i++) {
        double best_price = INFINITY;
        int best_idx = -1;

        for (int s = 0; s < num_suppliers; s++) {
            if (price_matrix[s] == NULL) continue;
            double price = price_matrix[s][i];
            if (price < best_price && price > 0.0) {
                best_price = price;
                best_idx = s;
            }
        }

        if (best_idx >= 0) {
            double old_cost = bom->items[i].unit_cost;
            bom->items[i].unit_cost = best_price;
            if (supplier_list[best_idx]) {
                strncpy(bom->items[i].supplier, supplier_list[best_idx],
                        sizeof(bom->items[i].supplier) - 1);
            }
            total_savings += (old_cost - best_price) *
                             (double)bom->items[i].quantity;
        }
    }

    /* Recalculate total cost */
    bom->total_cost = bom_calculate_total_cost(bom);
    return 0;
}

/**
 * @brief Estimate assembly cost using IPC-2221 derived model
 *
 * L4: Cost = N_smd �� c_smd + N_tht �� c_tht + C_setup
 *
 * IPC-2221: Generic Standard on Printed Board Design.
 * Per-component placement cost: ~$0.005 for SMD (high-speed pick-place),
 * ~$0.03 for THT (manual/hand assembly), $50-500 setup cost per board.
 *
 * L7 Application: Detroit automotive electronics manufacturing,
 *   NASA J-STD-001 space-grade soldering standards.
 */
double bom_estimate_assembly_cost(const bom_report_t *bom,
                                   double cost_per_smd,
                                   double cost_per_tht,
                                   double setup_cost)
{
    if (!bom) return 0.0;

    int smd_count = 0, tht_count = 0;

    for (int i = 0; i < bom->num_items; i++) {
        /* Heuristic: SMD footprints contain "SMD", "0805", "0603", etc.;
         * THT footprints contain "THT", "DIP", "TO-", "Axial" */
        const char *fp = bom->items[i].footprint;
        int qty = bom->items[i].quantity;

        if (strstr(fp, "SMD") || strstr(fp, "0805") ||
            strstr(fp, "0603") || strstr(fp, "0402") ||
            strstr(fp, "0201") || strstr(fp, "QFN") ||
            strstr(fp, "BGA") || strstr(fp, "SOIC") ||
            strstr(fp, "TSSOP") || strstr(fp, "DFN") ||
            strstr(fp, "SOT")) {
            smd_count += qty;
        } else if (strstr(fp, "THT") || strstr(fp, "DIP") ||
                   strstr(fp, "TO-") || strstr(fp, "Axial") ||
                   strstr(fp, "Radial") || strstr(fp, "Connector") ||
                   strstr(fp, "PinHeader")) {
            tht_count += qty;
        } else {
            /* Unknown type �� assume SMD (conservative) */
            smd_count += qty;
        }
    }

    return (double)smd_count * cost_per_smd +
           (double)tht_count * cost_per_tht + setup_cost;
}

/* ������������������������ Supply Chain Risk Analysis (L7) ������������������������ */

/**
 * @brief Identify single-source components (supply chain risk)
 *
 * L7 Application: Toyota lean manufacturing �� single-source
 *   components pose supply chain vulnerability. The 2011 Fukushima
 *   disaster exposed critical single-source dependencies in the
 *   global electronics supply chain (e.g., Renesas microcontrollers
 *   for automotive ECUs).
 *
 * Returns number of risk items. Caller receives a newly allocated
 * array of at most num_items entries (must free via free()).
 */
int bom_identify_risk_items(const bom_report_t *bom,
                             bom_line_item_t **risk_items)
{
    if (!bom || !risk_items) return -1;
    *risk_items = NULL;

    int capacity = 8;
    bom_line_item_t *risks = calloc((size_t)capacity,
                                     sizeof(bom_line_item_t));
    if (!risks) return -1;
    int risk_count = 0;

    for (int i = 0; i < bom->num_items; i++) {
        bom_line_item_t *item = &bom->items[i];
        bool is_risk = false;

        /* Risk criteria:
         *  1. No supplier assigned (single-source unknown)
         *  2. Critical component (IC, transformer, crystal)
         *  3. Sole-sourced (only one supplier name equals the manufacturer)
         */
        if (item->supplier[0] == '\0') {
            is_risk = true;
        } else if (item->is_critical) {
            is_risk = true;
        } else if (item->manufacturer[0] && item->supplier[0] &&
                   strcmp(item->manufacturer, item->supplier) == 0) {
            /* Direct buy from manufacturer = possible sole source */
            is_risk = true;
        }

        if (is_risk) {
            if (risk_count >= capacity) {
                capacity *= 2;
                void *tmp = realloc(risks,
                    (size_t)capacity * sizeof(bom_line_item_t));
                if (!tmp) { free(risks); return -1; }
                risks = tmp;
            }
            risks[risk_count] = *item; /* shallow copy, strings are fixed-size */
            risk_count++;
        }
    }

    *risk_items = risks;
    return risk_count;
}

/**
 * @brief Check for obsolete/end-of-life (EOL) parts
 *
 * L7: ISO 9001 obsolescence management. Aerospace (Boeing, NASA)
 *   requires EOL part tracking. Compares BOM items against a known
 *   list of EOL part numbers.
 *
 * Returns number of EOL matches found.
 */
int bom_check_obsolescence(const bom_report_t *bom,
                            const char *eol_parts[],
                            int num_eol_parts)
{
    if (!bom || !eol_parts || num_eol_parts <= 0) return 0;

    int matches = 0;
    for (int i = 0; i < bom->num_items; i++) {
        for (int j = 0; j < num_eol_parts; j++) {
            if (!eol_parts[j]) continue;
            /* Match against value or manufacturer_pn */
            if (strstr(bom->items[i].value, eol_parts[j]) ||
                strcmp(bom->items[i].manufacturer_pn, eol_parts[j]) == 0) {
                matches++;
                break; /* One match per item */
            }
        }
    }
    return matches;
}