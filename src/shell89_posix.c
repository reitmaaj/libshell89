/* shell89_posix.c - pipes, descriptors, posix_spawn, worker threads, wait. */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "shell89_priv.h"

#include "shell89_test.h"

struct pipe_pair
{
    int read_fd;
    int write_fd;
};

struct fd_table
{
    int fds[64];
    size_t count;
};

struct worker
{
    const struct shell89_command *cmd;
    const struct shell89_priv_command *call;
    struct shell89_priv_stage_result *result;
    shell89_io io;
    int status;
};

static char *const empty_envp[] = {NULL};

static int test_fail(int site)
{
#ifdef SHELL89_TESTING
    return shell89_test_fault_should_fail(site);
#else
    (void)site;
    return 0;
#endif
}

static int set_cloexec(int fd)
{
    int flags;

    flags = fcntl(fd, F_GETFD);
    if (flags < 0)
    {
        return -1;
    }
    flags |= FD_CLOEXEC;
    if (test_fail(SHELL89_TEST_FCNTL))
    {
        errno = EIO;
        return -1;
    }
    return fcntl(fd, F_SETFD, flags);
}

static int raise_fd(int fd)
{
    int nfd;
    int flags;

    if (fd >= 3)
    {
        return fd;
    }
    if (test_fail(SHELL89_TEST_FCNTL))
    {
        errno = EIO;
        return -1;
    }
    nfd = fcntl(fd, F_DUPFD, 3);
    if (nfd < 0)
    {
        return -1;
    }
    close(fd);
    flags = fcntl(nfd, F_GETFD);
    if (flags < 0)
    {
        close(nfd);
        return -1;
    }
    if (fcntl(nfd, F_SETFD, flags | FD_CLOEXEC) < 0)
    {
        close(nfd);
        return -1;
    }
    return nfd;
}

static int duplicate_fd(int fd)
{
    int nfd;
    int r;

    if (test_fail(SHELL89_TEST_DUP))
    {
        errno = EIO;
        return -1;
    }
    do
    {
        nfd = dup(fd);
    } while (nfd < 0 && errno == EINTR);
    if (nfd < 0)
    {
        return -1;
    }
    r = raise_fd(nfd);
    if (r < 0)
    {
        close(nfd);
        return -1;
    }
    nfd = r;
    if (set_cloexec(nfd) < 0)
    {
        close(nfd);
        return -1;
    }
    return nfd;
}

static int open_null(int flags)
{
    int fd;
    int r;

#ifdef SHELL89_TESTING
    if (shell89_test_fault_should_fail(SHELL89_TEST_OPEN))
    {
        errno = EIO;
        return -1;
    }
#endif
    do
    {
        fd = open("/dev/null", flags);
    } while (fd < 0 && errno == EINTR);
    if (fd < 0)
    {
        return -1;
    }
    r = raise_fd(fd);
    if (r < 0)
    {
        close(fd);
        return -1;
    }
    fd = r;
    if (set_cloexec(fd) < 0)
    {
        close(fd);
        return -1;
    }
    return fd;
}

static int base_input(int fd)
{
    if (fd < 0)
    {
        return open_null(O_RDONLY);
    }
    return duplicate_fd(fd);
}

static int base_output(int fd)
{
    if (fd < 0)
    {
        return open_null(O_WRONLY);
    }
    return duplicate_fd(fd);
}

static int fd_track(struct fd_table *table, int fd)
{
    if (table->count >= sizeof table->fds / sizeof table->fds[0])
    {
        return -1;
    }
    table->fds[table->count] = fd;
    table->count++;
    return 0;
}

static void fd_close_all(struct fd_table *table)
{
    size_t i;

    for (i = 0; i < table->count; i++)
    {
        if (table->fds[i] >= 0)
        {
            close(table->fds[i]);
            table->fds[i] = -1;
        }
    }
    table->count = 0;
}

static int make_pipe(struct fd_table *table, struct pipe_pair *pair)
{
    int fds[2];
    int r;
    int a;
    int b;

    if (test_fail(SHELL89_TEST_PIPE))
    {
        errno = EIO;
        return -1;
    }
    if (pipe(fds) != 0)
    {
        return -1;
    }
    a = raise_fd(fds[0]);
    if (a < 0)
    {
        close(fds[0]);
        close(fds[1]);
        return -1;
    }
    b = raise_fd(fds[1]);
    if (b < 0)
    {
        close(a);
        close(fds[1]);
        return -1;
    }
    if (set_cloexec(a) < 0 || set_cloexec(b) < 0)
    {
        close(a);
        close(b);
        return -1;
    }
    r = fd_track(table, a);
    if (r == 0)
    {
        r = fd_track(table, b);
    }
    if (r != 0)
    {
        close(a);
        close(b);
        return -1;
    }
    pair->read_fd = a;
    pair->write_fd = b;
    return 0;
}

