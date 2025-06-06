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
    /* Init automaton storage is currently not called on extension load until _PG_fini actually works
     * ac_init();
     */
}


/* Finalize */ 
void _PG_fini(void)
{
    /* Fini automaton storage is currently not called on extension load until _PG_fini actually works
     * ac_init();
     */
}


/*
 * Memory management
 */


/* Init automaton storage */ 
static void _ac_init() 
{
    HASHCTL ctl;
    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(int64);
    ctl.entrysize = sizeof(ac_automaton_entry);
    /* Use TopMemoryContext for global storage */
    ctl.hcxt = TopMemoryContext; 
    automaton_storage = hash_create("automaton storage", INITIAL_NELEM, &ctl, HASH_ELEM | HASH_CONTEXT);
}


/* Cleanup automatons */ 
static void _ac_fini() 
{
    HASH_SEQ_STATUS status;
    ac_automaton_entry *entry;

    if (automaton_storage == NULL)
        return;

    hash_seq_init(&status, automaton_storage);

    /*  Iterate through all entries and free associated automaton */
    while ((entry = (ac_automaton_entry *) hash_seq_search(&status)) != NULL)
    {
        ac_automaton *automaton = entry->automaton;
        if (automaton) 
        {
            /* Free the trie, TSVector, and automaton struct */
            ac_free_trie(automaton->root);
            pfree(automaton);
        }
    }

    /* Destroy the hash table after all entries are processed */
    hash_destroy(automaton_storage);
    automaton_storage = NULL;
}


/* Free trie */
void ac_free_trie(ac_state* current)
{
	if (current == NULL)
	{
		return;
	}
	for (int i = 0; i < MAX_CHILDREN; i++)
	{
		if (current->children[i] != NULL)
		{
			ac_free_trie(current->children[i]);
		}
	}
	pfree(current);
}


/*
 * Aho Corasick functions
 */


/* Creates a new Aho Corasick state */
ac_state* ac_create_state()
{
    /* Allocate and initialize a new state */
	ac_state* state = (ac_state*)palloc0(sizeof(ac_state)); 
	state->is_final = false;
	state->index = -1;
    state->depth = 0;
	state->fail_link = NULL;
	return state;
}


/* Adds a keyword to the trie */ 
void ac_add_keyword(ac_state* root, const char* keyword, const int index)
{
	ac_state* current = root;
    for (int i = 0; keyword[i]!= '\0'; i++)
    {
        if (current->children[keyword[i]] == NULL)
        {
            current->children[keyword[i]] = ac_create_state();
        }
        current = current->children[keyword[i]];
        current->depth = i + 1;
    }
    current->is_final = true;
    current->index = index;
}


/* Builds failure links */ 
void ac_build_failure_links(ac_state* root)
{
	int queue_capacity= 16;
	int queue_size = 0;
	ac_state** queue = (ac_state**)palloc0(queue_capacity * sizeof(ac_state*));
	int front = 0, rear = 0;
	queue[rear++] = root;
	queue_size++;

	while(front < rear)
	{
		ac_state* current = queue[front++];
        for (int i = 0; i < MAX_CHILDREN; i++)
		{
			ac_state* child = current->children[i];
			if (child && current == root)
				child->fail_link = root;
			else if (child)
			{
				ac_state* fail = current->fail_link;
                while(fail && !fail->children[i])
					fail = fail->fail_link;
				if (fail)
					child->fail_link = fail->children[i];
				else 
					child->fail_link = root;
			}
			if (child)
			{
				if (queue_size == queue_capacity)
				{
					queue_capacity *= 2;
					queue = (ac_state**)repalloc(queue, queue_capacity * sizeof(ac_state*));
				}
				queue[rear++] = child;
				queue_size++;
			}
		}
	}
	pfree(queue);
}


/* Builds dictionary links */
void ac_build_dictionary_links(ac_state* root)
{
    int queue_capacity = 16;
    int queue_size = 0;
    ac_state** queue = (ac_state**)palloc(queue_capacity * sizeof(ac_state*));
    int front = 0, rear = 0;

    for (int i = 0; i < MAX_CHILDREN; i++) 
	{
        if (root->children[i]) 
        {
            if (queue_size == queue_capacity) 
			{
                queue_capacity *= 2;
                queue = (ac_state**)repalloc(queue, queue_capacity * sizeof(ac_state*));
            }
            queue[rear++] = root->children[i];
            queue_size++;
        }
    }

    while (front < rear) 
	{
        ac_state* current = queue[front++];
        ac_state* fail = current->fail_link;
        if (fail && fail->is_final)
		{
            current->dictionary_link = current->fail_link;
        } 
		else if (current->fail_link && fail->dictionary_link)
		{
            current->dictionary_link = fail->dictionary_link;
        } 
		else
		{
            current->dictionary_link = NULL;
        }
        for (int i = 0; i < MAX_CHILDREN; i++) 
		{
            if (current->children[i]) 
            {
                if (queue_size == queue_capacity) 
				{
                    queue_capacity *= 2;
                    queue = (ac_state**)repalloc(queue, queue_capacity * sizeof(ac_state*));
                }
                queue[rear++] = current->children[i];
                queue_size++;
            }
        }
    }
    pfree(queue);
}


