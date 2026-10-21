# 0004 - example script suite

`examples/scripts/` contains twenty runnable `.shell` files that use only the
demonstration CLI vocabulary. The suite doubles as documentation and as an
end-to-end regression corpus.

Layout:

```text
examples/scripts/
  README.md                 index and semantics
  NN-name.shell             scripts, executed in lexical order
  fixtures/                 inputs copied into each isolated run directory
  expected/
    NN-name.out             expected stdout when non-empty
    NN-name.err             expected stderr when non-empty
    NN-name.status          expected status when nonzero
    NN-name.files/          expected created files when any
```

`scripts/example-scripts-check.sh`:

- resolves the binary and suite paths to absolute paths;
- recreates `build/tmp/example-scripts` and runs each script with a private cwd;
- copies only `fixtures/` into that cwd;
- compares status, stdout, stderr, and the complete set/content of files created
  outside `fixtures/`;
- reports every failing script before exiting nonzero.

`just example-scripts` builds the CLI and runs the suite. `just check` includes
it after the CLI checks.

The scripts use relative fixture paths and therefore are independent of the
repository's absolute location. Script 20 deliberately reads a missing fixture
to demonstrate that sequences continue after a runtime failure.
