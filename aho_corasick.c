/*
* AhoCorasick.c
*/


#include "aho_corasick.h"


AhoCorasickState* AhoCorasickCreateState()
{
	AhoCorasickState* state = (AhoCorasickState*) malloc(sizeof(AhoCorasickState));
	if (state == NULL)
	{
		return NULL;
	}
	state->is_root = false;
	state->is_final = false;
	state->index = -1;
	state->fail_link = NULL;
	for (int i = 0; i < MAX_CHILDREN; i++)
	{
		state->children[i] = NULL;
	}
	return state;
}


void AhoCorasickAddKeyword(AhoCorasickState* root, const char* keyword, const int index)
{
	AhoCorasickState* current = root;
    for (int i = 0; keyword[i]!= '\0'; i++)
    {
        if (current->children[keyword[i]] == NULL)
        {
            current->children[keyword[i]] = AhoCorasickCreateState();
        }
        current = current->children[keyword[i]];
    }
    current->is_final = true;
    current->index = index;
}


void AhoCorasickBuildFailLinks(AhoCorasickState* root)
{
	int queueCapacity= 16;
	int queueSize = 0;
	AhoCorasickState** queue = (AhoCorasickState**) malloc(queueCapacity * sizeof(AhoCorasickState*));
	int front = 0, rear = 0;
	queue[rear++] = root;
	queueSize++;

	while(front < rear)
	{
		AhoCorasickState* current = queue[front++];
        for (int i = 0; i < MAX_CHILDREN; i++)
		{
			AhoCorasickState* child = current->children[i];
			if (child && current == root)
			{
				child->fail_link = root;
			}
			else if (child)
			{
				AhoCorasickState* fail = current->fail_link;
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
				if (queueSize == queueCapacity)
				{
					queueCapacity *= 2;
					queue = (AhoCorasickState**) realloc(queue, queueCapacity * sizeof(AhoCorasickState*));
				}
				queue[rear++] = child;
				queueSize++;
			}
		}
	}
	free(queue);
}


void AhoCorasickBuildDictionaryLinks(AhoCorasickState* root)
{
    int queueCapacity = 16;
    int queueSize = 0;
    AhoCorasickState** queue = (AhoCorasickState**)malloc(queueCapacity * sizeof(AhoCorasickState*));
    int front = 0, rear = 0;
    
    // Initialize queue with root's children
    for (int i = 0; i < MAX_CHILDREN; i++) 
	{
        if (root->children[i]) {
            if (queueSize == queueCapacity) 
			{
                queueCapacity *= 2;
                queue = (AhoCorasickState**)realloc(queue, queueCapacity * sizeof(AhoCorasickState*));
            }
            queue[rear++] = root->children[i];
            queueSize++;
        }
    }
    
    // BFS traversal to set dictionary links
    while (front < rear) 
	{
        AhoCorasickState* current = queue[front++];
        AhoCorasickState* fail = current->fail_link;
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
            if (current->children[i]) {
                if (queueSize == queueCapacity) 
				{
                    queueCapacity *= 2;
                    queue = (AhoCorasickState**)realloc(queue, queueCapacity * sizeof(AhoCorasickState*));
                }
                queue[rear++] = current->children[i];
                queueSize++;
            }
        }
    }
    
    free(queue);
}


int AhoCorasickMatch(AhoCorasickState* root, char* text, int** matchIndices, bool isCaseSensitive)
{
    AhoCorasickState* current = root;
    int textLength = strlen(text);
    int matchIndicesCapacity = 16;
    int numMatches = 0;
    *matchIndices = (int*)malloc(matchIndicesCapacity * sizeof(int));
    
    char* processedText = text;  // Default to original text
    
    // Create a lowercase copy if case insensitive
    if (!isCaseSensitive) 
    {
        processedText = (char*)malloc(textLength + 1);
        for (int i = 0; i < textLength; i++) 
        {
            processedText[i] = tolower(text[i]);
        }
        processedText[textLength] = '\0';
    }

    for (int i = 0; i < textLength; i++) 
    {	
        if (isCaseSensitive)
        {
            // Move to fail link
            while (current && !current->children[(unsigned char)processedText[i]])
            {
                current = current->fail_link;
            }
            if (current)
            {
                current = current->children[(unsigned char)processedText[i]];
            }
            else 
            {
                current = root;
            }
        }
        else
        {
            // Move to fail link
            while (current && !current->children[(unsigned char)tolower(processedText[i])] && !current->children[(unsigned char)toupper(processedText[i])])
            {
                current = current->fail_link;
            }
            if (current)
            {
                if (current->children[(unsigned char)toupper(processedText[i])])
                {
                    current = current->children[(unsigned char)toupper(processedText[i])];
                }
                else
                {
                    current = current->children[(unsigned char)tolower(processedText[i])];
                }
            }
            else 
            {
                current = root;
            }
        }
        // Check for matches using dictionary links
        AhoCorasickState* temp = current;
        while (temp) 
        {
            if (temp->is_final) 
            {
                if (numMatches == matchIndicesCapacity) 
                {
                    matchIndicesCapacity *= 2;
                    *matchIndices = (int*)realloc(*matchIndices, matchIndicesCapacity * sizeof(int));
                }
                (*matchIndices)[numMatches++] = temp->index;
            }
            temp = temp->dictionary_link;
        }
    }
    
    // Free the temporary lowercase string if we created one
    if (!isCaseSensitive) 
    {
        free(processedText);
    }
    
    return numMatches;
}


void AhoCorasickFreeTrie(AhoCorasickState* current)
{
	if (current == NULL)
	{
		return;
	}
	for (int i = 0; i < MAX_CHILDREN; i++)
	{
		if (current->children[i] != NULL)
		{
			AhoCorasickFreeTrie(current->children[i]);
		}
	}
	free(current);
}


AhoCorasickState* AhoCorasickCreateTrie(const char** keywords, int size)
{
	AhoCorasickState* root = AhoCorasickCreateState();
	if (root == NULL)
	{
		return NULL;
	}

    for (int i = 0; i < size; i++)
    {
		int keywordLength = strlen(keywords[i]);
        AhoCorasickAddKeyword(root, keywords[i], i);
    }

    AhoCorasickBuildFailLinks(root);
	AhoCorasickBuildDictionaryLinks(root);

    return root;
}


void PrintTrie(AhoCorasickState* root)
{	
	for (int i = 0; i < MAX_CHILDREN; i++)
	{
		if (root->children[i] != NULL)
		{
			printf("%c -> %p\n", i, root->children[i]);
			PrintTrie(root->children[i]);
		}
	}
}