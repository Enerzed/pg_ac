-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_ac" to load this file. \quit


CREATE OR REPLACE FUNCTION ac_build(tsvector)
RETURNS integer
AS 'pg_ac', 'ac_build'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION ac_search(integer, tsquery)
RETURNS boolean
AS 'pg_ac', 'ac_search'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION ac_search(integer, text)
RETURNS boolean
AS 'pg_ac', 'ac_search_text'
LANGUAGE C STRICT;

CREATE OR REPLACE FUNCTION ac_rank_simple(integer, text) 
RETURNS real
AS 'pg_ac', 'ac_rank_simple'
LANGUAGE C STRICT;

/*
CREATE OR REPLACE FUNCTION ac_search_rank_simple(ac_automaton, text) 
RETURNS TABLE (found bool, rank float4)
AS $$
    SELECT ac_search($1, $2), ac_rank_simple($1, $2)
$$ LANGUAGE SQL;
*/