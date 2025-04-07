-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION AhoCorasick" to load this file. \quit

/*
CREATE OR REPLACE OPERATOR CLASS gin_aho_corasick_ops
FOR TYPE text USING gin
AS 
    OPERATOR    1   pg_catalog.~~ (text, text),
    OPERATOR    1   
    FUNCTION    1   aho_corasick_add_state(internal),
    FUNCTION    1   aho_corasick_add_keyword(internal, internal, text, int4, internal),
    FUNCTION    1   aho_corasick_finalize(internal),
    STORAGE     text;
*/