# Concept — embeddable capability shell

The library composes explicitly registered capabilities. A host registers a
command as either a C callback or a configured executable. Scripts contain only
`group operation`, named options, pipelines, and unconditional sequences.

The central design is: native callbacks run on worker threads in the host
process; processes are launched with `posix_spawn()`; OS pipes connect all
stages. This preserves host-visible callback state while giving process commands
Unix-style stream behavior.

There is no command discovery, expansion, persistent state, or policy. The host
decides which commands exist and what they may do.

A standalone `tool/` runner registers a small fixed vocabulary for demonstration
and evaluation. It is not part of the library archive and offers no external
command registration or implicit `PATH` lookup. `examples/scripts/` exercises
that vocabulary with checked fixtures, statuses, and outputs.
