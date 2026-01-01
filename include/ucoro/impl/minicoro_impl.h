/*
 * ucoro_impl.h - Full implementation of ucoro
 * Original: Eduardo Bart (https://github.com/edubart/ucoro)
 * License: Public Domain or MIT No Attribution
 *
 * This is a simplified x86_64 Linux/macOS implementation for demonstration.
 * For production use, get the full library from the original repository.
 */

#ifndef UCORO_IMPL_H
#define UCORO_IMPL_H

#include "../include/ucoro/minicoro.h"
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* Minimum stack size */
#ifndef MCO_MIN_STACK_SIZE
#define MCO_MIN_STACK_SIZE 32768
#endif

/* Default stack size */
#ifndef MCO_DEFAULT_STACK_SIZE
#define MCO_DEFAULT_STACK_SIZE (56 * 1024)
#endif

/* Magic number for stack overflow detection */
#define MCO_MAGIC_NUMBER 0x7E3CB1A9

/* Thread local for current coroutine */
#ifndef MCO_NO_MULTITHREAD
#ifdef __GNUC__
#define MCO_THREAD_LOCAL __thread
#else
#define MCO_THREAD_LOCAL
#endif
#else
#define MCO_THREAD_LOCAL
#endif

/* Force inline */
#ifdef __GNUC__
#define MCO_FORCE_INLINE inline __attribute__((always_inline))
#define MCO_NO_INLINE __attribute__((noinline))
#else
#define MCO_FORCE_INLINE inline
#define MCO_NO_INLINE
#endif

    /* Current running coroutine */
    static MCO_THREAD_LOCAL mco_coro *mco_current_co = NULL;

    /* Align forward utility */
    static MCO_FORCE_INLINE size_t mco_align_forward(size_t addr, size_t align)
    {
        return (addr + (align - 1)) & ~(align - 1);
    }

    /* Default allocator */
    static void *mco_alloc(size_t size, void *allocator_data)
    {
        (void)allocator_data;
        return calloc(1, size);
    }

    static void mco_dealloc(void *ptr, size_t size, void *allocator_data)
    {
        (void)size;
        (void)allocator_data;
        free(ptr);
    }

/* x86_64 context buffer (Linux/macOS) */
#if defined(__x86_64__) && !defined(_WIN32)

    typedef struct mco_ctxbuf
    {
        void *rip, *rsp, *rbp, *rbx, *r12, *r13, *r14, *r15;
    } mco_ctxbuf;

    typedef struct mco_context
    {
        mco_ctxbuf ctx;
        mco_ctxbuf back_ctx;
    } mco_context;

    /* Assembly functions */
    void _mco_wrap_main(void);
    int _mco_switch(mco_ctxbuf *from, mco_ctxbuf *to);

    /* Main wrapper - calls the coroutine function */
    __asm__(
        ".text\n"
#ifdef __MACH__
        ".globl __mco_wrap_main\n"
        "__mco_wrap_main:\n"
#else
        ".globl _mco_wrap_main\n"
        ".type _mco_wrap_main @function\n"
        ".hidden _mco_wrap_main\n"
        "_mco_wrap_main:\n"
#endif
        "  movq %r13, %rdi\n"
        "  jmpq *%r12\n"
#ifndef __MACH__
        ".size _mco_wrap_main, .-_mco_wrap_main\n"
#endif
    );

    /* Context switch */
    __asm__(
        ".text\n"
#ifdef __MACH__
        ".globl __mco_switch\n"
        "__mco_switch:\n"
#else
        ".globl _mco_switch\n"
        ".type _mco_switch @function\n"
        ".hidden _mco_switch\n"
        "_mco_switch:\n"
#endif
        "  leaq 0x3d(%rip), %rax\n"
        "  movq %rax, (%rdi)\n"
        "  movq %rsp, 8(%rdi)\n"
        "  movq %rbp, 16(%rdi)\n"
        "  movq %rbx, 24(%rdi)\n"
        "  movq %r12, 32(%rdi)\n"
        "  movq %r13, 40(%rdi)\n"
        "  movq %r14, 48(%rdi)\n"
        "  movq %r15, 56(%rdi)\n"
        "  movq 56(%rsi), %r15\n"
        "  movq 48(%rsi), %r14\n"
        "  movq 40(%rsi), %r13\n"
        "  movq 32(%rsi), %r12\n"
        "  movq 24(%rsi), %rbx\n"
        "  movq 16(%rsi), %rbp\n"
        "  movq 8(%rsi), %rsp\n"
        "  jmpq *(%rsi)\n"
        "  ret\n"
#ifndef __MACH__
        ".size _mco_switch, .-_mco_switch\n"
#endif
    );

    /* Forward declarations */
    static void mco_main(mco_coro *co);

    static mco_result mco_makectx(mco_coro *co, mco_ctxbuf *ctx, void *stack_base, size_t stack_size)
    {
        stack_size = stack_size - 128; /* Red Zone */
        void **stack_high_ptr = (void **)((size_t)stack_base + stack_size - sizeof(size_t));
        stack_high_ptr[0] = (void *)(0xdeaddeaddeaddead);
        ctx->rip = (void *)(_mco_wrap_main);
        ctx->rsp = (void *)(stack_high_ptr);
        ctx->r12 = (void *)(mco_main);
        ctx->r13 = (void *)(co);
        return MCO_SUCCESS;
    }

