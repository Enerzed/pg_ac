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
#define MAX_NUM_MATCHES 256

PG_MODULE_MAGIC;


/* Aho Corasick trie node */
typedef struct
{
	struct ac_state* children[MAX_CHILDREN];									// Children of the node
	struct ac_state* fail_link;													// Failure link traverses to the link that has the longest common prefix
	struct ac_state* dictionary_link;											// Dictionary link traverses to the link that is also need to be considered
	char *lexeme;																// Lexeme stored
	int index;																	// Index of the lexeme
	bool is_root;																// Is root
	bool is_final;																// Is final

} ac_state;


/* Aho Corasick Automaton */
typedef struct 
{
    TSVector tsv;           													// Original TSVector
    ac_state *root;         													// Trie structure (transient, not stored)
} ac_automaton;


typedef struct 
{
	// Useful for ranking only
    int *matches;																// Match indices in the text
    int *counts;																// Number of matches (stored as 1 for each match index)
    int num_matches;															// Total number of matches
	int num_lexemes;															// Total number of lexemes
} ac_match_result;


extern void _PG_init(void);


/* Aho Corasick functions */
//static ac_automaton* ac_create_trie(TSVector tsv);								// Create Aho Corasick trie using keywords
//void ac_free_trie(ac_state* trie);												// Free trie
ac_state* ac_create_state();													// Create Aho Corasick state
void ac_add_keyword(ac_state* root, const char* keyword, const int index);		// Add keyword to the trie
void ac_build_failure_links(ac_state* root);									// Build failure links for the trie
void ac_build_dictionary_links(ac_state* root);									// Build dictionary links for the trie
ac_match_result ac_match(ac_state* root, char* text);							// Match indices
bool ac_contains(ac_state *root, const char *token, int *entries_count);		// Look if the word is in the trie
bool evaluate_query(QueryItem *item, TSQuery *tsq, ac_automaton *automaton);	// Evaluate query for the result

/* PostgreSQL-specific functions */
Datum ac_build(PG_FUNCTION_ARGS);												// Build Aho Corasick automaton
Datum ac_search(PG_FUNCTION_ARGS);												// Search in Aho Corasick automaton using TSQuery
Datum ac_search_text(PG_FUNCTION_ARGS);											// Search in Aho Corasick automaton using text
Datum ac_rank_simple(PG_FUNCTION_ARGS);											// Rank search result
Datum ac_automaton_in(PG_FUNCTION_ARGS);										// Parse Aho Corasick automaton
Datum ac_automaton_out(PG_FUNCTION_ARGS);										// Serialize Aho Corasick automaton