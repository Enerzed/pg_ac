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


void ac_build_failire_links(ac_state* root)
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
	if (root == NULL)
	{
		return NULL;
	}

    for (int i = 0; i < size; i++)
    {
		int keywordLength = strlen(keywords[i]);
        ac_add_keyword(root, keywords[i], i);
    }

    ac_build_failire_links(root);
	ac_build_dictionary_links(root);

    return root;
}


bool ac_contains(ac_state *root, const char *token) 
{
    ac_state *current = root;
    for (int i = 0; token[i] != '\0'; i++) {
        unsigned char c = (unsigned char)token[i];
        if (!current->children[c]) {
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
Datum ac_search(PG_FUNCTION_ARGS) {
    text *text_arg = PG_GETARG_TEXT_PP(0);
    ArrayType *keywords_array = PG_GETARG_ARRAYTYPE_P(1);
    char *text_str = text_to_cstring(text_arg);
    Datum *keyword_datums;
    bool *keyword_nulls;
    int keyword_count;
    char **keywords;
    ac_state *root;
    StringInfoData buf;
    HTAB *seen_keywords;
    HASHCTL hash_ctl;

    // Deconstruct keywords array
    deconstruct_array(keywords_array, TEXTOID, -1, false, 'i', &keyword_datums, &keyword_nulls, &keyword_count);

    // Define SeenEntry structure
    typedef struct SeenEntry {
        char *key;
    } SeenEntry;

    // Initialize hash table for deduplication
    memset(&hash_ctl, 0, sizeof(hash_ctl));
    hash_ctl.keysize = sizeof(char*);
    hash_ctl.entrysize = sizeof(SeenEntry);
    hash_ctl.hash = string_hash_helper;  // Custom hash function
    hash_ctl.match = (HashCompareFunc)strcmp;
    hash_ctl.hcxt = CurrentMemoryContext;
    seen_keywords = hash_create("Seen keywords", keyword_count, &hash_ctl, HASH_ELEM | HASH_FUNCTION | HASH_COMPARE | HASH_CONTEXT);

    keywords = (char **)palloc(keyword_count * sizeof(char *));
    int unique_count = 0;

    // Normalize and deduplicate keywords
    for (int i = 0; i < keyword_count; i++) {
        if (keyword_nulls[i])
            ereport(ERROR, (errcode(ERRCODE_NULL_VALUE_NOT_ALLOWED), errmsg("NULL keyword not allowed")));

        char *orig_keyword = TextDatumGetCString(keyword_datums[i]);
        char *lower_keyword = (char *)palloc(strlen(orig_keyword) + 1);
        for (int j = 0; orig_keyword[j]; j++) {
            lower_keyword[j] = tolower(orig_keyword[j]);
        }
        lower_keyword[strlen(orig_keyword)] = '\0';

        bool found;
        SeenEntry *entry = (SeenEntry *)hash_search(seen_keywords, &lower_keyword, HASH_ENTER, &found);
        if (!found) {
            entry->key = lower_keyword;
            keywords[unique_count++] = lower_keyword;
        } else {
            pfree(lower_keyword);
        }
    }

    // Build trie with unique keywords
    root = ac_create_trie((const char **)keywords, unique_count);

    // Tokenize input text into lowercase tokens
    initStringInfo(&buf);
    StringInfoData token_buf;
    initStringInfo(&token_buf);
    int token_pos = 1;

    for (int i = 0; text_str[i] != '\0'; i++) {
        if (isalnum((unsigned char)text_str[i])) {
            appendStringInfoChar(&token_buf, tolower(text_str[i]));
        } else {
            if (token_buf.len > 0) {
                if (ac_contains(root, token_buf.data)) {
                    appendStringInfo(&buf, "%s:%d ", token_buf.data, token_pos);
                }
                token_pos++;
                resetStringInfo(&token_buf);
            }
        }
    }

    // Check last token
    if (token_buf.len > 0) 
    {
        if (ac_contains(root, token_buf.data)) {
            appendStringInfo(&buf, "%s:%d ", token_buf.data, token_pos);
        }
    }

    // Generate TSVector
    text *tsv_text = cstring_to_text(buf.data);
    TSVector tsv = DatumGetTSVector(DirectFunctionCall1(to_tsvector, PointerGetDatum(tsv_text)));

    // Cleanup
    pfree(buf.data);
    pfree(token_buf.data);
    pfree(text_str);
    hash_destroy(seen_keywords);
    for (int i = 0; i < unique_count; i++) {
        pfree(keywords[i]);
    }
    pfree(keywords);
    ac_free_trie(root);

    PG_RETURN_TSVECTOR(tsv);
}