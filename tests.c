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

Queue queue = {0};

#define DATA_LENGTH (1024ull * 1024ull)

void* consume(void*_arr)
{
    size_t* arr =_arr;
    size_t arr_length = 0;
    while (arr_length < DATA_LENGTH) {
        size_t* data = lf_dequeue(&queue, &(size_t){0}, sizeof(size_t));
        if (data == NULL)
            continue;
        arr[arr_length++] = *data;
    }
    return NULL;
}

void* produce(void*_)
{
    (void)_;
    for (size_t i = 0; i < DATA_LENGTH; ++i)
        while ( ! lf_enqueue(&queue, &(size_t){i + 1}, sizeof(size_t)));
    return NULL;
}

double filter(double f)
{
    #define FLT_WINDOW (1 << 8)
    static double arr[FLT_WINDOW] = {0};
    static size_t i = 0;

    arr[i] = f;
    i = (i + 1) & (FLT_WINDOW - 1);

    double sum = 0;
    for (size_t j = 0; j < FLT_WINDOW; ++j)
        sum += arr[j];
    return sum / FLT_WINDOW;
}

bool done = false;
void be_done(int _)
{
    (void)_;
    done = true;
}

int main(void)
{
    arena = gp_arena_new(1024 * 1024 * 1024);
    void* arena_start = gp_alloc(&arena, 0);
    static size_t arr[DATA_LENGTH] = {0};
    queue.buf = gp_alloc(&arena, QUEUE_BUF_SIZE * sizeof(size_t));
    queue.buf_length = QUEUE_BUF_SIZE;
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
    memset(arr, 0, sizeof arr);
    gp_arena_rewind(&arena, arena_start);
    gp_println("Success. Time: %f", filter((double)(t1 - t0) / 1000000.));
    gp_print("\r" GP_CURSOR_UP(2));
    if ( ! done)
        goto start;
    gp_println("");
    gp_arena_delete(&arena);
    return 0;
}

