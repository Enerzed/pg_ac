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

#include "access/tupdesc.h"
#include "access/htup_details.h"

#include "catalog/pg_ts_dict.h"
#include "catalog/namespace.h"

#include "nodes/parsenodes.h"
#include "common/hashfn.h"
#include "libpq/pqformat.h"
#include "mb/pg_wchar.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <funcapi.h>

#define MAX_LEXEME_SIZE 256
#define INITIAL_NELEM 256

PG_MODULE_MAGIC;

/* Edge - Unicode character and pointer to child */
typedef struct ac_edge
{
	int ch;									/* Unicode character */
	struct ac_state *child;					/* Pointer to child */
} ac_edge;

/* Aho Corasick trie node */
typedef struct ac_state
{
	struct ac_state *fail_link;				/* Failure link */
	struct ac_state *dictionary_link;		/* Dictionary link */
	ac_edge *edges;							/* Dynamic edges array */
	int64 edge_count;						/* Number of edges */
	int64 edge_capacity;					/* Current edges capacity */
	int64 index;							/* Index of current lexeme (or -1) */
	int64 depth;							/* Depth of the node */
	bool is_final;							/* Does the node contain a lexeme? */
} ac_state;

/* Keyword entry for dynamic management */
typedef struct ac_keyword
{
	char *keyword;
	int64 index;
} ac_keyword;

/* Aho Corasick Automaton */
typedef struct ac_automaton
{
	ac_state *root;							/* Trie root */
	int64 num_lexemes;						/* Total number of lexemes */
	int64 next_index;						/* Next free index for new words */
	bool dirty;								/* Needs rebuild */
	ac_keyword *keywords;					/* Array of active keywords */
	int64 keyword_count;
	int64 keyword_capacity;
} ac_automaton;

/* Automaton entry for storage in hash table */
typedef struct ac_automaton_entry
{
	int64 id;
	ac_automaton *automaton;
} ac_automaton_entry;

/* Result of ac_match */
typedef struct ac_match_result
{
	int64 *matches;							/* Match indices */
	int64 *counts;							/* Always 1 for each match */
	int64 num_matches;						/* Number of matches */
} ac_match_result;

/* Global storage */
static HTAB *automaton_storage = NULL;
static int64 next_automaton_id = 1;

/*
 * Init and fini
 */
void _PG_init(void);
void _PG_fini(void);

/*
 * Memory management
 */
static void _ac_init(void);
static void _ac_fini(void);
void ac_free_trie(ac_state *current);

/*
 * Aho Corasick core functions
 */
ac_state *ac_create_state(void);
void ac_add_keyword(ac_state *root, const char *keyword, const int64 index);
void ac_build_failure_links(ac_state *root);
void ac_build_dictionary_links(ac_state *root);
ac_match_result ac_match(ac_state *root, char *text);
bool ac_contains(ac_state *root, const char *text);
bool evaluate_query(QueryItem *item, TSQuery tsq, ac_automaton *automaton);

/*
 * PostgreSQL‑callable functions
 */
Datum ac_init(PG_FUNCTION_ARGS);
Datum ac_fini(PG_FUNCTION_ARGS);
Datum ac_build_tsvector(PG_FUNCTION_ARGS);
Datum ac_build_array(PG_FUNCTION_ARGS);
Datum ac_add(PG_FUNCTION_ARGS);
Datum ac_remove(PG_FUNCTION_ARGS);
Datum ac_destroy(PG_FUNCTION_ARGS);
Datum ac_search_tsquery(PG_FUNCTION_ARGS);
Datum ac_search_text(PG_FUNCTION_ARGS);
Datum ac_match_text(PG_FUNCTION_ARGS);
Datum ac_rank_simple(PG_FUNCTION_ARGS);

/*
 * Function info declarations
 */
PG_FUNCTION_INFO_V1(ac_init);
PG_FUNCTION_INFO_V1(ac_fini);
PG_FUNCTION_INFO_V1(ac_build_tsvector);
PG_FUNCTION_INFO_V1(ac_build_array);
PG_FUNCTION_INFO_V1(ac_add);
PG_FUNCTION_INFO_V1(ac_remove);
PG_FUNCTION_INFO_V1(ac_destroy);
PG_FUNCTION_INFO_V1(ac_search_tsquery);
PG_FUNCTION_INFO_V1(ac_search_text);
PG_FUNCTION_INFO_V1(ac_match_text);
PG_FUNCTION_INFO_V1(ac_rank_simple);