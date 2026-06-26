/**
 * @file schematic_sexpr.h
 * @brief S-expression parser for KiCad schematic file format (.kicad_sch)
 *
 * KiCad v6+ uses a Lisp-style S-expression format for schematic files.
 * This module provides a tokenizer, parser, and AST construction for
 * reading and writing KiCad schematic data.
 *
 * S-expression grammar (subset for KiCad):
 *   sexpr  ::= atom | '(' sexpr* ')'
 *   atom   ::= string | number | symbol
 *   string ::= '"' [^"]* '"'
 *   number ::= [-+]?[0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?
 *   symbol ::= [a-zA-Z_][a-zA-Z0-9_-]*
 *
 * Course Mapping:
 *   - MIT 6.035 Compiler Design — lexing/parsing
 *   - CMU 15-411 Compiler Design — recursive descent parsing
 *   - Stanford CS143 — tokenizers and AST representation
 *
 * Reference: KiCad Schematic File Format v20230121
 */

#ifndef SCHEMATIC_SEXPR_H
#define SCHEMATIC_SEXPR_H

#include "schematic_core.h"
#include <stdio.h>

/* ──────────── S-Expression Token Types ──────────── */

typedef enum {
    SEXPR_TOK_LPAREN  = 0,   /* ( */
    SEXPR_TOK_RPAREN  = 1,   /* ) */
    SEXPR_TOK_SYMBOL  = 2,   /* unquoted identifier */
    SEXPR_TOK_STRING  = 3,   /* "quoted string" */
    SEXPR_TOK_NUMBER  = 4,   /* integer or float */
    SEXPR_TOK_EOF     = 5    /* end of input */
} sexpr_token_type_t;

/**
 * @brief Single token in S-expression stream
 */
typedef struct {
    sexpr_token_type_t type;
    char       text[256];      /* Token text content */
    int        line;            /* Source line number */
    int        column;          /* Source column */
} sexpr_token_t;

/* ──────────── S-Expression AST ──────────── */

/**
 * @brief S-expression tree node
 *
 * Each node is either an atom (symbol/string/number) or a list
 * of child nodes. This mirrors the Lisp cons-cell model using
 * a tree structure suitable for KiCad file parsing.
 */
typedef struct sexpr_node {
    enum { SEXPR_ATOM, SEXPR_LIST } node_type;
    union {
        struct {
            char value[256];        /* Atom string value */
            sexpr_token_type_t atom_type; /* SYMBOL, STRING, or NUMBER */
        } atom;
        struct {
            int      num_children;
            struct sexpr_node **children;
        } list;
    } data;
    int        line;            /* Source line for error reporting */
} sexpr_node_t;

/**
 * @brief Tokenizer state machine
 */
typedef struct {
    const char *input;           /* Input buffer */
    size_t      pos;             /* Current position */
    size_t      length;          /* Input length */
    int         line;            /* Current line number */
    int         column;          /* Current column */
    sexpr_token_t current;       /* Current token (after peek/next) */
    bool        has_current;     /* Whether current token is valid */
} sexpr_tokenizer_t;

/* ──────────── KiCad Specific Keywords (L1) ──────────── */

/** Root element names in KiCad schematic file */
#define SEXPR_KICAD_SCH         "kicad_sch"
#define SEXPR_VERSION           "version"
#define SEXPR_GENERATOR         "generator"
#define SEXPR_TITLE_BLOCK       "title_block"
#define SEXPR_SYMBOL            "symbol"
#define SEXPR_WIRE              "wire"
#define SEXPR_JUNCTION          "junction"
#define SEXPR_NO_CONNECT        "no_connect"
#define SEXPR_LABEL             "label"
#define SEXPR_GLOBAL_LABEL      "global_label"
#define SEXPR_HIERARCHICAL_LABEL "hierarchical_label"
#define SEXPR_SHEET             "sheet"
#define SEXPR_TEXT               "text"
#define SEXPR_BUS                "bus"
#define SEXPR_BUS_ENTRY          "bus_entry"
#define SEXPR_POLYLINE           "polyline"

/** Property names within symbols */
#define SEXPR_PROPERTY          "property"
#define SEXPR_REFERENCE_PROP    "Reference"
#define SEXPR_VALUE_PROP        "Value"
#define SEXPR_FOOTPRINT_PROP    "Footprint"
#define SEXPR_DATASHEET_PROP    "Datasheet"

/** Pin-related elements */
#define SEXPR_PIN               "pin"
#define SEXPR_PIN_NAME          "name"
#define SEXPR_PIN_NUMBER        "number"
#define SEXPR_PIN_TYPE          "pin_type"

/* ──────────── S-Expression Parser API ──────────── */

/** Initialize tokenizer with input buffer */
void sexpr_tokenizer_init(sexpr_tokenizer_t *t, const char *input, size_t len);

/** Advance to next token, return token type */
sexpr_token_type_t sexpr_next_token(sexpr_tokenizer_t *t);

/** Peek at current token without consuming */
const sexpr_token_t* sexpr_peek_token(sexpr_tokenizer_t *t);

/** Parse one S-expression from tokenizer, returns AST root */
sexpr_node_t* sexpr_parse(sexpr_tokenizer_t *t);

/** Parse a list of S-expressions until EOF */
sexpr_node_t* sexpr_parse_all(sexpr_tokenizer_t *t, int *num_exprs);

/** Free an S-expression AST */
void sexpr_free(sexpr_node_t *node);

/** Pretty-print S-expression to FILE* for debugging */
void sexpr_print(FILE *fp, const sexpr_node_t *node, int indent);

/** Serialize S-expression to string buffer */
int sexpr_serialize(const sexpr_node_t *node, char *buf, size_t bufsz);

/** Serialize to FILE* in KiCad-compatible format */
int sexpr_write_kicad(FILE *fp, const sexpr_node_t *node, int indent);

/* ──────────── AST Navigation / Query ──────────── */

/** Check if node is a list starting with given symbol */
bool sexpr_is_list_with(const sexpr_node_t *node, const char *symbol);

/** Get child node by index (0-based) */
sexpr_node_t* sexpr_child(const sexpr_node_t *node, int idx);

/** Get number of children in a list node */
int sexpr_num_children(const sexpr_node_t *node);

/** Get string value of an atom node */
const char* sexpr_atom_value(const sexpr_node_t *node);

/** Find first child list with given head symbol */
sexpr_node_t* sexpr_find_child(const sexpr_node_t *node, const char *symbol);

/** Get property value string from a symbol node (e.g., get Value from (property "Value" "10k")) */
const char* sexpr_get_property(const sexpr_node_t *symbol_node, const char *prop_name);

/* ──────────── KiCad Schematic File I/O ──────────── */

/**
 * @brief Load a KiCad .kicad_sch file into a schematic_design_t
 *
 * Parses the S-expression format, extracting all symbols, wires,
 * junctions, labels, sheets, and metadata into the canonical
 * schematic_design_t representation.
 *
 * @param filename Path to .kicad_sch file
 * @param sch Pointer to design (must be pre-allocated)
 * @return 0 on success, negative on error
 */
int kicad_sch_load(const char *filename, schematic_design_t *sch);

/**
 * @brief Write a schematic_design_t to a .kicad_sch file
 *
 * @param filename Output path
 * @param sch Design to write
 * @return 0 on success, negative on error
 */
int kicad_sch_write(const char *filename, const schematic_design_t *sch);

/**
 * @brief Parse a single KiCad symbol (kicad_sym library entry)
 */
schematic_component_t* kicad_sym_parse(const char *filename);

#endif /* SCHEMATIC_SEXPR_H */