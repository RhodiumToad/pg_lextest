-- lextest extension

-- complain if script is sourced in psql, rather than via CREATE EXTENSION
\echo Use "CREATE EXTENSION lextest" to load this file. \quit

CREATE FUNCTION lexer_test(cstring) RETURNS void AS 'MODULE_PATHNAME' LANGUAGE C VOLATILE;

-- end
