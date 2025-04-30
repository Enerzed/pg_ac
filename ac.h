/*
 * ac.h
 */


#pragma once


#include "postgres.h"
#include "fmgr.h"

#include "utils/palloc.h"
#include "utils/memutils.h"
#include "utils/array.h"
#include "utils/lsyscache.h"
#include "utils/builtins.h"
#include "utils/hsearch.h"
#include "utils/jsonb.h"

#include "tsearch/ts_cache.h"
#include "tsearch/ts_locale.h"
#include "tsearch/ts_public.h"
#include "tsearch/ts_type.h"
#include "tsearch/ts_utils.h"

#include "catalog/pg_ts_dict.h"
#include "catalog/namespace.h"
#include "nodes/parsenodes.h"
#include "access/tupdesc.h"
#include "common/hashfn.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <funcapi.h>


#define MAX_CHILDREN 256


PG_MODULE_MAGIC;


/* Aho Corasick trie node */
typedef struct
{
	struct ac_state* children[MAX_CHILDREN];
	struct ac_state* fail_link;
	struct ac_state* dictionary_link;
	char *lexeme;
	int index;
	bool is_root;
	bool is_final;

} ac_state;


/* Aho Corasick Automaton */
typedef struct
{
    ac_state *root;
} ac_automaton;


extern void _PG_init(void);


/* Aho Corasick functions */
ac_state* ac_create_state();
void ac_add_keyword(ac_state* root, const char* keyword, const int index);
void ac_build_failure_links(ac_state* root);
void ac_build_dictionary_links(ac_state* root);
int ac_match(ac_state* root, char* text, int** match_indices, bool is_case_sensitive);
void ac_free_trie(ac_state* trie);
ac_state* ac_create_trie(const char** keywords, int size);
static bool ac_contains(ac_state *root, const char *token);
void print_trie(ac_state *root);
static void ac_automaton_destroy(ac_automaton *automaton);

/* PostgreSQL-specific functions */
Datum ac_search(PG_FUNCTION_ARGS);