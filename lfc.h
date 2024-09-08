// MIT License
// Copyright (c) 2023 Lauri Lorenzo Fiestas
// https://github.com/PrinssiFiestas/LockFreeC/blob/main/LICENSE

#ifndef LFC_H_INCLUDED
#define LFC_H_INCLUDED

#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#if ATOMIC_LLONG_LOCK_FREE == 2
typedef unsigned long long LFUint;
#else
static_assert(ATOMIC_INT_LOCK_FREE == 2, "No suitable atomics available.");
typedef unsigned LFUint;
#endif

#define LFQueue(T) T*

#define lf_queue(T,...) (T*)&LF_OVERLOAD2(__VA_ARGS__, LF_QUEUE_WITH_BUFFER, LF_QUEUE_WOUT_BUFFER,)(T,__VA_ARGS__)

#if __STDC_VERSION__ >= 202311L
#define LF_TYPEOF(...) typeof(__VA_ARGS__)
#else
#define LF_TYPEOF(...) __typeof__(__VA_ARGS__)
#endif

#define lf_enqueue(QUEUE, ELEM) lf_spsc_enqueue((LFSPSCQueue*)(QUEUE), \
    &(struct { LF_TYPEOF(*(QUEUE))_; }) { ELEM }, sizeof(ELEM))

#define LF_OVERLOAD2(_0, _1, RESOLVED, ...) RESOLVED

#define LF_QUEUE_WOUT_BUFFER(T, BUFFER_LENGTH)         (LFSPSCQueue){.buffer = (T[BUFFER_LENGTH]){0}, .buffer_length = (BUFFER_LENGTH) }
#define LF_QUEUE_WITH_BUFFER(T, BUFFER, BUFFER_LENGTH) (LFSPSCQueue){.buffer = (BUFFER),              .buffer_length = (BUFFER_LENGTH) }

#if !__cplusplus
#define LF_RESTRICT restrict
#else
#define LF_RESTRICT __restrict
#endif

typedef struct lf_spsc_queue
{
    _Alignas(64)_Atomic LFUint head;
    _Alignas(64)_Atomic LFUint tail;
    void*  buffer;
    size_t buffer_length;
} LFSPSCQueue;

static inline LFUint lf_index(LFUint x, LFUint queue_buffer_size)
{
    const bool queue_buffer_size_is_power_of_2 = (queue_buffer_size & (queue_buffer_size - 1)) == 0;
    assert(queue_buffer_size_is_power_of_2); (void)queue_buffer_size_is_power_of_2;
    assert(queue_buffer_size > 0);
    return x & (queue_buffer_size - 1);
}

static inline bool lf_spsc_enqueue(LFSPSCQueue* queue, const void*LF_RESTRICT data, size_t data_size)
{
    LFUint old_head = atomic_load_explicit(&queue->head, memory_order_acquire);
    if (lf_index(old_head, queue->buffer_length) ==
        lf_index(atomic_load_explicit(&queue->tail, memory_order_relaxed) - 1, queue->buffer_length))
        return false;

    memcpy((char*)queue->buffer + data_size * lf_index(old_head, queue->buffer_length), data, data_size);
    LFUint new_head = old_head + 1;
    atomic_store_explicit(&queue->head, new_head, memory_order_release);

    return true;
}

#define lf_dequeue(...) LF_OVERLOAD2(__VA_ARGS__, LF_DEQUEUE_WITH_BUFFER, LF_DEQUEUE_WOUT_BUFFER,)(__VA_ARGS__)
#define LF_DEQUEUE_WOUT_BUFFER(QUEUE) \
    (LF_TYPEOF(QUEUE))lf_spsc_dequeue((LFSPSCQueue*)(QUEUE), &(LF_TYPEOF(*(QUEUE))){0}, sizeof(*(QUEUE)))
#define LF_DEQUEUE_WITH_BUFFER(QUEUE, BUFFER) (LF_TYPEOF(QUEUE))lf_spsc_dequeue((LFSPSCQueue*)(QUEUE), (BUFFER), sizeof(*(QUEUE) = (BUFFER)))

static inline void* lf_spsc_dequeue(LFSPSCQueue* queue, void*LF_RESTRICT out, size_t out_size)
{
    LFUint old_tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
    if (old_tail == atomic_load_explicit(&queue->head, memory_order_relaxed))
        return NULL;

    LFUint new_tail = old_tail + 1;
    atomic_store_explicit(&queue->tail, new_tail, memory_order_release);

    memcpy(out, (char*)queue->buffer + out_size * lf_index(old_tail, queue->buffer_length), out_size);
    return out;
}

#endif // LFC_H_INCLUDED
