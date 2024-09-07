// MIT License
// Copyright (c) 2023 Lauri Lorenzo Fiestas
// https://github.com/PrinssiFiestas/LockFreeC/blob/main/LICENSE

#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

#ifdef __GNUC__
#define LF_NONNULL_ARGS(...) __attribute__((nonnull (__VA_ARGS__)))
#else
#define LF_NONNULL_ARGS(...)
#endif

#if __GNUG__ || _MSC_VER
#define LF_RESTRICT __restrict
#elif __cplusplus
#define LF_RESTRICT
#else
#define LF_RESTRICT restrict
#endif

#if __cplusplus
extern "C" {
#endif // __cplusplus


// ----------------------------------------------------------------------------
//
//          API REFERENCE
//
// ----------------------------------------------------------------------------


// Ideally, LFUint will be 64 bits to prevent wrapping.
#if ATOMIC_LLONG_LOCK_FREE == 2
typedef atomic_llong LFAtomicUint;
typedef unsigned long long LFUint;
#else
static_assert(ATOMIC_INT_LOCK_FREE == 2, "No suitable lock-free integers available.");
typedef atomic_uint LFAtomicUint;
typedef unsigned    LFUint;
#endif

// A ring buffer based wait-free Single Producer Single Consumer queue.
typedef struct lf_spsc_queue
{
    _Alignas(64) LFAtomicUint head;
    _Alignas(64) LFAtomicUint tail;
    void*  buffer;      // Allocated memory with size of a power of 2.
    size_t buffer_size; // In bytes. Less than 32 MB recommended to fit in L3 cache.
    size_t element_size;
} LFSPSCQueue;

LF_NONNULL_ARGS()
LFSPSCQueue* lf_spsc_queue(LFSPSCQueue* queue, size_t element_size, void* buffer, size_t buffer_size);

// If queue is full, enqueue() returns false, otherwise copies data to queue and
// returns true. If queue is empty, dequeue() returns NULL, otherwise copies
// data to out_buffer and returns it.
bool  lf_spsc_enqueue(LFSPSCQueue*, const void*LF_RESTRICT data) LF_NONNULL_ARGS();
void* lf_spsc_dequeue(LFSPSCQueue*, void*LF_RESTRICT out_buffer) LF_NONNULL_ARGS();

// Type generic and type safe ring buffer based wait-free queue.
#define LFQueue(T) T*

// Initialize LFSPSCQueue and convert to LFQueue(T).
#define lf_queue(SPSC_QUEUE_PTR, T, BUFFER, BUFFER_SIZE) \
    (T*)lf_spsc_queue(SPSC_QUEUE_PTR, sizeof(T), BUFFER, BUFFER_SIZE)

// If queue is full, enqueue() returns false, otherwise copies data to queue and
// returns true. If queue is empty, dequeue() returns NULL, otherwise copies
// data to out_buffer and returns it. In C, out_buffer is optional, the element
// returned will be allocated on stack.
#define lf_enqueue(/* LFQueue(T) */QUEUE,/* T */ELEMENT)                  LF_ENQUEUE(QUEUE, ELEMENT)
#define lf_dequeue(/* LFQueue(T) queue, void* out_buffer = &(T){0} */...) LF_DEQUEUE(__VA_ARGS__)


// ----------------------------------------------------------------------------
//
//          END OF API REFERENCE
//
//          Code below is for internal usage and may change without notice.
//
// ----------------------------------------------------------------------------


#if !__cplusplus

#if __STDC_VERSION__ >= 202311L
#define LF_TYPEOF(...) typeof(__VA_ARGS__)
#elif __clang__ || _MSC_VER
#define LF_TYPEOF(...) __typeof__(__VA_ARGS__)
#elif __GNUC__
#pragma GCC system_header
#define LF_TYPEOF(...) typeof(__VA_ARGS__)
#endif

#define LF_ENQUEUE(QUEUE, ELEM) lf_spsc_enqueue( \
    (LFSPSCQueue*)(QUEUE), &(struct { LF_TYPEOF(ELEM) elem; }){ ELEM })

#define LF_OVERLOAD2(_0, _1, RESOLVED, ...) RESOLVED
#define LF_DEQUEUE1(QUEUE)      lf_spsc_dequeue((LFSPSCQueue*)(QUEUE), &(LF_TYPEOF(*(QUEUE))){0})
#define LF_DEQUEUE2(QUEUE, OUT) lf_spsc_dequeue((LFSPSCQueue*)(QUEUE), OUT)
#define LF_DEQUEUE(...) LF_OVERLOAD2(__VA_ARGS__, LF_DEQUEUE2, LF_DEQUEUE1)(__VA_ARGS__)

#else // C++
} // extern "C"

template <typename T>
static inline bool LF_ENQUEUE(LFQueue(T) queue, const T& elem)
{
    return lf_spsc_enqueue((LFSPSCQueue*)queue, &elem);
}

template <typename T>
static inline T* LF_DEQUEUE(LFQueue(T) queue, T* out)
{
    return lf_spsc_dequeue((LFSPSCQueue*)queue, out);
}

#endif

// ----------------------------------------------------------------------------

#ifdef LFC_IMPLEMENTATION

LFSPSCQueue* lf_spsc_queue(LFSPSCQueue* queue, size_t element_size, void* buffer, size_t buffer_size)
{
    #ifndef NDEBUG
    const bool queue_buffer_size_is_power_of_2 =
        (queue->buffer_size & (queue->buffer_size - 1)) == 0;
    assert(queue_buffer_size_is_power_of_2);
    assert(element_size > 0 && buffer_size > 0);
    #endif
    memset(queue, 0, sizeof *queue);
    queue->buffer       = buffer;
    queue->buffer_size  = buffer_size;
    queue->element_size = element_size;
    return queue;
}

LF_NONNULL_ARGS()
static inline LFUint lf_index(const LFSPSCQueue* queue, const LFUint i)
{
    return i & (queue->buffer_size - 1);
}

bool lf_spsc_enqueue(LFSPSCQueue*LF_RESTRICT queue, const void*LF_RESTRICT data)
{
    const LFUint old_head = atomic_load_explicit(&queue->head, memory_order_acquire);
    const LFUint i = lf_index(queue, old_head);
    if (i == lf_index(queue, atomic_load_explicit(&queue->tail, memory_order_relaxed) - 1))
        return false;
    memcpy((char*)queue->buffer + i * queue->element_size, data, queue->element_size);
    const LFUint new_head = old_head + 1;
    atomic_store_explicit(&queue->head, new_head, memory_order_release);
    return true;
}

void* lf_spsc_dequeue(LFSPSCQueue*LF_RESTRICT queue, void*LF_RESTRICT out_buffer)
{
    const LFUint old_tail = atomic_load_explicit(&queue->tail, memory_order_acquire);
    if (old_tail == atomic_load_explicit(&queue->head, memory_order_relaxed))
        return NULL;
    memcpy(out_buffer,
        (char*)queue->buffer + lf_index(queue, old_tail) * queue->element_size,
        queue->element_size);
    const LFUint new_tail = old_tail + 1;
    atomic_store_explicit(&queue->tail, new_tail, memory_order_release);
    return out_buffer;
}

#endif // LFC_IMPLEMENTATION
