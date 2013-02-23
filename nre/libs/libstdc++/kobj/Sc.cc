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

#include <kobj/Sc.h>
#include <kobj/Pt.h>
#include <kobj/UserSm.h>
#include <utcb/UtcbFrame.h>
#include <util/ScopedCapSels.h>

namespace nre {

static UserSm sm;

Sc::~Sc() {
    ScopedLock<UserSm> guard(&sm);
    if(sel() != INVALID) {
        try {
            UtcbFrame uf;
            uf << STOP;
            uf.translate(sel());
            CPU::current().sc_pt().call(uf);
        }
        catch(...) {
            // the destructor shouldn't throw
        }
    }
}

void Sc::start(const String &name) {
    UtcbFrame uf;
    ScopedCapSels sc;
    uf.delegation_window(Crd(sc.get(), 0, Crd::OBJ_ALL));
    uf << START << name << _ec->cpu() << _qpd;
    uf.delegate(_ec->sel());

    // ensure that we don't want to destroy the Sc before we've completely created it, i.e. received
    // the capability.
    ScopedLock<UserSm> guard(&sm);
    CPU::current().sc_pt().call(uf);
    uf.check_reply();
    sel(sc.release());
    uf >> _qpd;
}

}
