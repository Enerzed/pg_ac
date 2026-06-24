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
	ac_edge *edges;							/* Dynamic edges array */
	int edge_count;							/* Number of edges */
	int edge_capacity;						/* Current edges capacity */
	struct ac_state* fail_link;				/* Failure link traverses to the link that has the longest common prefix */
	struct ac_state* dictionary_link;		/* Dictionary link traverses to the link that is also need to be considered */
	int index;								/* Index of current lexeme */
	int depth;								/* Depth of the node */
	bool is_final;							/* Does the node contain a lexeme? */
} ac_state;


/* Aho Corasick Automaton */
typedef struct ac_automaton
{
	ac_state *root;							/* Trie */
    int num_lexemes;						/* Total number of lexemes */
} ac_automaton;


/* Aho Corasick automaton entry for storage */
typedef struct ac_automaton_entry
{
	int64 id;								/* Automaton id */
	ac_automaton *automaton;				/* Automaton */
} ac_automaton_entry;


/* Useful for ranking only */
typedef struct ac_match_result
{
    int *matches;							/* Match indices */
    int *counts;							/* Number of matches for each match index, stored as 1 for each match */
    int num_matches;						/* Total number of matches */
} ac_match_result;


/* Global variables for automaton storage */
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
void ac_free_trie(ac_state* current);


/*
 * Aho Corasick functions 
 */

/* Create and initialize Aho Corasick state */
ac_state* ac_create_state(void);
/* Add keyword to the trie */
void ac_add_keyword(ac_state* root, const char* keyword, const int index);
/* Build failure and dictionary links */
void ac_build_failure_links(ac_state* root);
void ac_build_dictionary_links(ac_state* root);
/* Match indices */
ac_match_result ac_match(ac_state* root, char* text);
/* Look if the word is in the trie */
bool ac_contains(ac_state *root, const char *text);
/* Evaluate query (recursive)*/
bool evaluate_query(QueryItem *item, TSQuery tsq, ac_automaton *automaton);


/* 
 * PostgreSQL-specific functions 
 */

/* Init Aho Corasick automaton storage */
Datum ac_init(PG_FUNCTION_ARGS);
/* Destroy all Aho Corasick automatons */
Datum ac_fini(PG_FUNCTION_ARGS);
/* Build Aho Corasick automaton from TSVector */
Datum ac_build_tsvector(PG_FUNCTION_ARGS);
/* Build Aho Corasick automaton from Array*/
Datum ac_build_array(PG_FUNCTION_ARGS);
/* Destroy Aho Corasick automaton */
Datum ac_destroy(PG_FUNCTION_ARGS);
/* Search in Aho Corasick automaton using TSQuery */
Datum ac_search_tsquery(PG_FUNCTION_ARGS);
/* Search in Aho Corasick automaton using text */
Datum ac_search_text(PG_FUNCTION_ARGS);
/* Match indices in Aho Corasick automaton using text */
Datum ac_match_text(PG_FUNCTION_ARGS);
/* Rank search result */
Datum ac_rank_simple(PG_FUNCTION_ARGS);

/*
 * PostgreSQL initialize functions 
*/

PG_FUNCTION_INFO_V1(ac_init);
PG_FUNCTION_INFO_V1(ac_fini);
PG_FUNCTION_INFO_V1(ac_build_tsvector);
PG_FUNCTION_INFO_V1(ac_build_array);
PG_FUNCTION_INFO_V1(ac_destroy);
PG_FUNCTION_INFO_V1(ac_search_tsquery);
PG_FUNCTION_INFO_V1(ac_search_text);
PG_FUNCTION_INFO_V1(ac_match_text);
PG_FUNCTION_INFO_V1(ac_rank_simple);