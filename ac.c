/*
 * ac.c
 */

#include "ac.h"

/*
 * Init and fini
 */
void _PG_init(void) { /* called on load – we do nothing, user must call ac_init() */ }
void _PG_fini(void) { /* called on unload – we do nothing, user must call ac_fini() */ }

/*
 * Memory management
 */
static void _ac_init(void)
{
	HASHCTL ctl;
	memset(&ctl, 0, sizeof(ctl));
	ctl.keysize = sizeof(int64);
	ctl.entrysize = sizeof(ac_automaton_entry);
	ctl.hcxt = TopMemoryContext;
	automaton_storage = hash_create("automaton storage", INITIAL_NELEM, &ctl, HASH_ELEM | HASH_CONTEXT);
}

static void _ac_fini(void)
{
	HASH_SEQ_STATUS status;
	ac_automaton_entry *entry;

	if (automaton_storage == NULL)
		return;

	hash_seq_init(&status, automaton_storage);
	while ((entry = (ac_automaton_entry *) hash_seq_search(&status)) != NULL)
	{
		ac_automaton *automaton = entry->automaton;
		if (automaton)
		{
			if (automaton->root)
				ac_free_trie(automaton->root);
			if (automaton->keywords)
			{
				for (int64 i = 0; i < automaton->keyword_count; i++)
					pfree(automaton->keywords[i].keyword);
				pfree(automaton->keywords);
			}
			pfree(automaton);
		}
	}
	hash_destroy(automaton_storage);
	automaton_storage = NULL;
}

void ac_free_trie(ac_state *current)
{
	if (current == NULL)
		return;
	for (int64 i = 0; i < current->edge_count; i++)
		ac_free_trie(current->edges[i].child);
	if (current->edges)
		pfree(current->edges);
	pfree(current);
}

/*
 * Edge helpers
 */
static ac_edge *find_edge(ac_state *state, int ch)
{
	int64 lo = 0, hi = state->edge_count - 1;
	while (lo <= hi)
	{
		int64 mid = (lo + hi) / 2;
		if (state->edges[mid].ch == ch)
			return &state->edges[mid];
		else if (state->edges[mid].ch < ch)
			lo = mid + 1;
		else
			hi = mid - 1;
	}
	return NULL;
}

static void add_edge(ac_state *state, int ch, ac_state *child)
{
	if (state->edge_count == state->edge_capacity)
	{
		int64 new_capacity = state->edge_capacity ? state->edge_capacity * 2 : 8;
		if (state->edges == NULL)
			state->edges = (ac_edge *) palloc(new_capacity * sizeof(ac_edge));
		else
			state->edges = (ac_edge *) repalloc(state->edges, new_capacity * sizeof(ac_edge));
		state->edge_capacity = new_capacity;
	}
	int64 pos = state->edge_count;
	while (pos > 0 && state->edges[pos - 1].ch > ch)
	{
		state->edges[pos] = state->edges[pos - 1];
		pos--;
	}
	state->edges[pos].ch = ch;
	state->edges[pos].child = child;
	state->edge_count++;
}

/*
 * Trie operations
 */
ac_state *ac_create_state(void)
{
	ac_state *state = (ac_state *) palloc0(sizeof(ac_state));
	state->is_final = false;
	state->index = -1;
	state->depth = 0;
	state->fail_link = NULL;
	state->dictionary_link = NULL;
	state->edges = NULL;
	state->edge_count = 0;
	state->edge_capacity = 0;
	return state;
}

void ac_add_keyword(ac_state *root, const char *keyword, const int64 index)
{
	ac_state *current = root;
	const unsigned char *p = (const unsigned char *) keyword;
	int64 depth = 0;
	while (*p)
	{
		int ch = utf8_to_unicode(p);
		p += pg_utf_mblen(p);

		ac_edge *e = find_edge(current, ch);
		if (!e)
		{
			ac_state *child = ac_create_state();
			add_edge(current, ch, child);
			e = find_edge(current, ch);
		}
		current = e->child;
		current->depth = ++depth;
	}
	current->is_final = true;
	current->index = index;
}

/*
 * Internal functions for dynamic management
 */
