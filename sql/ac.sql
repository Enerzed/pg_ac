CREATE EXTENSION pg_ac;
SELECT ac_search(ac_build(to_tsvector('english', 'He likes dogs and cats')), to_tsquery('like & cat'));
SELECT ac_search(ac_build(to_tsvector('english', 'He likes dogs')), to_tsquery('like & cat'));
SELECT ac_search(ac_build(to_tsvector('english', 'He likes dogs')), to_tsquery('like & (cat | dog)'));
SELECT ac_search(ac_build(to_tsvector('english', 'He likes snakes')), to_tsquery('like & (cat | dog)'));
SELECT ac_search(ac_build(to_tsvector('english', 'He likes snakes')), to_tsquery('like & !(cat | dog)'));
SELECT ac_search(ac_build(to_tsvector('english', 'He likes cats')), to_tsquery('like & !(cat | dog)'));

DROP EXTENSION pg_ac;