static void *worker_entry(void *arg)
{
    struct worker *w;
    shell89_args args;
    sigset_t set;

    w = (struct worker *)arg;
    sigemptyset(&set);
    sigaddset(&set, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    args.command = w->cmd;
    args.call = w->call;
    w->status = w->cmd->function(w->cmd->context, &args, &w->io);
    if (w->io.in_fd >= 0)
    {
        close(w->io.in_fd);
        w->io.in_fd = -1;
    }
    if (w->io.out_fd >= 0)
    {
        close(w->io.out_fd);
        w->io.out_fd = -1;
    }
    if (w->io.err_fd >= 0)
    {
        close(w->io.err_fd);
        w->io.err_fd = -1;
    }
    return NULL;
}

static int spawn_stage(const struct shell89_command *cmd,
                       const struct shell89_priv_command *call, int in_fd,
                       int out_fd, int err_fd, const struct fd_table *table,
                       pid_t *pid)
{
    posix_spawn_file_actions_t actions;
    size_t i;
    int r;

    r = posix_spawn_file_actions_init(&actions);
    if (r != 0)
    {
        return r;
    }
    r = posix_spawn_file_actions_adddup2(&actions, in_fd, 0);
    if (r == 0)
    {
        r = posix_spawn_file_actions_adddup2(&actions, out_fd, 1);
    }
    if (r == 0)
    {
        r = posix_spawn_file_actions_adddup2(&actions, err_fd, 2);
    }
    (void)table;
    (void)i;
    if (r == 0)
    {
        if (test_fail(SHELL89_TEST_POSIX_SPAWN))
        {
            r = EIO;
        }
        else
        {
            r = posix_spawn(pid, cmd->path.data, &actions, NULL,
                            call->proc_argv,
                            cmd->envp != NULL ? cmd->envp : empty_envp);
        }
    }
    posix_spawn_file_actions_destroy(&actions);
    return r;
}

static int spawn_status(int error)
{
    if (error == ENOENT)
    {
        return 127;
    }
    if (error == EACCES || error == ENOEXEC || error == EPERM)
    {
        return 126;
    }
    return 125;
}

static void join_workers(pthread_t *tids, int *started, struct worker *workers,
                         size_t count)
{
    size_t i;
    int r;

    for (i = 0; i < count; i++)
    {
        if (started[i] == 0)
        {
            continue;
        }
        do
        {
            if (test_fail(SHELL89_TEST_PTHREAD_JOIN))
            {
                r = EINTR;
            }
            else
            {
                r = pthread_join(tids[i], NULL);
            }
        } while (r == EINTR);
        if (r == 0)
        {
            started[i] = 2;
        }
        else
        {
            started[i] = 3;
            workers[i].result->status = 125;
            workers[i].result->termination = SHELL89_TERM_SHELL;
        }
    }
}

static void reap_children(pid_t *pids, int *spawned, struct worker *workers,
                          size_t count)
{
    size_t i;
    int status;
    pid_t r;

    for (i = 0; i < count; i++)
    {
        if (spawned[i] == 0)
        {
            continue;
        }
        status = 0;
        do
        {
            if (test_fail(SHELL89_TEST_WAITPID))
            {
                errno = EINTR;
                r = -1;
            }
            else
            {
                r = waitpid(pids[i], &status, 0);
            }
        } while (r < 0 && errno == EINTR);
        spawned[i] = 2;
        if (r < 0)
        {
            workers[i].result->status = 125;
            workers[i].result->termination = SHELL89_TERM_SHELL;
            continue;
        }
        if (WIFEXITED(status))
        {
            workers[i].result->status = WEXITSTATUS(status);
            workers[i].result->termination = SHELL89_TERM_EXIT;
            workers[i].result->signal = 0;
        }
        else if (WIFSIGNALED(status))
        {
            int sig;

            sig = WTERMSIG(status);
            workers[i].result->signal = sig;
            workers[i].result->termination = SHELL89_TERM_SIGNAL;
            if (sig > 127)
            {
                workers[i].result->status = 255;
            }
            else
            {
                workers[i].result->status = 128 + sig;
            }
        }
        else
        {
            workers[i].result->status = 125;
            workers[i].result->termination = SHELL89_TERM_SHELL;
        }
    }
}

static void kill_children(pid_t *pids, int *spawned, size_t count)
{
    size_t i;

    for (i = 0; i < count; i++)
    {
        if (spawned[i] == 1)
        {
            kill(pids[i], SIGTERM);
        }
    }
}

static void pipeline_status(struct shell89_priv_pipeline_result *pr,
                            struct shell89_priv_stage_result *stages)
{
    size_t i;

    pr->status = 0;
    for (i = 0; i < pr->stage_count; i++)
    {
        if (stages[i].status != 0)
        {
            pr->status = stages[i].status;
        }
    }
}

static int run_pipeline(const struct shell89_priv_pipeline *pipeline,
                        struct shell89_priv_pipeline_result *pr,
                        struct shell89_priv_stage_result *stages,
                        const struct shell89_run_io *io)
{
    struct fd_table table;
    struct pipe_pair pairs[SHELL89_MAX_STAGES];
    size_t n;
    size_t i;
    int base_in;
    int base_out;
    int base_err;
    int started_ok;
    int failed_status;
    pthread_t tids[SHELL89_MAX_STAGES];
    int started[SHELL89_MAX_STAGES];
    pid_t pids[SHELL89_MAX_STAGES];
    int spawned[SHELL89_MAX_STAGES];
    struct worker workers[SHELL89_MAX_STAGES];

    n = pipeline->command_count;
    table.count = 0;
    base_in = -1;
    base_out = -1;
    base_err = -1;
    started_ok = 1;
    failed_status = 125;
    for (i = 0; i < SHELL89_MAX_STAGES; i++)
    {
        tids[i] = (pthread_t)0;
        started[i] = 0;
        pids[i] = (pid_t)0;
        spawned[i] = 0;
        workers[i].cmd = NULL;
        workers[i].call = NULL;
        workers[i].result = &stages[i];
        workers[i].status = 0;
        workers[i].io.in_fd = -1;
        workers[i].io.out_fd = -1;
        workers[i].io.err_fd = -1;
        workers[i].io.error = 0;
    }
    base_in = base_input(io->stdin_fd);
    if (base_in >= 0)
    {
        if (fd_track(&table, base_in) != 0)
        {
            close(base_in);
            base_in = -1;
        }
    }
    if (base_in >= 0)
    {
        base_out = base_output(io->stdout_fd);
        if (base_out >= 0)
        {
            if (fd_track(&table, base_out) != 0)
            {
                close(base_out);
                base_out = -1;
            }
        }
    }
    if (base_out >= 0)
    {
        base_err = base_output(io->stderr_fd);
        if (base_err >= 0)
        {
            if (fd_track(&table, base_err) != 0)
            {
                close(base_err);
                base_err = -1;
            }
        }
    }
    if (base_in < 0 || base_out < 0 || base_err < 0)
    {
        fd_close_all(&table);
        for (i = 0; i < n; i++)
        {
            stages[i].status = 125;
            stages[i].termination = SHELL89_TERM_NONE;
        }
        pipeline_status(pr, stages);
        return 0;
    }
    for (i = 0; i + 1 < n; i++)
    {
        if (make_pipe(&table, &pairs[i]) != 0)
        {
            fd_close_all(&table);
            {
                size_t j;

                for (j = 0; j < n; j++)
                {
                    stages[j].status = 125;
                    stages[j].termination = SHELL89_TERM_NONE;
                }
            }
            pipeline_status(pr, stages);
            return 0;
        }
    }
    /* Spawn all process stages before creating worker threads. */
    for (i = 0; i < n; i++)
    {
        const struct shell89_command *cmd;
        int in_fd;
        int out_fd;
        int r;

        cmd = pipeline->commands[i].reg;
        if (cmd->kind != SHELL89_STAGE_PROCESS)
        {
            continue;
        }
        in_fd = i == 0 ? base_in : pairs[i - 1].read_fd;
        out_fd = i + 1 == n ? base_out : pairs[i].write_fd;
        r = spawn_stage(cmd, &pipeline->commands[i], in_fd, out_fd, base_err,
                        &table, &pids[i]);
        if (r != 0)
        {
            size_t j;

            stages[i].status = spawn_status(r);
            stages[i].termination = SHELL89_TERM_SHELL;
            for (j = i + 1; j < n; j++)
            {
                stages[j].status = 125;
                stages[j].termination = SHELL89_TERM_NONE;
            }
            started_ok = 0;
            failed_status = stages[i].status;
            break;
        }
        spawned[i] = 1;
    }
    if (started_ok != 0)
    {
        for (i = 0; i < n; i++)
        {
            const struct shell89_command *cmd;
            int r;

            cmd = pipeline->commands[i].reg;
            if (cmd->kind != SHELL89_STAGE_FUNCTION)
            {
                continue;
            }
            workers[i].cmd = cmd;
            workers[i].call = &pipeline->commands[i];
            workers[i].result = &stages[i];
            workers[i].status = 0;
            workers[i].io.in_fd =
                duplicate_fd(i == 0 ? base_in : pairs[i - 1].read_fd);
            workers[i].io.out_fd =
                duplicate_fd(i + 1 == n ? base_out : pairs[i].write_fd);
            workers[i].io.err_fd = duplicate_fd(base_err);
            workers[i].io.error = 0;
            if (workers[i].io.in_fd < 0 || workers[i].io.out_fd < 0 ||
                workers[i].io.err_fd < 0)
            {
                if (workers[i].io.in_fd >= 0)
                {
                    close(workers[i].io.in_fd);
                }
                if (workers[i].io.out_fd >= 0)
                {
                    close(workers[i].io.out_fd);
                }
                if (workers[i].io.err_fd >= 0)
                {
                    close(workers[i].io.err_fd);
                }
                stages[i].status = 125;
                stages[i].termination = SHELL89_TERM_SHELL;
                started_ok = 0;
                failed_status = 125;
                break;
            }
            if (test_fail(SHELL89_TEST_PTHREAD_CREATE))
            {
                r = EAGAIN;
            }
            else
            {
                r = pthread_create(&tids[i], NULL, worker_entry, &workers[i]);
            }
            if (r != 0)
            {
                close(workers[i].io.in_fd);
                close(workers[i].io.out_fd);
                close(workers[i].io.err_fd);
                stages[i].status = 125;
                stages[i].termination = SHELL89_TERM_SHELL;
                started_ok = 0;
                failed_status = 125;
                break;
            }
            started[i] = 1;
        }
    }
    fd_close_all(&table);
    if (started_ok == 0)
    {
        size_t j;

        kill_children(pids, spawned, n);
        join_workers(tids, started, workers, n);
        reap_children(pids, spawned, workers, n);
        for (j = 0; j < n; j++)
        {
            if (stages[j].status == 0 &&
                stages[j].termination == SHELL89_TERM_NONE)
            {
                stages[j].status = failed_status;
            }
        }
        pipeline_status(pr, stages);
        return 0;
    }
    join_workers(tids, started, workers, n);
    reap_children(pids, spawned, workers, n);
    for (i = 0; i < n; i++)
    {
        if (started[i] == 2)
        {
            if (workers[i].status >= 0 && workers[i].status <= 255)
            {
                stages[i].status = workers[i].status;
                stages[i].termination = SHELL89_TERM_CALLBACK;
            }
            else
            {
                stages[i].status = 125;
                stages[i].termination = SHELL89_TERM_INVALID;
            }
        }
    }
    pipeline_status(pr, stages);
    return 0;
}

int shell89_priv_execute(shell89 *sh, const struct shell89_priv_script *script,
                         const struct shell89_run_io *io,
                         struct shell89_result *result)
{
    size_t p;
    size_t c;
    size_t stage_index;

    (void)sh;
    stage_index = 0;
    for (p = 0; p < script->pipeline_count; p++)
    {
        result->pipelines[p].first_stage = stage_index;
        result->pipelines[p].stage_count = script->pipelines[p].command_count;
        for (c = 0; c < script->pipelines[p].command_count; c++)
        {
            result->stages[stage_index + c].kind =
                script->pipelines[p].commands[c].reg->kind;
            result->stages[stage_index + c].status = 125;
            result->stages[stage_index + c].termination = SHELL89_TERM_NONE;
            result->stages[stage_index + c].signal = 0;
        }
        stage_index += script->pipelines[p].command_count;
    }
    for (p = 0; p < script->pipeline_count; p++)
    {
        struct shell89_priv_pipeline_result *pr;
        struct shell89_priv_stage_result *stages;

        pr = &result->pipelines[p];
        stages = &result->stages[pr->first_stage];
        run_pipeline(&script->pipelines[p], pr, stages, io);
    }
    if (script->pipeline_count == 0)
    {
        result->status = 0;
    }
    else
    {
        result->status = result->pipelines[script->pipeline_count - 1].status;
    }
    return 0;
}
