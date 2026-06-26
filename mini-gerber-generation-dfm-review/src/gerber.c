/**
 * gerber.c — Gerber Format Core Implementation
 *
 * Implements core Gerber RS-274X data structures: state machine,
 * coordinate conversion, aperture tables, aperture macros, and
 * operation lists.
 *
 * References:
 *   - Ucamco Gerber Format Specification RS-274X, Rev. 2021.03
 *   - Ucamco Gerber X2 Format Specification
 */

#include "gerber.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

/* ─── State Machine ─────────────────────────────────────────────── */

void gerber_state_init(GerberState *state)
{
    assert(state != NULL);

    memset(state, 0, sizeof(GerberState));
    state->current_pos.x     = 0;
    state->current_pos.y     = 0;
    state->current_dcode     = 10;
    state->interpolation     = GERBER_INTERP_LINEAR;
    state->quadrant          = GERBER_QUAD_MULTI;
    state->coord_mode        = GERBER_ABS;
    state->polarity          = GERBER_POLARITY_DARK;
    state->unit              = GERBER_UNIT_MM;
    state->zero_suppress     = GERBER_ZERO_LEADING;
    state->int_digits        = 3;
    state->dec_digits        = 3;
    state->current_aperture  = -1;
}

void gerber_set_format(GerberState *state, int int_digits, int dec_digits)
{
    assert(state != NULL);
    assert(int_digits > 0 && int_digits <= 6);
    assert(dec_digits > 0 && dec_digits <= 7);

    state->int_digits = int_digits;
    state->dec_digits = dec_digits;
}

void gerber_set_zero_suppression(GerberState *state, GerberZeroSuppression mode)
{
    assert(state != NULL);
    state->zero_suppress = mode;
}

/* ─── Coordinate Conversion ─────────────────────────────────────── */

int64_t gerber_phys_to_int(double value, int dec_digits)
{
    /* Compute scale factor = 10^dec_digits */
    double scale = 1.0;
    for (int i = 0; i < dec_digits; i++) {
        scale *= 10.0;
    }
    /* Round to nearest integer */
    if (value >= 0.0) {
        return (int64_t)(value * scale + 0.5);
    } else {
        return (int64_t)(value * scale - 0.5);
    }
}

double gerber_int_to_phys(int64_t int_val, int dec_digits)
{
    double scale = 1.0;
    for (int i = 0; i < dec_digits; i++) {
        scale *= 10.0;
    }
    return (double)int_val / scale;
}

/**
 * Format a Gerber coordinate integer as a string with zero suppression.
 *
 * Given format N.M, the integer represents N+M digits.
 * Leading suppression: remove leading zeros, pad to minimum length.
 * Trailing suppression: remove trailing zeros, pad to minimum length.
 *
 * @param value   Integer coordinate value
 * @param int_d   Integer digits (N)
 * @param dec_d   Decimal digits (M)
 * @param zs      Zero suppression mode
 * @param buf     Output buffer (must be >= 16 chars)
 * @return        Number of characters written
 */
int format_coordinate(int64_t value, int int_d, int dec_d,
                             GerberZeroSuppression zs, char *buf)
{
    /* Total digits = int_d + dec_d */
    int total = int_d + dec_d;
    int is_negative = (value < 0);
    int64_t abs_val = is_negative ? -value : value;

    /* Build the full-width digit string from least significant digit */
    char digits[16];
    int pos = 0;
    if (abs_val == 0) {
        digits[pos++] = '0';
    } else {
        while (abs_val > 0 && pos < 16) {
            digits[pos++] = (char)('0' + (abs_val % 10));
            abs_val /= 10;
        }
    }

    /* Pad to total digits with leading zeros */
    while (pos < total) {
        digits[pos++] = '0';
    }
    digits[pos] = '\0';

    /* Now digits[0..pos-1] has reversed digits (LSD first) */
    /* Reverse to get MSD first */
    for (int i = 0; i < pos / 2; i++) {
        char tmp = digits[i];
        digits[i] = digits[pos - 1 - i];
        digits[pos - 1 - i] = tmp;
    }

    /* Apply zero suppression */
    int out_pos = 0;
    if (is_negative) {
        buf[out_pos++] = '-';
    }

    if (zs == GERBER_ZERO_LEADING) {
        /* Skip leading zeros, but keep at least the last int digit */
        int first_nonzero = 0;
        while (first_nonzero < int_d - 1 && digits[first_nonzero] == '0') {
            first_nonzero++;
        }
        /* Copy from first_nonzero to end */
        for (int i = first_nonzero; i < total; i++) {
            buf[out_pos++] = digits[i];
        }
    } else if (zs == GERBER_ZERO_TRAILING) {
        /* Skip trailing zeros after decimal point */
        int last_significant = total - 1;
        while (last_significant > int_d && digits[last_significant] == '0') {
            last_significant--;
        }
        for (int i = 0; i <= last_significant; i++) {
            buf[out_pos++] = digits[i];
        }
    } else {
        /* No suppression — copy all digits */
        for (int i = 0; i < total; i++) {
            buf[out_pos++] = digits[i];
        }
    }

    buf[out_pos] = '\0';
    return out_pos;
}

int gerber_state_validate(const GerberState *state)
{
    assert(state != NULL);

    if (state->int_digits <= 0 || state->dec_digits <= 0) {
        return 1;  /* Invalid format */
    }
    /* All modes compatible with any valid format */
    return 0;
}

/* ─── Aperture Table ────────────────────────────────────────────── */

void aperture_table_init(ApertureTable *table)
{
    assert(table != NULL);

    memset(table, 0, sizeof(ApertureTable));
    table->count = 0;
}

