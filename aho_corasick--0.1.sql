-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION aho_corasick" to load this file. \quit


CREATE OR REPLACE FUNCTION parser_start(internal, int4)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;


CREATE OR REPLACE FUNCTION get_lex(internal, internal, internal)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;


CREATE OR REPLACE FUNCTION parser_end(internal)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;


CREATE OR REPLACE FUNCTION lex_type(internal)
RETURNS internal
AS 'MODULE_PATHNAME'
LANGUAGE C STRICT;


CREATE TEXT SEARCH PARSER parser
(
    START = ParserStart,
    GETTOKEN = GetLex,
    END = ParserEnd,
    HEADLINE = pg_catalog.prsd_headline,
    LEXTYPES = LexType
);