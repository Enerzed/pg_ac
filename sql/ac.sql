-- ============================================================
-- Test suite for pg_ac extension
-- Verifies both ASCII and UTF-8 (Cyrillic, diacritics) support
-- ============================================================

\echo '=== Creating extension ==='
CREATE EXTENSION IF NOT EXISTS pg_ac;

\echo '=== Initializing automaton storage ==='
SELECT ac_init();

\echo '=== Building ASCII automata ==='
-- 1: English words from tsvector (Dogs, cats, pets...)
SELECT ac_build(to_tsvector('english', 'Dogs and cats are the best pets')) AS id1;
-- 2: another tsvector
SELECT ac_build(to_tsvector('english', 'Dogs are the best pets')) AS id2;
-- 3: yet another
SELECT ac_build(to_tsvector('english', 'Cats are the best pets')) AS id3;
-- 4: Snakes
SELECT ac_build(to_tsvector('english', 'Snakes are the best animals')) AS id4;
-- 5: Quick brown fox...
SELECT ac_build(to_tsvector('english', 'Quick brown fox jumps over the lazy dog')) AS id5;
-- 6: array of English words
SELECT ac_build(ARRAY['quick', 'brown', 'fox', 'jumps', 'over', 'the', 'lazy', 'dog']) AS id6;

\echo '=== Search tests with tsquery (automaton 1) ==='
SELECT ac_search(1, to_tsquery('english', 'dog & cat')) AS should_be_true;
SELECT ac_search(1, to_tsquery('english', '!dog & !cat')) AS should_be_false;
SELECT ac_search(1, to_tsquery('english', 'dog | snake')) AS should_be_true;

\echo '=== Search tests (automaton 2) ==='
SELECT ac_search(2, to_tsquery('english', 'pet & (cat | dog)')) AS should_be_true;
SELECT ac_search(2, to_tsquery('english', 'pet & !(cat | dog)')) AS should_be_false;
SELECT ac_search(2, to_tsquery('english', 'likely')) AS should_be_false;

\echo '=== Search tests (automaton 3) ==='
SELECT ac_search(3, to_tsquery('english', 'pet & (dog | cat)')) AS should_be_true;
SELECT ac_search(3, to_tsquery('english', 'pet & !(dog | cat)')) AS should_be_false;
SELECT ac_search(3, to_tsquery('english', '')) AS should_be_false;   -- empty query

\echo '=== Search tests (automaton 4) ==='
SELECT ac_search(4, to_tsquery('english', 'are')) AS should_be_false;                -- stop word
SELECT ac_search(4, to_tsquery('english', 'pet & (dog | cat)')) AS should_be_false;
SELECT ac_search(4, to_tsquery('english', 'the')) AS should_be_false;                -- stop word

\echo '=== Ranking and ac_match tests (automata 5, 6) ==='
SELECT ac_rank_simple(5, 'jump dog fox') AS rank_0_5;
SELECT ac_rank_simple(5, 'fox') AS rank_0_1667;
SELECT ac_rank_simple(5, 'pink horse') AS rank_0;
SELECT ac_match(5, 'quick quick jump dog fox') AS matches_5;   -- {1,1,4,8,3}
SELECT ac_match(6, 'quick quick jump dog fox') AS matches_6;   -- {1,1,8,3}

\echo '=== Building UTF-8 automata (Cyrillic words) ==='
-- ID will be 7
SELECT ac_build(ARRAY['привет', 'мир', 'пока', 'до свидания', 'здравствуй']) AS id_russian;

\echo '=== Building UTF-8 automata (diacritics) ==='
-- ID will be 8
SELECT ac_build(ARRAY['café', 'résumé', 'über', 'straße', 'façade']) AS id_accent;

\echo '=== Search tests with Cyrillic text (ac_search) ==='
SELECT ac_search(7, 'привет мир!') AS should_be_true;
SELECT ac_search(7, 'пока, друзья') AS should_be_true;
SELECT ac_search(7, 'до свидания, мир') AS should_be_true;
SELECT ac_search(7, 'здравствуйте') AS should_be_true;
SELECT ac_search(7, 'hello world') AS should_be_false;
SELECT ac_search(7, 'goodbye') AS should_be_false;

\echo '=== ac_match tests with Cyrillic text ==='
SELECT ac_match(7, 'привет пока мир') AS matches_russian;   -- {1,3,2}

\echo '=== Search tests with diacritics ==='
SELECT ac_search(8, 'café résumé') AS should_be_true;
SELECT ac_search(8, 'über die Straße') AS should_be_true;
SELECT ac_search(8, 'façade') AS should_be_true;
SELECT ac_search(8, 'cafe') AS should_be_false;   -- missing accent

\echo '=== ac_match tests with diacritics ==='
SELECT ac_match(8, 'café résumé') AS matches_accent;   -- {1,2}

\echo '=== Cleanup ==='
SELECT ac_fini();
DROP EXTENSION pg_ac CASCADE;

\echo '=== All tests passed ==='