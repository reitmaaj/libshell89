# Testing scenarios

## Parser

SCENARIO valid grammar
GIVEN an empty script, separators, CRLF, pipelines split across newlines, or
quoted operator characters
WHEN the script is parsed
THEN the AST has the expected commands and decoded option values.

SCENARIO invalid grammar
GIVEN bare arguments, unquoted values, dangling pipes, duplicate options,
malformed JSON, escaped NUL, lone surrogates, or invalid UTF-8
WHEN the script is parsed
THEN parsing fails with a structured diagnostic and no AST.

## Registry and preflight

SCENARIO whole-script rejection
GIVEN a valid native command followed by an unknown command or option
WHEN the script runs
THEN the native command is not invoked and the result reports the error.

## Streams and callbacks

SCENARIO binary identity
GIVEN arbitrary bytes including NUL and high bytes
WHEN copied through native/process pipelines
THEN the output matches byte-for-byte.

SCENARIO host state
GIVEN a native callback that increments host memory
WHEN a pipeline completes
THEN the increment is visible in the embedding process.

## Pipelines and sequences

SCENARIO mixed matrix
GIVEN every native/process combination for two and three stages
WHEN output is compared
THEN all combinations produce the same logical result.

SCENARIO sequences
GIVEN an earlier failing pipeline
WHEN later pipelines follow
THEN they still run and the last pipeline determines script status.

## Failure and embedding

SCENARIO broken pipe
GIVEN a native producer writing to a closed consumer
WHEN the write fails
THEN the callback observes EPIPE and the host survives.

SCENARIO resource failure
GIVEN an injected failure at any resource operation
WHEN a run completes or aborts
THEN no child, fd, or cooperative thread survives and a result or library error
is reported without masking failure.

SCENARIO host isolation
GIVEN pre-existing threads, signal dispositions, open descriptors, and strings
WHEN scripts execute repeatedly
THEN host state is unchanged except intended callback effects.
