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

#if !__cplusplus
#define LF_RESTRICT restrict
#else
#define LF_RESTRICT __restrict
#endif

typedef struct lf_queue
{
    _Alignas(64)_Atomic LFUint head;
    _Alignas(64)_Atomic LFUint tail;
    void*  buf;
    size_t buf_length;
} LFQueue;

static inline LFUint lf_index(LFUint x, LFUint queue_buffer_size)
{
    const bool queue_buffer_size_is_power_of_2 = (queue_buffer_size & (queue_buffer_size - 1)) == 0;
    assert(queue_buffer_size_is_power_of_2); (void)queue_buffer_size_is_power_of_2;
    assert(queue_buffer_size > 0);
    return x & (queue_buffer_size - 1);
}

static inline bool lf_enqueue(LFQueue* queue, const void*LF_RESTRICT data, size_t data_size)
{
    LFUint old_head = atomic_load_explicit(&queue->head, memory_order_acquire);
    if (lf_index(old_head, queue->buf_length) ==
        lf_index(atomic_load_explicit(&queue->tail, memory_order_relaxed) - 1, queue->buf_length))
        return false;

    memcpy((char*)queue->buf + data_size * lf_index(old_head, queue->buf_length), data, data_size);
    LFUint new_head = old_head + 1;
    atomic_store_explicit(&queue->head, new_head, memory_order_release);

    return true;
}

static inline void* lf_dequeue(LFQueue* queue, void*LF_RESTRICT out, size_t out_size)
{
    LFUint old_tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
    if (old_tail == atomic_load_explicit(&queue->head, memory_order_relaxed))
        return NULL;

    LFUint new_tail = old_tail + 1;
    atomic_store_explicit(&queue->tail, new_tail, memory_order_release);

    memcpy(out, (char*)queue->buf + out_size * lf_index(old_tail, queue->buf_length), out_size);
    return out;
}

#endif // LFC_H_INCLUDED
