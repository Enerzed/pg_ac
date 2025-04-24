-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION pg_ac" to load this file. \quit


CREATE OR REPLACE FUNCTION ac_search(text, text[])
RETURNS tsvector
AS 'pg_ac', 'ac_search'
LANGUAGE C STRICT;