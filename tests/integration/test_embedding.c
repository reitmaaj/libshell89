#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "shell89_capture.h"
#include "shell89_fixtures.h"
#include "shell89_support.h"

static int open_count(void)
{
    int i;
    int n;
    int max;

    n = 0;
    max = 256;
    for (i = 0; i < max; i++)
    {
        if (fcntl(i, F_GETFD) >= 0)
        {
            n++;
        }
    }
    return n;
}

static pthread_mutex_t thread_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t thread_cond = PTHREAD_COND_INITIALIZER;
static int thread_stop = 0;
static int thread_started = 0;

static void *background_thread(void *arg)
{
    (void)arg;
    pthread_mutex_lock(&thread_lock);
    thread_started = 1;
    while (thread_stop == 0)
    {
        pthread_cond_wait(&thread_cond, &thread_lock);
    }
    pthread_mutex_unlock(&thread_lock);
    return NULL;
}

struct shell_job
{
    const char *path;
    int ok;
};

static void *run_job(void *arg)
{
    struct shell_job *job;
    shell89 *sh;
    shell89_result *res;
    unsigned char *out;
    unsigned char *err;
    size_t out_len;
    size_t err_len;
    struct shell89_fixture_context ctx;

    job = (struct shell_job *)arg;
    ctx.counter = 0;
    sh = shell89_create();
    if (sh == NULL)
    {
        return NULL;
    }
    shell89_fixture_register_native(sh, &ctx);
    shell89_fixture_register_process(sh, job->path);
    if (shell89_capture_run(sh, "fn state; proc copy", "x", 1, &out, &out_len,
                            &err, &err_len, &res) == SHELL89_OK)
    {
        job->ok = shell89_status(res) == 0 && ctx.counter == 1 &&
                  out_len == 7 && memcmp(out, "state\nx", 7) == 0;
        shell89_result_destroy(res);
        shell89_capture_free(out);
        shell89_capture_free(err);
    }
    shell89_destroy(sh);
    return NULL;
}

