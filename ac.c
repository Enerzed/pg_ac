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

// Not really used anymore but could be usefull to count entries
int ac_match(ac_state* root, char* text, int** match_indices)
{
    ac_state* current = root;
    int text_length = strlen(text);
    int match_indices_capacity = 16;
    int num_matches = 0;
    *match_indices = (int*)palloc(match_indices_capacity * sizeof(int));
    
    for (int i = 0; i < text_length; i++) 
    {	
        while (current && !current->children[(unsigned char)text[i]])
        {
            current = current->fail_link;
        }
        if (current)
        {
            current = current->children[(unsigned char)text[i]];
        }
        else 
        {
            current = root;
        }
        ac_state* temp = current;
        while (temp) 
        {
            if (temp->is_final) 
            {
                if (num_matches == match_indices_capacity) 
                {
                    match_indices_capacity *= 2;
                    *match_indices = (int*)repalloc(*match_indices, match_indices_capacity * sizeof(int));
                }
                (*match_indices)[num_matches++] = temp->index;
            }
            temp = temp->dictionary_link;
        }
    }

    return num_matches;
}


static bool ac_contains(ac_state *root, const char *text)
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
            if (temp->is_final) return true;
        }
    }
    return false;
}


PG_FUNCTION_INFO_V1(ac_build);
Datum ac_build(PG_FUNCTION_ARGS) 
{
    TSVector tsv = PG_GETARG_TSVECTOR(0);
    ac_automaton *automaton = palloc0(sizeof(ac_automaton));
    WordEntry *entries = ARRPTR(tsv);
    
    automaton->root = ac_create_state();
    for (int i = 0; i < tsv->size; i++)
    {
        char *lexeme = pnstrdup(STRPTR(tsv) + entries[i].pos, entries[i].len);  // Get lexeme from TSVector
        ac_add_keyword(automaton->root, lexeme, i);                             // Add that lexeme to the trie
        pfree(lexeme);
    }
    
    ac_build_failure_links(automaton->root);
    ac_build_dictionary_links(automaton->root);
    PG_RETURN_POINTER(automaton);
}


PG_FUNCTION_INFO_V1(ac_build_array);
Datum ac_build_array(PG_FUNCTION_ARGS)
{
    ArrayType *arr = PG_GETARG_ARRAYTYPE_P(0);
    ac_automaton *automaton = (ac_automaton*)palloc0(sizeof(ac_automaton));
    automaton->root = ac_create_state();

    Oid elemtype = ARR_ELEMTYPE(arr);
    int ndim = ARR_NDIM(arr);
    int *dims = ARR_DIMS(arr);
    int nitems = ArrayGetNItems(ndim, dims);

    if (elemtype != TEXTOID)
        ereport(ERROR, (errcode(ERRCODE_INVALID_PARAMETER_VALUE), errmsg("ac_build_array requires a text array")));

    Datum *elements;
    bool *nulls;
    int actual_nitems;
    deconstruct_array(arr, TEXTOID, -1, false, 'i', &elements, &nulls, &actual_nitems);

    for (int i = 0; i < actual_nitems; i++) 
    {
        if (nulls[i])
            continue;

        char *lexeme = TextDatumGetCString(elements[i]);
        ac_add_keyword(automaton->root, lexeme, i);
    }

    ac_build_failure_links(automaton->root);
    ac_build_dictionary_links(automaton->root);

    PG_RETURN_POINTER(automaton);
}


bool evaluate_query(QueryItem *item, TSQuery *tsq, ac_automaton *automaton) 
{
    if (item->type == QI_VAL) 
    {
        char *lexeme = pnstrdup(GETOPERAND(tsq) + item->qoperand.distance, item->qoperand.length);  // Get lexeme from TSQuery
        bool found = ac_contains(automaton->root, lexeme);                                          // Look for that lexeme in the trie
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
    ac_automaton *automaton = (ac_automaton *)PG_GETARG_POINTER(0);
    TSQuery tsq = PG_GETARG_TSQUERY(1);
    QueryItem *items = GETQUERY(tsq);
    bool result = false;

    result = evaluate_query(items, tsq, automaton);

    PG_RETURN_BOOL(result);
}


PG_FUNCTION_INFO_V1(ac_search_text);
Datum ac_search_text(PG_FUNCTION_ARGS) 
{
    ac_automaton *automaton = (ac_automaton *)PG_GETARG_POINTER(0);
    text *input = PG_GETARG_TEXT_PP(1);
    char *text_str = text_to_cstring(input);
    bool found = ac_contains(automaton->root, text_str);
    
    pfree(text_str);
    PG_RETURN_BOOL(found);
}