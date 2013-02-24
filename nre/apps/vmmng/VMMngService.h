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

#include <ipc/ServiceSession.h>
#include <ipc/Service.h>
#include <services/VMManager.h>
#include <utcb/UtcbFrame.h>

#include "RunningVM.h"
#include "RunningVMList.h"

class VMMngServiceSession : public nre::ServiceSession {
public:
    explicit VMMngServiceSession(nre::Service *s, size_t id, capsel_t caps, nre::Pt::portal_func func)
        : ServiceSession(s, id, caps, func), _vm(), _ds(), _sm(), _prod() {
    }
    virtual ~VMMngServiceSession() {
        delete _ds;
        delete _sm;
        delete _prod;
    }

    virtual void invalidate() {
        RunningVMList::get().remove(_vm);
    }

    void init(nre::DataSpace *ds, nre::Sm *sm, capsel_t pd) {
        RunningVM *vm = RunningVMList::get().get_by_pd(pd);
        if(!vm)
            throw nre::Exception(nre::E_NOT_FOUND, "Corresponding VM not found");
        if(_ds || vm->initialized())
            throw nre::Exception(nre::E_EXISTS, "Already initialized");
        _vm = vm;
        _ds = ds;
        _sm = sm;
        _prod = new nre::Producer<nre::VMManager::Packet>(*_ds, *_sm, false);
        vm->set_producer(_prod);
    }

private:
    RunningVM *_vm;
    nre::DataSpace *_ds;
    nre::Sm *_sm;
    nre::Producer<nre::VMManager::Packet> *_prod;
};

class VMMngService : public nre::Service {
    explicit VMMngService(const char *name)
        : Service(name, nre::CPUSet(nre::CPUSet::ALL), portal) {
        // we want to accept one dataspaces and pd-translations
        for(auto it = nre::CPU::begin(); it != nre::CPU::end(); ++it) {
            nre::LocalThread *ec = get_thread(it->log_id());
            nre::UtcbFrameRef uf(ec->utcb());
            uf.accept_translates();
            uf.accept_delegates(1);
        }
    }

public:
    static VMMngService *create(const char *name) {
        return _inst = new VMMngService(name);
    }

private:
    virtual VMMngServiceSession *create_session(size_t id, const nre::String &, capsel_t caps,
                                                nre::Pt::portal_func func) {
        return new VMMngServiceSession(this, id, caps, func);
    }

    PORTAL static void portal(capsel_t pid);

    static VMMngService *_inst;
};