/* Matches a text against the trie and counts the number of matches */
ac_match_result ac_match(ac_state* root, char* text)
{
    ac_state* current = root;
    ac_match_result result = {0};
    int *matches = palloc(16 * sizeof(int));
    int *counts = palloc(16 * sizeof(int));
    int capacity = 16;
    
    for (int i = 0; i < text[i] != '\0'; i++) 
    {	
        unsigned char c = (unsigned char)text[i];
        while (current && !current->children[c])
        {
            current = current->fail_link;
        }
        current = current ? current->children[c] : root;

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


/*
 * Searches for a given text in the trie 
 * (at least one match and not necessarily the same as in the text)
 * and immediately returns if found 
 */
bool ac_contains(ac_state *root, const char *text) 
{
    ac_state *current = root;
    for (int i = 0; text[i] != '\0'; i++)
    {
        unsigned char c = (unsigned char)text[i];
        while (current && !current->children[c])
        {
            current = current->fail_link;
        }
        current = current ? current->children[c] : root;
        
        for (ac_state *temp = current; temp; temp = temp->dictionary_link)
        {
            if (temp->is_final)
                return true;
        }
    }
    return false;
}


/* Helper function for ac_search (recursive) */
bool evaluate_query(QueryItem *item, TSQuery *tsq, ac_automaton *automaton) 
{
    /* If the item is a value */ 
    if (item->type == QI_VAL)            
    {
        char *lexeme;
        bool found;
        /* Get lexeme from TSQuery */
        lexeme = pnstrdup(GETOPERAND(tsq) + item->qoperand.distance, item->qoperand.length);  
        /* Look for that lexeme in the trie*/
        found = ac_contains(automaton->root, lexeme);                                          
        pfree(lexeme);
        return found;
    }
    /* If the item is an operator */
    else if (item->type == QI_OPR)   
    {
        switch (item->qoperator.oper) 
        {
            case OP_AND:
                return evaluate_query(item + item->qoperator.left, tsq, automaton) && evaluate_query(item + 1, tsq, automaton);
            case OP_OR:
                return evaluate_query(item + item->qoperator.left, tsq, automaton) || evaluate_query(item + 1, tsq, automaton);
            case OP_NOT:
                return !evaluate_query(item + item->qoperator.left, tsq, automaton);
            default:
                elog(ERROR, "unrecognized operator: %d", item->qoperator.oper);
        }
    }
    return false;
}


/*
 * PostgreSQL-specific functions
 */


/* Init Aho Corasick automaton storage */
Datum ac_init(PG_FUNCTION_ARGS)
{
    _ac_init();
    PG_RETURN_BOOL(true);
}


/* Fini Aho Corasick automaton storage */
Datum ac_fini(PG_FUNCTION_ARGS)
{
    _ac_fini();
    PG_RETURN_BOOL(true);
}


/* Build Aho Corasick automaton */
Datum ac_build_tsvector(PG_FUNCTION_ARGS) 
{
    TSVector tsv = PG_GETARG_TSVECTOR_COPY(0);
    WordEntry *entries = ARRPTR(tsv);
    ac_automaton *automaton;
    int64 id;
    ac_automaton_entry *entry;
    MemoryContext oldctx;

    oldctx = MemoryContextSwitchTo(TopMemoryContext);

    /* Initialize automaton */
    automaton = (ac_automaton *)palloc0(sizeof(ac_automaton));
    automaton->num_lexemes = tsv->size;
    automaton->root = ac_create_state();

    /* Add keywords with their original positional indexes */
    for (int i = 0; i < tsv->size; i++) 
    {
        WordEntry *entry = &entries[i];
        char *lexeme = pnstrdup(STRPTR(tsv) + entry->pos, entry->len);
        int32 index = -1;

        /* Extract the first positional index from tsvector */
        if (entry->haspos) 
        {
            uint16      npos = POSDATALEN(tsv, entry);
            WordEntryPos *pos_ptr = POSDATAPTR(tsv, entry);
            if (npos > 0)
                index = (int32) WEP_GETPOS(pos_ptr[0]);
        }

        if (index == -1)
            ereport(ERROR, (errcode(ERRCODE_DATA_EXCEPTION), errmsg("tsvector entry has no positional data")));

        /* Add to trie with the extracted index */
        ac_add_keyword(automaton->root, lexeme, index);
        pfree(lexeme);
    }

    ac_build_failure_links(automaton->root);
    ac_build_dictionary_links(automaton->root);

    /* Store automaton */
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

    /* Get array elements */
    deconstruct_array(input_array, TEXTOID, -1, false, 'i', &lexeme_datums, &nulls, &nlexemes);

    oldctx = MemoryContextSwitchTo(TopMemoryContext);

    /* Initialize automaton */
    automaton = (ac_automaton *)palloc0(sizeof(ac_automaton));
    automaton->num_lexemes = nlexemes;
    automaton->root = ac_create_state();

    /* Add lexemes with array indexes */
    for (i = 0; i < nlexemes; i++) 
    {
        char *lexeme;
        if (nulls[i])
            ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("array element cannot be NULL")));

        lexeme = TextDatumGetCString(lexeme_datums[i]);
        ac_add_keyword(automaton->root, lexeme, i + 1);
        pfree(lexeme);
    }

    ac_build_failure_links(automaton->root);
    ac_build_dictionary_links(automaton->root);

    /* Store automaton */
    id = next_automaton_id++;
    entry = hash_search(automaton_storage, &id, HASH_ENTER, NULL);
    entry->id = id;
    entry->automaton = automaton;

    MemoryContextSwitchTo(oldctx);
    PG_RETURN_INT64(id);
}


