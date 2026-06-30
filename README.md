Features

✅ Build automata from tsvector or text[]
✅ Search using plain text or tsquery with boolean operators (AND, OR, NOT)
✅ Match – get all matched keyword positions (indices)
✅ Rank – simple relevance score (ratio of matched keywords)
✅ Dynamic – add or remove keywords on the fly (full rebuild)
✅ Persistent – serialize automata to bytea and restore later
✅ UTF‑8 aware – works with Unicode characters (Cyrillic, diacritics, etc.)
✅ Memory efficient – uses sorted edge arrays instead of 256‑slot tables per node

Installation

From source

bash
git clone https://github.com/Enerzed/pg_ac.git
cd pg_ac
make
sudo make install
Then, in your database:

sql
CREATE EXTENSION pg_ac;
Requirements

PostgreSQL 12 or later
C compiler with C99 support
pg_config available in PATH
Getting started

After creating the extension, you must initialize the global automaton storage (per‑session memory):

sql
SELECT ac_init();
This creates an internal hash table that will hold all automata created in the current session.
Note: Automata exist only for the lifetime of the session. To persist them across sessions, use serialization (see below).

Building automata

You can build an automaton from either a tsvector (with positional information) or a plain array of words.

From tsvector

sql
SELECT ac_build(to_tsvector('english', 'Quick brown fox jumps over the lazy dog'));
Returns: bigint – the automaton ID.

The lexemes are extracted using the specified text search configuration ('english' in this example) and their positions (as stored in the tsvector) become the keyword indices used by ac_match. Stop words are ignored.

From text[]

sql
SELECT ac_build(ARRAY['Quick', 'brown', 'fox', 'jumps', 'over', 'the', 'lazy', 'dog']);
Returns: bigint – automaton ID.

Each array element becomes a keyword, and its array index (1‑based) becomes its match index.

💡 Indices returned by ac_match correspond to the positions from the tsvector (or array indexes) and are stable even when you later add/remove keywords.
Searching

All search functions require an automaton ID and return a boolean (ac_search) or an array of integers (ac_match) or a float (ac_rank_simple).

Plain text search

sql
SELECT ac_search(1, 'The quick brown fox');   -- true if any keyword is found
TSQuery search

sql
SELECT ac_search(1, to_tsquery('english', '!slow & (fox | dog)'));
The automaton checks each lexeme from the tsquery against the trie. The query is evaluated with the usual AND, OR, NOT operators. Stop‑words are handled by PostgreSQL’s to_tsquery.

Get all match indices

sql
SELECT ac_match(1, 'The quick brown fox jumps over the lazy dog');
Returns an array of all keyword indices that appear in the text, in the order they are encountered. Duplicates are preserved (if the same keyword appears multiple times, its index appears multiple times).

Simple ranking

sql
SELECT ac_rank_simple(1, 'The quick brown fox');
Returns a float between 0 and 1:

0 – no keyword found,
1 – all keywords in the automaton were found.
The score is calculated as (#matched_keywords) / (total_keywords).

Dynamic updates

You can add or remove keywords from an existing automaton. The automaton is fully rebuilt after each change, which may be heavy for large sets, but is simple and consistent.

Add a keyword

sql
SELECT ac_add(1, 'new_keyword');
Remove a keyword

sql
SELECT ac_remove(1, 'old_keyword');
Both functions return true on success.
After an update, the automaton is immediately ready for searching. The indices of existing keywords do not change; new keywords receive the next free index.

⚠️ Performance note: Each ac_add / ac_remove triggers a full rebuild of failure and dictionary links, which takes O(total length of all keywords). For very large automata (hundreds of thousands of words) with frequent updates, consider batching changes or using an offline rebuild.
Persistence (serialization)

Automata live only in the session memory. To save them permanently, you can serialize an automaton to a bytea blob and later deserialize it.

Serialize to bytea

sql
SELECT ac_serialize(1) AS blob;
Returns a bytea value that contains the complete automaton (keywords and indices). You can store this in a table:

sql
CREATE TABLE saved_automata (id SERIAL PRIMARY KEY, data BYTEA);
INSERT INTO saved_automata (data) SELECT ac_serialize(1);
Deserialize from bytea

sql
SELECT ac_deserialize( (SELECT data FROM saved_automata WHERE id = 1) );
This creates a new automaton in the current session, assigns a fresh ID, and returns it. The restored automaton is fully functional.

Cleaning up

Remove a single automaton

sql
SELECT ac_destroy(1);
Frees all memory associated with the automaton and removes it from the session storage.

Remove all automata

sql
SELECT ac_fini();
Destroys all automata in the current session and deinitialises the storage. After this, you must call ac_init() again if you want to create new automata.

Full example session

sql
-- 1. Init
SELECT ac_init();

-- 2. Build from an array
SELECT ac_build(ARRAY['hello', 'world', 'postgres']) AS id;  -- returns 1

-- 3. Search
SELECT ac_search(1, 'hello postgres');   -- true
SELECT ac_match(1, 'hello world');       -- {1,2}
SELECT ac_rank_simple(1, 'hello world postgres');  -- 1.0 (all three found)

-- 4. Add a keyword
SELECT ac_add(1, 'extension');

-- 5. Check new word
SELECT ac_search(1, 'extension');        -- true

-- 6. Remove a keyword
SELECT ac_remove(1, 'world');

-- 7. Verify removal
SELECT ac_match(1, 'hello world');       -- {1} (world is gone)

-- 8. Serialize and save
CREATE TABLE automaton_backup (id SERIAL, data BYTEA);
INSERT INTO automaton_backup (data) SELECT ac_serialize(1);

-- 9. Destroy and restore
SELECT ac_destroy(1);
SELECT ac_deserialize( (SELECT data FROM automaton_backup WHERE id = 1) );  -- returns new ID

-- 10. Final cleanup
SELECT ac_fini();
Notes and limitations

Session‑local storage – automata reside in memory (TopMemoryContext) and are lost when the session ends. Use serialization for persistence.
UTF‑8 support – all functions handle Unicode correctly; keywords and texts are processed code‑point by code‑point.
Performance – matching is O(text length + number of matches). Rebuilding (after ac_add/ac_remove) is O(total length of all keywords), so batch updates when possible.
Index stability – indices assigned to keywords never change, even after deletion/additions. This makes ac_match results consistent across sessions.
Memory – the automaton uses a compact edge‑array representation, which is more memory‑efficient than a full 256‑slot table per node.
License

This project is licensed under the PostgreSQL License – see the LICENSE file for details.

Contributing

Bug reports, feature requests, and pull requests are welcome. Please open an issue or submit a PR on GitHub.

Acknowledgements

The Aho‑Corasick algorithm is described in Efficient String Matching: An Aid to Bibliographic Search by Aho and Corasick (1975).
This extension was inspired by the need for fast multi‑pattern matching inside PostgreSQL without external dependencies.
