--TEST CASES--


CREATE EXTENSION pg_ac;

SELECT ac_build(to_tsvector('english', 'Dogs and cats are the best pets'));
SELECT ac_build(to_tsvector('english', 'Dogs are the best pets'));
SELECT ac_build(to_tsvector('english', 'Cats are the best pets'));
SELECT ac_build(to_tsvector('english', 'Snakes are the best animals'));
SELECT ac_build(to_tsvector('english', 'Quick brown fox jumps over the lazy dog'));
/*Test first automaton*/
SELECT ac_search(1, to_tsquery('english', 'dog & cat'));                        -- Should be true
SELECT ac_search(1, to_tsquery('english', '!dog & !cat'));                      -- Should be false
SELECT ac_search(1, to_tsquery('english', 'dog | snake'));                      -- Should be true
/*Test second automaton*/
SELECT ac_search(2, to_tsquery('english', 'pet & (cat | dog)'));                -- Should be true
SELECT ac_search(2, to_tsquery('english', 'pet & !(cat | dog)'));               -- Should be false
SELECT ac_search(2, to_tsquery('english', 'likely'));                           -- Should be false
/*Test third automaton*/
SELECT ac_search(3, to_tsquery('english', 'pet & (dog | cat)'));                -- Should be true
SELECT ac_search(3, to_tsquery('english', 'pet & !(dog | cat)'));               -- Should be false
SELECT ac_search(3, to_tsquery('english', ''));                                 -- Should be false
/*Test fourth automaton*/
SELECT ac_search(4, to_tsquery('english', 'are'));                              -- Should be false
SELECT ac_search(4, to_tsquery('english', 'pet & (dog | cat)'));                -- Should be false
SELECT ac_search(4, to_tsquery('english', 'the'));                              -- Should be false
/*Test fifth automaton for ranked search*/
SELECT ac_rank_simple(5, 'jump dog fox');                                       -- Should be 0.5
SELECT ac_rank_simple(5,'fox');                                                 -- Should be 0.1(6)
SELECT ac_rank_simple(5,'pink horse');                                          -- Should be 0

DROP EXTENSION pg_ac;