#else
/* Fallback using ucontext for other platforms */
#include <ucontext.h>

typedef ucontext_t mco_ctxbuf;

typedef struct mco_context
{
    mco_ctxbuf ctx;
    mco_ctxbuf back_ctx;
} mco_context;

static void mco_main(mco_coro *co);

#if defined(_LP64) || defined(__LP64__)
static void mco_wrap_main(unsigned int lo, unsigned int hi)
{
    mco_coro *co = (mco_coro *)(((size_t)lo) | (((size_t)hi) << 32));
    mco_main(co);
}
#else
static void mco_wrap_main(unsigned int lo)
{
    mco_coro *co = (mco_coro *)((size_t)lo);
    mco_main(co);
}
#endif

static MCO_FORCE_INLINE void _mco_switch(mco_ctxbuf *from, mco_ctxbuf *to)
{
    swapcontext(from, to);
}

static mco_result mco_makectx(mco_coro *co, mco_ctxbuf *ctx, void *stack_base, size_t stack_size)
{
    if (getcontext(ctx) != 0)
    {
        return MCO_MAKE_CONTEXT_ERROR;
    }
    ctx->uc_link = NULL;
    ctx->uc_stack.ss_sp = stack_base;
    ctx->uc_stack.ss_size = stack_size;
    unsigned int lo = (unsigned int)((size_t)co);
#if defined(_LP64) || defined(__LP64__)
    unsigned int hi = (unsigned int)(((size_t)co) >> 32);
    makecontext(ctx, (void (*)(void))mco_wrap_main, 2, lo, hi);
#else
    makecontext(ctx, (void (*)(void))mco_wrap_main, 1, lo);
#endif
    return MCO_SUCCESS;
}

