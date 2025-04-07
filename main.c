#include "AhoCorasick.h"


int main(int argc, char **argv)
{
    // Create Aho-Corasick trie from a list of keywords
    const char* dictionary[] = {"apple", "banana", "cherry", "date", "elderberry"};
    int numKeywords = sizeof(dictionary) / sizeof(dictionary[0]);

    AhoCorasickState* root = AhoCorasickCreateTrie(dictionary, numKeywords);
    if (root == NULL)
    {
        printf("Failed to create Aho-Corasick trie.\n");
        return 1;
    }

    // Build failure links for the trie
    AhoCorasickBuildFailLinks(root);

    // Perform pattern matching on a text
    const char* text = "is apple the best fruit? cherry is a popular fruit.";
    int* matchIndices;
    int numMatches = AhoCorasickMatch(root, text, &matchIndices);

    // Print the matches
    if (numMatches > 0)
    {
        printf("Matches found in the text:\n");
        for (int i = 0; i < numMatches; i++)
        {
            printf("Match at index %d\n", matchIndices[i]);
        }
    }
    else
    {
        printf("No matches found in the text.\n");
    }

    // Free memory allocated for the trie and match indices
    AhoCorasickFreeTrie(root);
    free(matchIndices);

    return 0;
}