/* Destroy Aho Corasick automaton */
Datum ac_destroy(PG_FUNCTION_ARGS)
{
    int64 id = PG_GETARG_INT64(0); 
    bool found;
    ac_automaton_entry *entry;

    /* Look for the automaton*/
    entry = hash_search(automaton_storage, &id, HASH_FIND, &found);

    /* If not found, return false */
    if (!found)
        PG_RETURN_BOOL(false);

    /* Else, destroy */
    ac_free_trie(entry->automaton->root);
    hash_search(automaton_storage, &id, HASH_REMOVE, NULL);
    
    /* Return true */
    PG_RETURN_BOOL(true);
}


/* Search in Aho Corasick automaton using TSQuery */
Datum ac_search_tsquery(PG_FUNCTION_ARGS) 
{
    int64 id = PG_GETARG_INT64(0);;
    TSQuery tsq = PG_GETARG_TSQUERY(1);;
    bool found;
    ac_automaton_entry *entry;
    QueryItem *items;
    bool result = false;

    /* Look for the automaton */ 
    entry = hash_search(automaton_storage, &id, HASH_FIND, &found);

    /* If not found, return false */
    if (!found)
        PG_RETURN_BOOL(false);

    /* Else, evaluate */
    items = GETQUERY(tsq);
    result = evaluate_query(items, tsq, entry->automaton);

    /* Return result */
    PG_RETURN_BOOL(result);
}


/* Search in Aho Corasick automaton using text */
Datum ac_search_text(PG_FUNCTION_ARGS) 
{
    int64 id = PG_GETARG_INT64(0);
    text *input = PG_GETARG_TEXT_PP(1);
    bool found;
    ac_automaton_entry *entry;
    char *text_str;
    bool result = false;

    /* Look for the automaton */
    entry = hash_search(automaton_storage, &id, HASH_FIND, &found);

    /* If not found, return false */
    if (!found)
        PG_RETURN_BOOL(false);

    /* Else, evaluate */
    text_str = text_to_cstring(input);
    result = ac_contains(entry->automaton->root, text_str);
    pfree(text_str);

    /* Return result */
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

    /* Lookup automaton */
    entry = hash_search(automaton_storage, &id, HASH_FIND, &found);
    if (!found)
        PG_RETURN_NULL();

    /* Get matches */
    result = ac_match(entry->automaton->root, text_str);
    
    /* Convert matches to PostgreSQL array */
    if (result.num_matches == 0)
        PG_RETURN_NULL();

    elements = (Datum *)palloc(sizeof(Datum) * result.num_matches);
    for (int i = 0; i < result.num_matches; i++) 
    {
        elements[i] = Int32GetDatum(result.matches[i]);
    }

    get_typlenbyvalalign(INT4OID, &elmlen, &elmbyval, &elmalign);
    array = construct_array(elements, result.num_matches, INT4OID, elmlen, elmbyval, elmalign);

    /* Cleanup */
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

    /* Look for the automaton */ 
    entry = hash_search(automaton_storage, &id, HASH_FIND, &found);

    /* If not found, return false */ 
    if (!found)
        PG_RETURN_NULL();

    /* Else, evaluate */ 
    text_str = text_to_cstring(input);
    result = ac_match(entry->automaton->root, text_str);
    score = (float)result.num_matches / entry->automaton->num_lexemes;

    /* Clean up */
    pfree(result.matches);
    pfree(result.counts);
    pfree(text_str);

    /* Return score */
    PG_RETURN_FLOAT4(score);
}