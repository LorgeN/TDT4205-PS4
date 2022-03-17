#include <vslc.h>

#define STRING_LIST_EXPANSION_STEP 8

// Externally visible, for the generator
extern tlhash_t *global_names;
extern char **string_list;
extern size_t n_string_list, stringc;

// Implementation choices, only relevant internally
static void find_globals(void);
/** @param function Function's symbol table entry
 *  @param root Function's root node */
static void bind_names(tlhash_t *global_lookup, symbol_t *function, node_t *root);

void __string_list_expand() {
    n_string_list += STRING_LIST_EXPANSION_STEP;
    string_list = realloc(string_list, sizeof(char *) * n_string_list);
}

void __string_list_append(char *str, size_t *index) {
    if (stringc >= n_string_list) {
        __string_list_expand();
    }

    *index = stringc++;
    string_list[(*index)] = str;
}

char *__format_name_type(symtype_t type) {
    switch (type) {
        case SYM_GLOBAL_VAR:
            return "Global Variable";
        case SYM_FUNCTION:
            return "Function";
        case SYM_PARAMETER:
            return "Parameter";
        case SYM_LOCAL_VAR:
            return "Local Variable";
        default:
            return "?";
    }
}

symbol_t *__declare(tlhash_t *table, node_t *node, char *name, symtype_t type, size_t *seq) {
    symbol_t *sym = malloc(sizeof(symbol_t));
    memset(sym, 0, sizeof(symbol_t));

    // Duplicate so that when the syntax tree is destroyed we don't lose name
    sym->name = strdup(name);
    sym->type = type;
    sym->seq = (*seq)++;
    sym->node = node;
    tlhash_insert(table, &sym->seq, sizeof(size_t), sym);
    return sym;
}

void __declare_variables(tlhash_t *symtab, tlhash_t *lookup, node_t *declaration, symtype_t type, size_t *seq) {
    if (declaration->n_children < 1) {
        exit(EXIT_FAILURE);
    }

    node_t *decl_list = declaration->children[0];
    node_t *identifier;
    symbol_t *curr;
    for (uint64_t i = 0; i < decl_list->n_children; i++) {
        identifier = decl_list->children[i];
        if (identifier->type != IDENTIFIER_DATA) {
            continue;
        }

        curr = __declare(symtab, identifier, (char *)identifier->data, type, seq);
        identifier->entry = curr;

        if (lookup != NULL) {
            tlhash_insert(lookup, curr->name, strlen(curr->name) + 1, curr);
        }
    }
}

void __declare_function(tlhash_t *symtab, node_t *function, size_t *seq) {
    if (function->n_children < 3)  // Need identifier, parameters and code block
    {
        exit(EXIT_FAILURE);
    }

    node_t *identifier = function->children[0];
    node_t *parameters = function->children[1];

    symbol_t *sym = __declare(symtab, function, identifier->data, SYM_FUNCTION, seq);

    // For now we will just assume this will always work fine
    tlhash_t *locals = malloc(sizeof(tlhash_t));
    memset(locals, 0, sizeof(tlhash_t));  // To be safe
    tlhash_init(locals, 16, NULL);
    sym->locals = locals;

    if (parameters == NULL) {
        sym->nparms = 0;
        return;
    }

    size_t local_seq = 0;  // Local sequencing number for the parameters
    size_t parameter_count = parameters->n_children;
    sym->nparms = parameter_count;

    node_t *param;
    for (uint64_t i = 0; i < parameter_count; i++) {
        param = parameters->children[i];
        if (param->type != IDENTIFIER_DATA) {
            continue;
        }

        param->entry = __declare(locals, param, (char *)param->data, SYM_PARAMETER, &local_seq);
    }
}

void create_symbol_table(void) {
    /* TODO: traverse the syntax tree and create the symbol table */

    // ! Example code solely to demonstrate usage of tlhash. Make sure to remove
    // ! or comment this out when implementing your solution.

    // Initialize table
    global_names = malloc(sizeof(tlhash_t));
    memset(global_names, 0, sizeof(tlhash_t));  // To be safe

    tlhash_init(global_names, 64, NULL);

    find_globals();

    size_t size = tlhash_size(global_names);

    size_t **keys = calloc(size, sizeof(size_t *));

    tlhash_keys(global_names, (void **)keys);

    // Make global lookup table with names as key
    tlhash_t *lookup = malloc(sizeof(tlhash_t));
    memset(lookup, 0, sizeof(tlhash_t));

    tlhash_init(lookup, 16, NULL);

    // Copy global data into temporary lookup table
    symbol_t *val;
    for (size_t i = 0; i < size; i++) {
        tlhash_lookup(global_names, keys[i], sizeof(size_t), (void **)&val);
        tlhash_insert(lookup, val->name, strlen(val->name) + 1, val);
    }

    for (size_t i = 0; i < size; i++) {
        tlhash_lookup(global_names, keys[i], sizeof(size_t), (void **)&val);
        if (val->type == SYM_FUNCTION) {
            bind_names(lookup, val, val->node);
        }
    }

    free(keys);
    tlhash_finalize(lookup);
    free(lookup);
}

