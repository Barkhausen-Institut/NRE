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

#pragma once

#ifdef __i386__
#    include <arch/x86_32/Defines.h>
#elif defined __x86_64__
#    include <arch/x86_64/Defines.h>
#else
#    error "Unsupported architecture"
#endif

#if !defined(__GXX_EXPERIMENTAL_CXX0X__) || !defined(__cplusplus)
#   define nullptr             0
#endif
#define STRING(x)               # x
#define EXPAND(x)               STRING(x)
#define ARRAY_SIZE(X)           (sizeof((X)) / sizeof((X)[0]))

/**
 * Whether the changes that have been done in the NRE-branch of NOVA exist
 */
#define HAVE_KERNEL_EXTENSIONS  1
