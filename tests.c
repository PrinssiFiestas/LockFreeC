#include "lfc.h"
#include "gpc.h"
#include <pthread.h>
#include <signal.h>
#include <x86intrin.h>

GPArena arena;

// This value is only for testing. Normally, you want a buffer size that fits in
// L3 cache. Why L3? L3 is the cache that is shared between cores so it will be
// the fastest.
#define QUEUE_BUF_SIZE (1 << 10)

#define MACRO_TEST 1
#if MACRO_TEST && !__cplusplus
LFQueue(size_t) queue = lf_queue(size_t, QUEUE_BUF_SIZE);
#elif __cplusplus
LFSPSCQueue queue = {};
#else
LFSPSCQueue queue = {0};
#endif

#define DATA_LENGTH (1024ull * 1024ull)

void* consume(void*_arr)
{
    size_t* arr = (size_t*)_arr;
    size_t arr_length = 0;
    while (arr_length < DATA_LENGTH) {
        #if __cplusplus
        size_t  data_mem;
        size_t* data = (size_t*)lf_spsc_dequeue(&queue, &data_mem, sizeof*data);
        #elif MACRO_TEST
        size_t* data = lf_dequeue(queue);
        #else
        size_t* data = lf_spsc_dequeue(&queue, &(size_t){0}, sizeof*data);
        #endif
        if (data == NULL)
            continue;
        arr[arr_length++] = *data;
    }
    return NULL;
}

void* produce(void*_)
{
    (void)_;
    for (size_t i = 0; i < DATA_LENGTH; ++i) {
        #if __cplusplus
        size_t elem = i + 1;
        while ( ! lf_spsc_enqueue(&queue, &elem, sizeof elem));
        #elif MACRO_TEST
        while ( ! lf_enqueue(queue, i + 1));
        #else
        while ( ! lf_spsc_enqueue(&queue, &(size_t){i + 1}, sizeof(size_t)));
        #endif
    }
    return NULL;
}

// Moving average for benchmark
double filter(double f)
{
    #define FLT_WINDOW (1 << 8)
    static double arr[FLT_WINDOW] = {0};
    static size_t i   = 0 ;
    static double sum = 0.;

    sum += f;
    sum -= arr[i];
    arr[i] = f;
    i = (i + 1) & (FLT_WINDOW - 1);

    return sum / FLT_WINDOW;
}

// Signal handler
bool done = false;
void be_done(int _)
{
    (void)_;
    done = true;
}

int main(void)
{
    arena = gp_arena_new(1024 * 1024 * 1024);
    size_t* arr = gp_alloc(&arena, DATA_LENGTH * sizeof arr[0]);
    #if ! MACRO_TEST || __cplusplus
    queue.buffer = gp_alloc(&arena, QUEUE_BUF_SIZE * sizeof(size_t));
    queue.buffer_length = QUEUE_BUF_SIZE;
    #endif
    signal(SIGINT, be_done);

    start:
    gp_println("Starting work.");
    volatile uint64_t t0 = __rdtsc();
    pthread_t reader, writer;
    pthread_create(&writer, NULL, produce, NULL);
    pthread_create(&reader, NULL, consume, arr);
    pthread_join(writer, NULL);
    pthread_join(reader, NULL);
    volatile uint64_t t1 = __rdtsc();

    for (size_t i = 0; i < DATA_LENGTH - 1; ++i)
        gp_assert(arr[i] == arr[i + 1] - 1 && arr[i] != 0, arr[i], arr[i + 1], i);
    memset(arr, 0, DATA_LENGTH * sizeof arr[0]);
    gp_println("Success. Time: %f", filter((double)(t1 - t0) / 1000000.));
    gp_print("\r" GP_CURSOR_UP(2));
    if ( ! done)
        goto start;
    gp_println("");
    gp_arena_delete(&arena);
    return 0;
}

