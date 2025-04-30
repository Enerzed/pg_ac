/*
* ac.c
*/


#include "ac.h"


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
    
    // Initialize queue with root's children
    for (int i = 0; i < MAX_CHILDREN; i++) 
	{
        if (root->children[i]) {
            if (queue_size == queue_capacity) 
			{
                queue_capacity *= 2;
                queue = (ac_state**)repalloc(queue, queue_capacity * sizeof(ac_state*));
            }
            queue[rear++] = root->children[i];
            queue_size++;
        }
    }
    
    // BFS traversal to set dictionary links
    while (front < rear) 
	{
        ac_state* current = queue[front++];
        ac_state* fail = current->fail_link;
        // Set dictionary link
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
        
        // Add children to queue
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


int ac_match(ac_state* root, char* text, int** match_indices, bool is_case_sensitive)
{
    ac_state* current = root;
    int text_length = strlen(text);
    int match_indices_capacity = 16;
    int num_matches = 0;
    *match_indices = (int*)palloc(match_indices_capacity * sizeof(int));
    
    char* processed_text = text;  // Default to original text
    
    // Create a lowercase copy if case insensitive
    if (!is_case_sensitive) 
    {
        processed_text = (char*)palloc(text_length + 1);
        for (int i = 0; i < text_length; i++) 
        {
            processed_text[i] = tolower(text[i]);
        }
        processed_text[text_length] = '\0';
    }

    for (int i = 0; i < text_length; i++) 
    {	
        if (is_case_sensitive)
        {
            // Move to fail link
            while (current && !current->children[(unsigned char)processed_text[i]])
            {
                current = current->fail_link;
            }
            if (current)
            {
                current = current->children[(unsigned char)processed_text[i]];
            }
            else 
            {
                current = root;
            }
        }
        else
        {
            // Move to fail link
            while (current && !current->children[(unsigned char)tolower(processed_text[i])] && !current->children[(unsigned char)toupper(processed_text[i])])
            {
                current = current->fail_link;
            }
            if (current)
            {
                if (current->children[(unsigned char)toupper(processed_text[i])])
                {
                    current = current->children[(unsigned char)toupper(processed_text[i])];
                }
                else
                {
                    current = current->children[(unsigned char)tolower(processed_text[i])];
                }
            }
            else 
            {
                current = root;
            }
        }
        // Check for matches using dictionary links
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
    
    // Free the temporary lowercase string if we created one
    if (!is_case_sensitive) 
    {
        pfree(processed_text);
    }
    
    return num_matches;
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


bool ac_contains(ac_state *root, const char *token) 
{
    ac_state *current = root;
    for (int i = 0; token[i] != '\0'; i++) 
    {
        unsigned char c = (unsigned char)token[i];
        if (!current->children[c]) 
        {
            return false;
        }
        current = current->children[c];
    }
    return current->is_final;
}


void print_trie(ac_state* root)
{	
	for (int i = 0; i < MAX_CHILDREN; i++)
	{
		if (root->children[i] != NULL)
		{
			printf("%c -> %p\n", i, root->children[i]);
			print_trie(root->children[i]);
		}
	}
}


PG_FUNCTION_INFO_V1(ac_search);
Datum ac_search(PG_FUNCTION_ARGS) 
{
    text* text_arg = PG_GETARG_TEXT_PP(0);
    ArrayType* keywords_array = PG_GETARG_ARRAYTYPE_P(1);
    char* text_str = text_to_cstring(text_arg);
    
    /* Process keywords */
    Datum* keyword_datums;
    bool* keyword_nulls;
    int num_keywords;
    deconstruct_array(keywords_array, TEXTOID, -1, false, 'i', &keyword_datums, &keyword_nulls, &num_keywords);
    
    char** keywords = palloc(num_keywords * sizeof(char*));
    HASHCTL hash_ctl = {0};
    hash_ctl.keysize = sizeof(char*);
    hash_ctl.entrysize = sizeof(char*);
    hash_ctl.hash = string_hash;
    hash_ctl.match = (HashCompareFunc)strcmp;
    HTAB* seen = hash_create("keywords", num_keywords, &hash_ctl, HASH_ELEM | HASH_FUNCTION | HASH_COMPARE);
    
    int unique_count = 0;
    for (int i = 0; i < num_keywords; i++) 
    {
        if (keyword_nulls[i]) continue;
        char* kw = TextDatumGetCString(keyword_datums[i]);
        char* lower = palloc(strlen(kw) + 1);
        for (int j = 0; kw[j]; j++) lower[j] = tolower(kw[j]);
        lower[strlen(kw)] = '\0';
        
        bool found;
        hash_search(seen, &lower, HASH_ENTER, &found);
        if (!found) keywords[unique_count++] = lower;
        else pfree(lower);
    }
    
    ac_state* root = ac_create_trie((const char**)keywords, unique_count);
    int* matches;
    int num_matches = ac_match(root, text_str, &matches, false);
    
    /* Build TSVector string */
    StringInfoData buf;
    initStringInfo(&buf);
    for (int i = 0; i < num_matches; i++)
    {
        appendStringInfo(&buf, "%s:%d ", keywords[matches[i]], i+1);
    }
    
    /* Convert to TSVector using standard function */
    text* tsv_text = cstring_to_text(buf.data);
    TSVector tsv = DatumGetTSVector(DirectFunctionCall1(to_tsvector, PointerGetDatum(tsv_text)));
    
    /* Cleanup */
    ac_free_trie(root);
    hash_destroy(seen);
    for (int i = 0; i < unique_count; i++) pfree(keywords[i]);
    pfree(keywords);
    pfree(text_str);
    pfree(buf.data);
    pfree(tsv_text);
    pfree(matches);
    
    PG_RETURN_TSVECTOR(tsv);
}