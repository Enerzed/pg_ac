/* Create extension */
CREATE EXTENSION pg_ac;
/* Init storage */
SELECT ac_init();
/* Create test table with 10 words for PostgreSQL Full Text Search */
CREATE TABLE test_table1
(
    id SERIAL PRIMARY KEY,
    tsv TSVECTOR
);
INSERT INTO test_table1 (tsv)
SELECT 
to_tsvector('english',
'Golden leaves rustle crimson sunset ignites dark valleys stars shimmer')
FROM generate_series(1, 10000);
/* Create test table with 10 words for pg_ac */
CREATE TABLE test_table2
(
    id SERIAL PRIMARY KEY,
    hid INTEGER
);
INSERT INTO test_table2 (hid)
SELECT
ac_build(to_tsvector('english',
'Golden leaves rustle crimson sunset ignites dark valleys stars shimmer'))
FROM generate_series(1, 10000 );
/* Test 10 words performance */
/* PostgreSQL Full Text Search */
SELECT * FROM test_table1
WHERE tsv @@ 'cat';
EXPLAIN ANALYZE SELECT * FROM test_table1
WHERE tsv @@ 'cat';
/* pg_ac */
SELECT * FROM test_table2
WHERE ac_search(hid, 'cat');
EXPLAIN ANALYZE SELECT * FROM test_table2
WHERE ac_search(hid, 'cat');
SELECT * FROM test_table2
WHERE ac_match(hid, 'cat') IS NOT NULL;
EXPLAIN ANALYZE SELECT * FROM test_table2
WHERE ac_match(hid, 'cat') IS NOT NULL;
SELECT * FROM test_table2
WHERE ac_rank_simple(hid, 'cat') > 0;
EXPLAIN ANALYZE SELECT * FROM test_table2
WHERE ac_rank_simple(hid, 'cat') > 0;
/* Test 10 words performance */
/* PostgreSQL Full Text Search */
SELECT * FROM test_table1
WHERE tsv @@ to_tsquery('english', 'cat & dog | snake');
EXPLAIN ANALYZE SELECT * FROM test_table1
WHERE tsv @@ to_tsquery('english', 'cat & dog | snake');
/* pg_ac */
SELECT * FROM test_table2
WHERE ac_search(hid, to_tsquery('english', 'cat & dog | snake'));
EXPLAIN ANALYZE SELECT * FROM test_table2
WHERE ac_search(hid, to_tsquery('english', 'cat & dog | snake'));
/* Clean up */
SELECT ac_fini();
DROP TABLE test_table1 CASCADE;
DROP TABLE test_table2 CASCADE;
DROP EXTENSION pg_ac;