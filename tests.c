#define GPC_IMPLEMENTATION
#define LFC_IMPLEMENTATION
#include "gpc.h"
#include "lfc.h"
#include <x86intrin.h>

#define DATA_LENGTH (1024ull * 1024ull)
#define QUEUE_BUF_SIZE (1024ull)

LFSPSCQueue    _queue = {0};
LFQueue(size_t) queue;

int read(void*_arr)
{
    size_t* arr =_arr;
    size_t arr_length = 0;
    while (arr_length < DATA_LENGTH) {
        size_t* data = lf_dequeue(queue, &(size_t){0});
        if (data == NULL)
            continue;
        arr[arr_length++] = *data;
    }
    return 0;
}

int write(void*_)
{
    (void)_;
    for (size_t i = 0; i < DATA_LENGTH; ++i)
        while ( ! lf_enqueue(&queue, i + 1));
    return 0;
}

int main(void)
{
    GPArena arena = gp_arena_new(1024ull * 1024ull * 1024ull);
    void* arena_start = gp_alloc(&arena, 0);
    static size_t arr[DATA_LENGTH] = {0};
    queue = lf_queue(&_queue, size_t,
        gp_alloc(&arena, QUEUE_BUF_SIZE * sizeof queue[0]),
        QUEUE_BUF_SIZE * sizeof queue[0]);

    gp_println("Starting work.");
    volatile uint64_t t0 = __rdtsc();
    thrd_t reader, writer;
    thrd_create(&writer, write, NULL);
    thrd_create(&reader, read, arr);
    thrd_join(writer, NULL);
    thrd_join(reader, NULL);
    volatile uint64_t t1 = __rdtsc();
    gp_println("Done reading.");

    for (size_t i = 0; i < DATA_LENGTH - 1; ++i)
        gp_assert(arr[i] == arr[i + 1] - 1 && arr[i] != 0, arr[i], arr[i + 1], i);
    memset(arr, 0, sizeof arr);
    gp_arena_rewind(&arena, arena_start);
    gp_println("Success. Time: %f", (double)(t1 - t0) / 1000000.);
    gp_arena_delete(&arena);
}
