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

#include <ipc/Service.h>
#include <ipc/Producer.h>
#include <kobj/GlobalThread.h>
#include <kobj/Gsi.h>
#include <kobj/Sc.h>
#include <services/ACPI.h>
#include <RCU.h>

#include "HostKeyboard.h"

using namespace nre;

enum {
    KEYBOARD_IRQ    = 1,
    MOUSE_IRQ       = 12
};

template<class T>
class KeyboardSessionData : public ServiceSession {
public:
    explicit KeyboardSessionData(Service *s, size_t id, capsel_t cap, capsel_t caps,
                                 Pt::portal_func func)
        : ServiceSession(s, id, cap, caps, func), _prod(), _ds(), _sm() {
    }
    virtual ~KeyboardSessionData() {
        delete _ds;
        delete _sm;
        delete _prod;
    }

    Producer<T> *prod() {
        return _prod;
    }

    void set_ds(DataSpace *ds, Sm *sm) {
        if(_ds != nullptr)
            throw Exception(E_EXISTS, "Keyboard session already initialized");
        _ds = ds;
        _sm = sm;
        _prod = new Producer<T>(*ds, *sm, false);
    }

private:
    Producer<T> *_prod;
    DataSpace *_ds;
    Sm *_sm;
};

template<class T>
class KeyboardService : public Service {
public:
    typedef SessionIterator<KeyboardSessionData<T> > iterator;

    explicit KeyboardService(const char *name, Pt::portal_func func)
        : Service(name, CPUSet(CPUSet::ALL), func) {
        // we want to accept one dataspaces
        for(auto it = CPU::begin(); it != CPU::end(); ++it) {
            LocalThread *ec = get_thread(it->log_id());
            UtcbFrameRef uf(ec->utcb());
            uf.accept_delegates(1);
        }
    }

    KeyboardSessionData<T> *get_session(capsel_t pid) {
        return Service::get_session<KeyboardSessionData<T> >(pid);
    }
    iterator sessions_begin() {
        return Service::sessions_begin<KeyboardSessionData<T> >();
    }
    iterator sessions_end() {
        return Service::sessions_end<KeyboardSessionData<T> >();
    }

private:
    virtual ServiceSession *create_session(size_t id, capsel_t cap, capsel_t caps,
                                           Pt::portal_func func) {
        return new KeyboardSessionData<T>(this, id, cap, caps, func);
    }
};

static HostKeyboard *hostkb;
static KeyboardService<Keyboard::Packet> *kbsrv;
static KeyboardService<Mouse::Packet> *mousesrv;
static uint kbgsi;
static uint msgsi;

template<class T>
static void broadcast(KeyboardService<T> *srv, const T &data) {
    ScopedLock<RCULock> guard(&RCU::lock());
    for(auto it = srv->sessions_begin(); it != srv->sessions_end(); ++it) {
        if(it->prod())
            it->prod()->produce(data);
    }
}

static void kbhandler(void*) {
    Gsi gsi(kbgsi);
    while(1) {
        gsi.down();

        Keyboard::Packet data;
        if(hostkb->read(data))
            broadcast(kbsrv, data);
    }
}

static void mousehandler(void*) {
    Gsi gsi(msgsi);
    while(1) {
        gsi.down();

        Mouse::Packet data;
        if(hostkb->read(data))
            broadcast(mousesrv, data);
    }
}

template<class T>
static void handle_share(UtcbFrameRef &uf, KeyboardService<typename T::Packet> *srv, capsel_t pid) {
    typedef KeyboardSessionData<typename T::Packet> sessdata_t;
    sessdata_t *sess = srv->get_session(pid);
    capsel_t dssel = uf.get_delegated(0).offset();
    capsel_t smsel = uf.get_delegated(0).offset();
    uf.finish_input();
    sess->set_ds(new DataSpace(dssel), new Sm(smsel, false));
}

PORTAL static void portal_keyboard(capsel_t pid) {
    UtcbFrameRef uf;
    try {
        Keyboard::Command cmd;
        uf >> cmd;
        switch(cmd) {
            case Keyboard::REBOOT:
                uf.finish_input();
                hostkb->reboot();
                break;

            case Keyboard::SHARE_DS:
                handle_share<Keyboard>(uf, kbsrv, pid);
                break;
        }
        uf << E_SUCCESS;
    }
    catch(const Exception &e) {
        uf.clear();
        uf << e;
    }
}

PORTAL static void portal_mouse(capsel_t pid) {
    UtcbFrameRef uf;
    try {
        handle_share<Mouse>(uf, mousesrv, pid);
        uf << E_SUCCESS;
    }
    catch(const Exception &e) {
        uf.clear();
        uf << e;
    }
}

static void mouseservice(void*) {
    mousesrv = new KeyboardService<Mouse::Packet>("mouse", portal_mouse);
    GlobalThread::create(mousehandler, CPU::current().log_id(), "mouse-broadcast")->start();
    mousesrv->start();
}

int main(int argc, char *argv[]) {
    bool mouse = true;
    int scset = 2;
    for(int i = 1; i < argc; ++i) {
        if(strcmp(argv[i], "nomouse") == 0)
            mouse = false;
        if(strcmp(argv[i], "scset1") == 0)
            scset = 1;
    }

    // determine GSIs for keyboard and mouse
    {
        ACPISession acpi("acpi");
        kbgsi = acpi.irq_to_gsi(KEYBOARD_IRQ);
        msgsi = acpi.irq_to_gsi(MOUSE_IRQ);
    }

    hostkb = new HostKeyboard(scset, mouse);
    hostkb->reset();

    kbsrv = new KeyboardService<Keyboard::Packet>("keyboard", portal_keyboard);
    if(hostkb->mouse_enabled())
        GlobalThread::create(mouseservice, CPU::current().log_id(), "mouse")->start();

    GlobalThread::create(kbhandler, CPU::current().log_id(), "keyboard-broadcast")->start();
    kbsrv->start();
    return 0;
}
