CREATE EXTENSION pg_ac;
SELECT ac_search(ac_build(to_tsvector('english', 'likes')), 'liked');
DROP EXTENSION pg_ac;