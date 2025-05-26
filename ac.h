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

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <funcapi.h>


#define MAX_CHILDREN 256
#define MAX_LEXEME_SIZE 256
#define INITIAL_NELEM 256

PG_MODULE_MAGIC;


/* Aho Corasick trie node */
typedef struct
{
	/* Children of the node */
	struct ac_state* children[MAX_CHILDREN];
	/* Failure link traverses to the link that has the longest common prefix */
	struct ac_state* fail_link;
	/* Dictionary link traverses to the link that is also need to be considered */
	struct ac_state* dictionary_link;
	/* Index of the lexeme */
	int index;
	/* Depth of the node */
	int depth;
	/* Is the node contains a lexeme */
	bool is_final;
} ac_state;


/* Aho Corasick Automaton */
typedef struct 
{
	/* Trie */
	ac_state *root;
	/* Total number of lexemes */
    int num_lexemes;
} ac_automaton;


/* Aho Corasick automaton entry for storage */
typedef struct
{
	/* Automaton id */
	int32 id;
	/* Automaton */
	ac_automaton *automaton;
} ac_automaton_entry;


/* Useful for ranking only */
typedef struct 
{
	/* Match indices */
    int *matches;
	/* Number of matches for each match index, stored as 1 for each match */
    int *counts;
	/* Total number of matches */
    int num_matches;
} ac_match_result;


/* Global variables for automaton storage */
static HTAB *automaton_storage = NULL;
static int next_automaton_id = 1;


/* Init and fini */
void _PG_init(void);
void _PG_fini(void);


/*
 * Memory management 
 */
static void init_automaton_storage();
static void cleanup_automaton();
void ac_free_trie(ac_state* current);


/*
 * Aho Corasick functions 
 */

/* Create and initialize Aho Corasick state */
ac_state* ac_create_state();
/* Add keyword to the trie */
void ac_add_keyword(ac_state* root, const char* keyword, const int index);
/* Build failure and dictionary links */
void ac_build_links(ac_state* root);
/* Match indices */
ac_match_result ac_match(ac_state* root, char* text);
/* Look if the word is in the trie */
bool ac_contains(ac_state *root, const char *text);
/* Evaluate query (recursive)*/
bool evaluate_query(QueryItem *item, TSQuery *tsq, ac_automaton *automaton);


/* 
 * PostgreSQL-specific functions 
 */

/* Build Aho Corasick automaton */
Datum ac_build(PG_FUNCTION_ARGS);
/* Init Aho Corasick automaton storage */
Datum ac_init(PG_FUNCTION_ARGS);
/* Destroy all Aho Corasick automatons */
Datum ac_fini(PG_FUNCTION_ARGS);
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
PG_FUNCTION_INFO_V1(ac_build);
PG_FUNCTION_INFO_V1(ac_init);
PG_FUNCTION_INFO_V1(ac_destroy);
PG_FUNCTION_INFO_V1(ac_fini);
PG_FUNCTION_INFO_V1(ac_search_tsquery);
PG_FUNCTION_INFO_V1(ac_search_text);
PG_FUNCTION_INFO_V1(ac_match_text);
PG_FUNCTION_INFO_V1(ac_rank_simple);