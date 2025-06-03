/* Create extension */
CREATE EXTENSION pg_ac;
/* Init storage */
SELECT ac_init();
/* Create test table with 100 words for PostgreSQL Full Text Search */
CREATE TABLE test_table3
(
    id SERIAL PRIMARY KEY,
    tsv TSVECTOR
);
INSERT INTO test_table3 (tsv)
SELECT 
to_tsvector('english', 
'over say set try new great put kind sound where end hand take help does picture only through another again
little much well change work before large off know line must play place right big spell years too even air
live means such away me old because animal back any turn house give same here point most tell why page
very boy ask letter after follow went mother things came men answer our want read found just show need study
name also land still good around different learn sentence form home should man three us America think small move world')
FROM generate_series(1, 1000);
/* Create test table with 100 words for pg_ac */
CREATE TABLE test_table4
(
    id SERIAL PRIMARY KEY,
    hid INTEGER
);
INSERT INTO test_table4 (hid)
SELECT
ac_build(to_tsvector('english',
'over say set try new great put kind sound where end hand take help does picture only through another again
little much well change work before large off know line must play place right big spell years too even air
live means such away me old because animal back any turn house give same here point most tell why page
very boy ask letter after follow went mother things came men answer our want read found just show need study
name also land still good around different learn sentence form home should man three us America think small move world'))
FROM generate_series(1, 1000);
/* Test 100 words performance */
/* PostgreSQL Full Text Search */
SELECT * FROM test_table3
WHERE tsv @@ 'cat';
EXPLAIN ANALYZE SELECT * FROM test_table3
WHERE tsv @@ 'cat';
/* pg_ac */
SELECT * FROM test_table4
WHERE ac_search(hid, 'cat');
EXPLAIN ANALYZE SELECT * FROM test_table4
WHERE ac_search(hid, 'cat');
SELECT * FROM test_table4
WHERE ac_match(hid, 'cat') IS NOT NULL;
EXPLAIN ANALYZE SELECT * FROM test_table4
WHERE ac_match(hid, 'cat') IS NOT NULL;
SELECT * FROM test_table4
WHERE ac_rank_simple(hid, 'cat') > 0;
EXPLAIN ANALYZE SELECT * FROM test_table4
WHERE ac_rank_simple(hid, 'cat') > 0;
/* Test 100 words performance */
/* PostgreSQL Full Text Search */
SELECT * FROM test_table3
WHERE tsv @@ to_tsquery('english', 'cat | dog | snake');
EXPLAIN ANALYZE SELECT * FROM test_table3
WHERE tsv @@ to_tsquery('english', 'cat | dog | snake');
/* pg_ac */
SELECT * FROM test_table4
WHERE ac_search(hid, to_tsquery('english', 'cat | dog | snake'));
EXPLAIN ANALYZE SELECT * FROM test_table4
WHERE ac_search(hid, to_tsquery('english', 'cat | dog | snake'));
SELECT * FROM test_table4
WHERE ac_search(hid, 'cat dog snake');
EXPLAIN ANALYZE SELECT * FROM test_table4
WHERE ac_search(hid, 'cat dog snake');
SELECT * FROM test_table4
WHERE ac_match(hid, 'cat dog snake') IS NOT NULL;
EXPLAIN ANALYZE SELECT * FROM test_table4
WHERE ac_match(hid, 'cat dog snake') IS NOT NULL;
SELECT * FROM test_table4
WHERE ac_rank_simple(hid, 'cat dog snake') > 0;
EXPLAIN ANALYZE SELECT * FROM test_table4
WHERE ac_rank_simple(hid, 'cat dog snake') > 0;
/* Clean up */
SELECT ac_fini();
DROP TABLE test_table3 CASCADE;
DROP TABLE test_table4 CASCADE;
DROP EXTENSION pg_ac;