CREATE EXTENSION pg_ac;
SELECT ac_search(ac_build(to_tsvector('english', 'He likes dogs and cats')), to_tsquery('like & cat'));
SELECT ac_search(ac_build(to_tsvector('english', 'He likes dogs')), to_tsquery('like & cat'));
SELECT ac_search(ac_build(to_tsvector('english', 'He likes dogs')), to_tsquery('like & (cat | dog)'));
SELECT ac_search(ac_build(to_tsvector('english', 'He likes snakes')), to_tsquery('like & (cat | dog)'));
SELECT ac_search(ac_build(to_tsvector('english', 'He likes snakes')), to_tsquery('like & !(cat | dog)'));
SELECT ac_search(ac_build(to_tsvector('english', 'He likes cats')), to_tsquery('like & !(cat | dog)'));
SELECT ac_search(ac_build(to_tsvector('english', 'He liked cats')), to_tsquery('likely'));
-- Get raw ranking score
SELECT ac_rank
(
    ac_build(to_tsvector('The quick brown fox jumps over the lazy dog')),
    'quick'
) AS relevance_score;

-- Combined search with ranking
SELECT * FROM ac_search_rank
(
    ac_build(to_tsvector('Example document serach terms')),
    'search terms'
);
DROP EXTENSION pg_ac;