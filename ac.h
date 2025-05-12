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
#define MAX_LEXEME_SIZE 256
#define MAX_NUM_MATCHES 256

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
	TSVector tsv;
	char **lexemes;
	int *term_freq;
	int num_lexemes;
	int total_terms;
} ac_automaton;


typedef struct 
{
    int *matches;
    int *counts;
    int num_matches;
} ac_match_result;


extern void _PG_init(void);


/* Aho Corasick functions */
ac_state* ac_create_trie(const char** keywords, int size);						// Create Aho Corasick trie using keywords
void ac_free_trie(ac_state* trie);												// Free trie
ac_state* ac_create_state();													// Create Aho Corasick state
void ac_add_keyword(ac_state* root, const char* keyword, const int index);		// Add keyword to the trie
void ac_build_failure_links(ac_state* root);									// Build failure links for the trie
void ac_build_dictionary_links(ac_state* root);									// Build dictionary links for the trie
ac_match_result ac_match(ac_state* root, char* text);							// Match indices
bool ac_contains(ac_state *root, const char *token, int *entries_count);		// Look if the word is in the trie
bool evaluate_query(QueryItem *item, TSQuery *tsq, ac_automaton *automaton);	// Evaluate query for the result

/* PostgreSQL-specific functions */
Datum ac_automaton_in(PG_FUNCTION_ARGS);
Datum ac_automaton_out(PG_FUNCTION_ARGS);
Datum ac_search(PG_FUNCTION_ARGS);
Datum ac_search_text(PG_FUNCTION_ARGS);