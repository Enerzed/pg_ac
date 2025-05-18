/*
 * ac.c
 */


#include "ac.h"


/*
 * Init and fini
 */


// Init automaton storage
void _PG_init(void) 
{
    HASHCTL ctl;
    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(int32);
    ctl.entrysize = sizeof(ac_automaton_entry);
    ctl.hcxt = TopMemoryContext; // Use TopMemoryContext for global storage
    automaton_storage = hash_create("automaton storage", 100, &ctl, HASH_ELEM | HASH_CONTEXT);
}


// Finalize
void _PG_fini(void)
{
    cleanup_automaton();
    hash_destroy(automaton_storage);
}


/*
 * Memory management
 */


// Init automaton storage
static void init_automaton_storage()
{
    HASHCTL ctl;
    memset(&ctl, 0, sizeof(ctl));
    ctl.keysize = sizeof(int32);
    ctl.entrysize = sizeof(int32) + sizeof(ac_automaton*);
    automaton_storage = hash_create("automaton_storage", 100, &ctl, HASH_ELEM | HASH_ELEM);
}


// Cleanup automaton
static void cleanup_automaton() 
{
    HASH_SEQ_STATUS status;
    hash_seq_init(&status, automaton_storage);

    ac_automaton *entry;
    while((entry = (ac_automaton*)hash_seq_search(&status)) != NULL)
    {
        ac_free_trie(entry->root);
        pfree(entry);
    }
}


// Free trie
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


// Creates a new Aho Corasick state
ac_state* ac_create_state()
{
    // Allocate and initialize a new state
	ac_state* state = (ac_state*)palloc0(sizeof(ac_state)); 
	state->is_final = false;
	state->index = -1;
	state->fail_link = NULL;
	return state;
}


// Adds a keyword to the trie
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
    }
    current->is_final = true;
    current->index = index;
}


// Builds failure links
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
			{
				child->fail_link = root;
			}
			else if (child)
			{
				ac_state* fail = current->fail_link;
                while(fail && !fail->children[i])
				{
					fail = fail->fail_link;
				}
				if (fail)
				{
					child->fail_link = fail->children[i];
				}
				else 
				{
					child->fail_link = root;
				}
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


// Builds dictionary links
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


// Matches a text against the trie and counts the number of matches
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


// Searches for a text in the trie and immediately returns if found
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


// Helper function for ac_search (recursive)
bool evaluate_query(QueryItem *item, TSQuery *tsq, ac_automaton *automaton) 
{
    // If the item is a value 
    if (item->type == QI_VAL)            
    {
        char *lexeme = pnstrdup(GETOPERAND(tsq) + item->qoperand.distance, item->qoperand.length);  // Get lexeme from TSQuery
        bool found = ac_contains(automaton->root, lexeme);                                          // Look for that lexeme in the trie
        pfree(lexeme);
        return found;
    }
    // If the item is an operator
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
    int32 id = PG_GETARG_INT32(0);              // Get automaton id
    text *input = PG_GETARG_TEXT_PP(1);         // Get text

    // Look for the automaton
    bool found;
    ac_automaton_entry *entry = hash_search(automaton_storage, &id, HASH_FIND, &found);
    // If not found, return false
    if (!found)
        PG_RETURN_FLOAT4(0.0);
    // Else, evaluate
    char *text_str = text_to_cstring(input);
    
    ac_match_result result = ac_match(entry->automaton->root, text_str);
    float4 score = (float)result.num_matches / entry->automaton->tsv->size;

    pfree(result.matches);
    pfree(result.counts);
    pfree(text_str);
    
    PG_RETURN_FLOAT4(score);
}


// Build Aho Corasick automaton
Datum ac_build(PG_FUNCTION_ARGS) 
{
    // Set current memory context
    MemoryContext oldctx = MemoryContextSwitchTo(TopMemoryContext);
    // Get TSVector
    TSVector tsv = PG_GETARG_TSVECTOR_COPY(0);
    // Create automaton
    ac_automaton *automaton = palloc0(sizeof(ac_automaton));
    automaton->tsv = tsv;
    automaton->root = ac_create_state();
    // Build trie
    WordEntry *entries = ARRPTR(tsv);
    for (int i = 0; i < tsv->size; i++) {
        char *lexeme = pnstrdup(STRPTR(tsv) + entries[i].pos, entries[i].len);
        ac_add_keyword(automaton->root, lexeme, i);
        pfree(lexeme);
    }
    ac_build_failure_links(automaton->root);
    ac_build_dictionary_links(automaton->root);
    // Store automaton
    int32 id = next_automaton_id++;
    ac_automaton_entry *entry = hash_search(automaton_storage, &id, HASH_ENTER, NULL);
    entry->id = id;
    entry->automaton = automaton;
    // Set old memory context and return
    MemoryContextSwitchTo(oldctx);
    PG_RETURN_INT32(id);
}


// Search in Aho Corasick automaton using TSQuery
Datum ac_search(PG_FUNCTION_ARGS) 
{
    int32 id = PG_GETARG_INT32(0);              // Get automaton id
    TSQuery tsq = PG_GETARG_TSQUERY(1);         // Get TSQuery

    // Look for the automaton
    bool found;
    ac_automaton_entry *entry = hash_search(automaton_storage, &id, HASH_FIND, &found);
    // If not found, return false
    if (!found)
        PG_RETURN_BOOL(false);
    // Else, evaluate
    QueryItem *items = GETQUERY(tsq);
    bool result = false;

    result = evaluate_query(items, tsq, entry->automaton);

    PG_RETURN_BOOL(result);
}


// Search in Aho Corasick automaton using text
Datum ac_search_text(PG_FUNCTION_ARGS) 
{
    int32 id = PG_GETARG_INT32(0);              // Get automaton id
    text *input = PG_GETARG_TEXT_PP(1);         // Get text

    // Look for the automaton
    bool found;
    ac_automaton_entry *entry = hash_search(automaton_storage, &id, HASH_FIND, &found);
    // If not found, return false
    if (!found)
        PG_RETURN_BOOL(false);
    // Else, evaluate
    char *text_str = text_to_cstring(input);
    bool result = ac_contains(entry->automaton->root, text_str);
    
    pfree(text_str);
    PG_RETURN_BOOL(result);
}