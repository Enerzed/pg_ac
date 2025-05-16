--TEST CASES--


CREATE EXTENSION pg_ac;


SELECT ac_search
(
    ac_build(to_tsvector('english', 'He likes dogs and cats')),
    to_tsquery('like & cat')
);

SELECT ac_search
(
    ac_build(to_tsvector('english', 'He likes dogs')),
    to_tsquery('like & cat')
);

SELECT ac_search
(
    ac_build(to_tsvector('english', 'He likes dogs')),
    to_tsquery('like & (cat | dog)')
);

SELECT ac_search
(
    ac_build(to_tsvector('english', 'He likes snakes')),
    to_tsquery('like & (cat | dog)')
);

SELECT ac_search
(
    ac_build(to_tsvector('english', 'He likes cats')),
    to_tsquery('like & !(cat | dog)')
);

SELECT ac_search
(
    ac_build(to_tsvector('english', 'He likes cats')),
    to_tsquery('like & !(cat | dog)')
);

SELECT ac_search
(
    ac_build(to_tsvector('english', 'He likes cats')),
    to_tsquery('likely')
);

SELECT ac_rank_simple
(
    ac_build(to_tsvector('The quick brown fox jumps over the lazy dog')),
    'jump dog fox'
);

SELECT ac_rank_simple
(
    ac_build(to_tsvector('The quick brown fox jumps over the lazy dog')),
    'fox'
);

SELECT ac_rank_simple
(
    ac_build(to_tsvector('The quick brown fox jumps over the lazy dog')),
    'pink horse'
);

SELECT * FROM ac_search_rank_simple
(
    ac_build(to_tsvector('The quick brown fox jumps over the lazy dog')),
    'jump dog'
);

DROP EXTENSION pg_ac;