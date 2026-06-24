/*
 * ac.c
 */

#include "ac.h"


/*
 * Init and fini
 */

/* Init automaton storage */
void _PG_init(void)
{
    /* Not called automatically – use ac_init() explicitly */
}


/* Finalize */
void _PG_fini(void)
{
    /* Not called automatically – use ac_fini() explicitly */
}


/*
 * Memory management
 */

/* Init automaton storage */
static void _ac_init(void)
{
    HASHCTL ctl;
    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(int64);
    ctl.entrysize = sizeof(ac_automaton_entry);
    ctl.hcxt = TopMemoryContext;
    automaton_storage = hash_create("automaton storage", INITIAL_NELEM, &ctl, HASH_ELEM | HASH_CONTEXT);
}


/* Cleanup automatons */
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
            ac_free_trie(automaton->root);
            pfree(automaton);
        }
    }

    hash_destroy(automaton_storage);
    automaton_storage = NULL;
}


/* Free trie recursively */
void ac_free_trie(ac_state* current)
{
    if (current == NULL)
        return;
    for (int i = 0; i < current->edge_count; i++)
        ac_free_trie(current->edges[i].child);
    if (current->edges)
        pfree(current->edges);
    pfree(current);
}


/*
 * Aho Corasick functions
 */

/* ---- Edge helpers ---- */

