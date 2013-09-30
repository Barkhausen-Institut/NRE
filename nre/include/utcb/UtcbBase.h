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

#include <arch/ExecEnv.h>
#include <utcb/UtcbHead.h>

namespace nre {

class UtcbFrameRef;
class OStream;

OStream &operator<<(OStream &os, const UtcbFrameRef &frm);

/**
 * The base class for the two variants of the UTCB that exist currently.
 */
class UtcbBase : public UtcbHead {
    friend class UtcbFrameRef;
    friend class UtcbFrame;
    friend OStream & operator<<(OStream &os, const UtcbFrameRef &frm);

public:
    static const size_t SIZE        = ExecEnv::PAGE_SIZE;

protected:
    static const size_t WORDS       = SIZE / sizeof(word_t);

    char msg[(SIZE - sizeof(UtcbHead))];

    // no construction and copying
    UtcbBase();
    ~UtcbBase();
    UtcbBase(const UtcbBase&);
    UtcbBase& operator=(const UtcbBase&);

public:
    /**
     * Resets the UTCB, i.e. clears the typed and untyped items and resets the receive windows
     */
    void reset() {
        mtr = 0;
        crd = 0;
        crd_translate = 0;
    }
};

}
