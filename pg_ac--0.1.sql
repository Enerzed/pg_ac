-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_ac" to load this file. \quit


CREATE FUNCTION ac_build(tsvector)
RETURNS ac_automaton
AS 'pg_ac', 'ac_build'
LANGUAGE C IMMUTABLE;

CREATE FUNCTION ac_search(ac_automaton, text)
RETURNS jsonb
AS 'pg_ac', 'ac_search'
LANGUAGE C STRICT;