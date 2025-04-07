/*
* AhoCorasick.c
*/


#include "AhoCorasick.h"


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

int AhoCorasickMatch(AhoCorasickState* root, const char* text, int** matchIndices)
{
	AhoCorasickState* current = root;
	int textLength = strlen(text);
	int matchIndicesCapacity = 16;
	int numMatches = 0;
	*matchIndices = (int*) malloc(matchIndicesCapacity * sizeof(int));

	for (int i = 0; i < textLength; i++)
	{
		while (current && !current->children[text[i]])
		{
			current = current->fail_link;
		}
		if (current)
		{
			current = current->children[text[i]];
		}
		else
		{
			current = root;
		}
		AhoCorasickState* temp = current;
		while (temp && temp->is_final)
		{
			if (numMatches == matchIndicesCapacity)
			{
				matchIndicesCapacity *= 2;
                *matchIndices = (int*) realloc(*matchIndices, matchIndicesCapacity * sizeof(int));
			}
			(*matchIndices)[numMatches++] = temp->index;
			temp = temp->fail_link;
		}
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
    return root;
}