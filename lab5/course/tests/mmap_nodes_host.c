/* Host test for the real node-pool code; kernel spinlocks are mocked. */
#include "src/kernel/arch/type.h"
#include "src/kernel/mem/type.h"
#include <pthread.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define WORKERS 8
#define LOOPS 4000

void mmap_init(void);
mmap_region_t *mmap_region_alloc(void);
void mmap_region_free(mmap_region_t *node);

static pthread_mutex_t pool_guard = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t ownership_guard = PTHREAD_MUTEX_INITIALIZER;
static mmap_region_t *all_nodes[N_MMAP];
static int claimed[N_MMAP];

void spinlock_init(spinlock_t *lock, char *name) { (void)lock; (void)name; }
void spinlock_acquire(spinlock_t *lock) { (void)lock; pthread_mutex_lock(&pool_guard); }
void spinlock_release(spinlock_t *lock) { (void)lock; pthread_mutex_unlock(&pool_guard); }
void panic(const char *message) { (void)message; _Exit(91); }
void assert(bool condition, const char *message) { if (!condition) panic(message); }

static int expect_panic(mmap_region_t *node)
{
    pid_t child = fork();
    if (child < 0) return 0;
    if (child == 0) { mmap_region_free(node); _Exit(0); }
    int status;
    return waitpid(child, &status, 0) == child && WIFEXITED(status) &&
           WEXITSTATUS(status) == 91;
}

static void *worker(void *arg)
{
    uint64 value = (uint64)arg + 1;
    for (int i = 0; i < LOOPS; i++) {
        mmap_region_t *node = mmap_region_alloc();
        if (!node) return (void *)1;
        pthread_mutex_lock(&ownership_guard);
        int index = -1;
        for (int j = 0; j < N_MMAP; j++) {
            if (all_nodes[j] == node) { index = j; break; }
        }
        if (index < 0 || claimed[index]) {
            pthread_mutex_unlock(&ownership_guard);
            return (void *)1;
        }
        claimed[index] = 1;
        pthread_mutex_unlock(&ownership_guard);

        if (node->begin || node->npages || node->perm || node->next) return (void *)1;
        node->begin = value;
        node->npages = 1;
        node->perm = PTE_R;
        if ((i & 63) == 0) sched_yield();
        if (node->begin != value || node->npages != 1) return (void *)1;

        pthread_mutex_lock(&ownership_guard);
        claimed[index] = 0;
        pthread_mutex_unlock(&ownership_guard);
        mmap_region_free(node);
    }
    return NULL;
}

int main(void)
{
    mmap_init();
    for (int i = 0; i < N_MMAP; i++) {
        all_nodes[i] = mmap_region_alloc();
        if (!all_nodes[i]) return 1;
        if (all_nodes[i]->begin || all_nodes[i]->npages || all_nodes[i]->perm || all_nodes[i]->next) return 2;
        for (int j = 0; j < i; j++) if (all_nodes[j] == all_nodes[i]) return 3;
    }
    if (mmap_region_alloc() != NULL) return 4;

    all_nodes[0]->begin = 0x1234;
    all_nodes[0]->npages = 7;
    all_nodes[0]->next = all_nodes[0];
    mmap_region_free(all_nodes[0]);
    mmap_region_t *reused = mmap_region_alloc();
    if (reused != all_nodes[0] || reused->begin || reused->npages || reused->next) return 5;
    for (int i = 0; i < N_MMAP; i++) mmap_region_free(all_nodes[i]);

    mmap_region_t outside;
    if (!expect_panic(NULL) || !expect_panic(&outside) ||
        !expect_panic((mmap_region_t *)((char *)all_nodes[1] + 1)) ||
        !expect_panic(all_nodes[0])) return 6;

    pthread_t threads[WORKERS];
    for (uint64 i = 0; i < WORKERS; i++)
        if (pthread_create(&threads[i], NULL, worker, (void *)i)) return 7;
    for (int i = 0; i < WORKERS; i++) {
        void *result;
        if (pthread_join(threads[i], &result) || result) return 8;
    }

    for (int i = 0; i < N_MMAP; i++) {
        mmap_region_t *node = mmap_region_alloc();
        if (!node || node->begin || node->npages || node->next) return 9;
        for (int j = 0; j < i; j++) if (all_nodes[j] == node) return 10;
        all_nodes[i] = node;
    }
    if (mmap_region_alloc() != NULL) return 11;
    for (int i = 0; i < N_MMAP; i++) mmap_region_free(all_nodes[i]);
    return 0;
}