static void ac_add_word_to_automaton(ac_automaton *automaton, const char *keyword)
{
	/* Check for duplicate */
	for (int64 i = 0; i < automaton->keyword_count; i++)
	{
		if (strcmp(automaton->keywords[i].keyword, keyword) == 0)
			return; /* already exists */
	}

	if (automaton->keyword_count >= automaton->keyword_capacity)
	{
		automaton->keyword_capacity = automaton->keyword_capacity ? automaton->keyword_capacity * 2 : 8;
		automaton->keywords = (ac_keyword *) repalloc(automaton->keywords,
													  automaton->keyword_capacity * sizeof(ac_keyword));
	}
	automaton->keywords[automaton->keyword_count].keyword = pstrdup(keyword);
	automaton->keywords[automaton->keyword_count].index = automaton->next_index++;
	automaton->keyword_count++;
	automaton->dirty = true;
}

static void ac_remove_word_from_automaton(ac_automaton *automaton, const char *keyword)
{
	for (int64 i = 0; i < automaton->keyword_count; i++)
	{
		if (strcmp(automaton->keywords[i].keyword, keyword) == 0)
		{
			pfree(automaton->keywords[i].keyword);
			/* Shift remaining elements */
			for (int64 j = i; j < automaton->keyword_count - 1; j++)
				automaton->keywords[j] = automaton->keywords[j + 1];
			automaton->keyword_count--;
			automaton->dirty = true;
			return;
		}
	}
}

static void ac_rebuild_automaton(ac_automaton *automaton)
{
	if (!automaton->dirty)
		return;

	/* Free old trie */
	if (automaton->root)
	{
		ac_free_trie(automaton->root);
		automaton->root = NULL;
	}

	/* Create new root and add all keywords */
	automaton->root = ac_create_state();
	automaton->num_lexemes = automaton->keyword_count;
	automaton->next_index = 1;

	for (int64 i = 0; i < automaton->keyword_count; i++)
	{
		ac_add_keyword(automaton->root,
					   automaton->keywords[i].keyword,
					   automaton->keywords[i].index);
		if (automaton->keywords[i].index >= automaton->next_index)
			automaton->next_index = automaton->keywords[i].index + 1;
	}

	/* Build failure and dictionary links */
	ac_build_failure_links(automaton->root);
	ac_build_dictionary_links(automaton->root);

	automaton->dirty = false;
}

/*
 * Build failure links (BFS)
 */
void ac_build_failure_links(ac_state *root)
{
	int64 queue_capacity = 16, queue_size = 0;
	ac_state **queue = (ac_state **) palloc0(queue_capacity * sizeof(ac_state *));
	int64 front = 0, rear = 0;

	/* Children of root get fail_link = root */
	for (int64 i = 0; i < root->edge_count; i++)
	{
		ac_state *child = root->edges[i].child;
		child->fail_link = root;
		if (queue_size == queue_capacity)
		{
			queue_capacity *= 2;
			queue = (ac_state **) repalloc(queue, queue_capacity * sizeof(ac_state *));
		}
		queue[rear++] = child;
		queue_size++;
	}

	while (front < rear)
	{
		ac_state *current = queue[front++];
		for (int64 i = 0; i < current->edge_count; i++)
		{
			ac_edge *e = &current->edges[i];
			int ch = e->ch;
			ac_state *child = e->child;

			ac_state *fail = current->fail_link;
			while (fail && !find_edge(fail, ch))
				fail = fail->fail_link;
			if (fail)
			{
				ac_edge *fe = find_edge(fail, ch);
				child->fail_link = fe ? fe->child : root;
			}
			else
				child->fail_link = root;

			if (queue_size == queue_capacity)
			{
				queue_capacity *= 2;
				queue = (ac_state **) repalloc(queue, queue_capacity * sizeof(ac_state *));
			}
			queue[rear++] = child;
			queue_size++;
		}
	}
	pfree(queue);
}

/*
 * Build dictionary links (BFS)
 */
void ac_build_dictionary_links(ac_state *root)
{
	int64 queue_capacity = 16, queue_size = 0;
	ac_state **queue = (ac_state **) palloc(queue_capacity * sizeof(ac_state *));
	int64 front = 0, rear = 0;

	for (int64 i = 0; i < root->edge_count; i++)
	{
		ac_state *child = root->edges[i].child;
		if (queue_size == queue_capacity)
		{
			queue_capacity *= 2;
			queue = (ac_state **) repalloc(queue, queue_capacity * sizeof(ac_state *));
		}
		queue[rear++] = child;
		queue_size++;
	}

	while (front < rear)
	{
		ac_state *current = queue[front++];
		ac_state *fail = current->fail_link;
		if (fail && fail->is_final)
			current->dictionary_link = fail;
		else if (fail && fail->dictionary_link)
			current->dictionary_link = fail->dictionary_link;
		else
			current->dictionary_link = NULL;

		for (int64 i = 0; i < current->edge_count; i++)
		{
			ac_state *child = current->edges[i].child;
			if (queue_size == queue_capacity)
			{
				queue_capacity *= 2;
				queue = (ac_state **) repalloc(queue, queue_capacity * sizeof(ac_state *));
			}
			queue[rear++] = child;
			queue_size++;
		}
	}
	pfree(queue);
}

