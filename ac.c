/*
* ac.c
*/


#include "ac.h"


ac_state* ac_create_trie(const char** keywords, int size)
{
    ac_state* root = ac_create_state();
    root->is_root = true;
    for (int i = 0; i < size; i++) 
    {
        ac_add_keyword(root, keywords[i], i);
    }
    ac_build_failure_links(root);
    ac_build_dictionary_links(root);
    return root;
}


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


ac_state* ac_create_state()
{
	ac_state* state = (ac_state*)palloc0(sizeof(ac_state));
	state->is_root = false;
	state->is_final = false;
	state->index = -1;
	state->fail_link = NULL;
	return state;
}


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
        current = current ? current->children[c] : root;current = root;

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


bool ac_contains(ac_state *root, const char *text, int *entries_count) 
{
    ac_state *current = root;
    *entries_count = 0;
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
                (*entries_count)++;
        }
    }
    return (*entries_count > 0);
}


int ac_count_entries(ac_state *root, const char *text)
{
    int count = 0;
    ac_contains(root, text, &count);
    return count;
}


PG_FUNCTION_INFO_V1(ac_build);
Datum ac_build(PG_FUNCTION_ARGS)
{
    TSVector tsv = PG_GETARG_TSVECTOR_COPY(0);
    ac_automaton *automaton = (ac_automaton*)palloc0(sizeof(ac_automaton));
    WordEntry *entries = ARRPTR(tsv);
    
    automaton->root = ac_create_state();
    automaton->tsv = tsv;
    automaton->lexemes = palloc0(tsv->size * sizeof(char*));
    automaton->term_freq = palloc0(tsv->size * sizeof(int));
    automaton->total_terms = 0;

    for(int i=0; i<tsv->size; i++)
    {
        char *lexeme = pnstrdup(STRPTR(tsv) + entries[i].pos, entries[i].len);
        automaton->lexemes[i] = lexeme;
        automaton->term_freq[i] = POSDATALEN(tsv, &entries[i]);
        automaton->total_terms += automaton->term_freq[i];
        ac_add_keyword(automaton->root, lexeme, i);
    }
    
    ac_build_failure_links(automaton->root);
    PG_RETURN_POINTER(automaton);
}


bool evaluate_query(QueryItem *item, TSQuery *tsq, ac_automaton *automaton) 
{
    if (item->type == QI_VAL) 
    {
        char *lexeme = pnstrdup(GETOPERAND(tsq) + item->qoperand.distance, item->qoperand.length);  // Get lexeme from TSQuery
        int entries = 0;
        bool found = ac_contains(automaton->root, lexeme, &entries);                                // Look for that lexeme in the trie
        pfree(lexeme);
        return found;
    }
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


PG_FUNCTION_INFO_V1(ac_search);
Datum ac_search(PG_FUNCTION_ARGS) 
{
    ac_automaton *automaton = (ac_automaton*)PG_GETARG_POINTER(0);
    TSQuery tsq = PG_GETARG_TSQUERY(1);
    QueryItem *items = GETQUERY(tsq);
    bool result = false;

    result = evaluate_query(items, tsq, automaton);

    PG_RETURN_BOOL(result);
}


PG_FUNCTION_INFO_V1(ac_search_text);
Datum ac_search_text(PG_FUNCTION_ARGS) 
{
    ac_automaton *automaton = (ac_automaton*)PG_GETARG_POINTER(0);
    text *input = PG_GETARG_TEXT_PP(1);
    char *text_str = text_to_cstring(input);
    int entries_count = 0;
    bool found = ac_contains(automaton->root, text_str, &entries_count);
    
    pfree(text_str);
    PG_RETURN_BOOL(found);
}


PG_FUNCTION_INFO_V1(ac_rank);
Datum ac_rank(PG_FUNCTION_ARGS)
{
    ac_automaton *automaton = (ac_automaton*)PG_GETARG_POINTER(0);
    text *input = PG_GETARG_TEXT_PP(1);
    char *text_str = text_to_cstring(input);
    
    ac_match_result result = ac_match(automaton->root, text_str);
    float4 score = 0.0;

    for(int i = 0; i < result.num_matches; i++) 
    {
        int idx = result.matches[i];
        float tf = (float)result.counts[i] / result.num_matches;
        float idf = log((float)automaton->total_terms / automaton->term_freq[idx]);
        score += tf * idf;
    }

    pfree(result.matches);
    pfree(result.counts);
    pfree(text_str);
    
    PG_RETURN_FLOAT4(score);
}


PG_FUNCTION_INFO_V1(ac_automaton_in);
Datum ac_automaton_in(PG_FUNCTION_ARGS)
{
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("ac_automaton_in not implemented")));
    PG_RETURN_POINTER(NULL);
}


PG_FUNCTION_INFO_V1(ac_automaton_out);
Datum ac_automaton_out(PG_FUNCTION_ARGS)
{
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED), errmsg("ac_automaton_out not implemented")));
    PG_RETURN_CSTRING("\0");
}