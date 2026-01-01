/*
Minimal asymmetric stackful coroutine library in C - cleaned up for C++23 integration
Original: Eduardo Bart - https://github.com/edubart/ucoro
*/

#ifndef UCORO_H
#define UCORO_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef MCO_API
#define MCO_API extern
#endif

#ifndef MCO_DEFAULT_STORAGE_SIZE
#define MCO_DEFAULT_STORAGE_SIZE 1024
#endif

#include <stddef.h>

  typedef enum mco_state
  {
    MCO_DEAD = 0,
    MCO_NORMAL,
    MCO_RUNNING,
    MCO_SUSPENDED
  } mco_state;

  typedef enum mco_result
  {
    MCO_SUCCESS = 0,
    MCO_GENERIC_ERROR,
    MCO_INVALID_POINTER,
    MCO_INVALID_COROUTINE,
    MCO_NOT_SUSPENDED,
    MCO_NOT_RUNNING,
    MCO_MAKE_CONTEXT_ERROR,
    MCO_SWITCH_CONTEXT_ERROR,
    MCO_NOT_ENOUGH_SPACE,
    MCO_OUT_OF_MEMORY,
    MCO_INVALID_ARGUMENTS,
    MCO_INVALID_OPERATION,
    MCO_STACK_OVERFLOW
  } mco_result;

  typedef struct mco_coro mco_coro;
  struct mco_coro
  {
    void *context;
    mco_state state;
    void (*func)(mco_coro *co);
    mco_coro *prev_co;
    void *user_data;
    size_t coro_size;
    void *allocator_data;
    void (*dealloc_cb)(void *ptr, size_t size, void *allocator_data);
    void *stack_base;
    size_t stack_size;
    unsigned char *storage;
    size_t bytes_stored;
    size_t storage_size;
    void *asan_prev_stack;
    void *tsan_prev_fiber;
    void *tsan_fiber;
    size_t magic_number;
  };

  typedef struct mco_desc
  {
    void (*func)(mco_coro *co);
    void *user_data;
    void *(*alloc_cb)(size_t size, void *allocator_data);
    void (*dealloc_cb)(void *ptr, size_t size, void *allocator_data);
    void *allocator_data;
    size_t storage_size;
    size_t coro_size;
    size_t stack_size;
  } mco_desc;

  MCO_API mco_desc mco_desc_init(void (*func)(mco_coro *co), size_t stack_size);
  MCO_API mco_result mco_init(mco_coro *co, mco_desc *desc);
  MCO_API mco_result mco_uninit(mco_coro *co);
  MCO_API mco_result mco_create(mco_coro **out_co, mco_desc *desc);
  MCO_API mco_result mco_destroy(mco_coro *co);
  MCO_API mco_result mco_resume(mco_coro *co);
  MCO_API mco_result mco_yield(mco_coro *co);
  MCO_API mco_state mco_status(mco_coro *co);
  MCO_API void *mco_get_user_data(mco_coro *co);
  MCO_API mco_result mco_push(mco_coro *co, const void *src, size_t len);
  MCO_API mco_result mco_pop(mco_coro *co, void *dest, size_t len);
  MCO_API mco_result mco_peek(mco_coro *co, void *dest, size_t len);
  MCO_API size_t mco_get_bytes_stored(mco_coro *co);
  MCO_API size_t mco_get_storage_size(mco_coro *co);
  MCO_API mco_coro *mco_running(void);
  MCO_API const char *mco_result_description(mco_result res);

#ifdef __cplusplus
}
#endif

#endif /* UCORO_H */