/*
 * Match text (returns indices of all matches)
 */
ac_match_result ac_match(ac_state *root, char *text)
{
	ac_state *current = root;
	ac_match_result result = {0};
	int64 capacity = 16;
	int64 *matches = (int64 *) palloc(capacity * sizeof(int64));
	int64 *counts = (int64 *) palloc(capacity * sizeof(int64));
	const unsigned char *p = (const unsigned char *) text;

	while (*p)
	{
		int ch = utf8_to_unicode(p);
		p += pg_utf_mblen(p);

		while (current && !find_edge(current, ch))
			current = current->fail_link;
		if (current)
		{
			ac_edge *e = find_edge(current, ch);
			current = e ? e->child : root;
		}
		else
			current = root;

		for (ac_state *temp = current; temp; temp = temp->dictionary_link)
		{
			if (temp->is_final)
			{
				if (result.num_matches == capacity)
				{
					capacity *= 2;
					matches = (int64 *) repalloc(matches, capacity * sizeof(int64));
					counts = (int64 *) repalloc(counts, capacity * sizeof(int64));
				}
				matches[result.num_matches] = temp->index;
				counts[result.num_matches] = 1;
				result.num_matches++;
			}
		}
	}
	result.matches = matches;
	result.counts = counts;
	return result;
}

/*
 * Check if any keyword exists in text (early exit)
 */
bool ac_contains(ac_state *root, const char *text)
{
	ac_state *current = root;
	const unsigned char *p = (const unsigned char *) text;
	while (*p)
	{
		int ch = utf8_to_unicode(p);
		p += pg_utf_mblen(p);

		while (current && !find_edge(current, ch))
			current = current->fail_link;
		if (current)
		{
			ac_edge *e = find_edge(current, ch);
			current = e ? e->child : root;
		}
		else
			current = root;

		for (ac_state *temp = current; temp; temp = temp->dictionary_link)
			if (temp->is_final)
				return true;
	}
	return false;
}

/*
 * Evaluate TSQuery recursively
 */
bool evaluate_query(QueryItem *item, TSQuery tsq, ac_automaton *automaton)
{
	if (item->type == QI_VAL)
	{
		char *lexeme = pnstrdup(GETOPERAND(tsq) + item->qoperand.distance, item->qoperand.length);
		bool found = ac_contains(automaton->root, lexeme);
		pfree(lexeme);
		return found;
	}
	else if (item->type == QI_OPR)
	{
		switch (item->qoperator.oper)
		{
			case OP_AND:
				return evaluate_query(item + item->qoperator.left, tsq, automaton) &&
					   evaluate_query(item + 1, tsq, automaton);
			case OP_OR:
				return evaluate_query(item + item->qoperator.left, tsq, automaton) ||
					   evaluate_query(item + 1, tsq, automaton);
			case OP_NOT:
				return !evaluate_query(item + item->qoperator.left, tsq, automaton);
			default:
				elog(ERROR, "unrecognized operator: %d", item->qoperator.oper);
		}
	}
	return false;
}

/*
 * PostgreSQL‑callable functions
 */

Datum ac_init(PG_FUNCTION_ARGS)
{
	_ac_init();
	PG_RETURN_BOOL(true);
}

Datum ac_fini(PG_FUNCTION_ARGS)
{
	_ac_fini();
	PG_RETURN_BOOL(true);
}

