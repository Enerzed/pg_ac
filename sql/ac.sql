--TEST CASES--

/* Create extension */
CREATE EXTENSION pg_ac;
/* Init automaton storage */
SELECT ac_init();
/* Prepare test data */
SELECT ac_build(to_tsvector('english', 'Dogs and cats are the best pets'));
SELECT ac_build(to_tsvector('english', 'Dogs are the best pets'));
SELECT ac_build(to_tsvector('english', 'Cats are the best pets'));
SELECT ac_build(to_tsvector('english', 'Snakes are the best animals'));
SELECT ac_build(to_tsvector('english', 'Quick brown fox jumps over the lazy dog'));
SELECT ac_build(ARRAY['quick', 'brown', 'fox', 'jumps', 'over', 'the', 'lazy', 'dog']);

/* Test first automaton */
SELECT ac_search(1, to_tsquery('english', 'dog & cat'));                        -- Should be true
SELECT ac_search(1, to_tsquery('english', '!dog & !cat'));                      -- Should be false
SELECT ac_search(1, to_tsquery('english', 'dog | snake'));                      -- Should be true

/* Test second automaton */
SELECT ac_search(2, to_tsquery('english', 'pet & (cat | dog)'));                -- Should be true
SELECT ac_search(2, to_tsquery('english', 'pet & !(cat | dog)'));               -- Should be false
SELECT ac_search(2, to_tsquery('english', 'likely'));                           -- Should be false

/* Test third automaton */
SELECT ac_search(3, to_tsquery('english', 'pet & (dog | cat)'));                -- Should be true
SELECT ac_search(3, to_tsquery('english', 'pet & !(dog | cat)'));               -- Should be false
SELECT ac_search(3, to_tsquery('english', ''));                                 -- Should be false

/* Test fourth automaton */
SELECT ac_search(4, to_tsquery('english', 'are'));                              -- Should be false
SELECT ac_search(4, to_tsquery('english', 'pet & (dog | cat)'));                -- Should be false
SELECT ac_search(4, to_tsquery('english', 'the'));                              -- Should be false

/* Test fifth automaton for ranked search */
SELECT ac_rank_simple(5, 'jump dog fox');                                       -- Should be 0.5
SELECT ac_rank_simple(5,'fox');                                                 -- Should be 0.1(6)
SELECT ac_rank_simple(5,'pink horse');                                          -- Should be 0

/* Matches for fifth and sixth automaton, index structure is the same as corresponding TSVector indexes */
SELECT ac_match(5, 'quick quick jump dog fox');                            -- Should be 1,1,4,8,3
SELECT ac_match(6, 'quick quick jump dog fox');                            -- Should be 1,1,8,3
/* Clean up */
SELECT ac_fini();

DROP EXTENSION pg_ac;