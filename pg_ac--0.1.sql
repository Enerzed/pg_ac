-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_ac" to load this file. \quit

DROP TYPE IF EXISTS ac_automaton CASCADE;

CREATE TYPE ac_automaton;

CREATE FUNCTION ac_automaton_in(cstring)
RETURNS ac_automaton
AS 'pg_ac', 'ac_automaton_in'
LANGUAGE C IMMUTABLE STRICT;

CREATE FUNCTION ac_automaton_out(ac_automaton)
RETURNS cstring
AS 'pg_ac', 'ac_automaton_out'
LANGUAGE C IMMUTABLE STRICT;

CREATE TYPE ac_automaton 
(
    internallength = variable,
    input = ac_automaton_in,
    output = ac_automaton_out,
    storage = extended
);

CREATE FUNCTION ac_build(tsvector)
RETURNS ac_automaton
AS 'pg_ac', 'ac_build'
LANGUAGE C STRICT;

CREATE FUNCTION ac_search(ac_automaton, tsquery)
RETURNS boolean
AS 'pg_ac', 'ac_search'
LANGUAGE C STRICT;

CREATE FUNCTION ac_search(ac_automaton, text)
RETURNS boolean
AS 'pg_ac', 'ac_search_text'
LANGUAGE C STRICT;

CREATE FUNCTION ac_rank(ac_automaton, text) 
RETURNS float4
AS 'pg_ac', 'ac_rank'
LANGUAGE C STRICT;

-- Combined search and rank
CREATE FUNCTION ac_search_rank(ac_automaton, text) 
RETURNS TABLE (found bool, rank float4)
AS $$
    SELECT ac_search($1, $2), ac_rank($1, $2)
$$ LANGUAGE SQL;