int main(void)
{
    shell89 *sh;
    struct shell89_fixture_context ctx;
    struct sigaction act;
    struct sigaction old;
    struct sigaction now;
    pthread_t bg;
    pid_t unused;
    char cwd_before[512];
    char cwd_after[512];
    int fd[2];
    int before;
    int after;
    int runs;
    shell89_result *res;
    unsigned char *out;
    unsigned char *err;
    size_t out_len;
    size_t err_len;
    const char *path;
    char *group;
    char *op;
    char *opt_name;
    struct shell89_option *opts;
    struct shell89_process proc;
    const char *fixed[2];
    const char *env[2];
    char *fixed0;
    char *fixed1;
    char *env0;
    char *path_copy;
    struct shell_job jobs[8];
    pthread_t threads[8];
    size_t i;

    (void)unused;
    path = getenv("SHELL89_PROC_FIXTURE");
    if (path == NULL)
    {
        path = "./build/fixture_proc";
    }

    CHECK(getcwd(cwd_before, sizeof cwd_before) != NULL);
    memset(&act, 0, sizeof act);
    act.sa_handler = SIG_IGN;
    sigemptyset(&act.sa_mask);
    CHECK_EQ(sigaction(SIGUSR1, &act, &old), 0);

    CHECK_EQ(pipe(fd), 0);
    sh = shell89_create();
    ctx.counter = 0;
    shell89_fixture_register_native(sh, &ctx);
    shell89_fixture_register_process(sh, path);
    CHECK_EQ(shell89_capture_run(sh, "fn state", "", 0, &out, &out_len, &err,
                                 &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(ctx.counter, 1);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);
    CHECK_EQ(sigaction(SIGUSR1, NULL, &now), 0);
    CHECK_EQ(now.sa_handler, SIG_IGN);
    CHECK(getcwd(cwd_after, sizeof cwd_after) != NULL);
    CHECK_STR(cwd_before, cwd_after);
    CHECK(fcntl(fd[0], F_GETFD) >= 0);
    CHECK(fcntl(fd[1], F_GETFD) >= 0);

    before = open_count();
    for (runs = 0; runs < 50; runs++)
    {
        CHECK_EQ(shell89_capture_run(sh, "fn copy", "abc", 3, &out, &out_len,
                                     &err, &err_len, &res),
                 SHELL89_OK);
        shell89_result_destroy(res);
        shell89_capture_free(out);
        shell89_capture_free(err);
    }
    after = open_count();
    CHECK_EQ(before, after);
    CHECK(fcntl(fd[0], F_GETFD) >= 0);
    CHECK(fcntl(fd[1], F_GETFD) >= 0);
    close(fd[0]);
    close(fd[1]);
    shell89_destroy(sh);

    CHECK_EQ(pthread_create(&bg, NULL, background_thread, NULL), 0);
    pthread_mutex_lock(&thread_lock);
    while (thread_started == 0)
    {
        pthread_mutex_unlock(&thread_lock);
        sched_yield();
        pthread_mutex_lock(&thread_lock);
    }
    pthread_mutex_unlock(&thread_lock);

    sh = shell89_create();
    ctx.counter = 0;
    shell89_fixture_register_native(sh, &ctx);
    CHECK_EQ(shell89_capture_run(sh, "fn state; fn upper", "ab", 2, &out,
                                 &out_len, &err, &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(ctx.counter, 1);
    CHECK_EQ(out_len, 8);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);
    shell89_destroy(sh);

    pthread_mutex_lock(&thread_lock);
    thread_stop = 1;
    pthread_cond_signal(&thread_cond);
    pthread_mutex_unlock(&thread_lock);
    CHECK_EQ(pthread_join(bg, NULL), 0);

    group = strdup("owned");
    op = strdup("call");
    opt_name = strdup("value");
    opts = (struct shell89_option *)malloc(2 * sizeof *opts);
    CHECK(group != NULL && op != NULL && opt_name != NULL && opts != NULL);
    opts[0].name = opt_name;
    opts[0].type = SHELL89_STRING;
    opts[0].required = 0;
    opts[1].name = NULL;
    opts[1].type = 0;
    opts[1].required = 0;
    path_copy = strdup(path);
    fixed0 = strdup("record");
    fixed1 = NULL;
    env0 = strdup("LC_ALL=C");
    fixed[0] = fixed0;
    fixed[1] = fixed1;
    env[0] = env0;
    env[1] = NULL;
    proc.path = path_copy;
    proc.fixed_argv = fixed;
    proc.envp = env;
    sh = shell89_create();
    CHECK_EQ(shell89_register_process(sh, group, op, opts, &proc), SHELL89_OK);
    free(group);
    free(op);
    free(opt_name);
    free(opts);
    free(path_copy);
    free(fixed0);
    free(env0);
    CHECK_EQ(shell89_capture_run(sh, "owned call --value=\"kept\"", "", 0, &out,
                                 &out_len, &err, &err_len, &res),
             SHELL89_OK);
    CHECK_EQ(shell89_status(res), 0);
    CHECK_EQ(out_len, strlen("value=kept\n"));
    CHECK_EQ(memcmp(out, "value=kept\n", out_len), 0);
    shell89_result_destroy(res);
    shell89_capture_free(out);
    shell89_capture_free(err);
    shell89_destroy(sh);

    for (i = 0; i < 8; i++)
    {
        jobs[i].path = path;
        jobs[i].ok = 0;
        CHECK_EQ(pthread_create(&threads[i], NULL, run_job, &jobs[i]), 0);
    }
    for (i = 0; i < 8; i++)
    {
        CHECK_EQ(pthread_join(threads[i], NULL), 0);
        CHECK_EQ(jobs[i].ok, 1);
    }

    CHECK_EQ(sigaction(SIGUSR1, &old, NULL), 0);
    return shell89_test_report("integration_embedding");
}
