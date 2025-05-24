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
    HASHCTL ctl;
    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(int64);
    ctl.entrysize = sizeof(ac_automaton_entry);
    /* Use TopMemoryContext for global storage */
    ctl.hcxt = TopMemoryContext; 
    automaton_storage = hash_create("automaton storage", INITIAL_NELEM, &ctl, HASH_ELEM | HASH_CONTEXT);
}


/* Finalize */ 
void _PG_fini(void)
{
    cleanup_automaton();
}


/*
 * Memory management
 */


/* Init automaton storage */ 
static void init_automaton_storage()
{
    HASHCTL ctl;
    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(int64);
    ctl.entrysize = sizeof(int64) + sizeof(ac_automaton*);
    automaton_storage = hash_create("automaton_storage", INITIAL_NELEM, &ctl, HASH_ELEM | HASH_ELEM);
}


/* Cleanup automatons */ 
static void cleanup_automaton() 
{
    HASH_SEQ_STATUS status;
    ac_automaton_entry *entry;

    if (automaton_storage == NULL)
        return;

    hash_seq_init(&status, automaton_storage);

    // Iterate through all entries and free associated automata
    while ((entry = (ac_automaton_entry *) hash_seq_search(&status)) != NULL)
    {
        ac_automaton *automaton = entry->automaton;
        if (automaton) {
            // Free the trie, TSVector, and automaton struct
            ac_free_trie(automaton->root);
            pfree(automaton->tsv);  // Free the TSVector copy
            pfree(automaton);
        }
    }

    // Destroy the hash table after all entries are processed
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


/* Builds failure and dictionary links */ 
void ac_build_links(ac_state* root)
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
				{
                    if (fail->is_final)
                        child->dictionary_link = fail->children[i];
					child->fail_link = fail->children[i];
				}
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


Datum ac_rank_simple(PG_FUNCTION_ARGS)
{
    int64 id;
    text *input;
    bool found;
    ac_automaton_entry *entry;
    char *text_str;
    ac_match_result result;
    float4 score;
    /* Get automaton id */ 
    id = PG_GETARG_INT64(0);
    /* Get text */
    input = PG_GETARG_TEXT_PP(1);
    /* Look for the automaton */ 
    entry = hash_search(automaton_storage, &id, HASH_FIND, &found);
    /* If not found, return false */ 
    if (!found)
        PG_RETURN_FLOAT4(0.0);
    /* Else, evaluate */ 
    text_str = text_to_cstring(input);
    result = ac_match(entry->automaton->root, text_str);
    score = (float)result.num_matches / entry->automaton->tsv->size;
    /* Clean up */
    pfree(result.matches);
    pfree(result.counts);
    pfree(text_str);
    /* Return score */
    PG_RETURN_FLOAT4(score);
}


/* Build Aho Corasick automaton */
Datum ac_build(PG_FUNCTION_ARGS) 
{
    MemoryContext oldctx;;
    TSVector tsv;
    ac_automaton *automaton;
    WordEntry *entries;
    int64 id;
    ac_automaton_entry *entry;
    /* Set current memory context */ 
    oldctx = MemoryContextSwitchTo(TopMemoryContext);
    /* Get TSVector */
    tsv = PG_GETARG_TSVECTOR_COPY(0);
    /* Create automaton */ 
    automaton = palloc0(sizeof(ac_automaton));
    automaton->tsv = tsv;
    automaton->root = ac_create_state();
    /* Build trie */
    entries = ARRPTR(tsv);
    /* Get next automaton id*/
    id = next_automaton_id++;
    /* Add keywords to trie */
    for (int i = 0; i < tsv->size; i++) 
    {
        char *lexeme;
        lexeme = pnstrdup(STRPTR(tsv) + entries[i].pos, entries[i].len);
        ac_add_keyword(automaton->root, lexeme, i);
        pfree(lexeme);
    }
    /* Build links */
    ac_build_links(automaton->root);
    /* Store automaton */
    entry = hash_search(automaton_storage, &id, HASH_ENTER, NULL);
    entry->id = id;
    entry->automaton = automaton;
    /* Set old memory context and return */ 
    MemoryContextSwitchTo(oldctx);
    /* Return automaton id */
    PG_RETURN_INT64(id);
}


/* Destroy Aho Corasick automaton */
Datum ac_destroy(PG_FUNCTION_ARGS)
{
    int64 id;
    bool found;
    ac_automaton_entry *entry;
    /* Get automaton id*/
    id = PG_GETARG_INT64(0); 
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


Datum ac_destroy_all(PG_FUNCTION_ARGS)
{
    cleanup_automaton();
    PG_RETURN_BOOL(true);
}

/* Search in Aho Corasick automaton using TSQuery */
Datum ac_search_tsquery(PG_FUNCTION_ARGS) 
{
    int64 id;
    TSQuery tsq;
    bool found;
    ac_automaton_entry *entry;
    QueryItem *items;
    bool result = false;
    /* Get automaton id */
    id = PG_GETARG_INT64(0);
    /* Get TSQuery */
    tsq = PG_GETARG_TSQUERY(1);
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
    int64 id;
    text *input;
    bool found;
    ac_automaton_entry *entry;
    char *text_str;
    bool result = false;
    /* Get automaton id*/
    id = PG_GETARG_INT64(0);
    /* Get text */
    input = PG_GETARG_TEXT_PP(1);
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


/* TODO DONT USE */
Datum ac_match_text(PG_FUNCTION_ARGS) 
{
    int64 id = PG_GETARG_INT64(0);              // Get automaton id
    text *input = PG_GETARG_TEXT_PP(1);         // Get text
    char *text_str = text_to_cstring(input);
    ac_match_result result;

    // Look for the automaton
    bool found;
    ac_automaton_entry *entry = hash_search(automaton_storage, &id, HASH_FIND, &found);
    // If not found, return false
    if (!found)
        PG_RETURN_FLOAT4(0.0);
    // Else, evaluate
    result = ac_match(entry->automaton->root, text_str);

    pfree(result.matches);
    pfree(result.counts);
    pfree(text_str);
    
}