Datum ac_build_tsvector(PG_FUNCTION_ARGS)
{
	TSVector tsv = PG_GETARG_TSVECTOR_COPY(0);
	WordEntry *entries = ARRPTR(tsv);
	ac_automaton *automaton;
	int64 id;
	ac_automaton_entry *entry;
	MemoryContext oldctx;

	oldctx = MemoryContextSwitchTo(TopMemoryContext);

	automaton = (ac_automaton *) palloc0(sizeof(ac_automaton));
	automaton->num_lexemes = tsv->size;
	automaton->next_index = tsv->size + 1;
	automaton->dirty = false;
	automaton->keyword_count = 0;
	automaton->keyword_capacity = tsv->size;
	automaton->keywords = (ac_keyword *) palloc(automaton->keyword_capacity * sizeof(ac_keyword));
	automaton->root = ac_create_state();

	for (int i = 0; i < tsv->size; i++)
	{
		WordEntry *we = &entries[i];
		char *lexeme = pnstrdup(STRPTR(tsv) + we->pos, we->len);
		int64 index = -1;

		if (we->haspos)
		{
			uint16 npos = POSDATALEN(tsv, we);
			WordEntryPos *pos_ptr = POSDATAPTR(tsv, we);
			if (npos > 0)
				index = (int64) WEP_GETPOS(pos_ptr[0]);
		}

		if (index == -1)
			ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
							errmsg("tsvector entry has no positional data")));

		/* Store keyword */
		automaton->keywords[automaton->keyword_count].keyword = pstrdup(lexeme);
		automaton->keywords[automaton->keyword_count].index = index;
		automaton->keyword_count++;

		ac_add_keyword(automaton->root, lexeme, index);
		pfree(lexeme);
	}

	ac_build_failure_links(automaton->root);
	ac_build_dictionary_links(automaton->root);

	id = next_automaton_id++;
	entry = hash_search(automaton_storage, &id, HASH_ENTER, NULL);
	entry->id = id;
	entry->automaton = automaton;

	MemoryContextSwitchTo(oldctx);
	PG_RETURN_INT64(id);
}

Datum ac_build_array(PG_FUNCTION_ARGS)
{
	ArrayType *input_array = PG_GETARG_ARRAYTYPE_P(0);
	ac_automaton *automaton;
	MemoryContext oldctx;
	int64 id;
	ac_automaton_entry *entry;
	Datum *lexeme_datums;
	bool *nulls;
	int nlexemes;
	int i;

	deconstruct_array(input_array, TEXTOID, -1, false, 'i',
					  &lexeme_datums, &nulls, &nlexemes);

	oldctx = MemoryContextSwitchTo(TopMemoryContext);

	automaton = (ac_automaton *) palloc0(sizeof(ac_automaton));
	automaton->num_lexemes = nlexemes;
	automaton->next_index = nlexemes + 1;
	automaton->dirty = false;
	automaton->keyword_count = 0;
	automaton->keyword_capacity = nlexemes;
	automaton->keywords = (ac_keyword *) palloc(automaton->keyword_capacity * sizeof(ac_keyword));
	automaton->root = ac_create_state();

	for (i = 0; i < nlexemes; i++)
	{
		char *lexeme;
		if (nulls[i])
			ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
							errmsg("array element cannot be NULL")));

		lexeme = TextDatumGetCString(lexeme_datums[i]);
		int64 index = i + 1;

		automaton->keywords[automaton->keyword_count].keyword = pstrdup(lexeme);
		automaton->keywords[automaton->keyword_count].index = index;
		automaton->keyword_count++;

		ac_add_keyword(automaton->root, lexeme, index);
		pfree(lexeme);
	}

	ac_build_failure_links(automaton->root);
	ac_build_dictionary_links(automaton->root);

	id = next_automaton_id++;
	entry = hash_search(automaton_storage, &id, HASH_ENTER, NULL);
	entry->id = id;
	entry->automaton = automaton;

	MemoryContextSwitchTo(oldctx);
	PG_RETURN_INT64(id);
}

Datum ac_add(PG_FUNCTION_ARGS)
{
	int64 id = PG_GETARG_INT64(0);
	text *kw_text = PG_GETARG_TEXT_PP(1);
	char *keyword = text_to_cstring(kw_text);
	bool found;
	ac_automaton_entry *entry;

	entry = hash_search(automaton_storage, &id, HASH_FIND, &found);
	if (!found)
	{
		pfree(keyword);
		PG_RETURN_BOOL(false);
	}

	ac_automaton *automaton = entry->automaton;
	ac_add_word_to_automaton(automaton, keyword);
	ac_rebuild_automaton(automaton); /* rebuild immediately */
	pfree(keyword);
	PG_RETURN_BOOL(true);
}

Datum ac_remove(PG_FUNCTION_ARGS)
{
	int64 id = PG_GETARG_INT64(0);
	text *kw_text = PG_GETARG_TEXT_PP(1);
	char *keyword = text_to_cstring(kw_text);
	bool found;
	ac_automaton_entry *entry;

	entry = hash_search(automaton_storage, &id, HASH_FIND, &found);
	if (!found)
	{
		pfree(keyword);
		PG_RETURN_BOOL(false);
	}

	ac_automaton *automaton = entry->automaton;
	ac_remove_word_from_automaton(automaton, keyword);
	ac_rebuild_automaton(automaton);
	pfree(keyword);
	PG_RETURN_BOOL(true);
}

