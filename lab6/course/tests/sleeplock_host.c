/* Host-level behavior test. It does not replace in-kernel scheduler testing. */
#include "src/kernel/lock/mod.h"
#include "src/kernel/proc/method.h"
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>

static pthread_mutex_t mock_guard = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t mock_channel = PTHREAD_COND_INITIALIZER;
static _Thread_local proc_t current;
static int sleeping;
static int worker_succeeded;
static sleeplock_t lock_under_test;

proc_t *myproc(void) { return &current; }
void assert(bool condition, const char *warning)
{
    (void)warning;
    if (!condition) _Exit(91);
}
void spinlock_init(spinlock_t *lock, char *name) { (void)lock; (void)name; }
void spinlock_acquire(spinlock_t *lock) { (void)lock; pthread_mutex_lock(&mock_guard); }
void spinlock_release(spinlock_t *lock) { (void)lock; pthread_mutex_unlock(&mock_guard); }
void proc_sleep(void *channel, spinlock_t *lock)
{
    (void)channel; (void)lock;
    __atomic_store_n(&sleeping, 1, __ATOMIC_RELEASE);
    pthread_cond_wait(&mock_channel, &mock_guard);
}
void proc_wakeup(void *channel) { (void)channel; pthread_cond_broadcast(&mock_channel); }
static void *worker(void *unused)
{
    (void)unused;
    current.pid = 2;
    sleeplock_acquire(&lock_under_test);
    worker_succeeded = sleeplock_holding(&lock_under_test);
    sleeplock_release(&lock_under_test);
    return NULL;
}
int main(void)
{
    current.pid = 1;
    sleeplock_init(&lock_under_test, "test");
    if (sleeplock_holding(&lock_under_test)) return 1;
    sleeplock_acquire(&lock_under_test);
    if (!sleeplock_holding(&lock_under_test)) return 2;
    pid_t child = fork();
    if (child < 0) return 3;
    if (child == 0) { current.pid = 2; sleeplock_release(&lock_under_test); _Exit(0); }
    int status;
    if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 91) return 4;
    pthread_t thread;
    if (pthread_create(&thread, NULL, worker, NULL)) return 5;
    while (!__atomic_load_n(&sleeping, __ATOMIC_ACQUIRE)) usleep(1000);
    if (!sleeplock_holding(&lock_under_test)) return 6;
    sleeplock_release(&lock_under_test);
    if (pthread_join(thread, NULL) || !worker_succeeded) return 7;
    if (sleeplock_holding(&lock_under_test) || lock_under_test.locked) return 8;
    return 0;
}
