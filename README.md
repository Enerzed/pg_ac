_**Aho-Corasick algorithm for full text search in PostgreSQL**_


First after installing the extension create it in your database:
'''sql
  CREATE EXTENSION pg_ac;
'''

  
Then initialize the hash-table for automatons with:

  SELECT ac_init();
  
After that you may start creating automatons and using the search functions.



_**Build functions return an automaton id for use in the search functions:**_

1. Build function ac_build(tsvector):

   SELECT ac_build(to_tsvector('Quick brown fox jumps over the lazy dog'));

   
2. Build function ac_build(text[])

   SELECT ac_build(['Quick', 'brown', 'fox', 'jumps', 'over', 'the', 'lazy', 'dog']);


   
Results of these functions are different, look how PostgreSQL FTS uses dictionaries and word normalizationg



_**Search functions:**_
1. Search function ac_search(id, text) immediately returns TRUE if found at least 1 word

   SELECT ac_search(1, 'Text where we look for at least one word from the automaton');
   
2. Search function ac_search(id, tsquery) looks for a word in each word in the TSQuery and aplies the logical operators

   SELECT ac_search(1, to_query('!Text & where | we & !(look)'));
   
3. Match function ac_match(id, text) returns indices of all occurances in a text

   SELECT ac_match(1, 'Text where we look');
   
4. Rank function ac_rank_simple(id, text) returns float value from 0 to 1, where 0 means no occurances found, and 1 means every word is found

   SELECT ac_rank_simple(1, 'Text where we look');


   
If you want to delete an automaton you can use ac_destroy(id) function

  SELECT ac_destroy(1);

If you want to delete all the automatons you can use ac_fini() function

  SELECT ac_fini();