Datum ac_destroy(PG_FUNCTION_ARGS)
{
	int64 id = PG_GETARG_INT64(0);
	bool found;
	ac_automaton_entry *entry;

	entry = hash_search(automaton_storage, &id, HASH_FIND, &found);
	if (!found)
		PG_RETURN_BOOL(false);

	ac_automaton *automaton = entry->automaton;
	if (automaton->root)
		ac_free_trie(automaton->root);
	if (automaton->keywords)
	{
		for (int64 i = 0; i < automaton->keyword_count; i++)
			pfree(automaton->keywords[i].keyword);
		pfree(automaton->keywords);
	}
	pfree(automaton);
	hash_search(automaton_storage, &id, HASH_REMOVE, NULL);
	PG_RETURN_BOOL(true);
}

Datum ac_search_tsquery(PG_FUNCTION_ARGS)
{
	int64 id = PG_GETARG_INT64(0);
	TSQuery tsq = PG_GETARG_TSQUERY(1);
	bool found;
	ac_automaton_entry *entry;
	QueryItem *items;
	bool result = false;

	entry = hash_search(automaton_storage, &id, HASH_FIND, &found);
	if (!found)
		PG_RETURN_BOOL(false);

	ac_automaton *automaton = entry->automaton;
	if (automaton->dirty)
		ac_rebuild_automaton(automaton);

	items = GETQUERY(tsq);
	result = evaluate_query(items, tsq, automaton);
	PG_RETURN_BOOL(result);
}

Datum ac_search_text(PG_FUNCTION_ARGS)
{
	int64 id = PG_GETARG_INT64(0);
	text *input = PG_GETARG_TEXT_PP(1);
	bool found;
	ac_automaton_entry *entry;
	char *text_str;
	bool result = false;

	entry = hash_search(automaton_storage, &id, HASH_FIND, &found);
	if (!found)
		PG_RETURN_BOOL(false);

	ac_automaton *automaton = entry->automaton;
	if (automaton->dirty)
		ac_rebuild_automaton(automaton);

	text_str = text_to_cstring(input);
	result = ac_contains(automaton->root, text_str);
	pfree(text_str);

	PG_RETURN_BOOL(result);
}

Datum ac_match_text(PG_FUNCTION_ARGS)
{
	int64 id = PG_GETARG_INT64(0);
	text *input = PG_GETARG_TEXT_PP(1);
	char *text_str = text_to_cstring(input);
	ac_match_result result;
	Datum *elements;
	ArrayType *array;
	int16 elmlen;
	bool elmbyval;
	char elmalign;
	bool found;
	ac_automaton_entry *entry;

	entry = hash_search(automaton_storage, &id, HASH_FIND, &found);
	if (!found)
	{
		pfree(text_str);
		PG_RETURN_NULL();
	}

	ac_automaton *automaton = entry->automaton;
	if (automaton->dirty)
		ac_rebuild_automaton(automaton);

	result = ac_match(automaton->root, text_str);

	if (result.num_matches == 0)
	{
		pfree(result.matches);
		pfree(result.counts);
		pfree(text_str);
		PG_RETURN_NULL();
	}

	elements = (Datum *) palloc(sizeof(Datum) * result.num_matches);
	for (int64 i = 0; i < result.num_matches; i++)
		elements[i] = Int64GetDatum(result.matches[i]);

	get_typlenbyvalalign(INT8OID, &elmlen, &elmbyval, &elmalign);
	array = construct_array(elements, result.num_matches, INT8OID,
							elmlen, elmbyval, elmalign);

	pfree(result.matches);
	pfree(result.counts);
	pfree(text_str);
	pfree(elements);

	PG_RETURN_ARRAYTYPE_P(array);
}

Datum ac_rank_simple(PG_FUNCTION_ARGS)
{
	int64 id = PG_GETARG_INT64(0);
	text *input = PG_GETARG_TEXT_PP(1);
	bool found;
	ac_automaton_entry *entry;
	char *text_str;
	ac_match_result result;
	float4 score;

	entry = hash_search(automaton_storage, &id, HASH_FIND, &found);
	if (!found)
		PG_RETURN_NULL();

	ac_automaton *automaton = entry->automaton;
	if (automaton->dirty)
		ac_rebuild_automaton(automaton);

	text_str = text_to_cstring(input);
	result = ac_match(automaton->root, text_str);
	score = (float) result.num_matches / automaton->num_lexemes;

	pfree(result.matches);
	pfree(result.counts);
	pfree(text_str);

	PG_RETURN_FLOAT4(score);
}