/* Binary search for an edge by character */
static ac_edge* find_edge(ac_state *state, int ch)
{
    int lo = 0, hi = state->edge_count - 1;
    while (lo <= hi)
    {
        int mid = (lo + hi) / 2;
        if (state->edges[mid].ch == ch)
            return &state->edges[mid];
        else if (state->edges[mid].ch < ch)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return NULL;
}


/* Add an edge, keeping edges sorted by character */
static void add_edge(ac_state *state, int ch, ac_state *child)
{
    if (state->edge_count == state->edge_capacity)
    {
        int new_capacity = state->edge_capacity ? state->edge_capacity * 2 : 8;
        if (state->edges == NULL)
            state->edges = (ac_edge*) palloc(new_capacity * sizeof(ac_edge));
        else
            state->edges = (ac_edge*) repalloc(state->edges, new_capacity * sizeof(ac_edge));
        state->edge_capacity = new_capacity;
    }
    int pos = state->edge_count;
    while (pos > 0 && state->edges[pos-1].ch > ch)
    {
        state->edges[pos] = state->edges[pos-1];
        pos--;
    }
    state->edges[pos].ch = ch;
    state->edges[pos].child = child;
    state->edge_count++;
}


/* ---- Trie operations ---- */

/* Create a new state */
ac_state* ac_create_state(void)
{
    ac_state* state = (ac_state*) palloc0(sizeof(ac_state));
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


/* Add a keyword to the trie (UTF-8 aware) */
void ac_add_keyword(ac_state* root, const char* keyword, const int index)
{
    ac_state* current = root;
    const unsigned char *p = (const unsigned char*) keyword;
    int depth = 0;
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


/* Build failure links (BFS) */
void ac_build_failure_links(ac_state* root)
{
    int queue_capacity = 16;
    int queue_size = 0;
    ac_state** queue = (ac_state**) palloc0(queue_capacity * sizeof(ac_state*));
    int front = 0, rear = 0;

    /* Initialize: children of root get fail_link = root */
    for (int i = 0; i < root->edge_count; i++)
    {
        ac_state *child = root->edges[i].child;
        child->fail_link = root;
        if (queue_size == queue_capacity)
        {
            queue_capacity *= 2;
            queue = (ac_state**) repalloc(queue, queue_capacity * sizeof(ac_state*));
        }
        queue[rear++] = child;
        queue_size++;
    }

    while (front < rear)
    {
        ac_state* current = queue[front++];
        for (int i = 0; i < current->edge_count; i++)
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
            {
                child->fail_link = root;
            }

            if (queue_size == queue_capacity)
            {
                queue_capacity *= 2;
                queue = (ac_state**) repalloc(queue, queue_capacity * sizeof(ac_state*));
            }
            queue[rear++] = child;
            queue_size++;
        }
    }
    pfree(queue);
}


/* Build dictionary links (BFS) */
void ac_build_dictionary_links(ac_state* root)
{
    int queue_capacity = 16;
    int queue_size = 0;
    ac_state** queue = (ac_state**) palloc(queue_capacity * sizeof(ac_state*));
    int front = 0, rear = 0;

    /* Initialize: children of root */
    for (int i = 0; i < root->edge_count; i++)
    {
        ac_state *child = root->edges[i].child;
        if (queue_size == queue_capacity)
        {
            queue_capacity *= 2;
            queue = (ac_state**) repalloc(queue, queue_capacity * sizeof(ac_state*));
        }
        queue[rear++] = child;
        queue_size++;
    }

    while (front < rear)
    {
        ac_state* current = queue[front++];
        ac_state* fail = current->fail_link;
        if (fail && fail->is_final)
            current->dictionary_link = fail;
        else if (fail && fail->dictionary_link)
            current->dictionary_link = fail->dictionary_link;
        else
            current->dictionary_link = NULL;

        for (int i = 0; i < current->edge_count; i++)
        {
            ac_state *child = current->edges[i].child;
            if (queue_size == queue_capacity)
            {
                queue_capacity *= 2;
                queue = (ac_state**) repalloc(queue, queue_capacity * sizeof(ac_state*));
            }
            queue[rear++] = child;
            queue_size++;
        }
    }
    pfree(queue);
}


/* Match text and return indices of matched keywords */
ac_match_result ac_match(ac_state* root, char* text)
{
    ac_state* current = root;
    ac_match_result result = {0};
    int *matches = palloc(16 * sizeof(int));
    int *counts = palloc(16 * sizeof(int));
    int capacity = 16;
    const unsigned char *p = (const unsigned char*) text;

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
                    matches = repalloc(matches, capacity * sizeof(int));
                    counts = repalloc(counts, capacity * sizeof(int));
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


/* Check if any keyword appears in the text (early exit) */
bool ac_contains(ac_state *root, const char *text)
{
    ac_state *current = root;
    const unsigned char *p = (const unsigned char*) text;
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


/* Recursive query evaluation */
bool evaluate_query(QueryItem *item, TSQuery tsq, ac_automaton *automaton)
{
    if (item->type == QI_VAL)
    {
        char *lexeme;
        bool found;
        lexeme = pnstrdup(GETOPERAND(tsq) + item->qoperand.distance, item->qoperand.length);
        found = ac_contains(automaton->root, lexeme);
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
 * PostgreSQL‑specific functions
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
    automaton->root = ac_create_state();

    for (int i = 0; i < tsv->size; i++)
    {
        WordEntry *we = &entries[i];
        char *lexeme = pnstrdup(STRPTR(tsv) + we->pos, we->len);
        int32 index = -1;

        if (we->haspos)
        {
            uint16 npos = POSDATALEN(tsv, we);
            WordEntryPos *pos_ptr = POSDATAPTR(tsv, we);
            if (npos > 0)
                index = (int32) WEP_GETPOS(pos_ptr[0]);
        }

        if (index == -1)
            ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION),
                            errmsg("tsvector entry has no positional data")));

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
    automaton->root = ac_create_state();

    for (i = 0; i < nlexemes; i++)
    {
        char *lexeme;
        if (nulls[i])
            ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED),
                            errmsg("array element cannot be NULL")));

        lexeme = TextDatumGetCString(lexeme_datums[i]);
        ac_add_keyword(automaton->root, lexeme, i + 1);
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

Datum ac_destroy(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0);
    bool found;
    ac_automaton_entry *entry;

    entry = hash_search(automaton_storage, &id, HASH_FIND, &found);
    if (!found)
        PG_RETURN_BOOL(false);

    ac_free_trie(entry->automaton->root);
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

    items = GETQUERY(tsq);
    result = evaluate_query(items, tsq, entry->automaton);
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

    text_str = text_to_cstring(input);
    result = ac_contains(entry->automaton->root, text_str);
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
        PG_RETURN_NULL();

    result = ac_match(entry->automaton->root, text_str);

    if (result.num_matches == 0)
        PG_RETURN_NULL();

    elements = (Datum *) palloc(sizeof(Datum) * result.num_matches);
    for (int i = 0; i < result.num_matches; i++)
        elements[i] = Int32GetDatum(result.matches[i]);

    get_typlenbyvalalign(INT4OID, &elmlen, &elmbyval, &elmalign);
    array = construct_array(elements, result.num_matches, INT4OID,
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

    text_str = text_to_cstring(input);
    result = ac_match(entry->automaton->root, text_str);
    score = (float) result.num_matches / entry->automaton->num_lexemes;

    pfree(result.matches);
    pfree(result.counts);
    pfree(text_str);

    PG_RETURN_FLOAT4(score);
}