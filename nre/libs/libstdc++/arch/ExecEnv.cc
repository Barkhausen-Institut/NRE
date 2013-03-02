/*
 * Copyright (C) 2012, Nils Asmussen <nils@os.inf.tu-dresden.de>
 * Economic rights: Technische Universitaet Dresden (Germany)
 *
 * This file is part of NRE (NOVA runtime environment).
 *
 * NRE is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * NRE is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 */

#include <arch/ExecEnv.h>
#include <kobj/UserSm.h>
#include <kobj/Thread.h>
#include <kobj/GlobalThread.h>
#include <util/ScopedLock.h>
#include <util/Math.h>
#include <cap/CapRange.h>
#include <Compiler.h>

namespace nre {

void ExecEnv::exit(int code) {
    // jump to that address to cause a pagefault. this way, we tell our parent that we exited
    // voluntarily with given exit code
    asm volatile (
        "jmp	*%0;"
        :
        : "r" (EXIT_START + (code & (EXIT_CODE_NUM - 1)))
    );
    UNREACHED;
}

void ExecEnv::thread_exit() {
    Thread *t = Thread::current();
    // tell our parent the stack and utcb address, if we've got it from him, via rsi and rdi and
    // announce our termination via pagefault at THREAD_EXIT. he will free the resources.
    uintptr_t stack = t->stack();
    uintptr_t utcb = reinterpret_cast<uintptr_t>(t->utcb());
    uint flags = t->flags();
    ulong id = reinterpret_cast<GlobalThread*>(t)->id();
    // we have to revoke the utcb because NOVA doesn't do so and our parent can't do it for us
    if(~flags & Thread::HAS_OWN_UTCB)
        CapRange(utcb >> ExecEnv::PAGE_SHIFT, 1, Crd::MEM_ALL).revoke(true);
    // now its safe to delete our thread object
    delete t;
    asm volatile (
        "jmp	*%0;"
        :
        : "r" (THREAD_EXIT),
        // TODO just temporary (we're limited to 4096 thread-ids)
        "S" (((~flags & Thread::HAS_OWN_STACK) ? stack : 0) | id),
        "D" ((~flags & Thread::HAS_OWN_UTCB) ? utcb : 0)
    );
    UNREACHED;
}

void *ExecEnv::setup_stack(Pd *pd, Thread *t, startup_func start, uintptr_t ret, uintptr_t stack) {
    void **sp = reinterpret_cast<void**>(stack);
    size_t stack_top = STACK_SIZE / sizeof(void*);
    sp[--stack_top] = t;
    sp[--stack_top] = pd;
    // add another word here to align the stack to 16-bytes + 8. this is necessary to get SSE
    // working. as it seems, the compiler expects that at a function-entry it is not aligned to
    // 16-byte.
    sp[--stack_top] = nullptr;
    sp[--stack_top] = reinterpret_cast<void*>(start);
    sp[--stack_top] = reinterpret_cast<void*>(ret);
    return sp + stack_top;
}

size_t ExecEnv::collect_backtrace(uintptr_t *frames, size_t max) {
    uintptr_t bp;
    asm volatile ("mov %%" EXPAND(REG(bp)) ",%0" : "=a" (bp));
    return collect_backtrace(Math::round_dn<uintptr_t>(bp, ExecEnv::STACK_SIZE), bp, frames, max);
}

size_t ExecEnv::collect_backtrace(uintptr_t stack, uintptr_t bp, uintptr_t *frames, size_t max) {
    uintptr_t end, start;
    uintptr_t *frame = frames;
    size_t count = 0;
    end = Math::round_up<uintptr_t>(bp, ExecEnv::STACK_SIZE);
    start = end - ExecEnv::STACK_SIZE;
    for(size_t i = 0; i < max - 1; i++) {
        // prevent page-fault
        if(bp < start || bp >= end)
            break;
        bp = stack + (bp & (ExecEnv::STACK_SIZE - 1));
        *frame = *(reinterpret_cast<uintptr_t*>(bp) + 1) - CALL_INSTR_SIZE;
        frame++;
        count++;
        bp = *reinterpret_cast<uintptr_t*>(bp);
    }
    // terminate
    *frame = 0;
    return count;
}

}
