/*
* AhoCorasick.h
*/


#pragma once


#include "parser.h"

#include "postgres.h"
#include "fmgr.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>


#define MAX_CHILDREN 256


PG_MODULE_MAGIC;


typedef struct
{
	struct AhoCorasickState* children[MAX_CHILDREN];
	struct AhoCorasickState* fail_link;
	//struct AhoCorasickState* output_link;
	struct AhoCorasickState* dictionary_link;
	//struct AhoCorasickState* sibling_link;
	int index;
	bool is_root;
	bool is_final;

} AhoCorasickState;


AhoCorasickState* AhoCorasickCreateState();

void AhoCorasickAddKeyword(AhoCorasickState* root, const char* keyword, const int index);

void AhoCorasickBuildFailLinks(AhoCorasickState* root);

void AhoCorasickBuildDictionaryLinks(AhoCorasickState* root);

int AhoCorasickMatch(AhoCorasickState* root, char* text, int** matchIndices, bool isCaseSensitive);

void AhoCorasickFreeTrie(AhoCorasickState* trie);

AhoCorasickState* AhoCorasickCreateTrie(const char** keywords, int size);

void PrintTrie(AhoCorasickState* root);