/*
* AhoCorasick.h
*/


#pragma once


//#include "postgres.h"
//#include "fmgr.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>


//PG_MODULE_MAGIC;


#define MAX_CHILDREN 256


typedef struct
{
	struct AhoCorasickState* children[MAX_CHILDREN];
	struct AhoCorasickState* fail_link;
	struct AhoCorasickState* output_link;
	struct AhoCorasickState* dictionary_link;
	struct AhoCorasickState* sibling_link;
	int index;
	bool is_root;
	bool is_final;

} AhoCorasickState;


AhoCorasickState* AhoCorasickCreateState();

void AhoCorasickAddKeyword(AhoCorasickState* root, const char* keyword, const int index);

void AhoCorasickBuildFailLinks(AhoCorasickState* root);

void AhoCorasickBuildDictionaryLinks(AhoCorasickState* root);

int AhoCorasickMatch(AhoCorasickState* root, const char* text, int** matchIndices);

void AhoCorasickFreeTrie(AhoCorasickState* trie);

AhoCorasickState* AhoCorasickCreateTrie(const char** keywords, int size);

void PrintTrie(AhoCorasickState* root);