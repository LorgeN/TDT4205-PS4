#ifndef VSLC_H
#define VSLC_H
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Prototypes for the hash table functions
#include "tlhash.h"

// Numbers and names for the types of syntax tree nodes
#include "nodetypes.h"

// Definition of the tree node type
#include "ir.h"

// Token definitions and other things from bison, needs def. of node type
#include "y.tab.h"

/* This is generated from the bison grammar, calls on the flex specification */
int yyerror(const char *error);

/* These are defined in the parser generated by bison */
extern int yylineno;
extern int yylex(void);
extern char yytext[];

/* Global state */
extern node_t *root;

// Moving global defs to global header

extern tlhash_t *global_names;  // Defined in ir.c, used by generator.c
extern char **string_list;      // Defined in ir.c, used by generator.c
extern size_t stringc;          // Defined in ir.c, used by generator.c

/* Global routines, called from main in vslc.c */
void simplify_syntax_tree(void);
void print_syntax_tree(void);
void destroy_syntax_tree(void);

void create_symbol_table(void);
void print_symbol_table(void);
void destroy_symbol_table(void);

#endif
