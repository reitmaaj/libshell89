# Stories

## Host embedder

AS a C application embedding libshell89
I WANT to register C callbacks and external executables as commands
SO THAT scripts can compose application capabilities without a separate shell.

## Script author

AS a script author
I WANT pipelines and sequences with named options
SO THAT I can connect byte-stream commands predictably.

## Security reviewer

AS a reviewer
I WANT every command and option validated before execution and no implicit
command lookup
SO THAT invalid scripts cannot cause partial effects and script text cannot
select arbitrary executables.