#endif /* Platform detection */

    /* Prepare jump in */
    static MCO_FORCE_INLINE void mco_prepare_jumpin(mco_coro *co)
    {
        mco_coro *prev_co = mco_current_co;
        co->prev_co = prev_co;
        if (prev_co)
        {
            prev_co->state = MCO_NORMAL;
        }
        mco_current_co = co;
    }

    /* Prepare jump out */
    static MCO_FORCE_INLINE void mco_prepare_jumpout(mco_coro *co)
    {
        mco_coro *prev_co = co->prev_co;
        co->prev_co = NULL;
        if (prev_co)
        {
            prev_co->state = MCO_RUNNING;
        }
        mco_current_co = prev_co;
    }

    /* Main coroutine function */
    static MCO_NO_INLINE void mco_main(mco_coro *co)
    {
        co->func(co);
        co->state = MCO_DEAD;
        mco_context *context = (mco_context *)co->context;
        mco_prepare_jumpout(co);
        _mco_switch(&context->ctx, &context->back_ctx);
    }

    /* Jump into coroutine */
    static void mco_jumpin(mco_coro *co)
    {
        mco_context *context = (mco_context *)co->context;
        mco_prepare_jumpin(co);
        _mco_switch(&context->back_ctx, &context->ctx);
    }

    /* Jump out of coroutine */
    static void mco_jumpout(mco_coro *co)
    {
        mco_context *context = (mco_context *)co->context;
        mco_prepare_jumpout(co);
        _mco_switch(&context->ctx, &context->back_ctx);
    }

    /* Calculate sizes */
    static MCO_FORCE_INLINE void mco_init_desc_sizes(mco_desc *desc, size_t stack_size)
    {
        desc->coro_size = mco_align_forward(sizeof(mco_coro), 16) +
                          mco_align_forward(sizeof(mco_context), 16) +
                          mco_align_forward(desc->storage_size, 16) +
                          stack_size + 16;
        desc->stack_size = stack_size;
    }

    /* Create context */
    static mco_result mco_create_context(mco_coro *co, mco_desc *desc)
    {
        size_t co_addr = (size_t)co;
        size_t context_addr = mco_align_forward(co_addr + sizeof(mco_coro), 16);
        size_t storage_addr = mco_align_forward(context_addr + sizeof(mco_context), 16);
        size_t stack_addr = mco_align_forward(storage_addr + desc->storage_size, 16);

        mco_context *context = (mco_context *)context_addr;
        memset(context, 0, sizeof(mco_context));

        unsigned char *storage = (unsigned char *)storage_addr;
        void *stack_base = (void *)stack_addr;
        size_t stack_size = desc->stack_size;

        mco_result res = mco_makectx(co, &context->ctx, stack_base, stack_size);
        if (res != MCO_SUCCESS)
        {
            return res;
        }

        co->context = context;
        co->stack_base = stack_base;
        co->stack_size = stack_size;
        co->storage = storage;
        co->storage_size = desc->storage_size;
        return MCO_SUCCESS;
    }

    /* Public API Implementation */

    MCO_API mco_desc mco_desc_init(void (*func)(mco_coro *co), size_t stack_size)
    {
        if (stack_size != 0)
        {
            if (stack_size < MCO_MIN_STACK_SIZE)
            {
                stack_size = MCO_MIN_STACK_SIZE;
            }
        }
        else
        {
            stack_size = MCO_DEFAULT_STACK_SIZE;
        }
        stack_size = mco_align_forward(stack_size, 16);

        mco_desc desc;
        memset(&desc, 0, sizeof(mco_desc));
        desc.alloc_cb = mco_alloc;
        desc.dealloc_cb = mco_dealloc;
        desc.func = func;
        desc.storage_size = MCO_DEFAULT_STORAGE_SIZE;
        mco_init_desc_sizes(&desc, stack_size);
        return desc;
    }

    MCO_API mco_result mco_init(mco_coro *co, mco_desc *desc)
    {
        if (!co)
            return MCO_INVALID_COROUTINE;
        if (!desc || !desc->func)
            return MCO_INVALID_ARGUMENTS;
        if (desc->stack_size < MCO_MIN_STACK_SIZE)
            return MCO_INVALID_ARGUMENTS;

        memset(co, 0, sizeof(mco_coro));

        mco_result res = mco_create_context(co, desc);
        if (res != MCO_SUCCESS)
            return res;

        co->state = MCO_SUSPENDED;
        co->dealloc_cb = desc->dealloc_cb;
        co->coro_size = desc->coro_size;
        co->allocator_data = desc->allocator_data;
        co->func = desc->func;
        co->user_data = desc->user_data;
        co->magic_number = MCO_MAGIC_NUMBER;
        return MCO_SUCCESS;
    }

    MCO_API mco_result mco_uninit(mco_coro *co)
    {
        if (!co)
            return MCO_INVALID_COROUTINE;
        if (!(co->state == MCO_SUSPENDED || co->state == MCO_DEAD))
        {
            return MCO_INVALID_OPERATION;
        }
        co->state = MCO_DEAD;
        return MCO_SUCCESS;
    }

    MCO_API mco_result mco_create(mco_coro **out_co, mco_desc *desc)
    {
        if (!out_co)
            return MCO_INVALID_POINTER;
        if (!desc || !desc->alloc_cb || !desc->dealloc_cb)
        {
            *out_co = NULL;
            return MCO_INVALID_ARGUMENTS;
        }

        mco_coro *co = (mco_coro *)desc->alloc_cb(desc->coro_size, desc->allocator_data);
        if (!co)
        {
            *out_co = NULL;
            return MCO_OUT_OF_MEMORY;
        }

        mco_result res = mco_init(co, desc);
        if (res != MCO_SUCCESS)
        {
            desc->dealloc_cb(co, desc->coro_size, desc->allocator_data);
            *out_co = NULL;
            return res;
        }

        *out_co = co;
        return MCO_SUCCESS;
    }

    MCO_API mco_result mco_destroy(mco_coro *co)
    {
        if (!co)
            return MCO_INVALID_COROUTINE;

        mco_result res = mco_uninit(co);
        if (res != MCO_SUCCESS)
            return res;

        if (!co->dealloc_cb)
            return MCO_INVALID_POINTER;
        co->dealloc_cb(co, co->coro_size, co->allocator_data);
        return MCO_SUCCESS;
    }

    MCO_API mco_result mco_resume(mco_coro *co)
    {
        if (!co)
            return MCO_INVALID_COROUTINE;
        if (co->state != MCO_SUSPENDED)
            return MCO_NOT_SUSPENDED;

        co->state = MCO_RUNNING;
        mco_jumpin(co);
        return MCO_SUCCESS;
    }

    MCO_API mco_result mco_yield(mco_coro *co)
    {
        if (!co)
            return MCO_INVALID_COROUTINE;

        /* Stack overflow check */
        volatile size_t dummy;
        size_t stack_addr = (size_t)&dummy;
        size_t stack_min = (size_t)co->stack_base;
        size_t stack_max = stack_min + co->stack_size;
        if (co->magic_number != MCO_MAGIC_NUMBER || stack_addr < stack_min || stack_addr > stack_max)
        {
            return MCO_STACK_OVERFLOW;
        }

        if (co->state != MCO_RUNNING)
            return MCO_NOT_RUNNING;

        co->state = MCO_SUSPENDED;
        mco_jumpout(co);
        return MCO_SUCCESS;
    }

    MCO_API mco_state mco_status(mco_coro *co)
    {
        return co ? co->state : MCO_DEAD;
    }

    MCO_API void *mco_get_user_data(mco_coro *co)
    {
        return co ? co->user_data : NULL;
    }

    MCO_API mco_result mco_push(mco_coro *co, const void *src, size_t len)
    {
        if (!co)
            return MCO_INVALID_COROUTINE;
        if (len > 0)
        {
            size_t bytes_stored = co->bytes_stored + len;
            if (bytes_stored > co->storage_size)
                return MCO_NOT_ENOUGH_SPACE;
            if (!src)
                return MCO_INVALID_POINTER;
            memcpy(&co->storage[co->bytes_stored], src, len);
            co->bytes_stored = bytes_stored;
        }
        return MCO_SUCCESS;
    }

    MCO_API mco_result mco_pop(mco_coro *co, void *dest, size_t len)
    {
        if (!co)
            return MCO_INVALID_COROUTINE;
        if (len > 0)
        {
            if (len > co->bytes_stored)
                return MCO_NOT_ENOUGH_SPACE;
            size_t bytes_stored = co->bytes_stored - len;
            if (dest)
            {
                memcpy(dest, &co->storage[bytes_stored], len);
            }
            co->bytes_stored = bytes_stored;
        }
        return MCO_SUCCESS;
    }

    MCO_API mco_result mco_peek(mco_coro *co, void *dest, size_t len)
    {
        if (!co)
            return MCO_INVALID_COROUTINE;
        if (len > 0)
        {
            if (len > co->bytes_stored)
                return MCO_NOT_ENOUGH_SPACE;
            if (!dest)
                return MCO_INVALID_POINTER;
            memcpy(dest, &co->storage[co->bytes_stored - len], len);
        }
        return MCO_SUCCESS;
    }

    MCO_API size_t mco_get_bytes_stored(mco_coro *co)
    {
        return co ? co->bytes_stored : 0;
    }

    MCO_API size_t mco_get_storage_size(mco_coro *co)
    {
        return co ? co->storage_size : 0;
    }

    MCO_API mco_coro *mco_running(void)
    {
        return mco_current_co;
    }

    MCO_API const char *mco_result_description(mco_result res)
    {
        switch (res)
        {
        case MCO_SUCCESS:
            return "No error";
        case MCO_GENERIC_ERROR:
            return "Generic error";
        case MCO_INVALID_POINTER:
            return "Invalid pointer";
        case MCO_INVALID_COROUTINE:
            return "Invalid coroutine";
        case MCO_NOT_SUSPENDED:
            return "Coroutine not suspended";
        case MCO_NOT_RUNNING:
            return "Coroutine not running";
        case MCO_MAKE_CONTEXT_ERROR:
            return "Make context error";
        case MCO_SWITCH_CONTEXT_ERROR:
            return "Switch context error";
        case MCO_NOT_ENOUGH_SPACE:
            return "Not enough space";
        case MCO_OUT_OF_MEMORY:
            return "Out of memory";
        case MCO_INVALID_ARGUMENTS:
            return "Invalid arguments";
        case MCO_INVALID_OPERATION:
            return "Invalid operation";
        case MCO_STACK_OVERFLOW:
            return "Stack overflow";
        }
        return "Unknown error";
    }

#ifdef __cplusplus
}
#endif

#endif /* UCORO_IMPL_H */
