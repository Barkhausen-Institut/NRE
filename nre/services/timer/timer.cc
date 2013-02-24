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

#include <kobj/Sm.h>
#include <services/Timer.h>
#include <Logging.h>

#include "HostTimer.h"

using namespace nre;

class TimerService;

static HostTimer *timer;
static TimerService *srv;

class TimerSessionData : public ServiceSession {
public:
    explicit TimerSessionData(Service *s, size_t id, capsel_t caps, Pt::portal_func func)
        : ServiceSession(s, id, caps, func), _data(new HostTimer::ClientData[CPU::count()]) {
        for(auto it = CPU::begin(); it != CPU::end(); ++it)
            timer->setup_clientdata(id, _data + it->log_id(), it->log_id());
    }
    virtual ~TimerSessionData() {
        delete[] _data;
    }

    HostTimer::ClientData *data(cpu_t cpu) {
        return _data + cpu;
    }

private:
    HostTimer::ClientData *_data;
};

class TimerService : public Service {
public:
    explicit TimerService(const char *name, Pt::portal_func func)
        : Service(name, CPUSet(CPUSet::ALL), func) {
    }

private:
    virtual ServiceSession *create_session(size_t id, const String &, capsel_t caps,
                                           Pt::portal_func func) {
        return new TimerSessionData(this, id, caps, func);
    }
};

PORTAL static void portal_timer(capsel_t pid) {
    ScopedLock<RCULock> guard(&RCU::lock());
    TimerSessionData *sess = srv->get_session<TimerSessionData>(pid);
    UtcbFrameRef uf;
    try {
        nre::Timer::Command cmd;
        uf >> cmd;

        switch(cmd) {
            case nre::Timer::GET_SMS:
                uf.finish_input();

                for(auto it = CPU::begin(); it != CPU::end(); ++it)
                    uf.delegate(sess->data(it->log_id())->sm->sel(), it->log_id());
                uf << E_SUCCESS;
                break;

            case nre::Timer::PROG_TIMER: {
                timevalue_t time;
                uf >> time;
                uf.finish_input();

                LOG(TIMER_DETAIL, "TIMER: (" << sess->id() << ") Programming for "
                                             << fmt("%#x", time) << " on "
                                             << CPU::current().log_id() << "\n");

                LOG(TIMER_DETAIL, "TIMER: (" << sess->id() << ") Programming for "
                                             << fmt(time, "#x") << " on "
                                             << CPU::current().log_id() << "\n");
                timer->program_timer(sess->data(CPU::current().log_id()), time);
                uf << E_SUCCESS;
            }
            break;

            case nre::Timer::GET_TIME: {
                uf.finish_input();

                timevalue_t uptime, unixts;
                timer->get_time(uptime, unixts);
                LOG(TIMER_DETAIL, "TIMER: (" << sess->id() << ") Getting time"
                                             << " up=" << fmt(uptime, "#x")
                                             << " unix=" << fmt(unixts, "#x") << "\n");
                uf << E_SUCCESS << uptime << unixts;
            }
            break;
        }
    }
    catch(const Exception &e) {
        uf.clear();
        uf << e;
    }
}

int main(int argc, char *argv[]) {
    bool forcepit = false;
    bool forcehpetlegacy = false;
    bool slowrtc = false;
    for(int i = 1; i < argc; ++i) {
        if(strcmp(argv[i], "forcepit") == 0)
            forcepit = true;
        if(strcmp(argv[i], "forcehpetlegacy") == 0)
            forcehpetlegacy = true;
        if(strcmp(argv[i], "slowrtc") == 0)
            slowrtc = true;
    }

    timer = new HostTimer(forcepit, forcehpetlegacy, slowrtc);
    srv = new TimerService("timer", portal_timer);
    srv->start();
    return 0;
}