void __print_sym_tab(tlhash_t *table) {
    // Iterate keys and lookup their values
    size_t size = tlhash_size(table);

    size_t **keys = calloc(size, sizeof(size_t *));

    tlhash_keys(table, (void **)keys);

    symbol_t *val;
    for (size_t i = 0; i < size; i++) {
        tlhash_lookup(table, keys[i], sizeof(size_t), (void **)&val);
        printf("%s [%ld] = %s\n", __format_name_type(val->type), *keys[i], val->name);
    }

    for (size_t i = 0; i < size; i++) {
        tlhash_lookup(table, keys[i], sizeof(size_t), (void **)&val);
        if (val->locals != NULL) {
            printf("\n%s \"%s\" Symbol Table\n", __format_name_type(val->type), val->name);
            __print_sym_tab(val->locals);
        }
    }

    free(keys);
}

void print_symbol_table() {
    printf("Global Symbol Table\n");
    __print_sym_tab(global_names);

    printf("\nString List\n");
    for (size_t i = 0; i < stringc; i++) {
        printf("%ld - %s\n", i, string_list[i]);
    }
}

void __destroy_recursive(tlhash_t *table) {
    size_t size = tlhash_size(table);

    size_t **keys = calloc(size, sizeof(size_t *));

    tlhash_keys(table, (void **)keys);

    symbol_t *val;
    for (size_t i = 0; i < size; i++) {
        tlhash_lookup(table, keys[i], sizeof(size_t), (void **)&val);
        if (val->locals != NULL) {
            __destroy_recursive(val->locals);
        }

        free(val->name);
        free(val);
    }

    tlhash_finalize(table);
    free(table);
    free(keys);
}

void destroy_symbol_table() {
    __destroy_recursive(global_names);
}

void find_globals(void) {
    if (root->n_children < 1) {
        exit(EXIT_FAILURE);
    }

    node_t *global_list = root->children[0];

    size_t seq = 0;
    node_t *child;
    for (uint64_t i = 0; i < global_list->n_children; i++) {
        child = global_list->children[i];

        switch (child->type) {
            case DECLARATION:
                __declare_variables(global_names, NULL, child, SYM_GLOBAL_VAR, &seq);
                break;
            case FUNCTION:
                __declare_function(global_names, child, &seq);
                break;
        }
    }
}

// Declare here so we can safely access in __traverse_scope
void __bind_block(symbol_t *function, node_t *root, tlhash_t *parent_lookup, size_t *seq);

void __traverse_scope(symbol_t *function, node_t *root, tlhash_t *lookup, size_t *seq) {
    if (root->n_children == 0) {
        return;
    }

    node_t *child;
    for (uint64_t i = 0; i < root->n_children; i++) {
        child = root->children[i];

        switch (child->type) {
            case BLOCK:
                // Continue recursively to evaluate any nested blocks
                __bind_block(function, child, lookup, seq);
                break;
            case DECLARATION:
                __declare_variables(function->locals, lookup, child, SYM_LOCAL_VAR, seq);
                break;
            case IDENTIFIER_DATA:
                char *id = child->data;
                symbol_t *sym;

                // Use recurse version so it will also consider parent tables
                int res = tlhash_lookup_recurse(lookup, id, strlen(id) + 1, (void **)&sym);
                if (res != TLHASH_SUCCESS) {
                    printf("Unknown variable %s\n", id);
                    exit(EXIT_FAILURE);  // Unknown variable
                }

                child->entry = sym;
                break;
            case STRING_DATA:
                size_t *index = malloc(sizeof(size_t));
                __string_list_append(child->data, index);
                child->data = index;
                break;
            default:
                // Recursively evaluate these. We specifically handle anything that changes
                // the scope, so it is safe to consider anything else in the same scope
                __traverse_scope(function, child, lookup, seq);
        }
    }
}

void __bind_block(symbol_t *function, node_t *root, tlhash_t *parent_lookup, size_t *seq) {
    tlhash_t *lookup = malloc(sizeof(tlhash_t));
    memset(lookup, 0, sizeof(tlhash_t));

    tlhash_init(lookup, 16, parent_lookup);

    __traverse_scope(function, root, lookup, seq);

    tlhash_finalize(lookup);
    free(lookup);
}

void bind_names(tlhash_t *global_lookup, symbol_t *function, node_t *root) {
    node_t *block = root->children[2];
    if (block->type != BLOCK) {
        exit(EXIT_FAILURE);
    }

    // Make global lookup table with names as key
    tlhash_t *lookup = malloc(sizeof(tlhash_t));
    memset(lookup, 0, sizeof(tlhash_t));

    tlhash_init(lookup, 16, global_lookup);

    // Insert the parameters of the function
    size_t size = tlhash_size(function->locals);
    size_t **keys = calloc(size, sizeof(size_t *));

    tlhash_keys(function->locals, (void **)keys);

    // Copy global data into temporary lookup table
    symbol_t *val;
    for (size_t i = 0; i < size; i++) {
        tlhash_lookup(function->locals, keys[i], sizeof(size_t), (void **)&val);
        tlhash_insert(lookup, val->name, strlen(val->name) + 1, val);
    }

    free(keys);

    size_t seq = 0;
    __bind_block(function, block, lookup, &seq);

    tlhash_finalize(lookup);
    free(lookup);
}