int aperture_table_add(ApertureTable *table, ApertureShape shape,
                       double p1, double p2, double p3,
                       double hole_dia, double rotation)
{
    assert(table != NULL);

    if (table->count >= GERBER_MAX_APERTURES) {
        return -1;  /* Table full */
    }

    int idx = table->count;
    ApertureDef *ap = &table->apertures[idx];

    ap->d_code        = 10 + idx;  /* Start at D10 */
    ap->shape         = shape;
    ap->param1        = p1;
    ap->param2        = p2;
    ap->param3        = p3;
    ap->hole_diameter = hole_dia;
    ap->rotation      = rotation;
    ap->n_vertices    = 0;

    /* For polygon: param3 = outer diameter, param1 = n_vertices */
    if (shape == APERTURE_POLYGON) {
        ap->n_vertices = (int)p1;
        ap->param1     = p3;   /* Outer diameter */
    }

    if (shape == APERTURE_MACRO) {
        ap->name[0] = '\0';  /* Caller must set via separate assignment */
    }

    table->count++;
    return ap->d_code;
}

int aperture_table_find(const ApertureTable *table, int d_code)
{
    assert(table != NULL);

    for (int i = 0; i < table->count; i++) {
        if (table->apertures[i].d_code == d_code) {
            return i;
        }
    }
    return -1;
}

/* ─── Aperture Macro Table ──────────────────────────────────────── */

void macro_table_init(ApertureMacroTable *table)
{
    assert(table != NULL);
    memset(table, 0, sizeof(ApertureMacroTable));
    table->count = 0;
}

int macro_table_add(ApertureMacroTable *table, const char *name,
                    const AMMacroPrim *primitives, int n_prims)
{
    assert(table != NULL);
    assert(name != NULL);
    assert(primitives != NULL);
    assert(n_prims > 0 && n_prims <= AM_MAX_PRIMITIVES);

    if (table->count >= 256) {
        return -1;  /* Table full */
    }

    /* Check for name conflict */
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->macros[i].name, name) == 0) {
            return -1;  /* Duplicate name */
        }
    }

    ApertureMacro *am = &table->macros[table->count];
    strncpy(am->name, name, sizeof(am->name) - 1);
    am->name[sizeof(am->name) - 1] = '\0';
    am->n_primitives = n_prims;

    for (int i = 0; i < n_prims; i++) {
        am->primitives[i] = primitives[i];
    }

    table->count++;
    return 0;
}

int macro_table_find(const ApertureMacroTable *table, const char *name)
{
    assert(table != NULL);
    assert(name != NULL);

    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->macros[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

/* ─── Operation List ────────────────────────────────────────────── */

void gerber_oplist_init(GerberOpList *list, int initial_capacity)
{
    assert(list != NULL);

    if (initial_capacity < 16) {
        initial_capacity = 16;
    }

    list->ops = (GerberOperation*)calloc((size_t)initial_capacity,
                                         sizeof(GerberOperation));
    list->count    = 0;
    list->capacity = (list->ops != NULL) ? initial_capacity : 0;
}

int gerber_oplist_append(GerberOpList *list, const GerberOperation *op)
{
    assert(list != NULL);
    assert(op != NULL);

    if (list->count >= list->capacity) {
        int new_cap = list->capacity * 2;
        if (new_cap < 16) new_cap = 16;

        GerberOperation *new_ops = (GerberOperation*)realloc(
            list->ops, (size_t)new_cap * sizeof(GerberOperation));
        if (new_ops == NULL) {
            return -1;  /* Allocation failure */
        }
        list->ops     = new_ops;
        list->capacity = new_cap;
    }

    list->ops[list->count] = *op;
    list->count++;
    return 0;
}

void gerber_oplist_free(GerberOpList *list)
{
    assert(list != NULL);

    free(list->ops);
    list->ops      = NULL;
    list->count    = 0;
    list->capacity = 0;
}

/* ─── X2 Attributes ─────────────────────────────────────────────── */

void x2_file_attr_init(X2FileAttribute *attr)
{
    assert(attr != NULL);

    memset(attr, 0, sizeof(X2FileAttribute));
    attr->function     = X2_FUNC_COPPER;
    attr->layer_number = 1;
    attr->part_name[0] = '\0';
    attr->part_number[0] = '\0';
    attr->revision[0]  = '\0';
    attr->n_part_attrs = 0;
    attr->part_attrs   = NULL;
}

int x2_add_part_attr(X2FileAttribute *attr, const char *designator,
                     const char *footprint, const char *value,
                     int pin, char mount_type)
{
    assert(attr != NULL);

    int n = attr->n_part_attrs;
    X2PartAttribute *new_arr = (X2PartAttribute*)realloc(
        attr->part_attrs, (size_t)(n + 1) * sizeof(X2PartAttribute));
    if (new_arr == NULL) {
        return -1;
    }
    attr->part_attrs = new_arr;

    X2PartAttribute *pa = &attr->part_attrs[n];
    strncpy(pa->designator, designator, sizeof(pa->designator) - 1);
    pa->designator[sizeof(pa->designator) - 1] = '\0';
    strncpy(pa->footprint, footprint, sizeof(pa->footprint) - 1);
    pa->footprint[sizeof(pa->footprint) - 1] = '\0';
    strncpy(pa->value, value, sizeof(pa->value) - 1);
    pa->value[sizeof(pa->value) - 1] = '\0';
    pa->pin        = pin;
    pa->mount_type = mount_type;

    attr->n_part_attrs = n + 1;
    return 0;
}

void x2_file_attr_free(X2FileAttribute *attr)
{
    assert(attr != NULL);
    free(attr->part_attrs);
    attr->part_attrs   = NULL;
    attr->n_part_attrs = 0;
}
