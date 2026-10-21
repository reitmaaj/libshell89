# Stories

## Host embedder

AS a C application embedding libshell89
I WANT to register C callbacks and external executables as commands
SO THAT scripts can compose application capabilities without a separate shell.

## Script author

AS a script author
I WANT pipelines and sequences with named options
SO THAT I can connect byte-stream commands predictably.

## CLI evaluator

AS a developer evaluating libshell89
I WANT a bundled runner with a small native command vocabulary
SO THAT I can execute example scripts without writing a host program.

## Example script reader

AS a developer learning the shell language
I WANT runnable scripts with checked expected outputs
SO THAT I can see pipelines, sequences, filtering, and file transforms working
without guessing about status or newline behavior.

## Security reviewer

AS a reviewer
I WANT every command and option validated before execution and no implicit
command lookup
SO THAT invalid scripts cannot cause partial effects and script text cannot
select arbitrary executables.
