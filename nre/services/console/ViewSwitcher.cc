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

#include <stream/OStringStream.h>
#include <services/Timer.h>
#include <util/Clock.h>
#include <Logging.h>

#include "ViewSwitcher.h"
#include "ConsoleService.h"
#include "ConsoleSessionData.h"

using namespace nre;

char ViewSwitcher::_backup[Screen::COLS * 2];
char ViewSwitcher::_buffer[Screen::COLS + 1];

ViewSwitcher::ViewSwitcher(ConsoleService *srv)
    : _usm(1), _ds(DS_SIZE, DataSpaceDesc::ANONYMOUS, DataSpaceDesc::RW), _sm(0),
      _prod(_ds, _sm, true), _cons(_ds, _sm, false),
      _ec(GlobalThread::create(switch_thread, CPU::current().log_id(), "console-vs")),
      _srv(srv) {
    _ec->set_tls<ViewSwitcher*>(Thread::TLS_PARAM, this);
}

void ViewSwitcher::switch_to(ConsoleSessionData *from, ConsoleSessionData *to) {
    SwitchCommand cmd;
    cmd.oldsessid = from ? from->id() : -1;
    cmd.sessid = to->id();
    LOG(CONSOLE, "Going to switch from " << cmd.oldsessid << " to " << cmd.sessid << "\n");
    // we can't access the producer concurrently
    ScopedLock<UserSm> guard(&_usm);
    _prod.produce(cmd);
}

void ViewSwitcher::switch_thread(void*) {
    ViewSwitcher *vs = Thread::current()->get_tls<ViewSwitcher*>(Thread::TLS_PARAM);
    Clock clock(1000);
    TimerSession timer("timer");
    timevalue_t until = 0;
    size_t sessid = 0;
    bool tag_done = false;
    while(1) {
        // are we finished?
        if(until && clock.source_time() >= until) {
            LOG(CONSOLE, "Giving " << sessid << " direct access\n");
            try {
                Reference<ConsoleSessionData> sess = vs->_srv->get_session<ConsoleSessionData>(sessid);
                // finally swap to that session. i.e. give him direct screen access
                sess->to_front();
            }
            catch(const Exception &e) {
                LOG(CONSOLE, e);
                // just ignore it
            }
            until = 0;
        }

        // either block until the next request, or - if we're switching - check for new requests
        if(until == 0 || vs->_cons.has_data()) {
            SwitchCommand *cmd = vs->_cons.get();
            LOG(CONSOLE, "Got switch " << cmd->oldsessid << " to " << cmd->sessid << "\n");
            // if there is an old one, make a backup and detach him from screen
            if(cmd->oldsessid == sessid && until == 0) {
                try {
                    Reference<ConsoleSessionData> old = vs->_srv->get_session<ConsoleSessionData>(sessid);
                    old->to_back();
                }
                catch(const Exception &e) {
                    LOG(CONSOLE, e);
                    // just ignore it
                }
            }
            sessid = cmd->sessid;
            // show the tag for 1sec
            until = clock.source_time(SWITCH_TIME);
            tag_done = false;
            vs->_cons.next();
        }

        try {
            Reference<ConsoleSessionData> sess = vs->_srv->get_session<ConsoleSessionData>(sessid);

            // repaint all lines from the buffer except the first
            uintptr_t start = vs->_srv->screen()->mem().virt();
            if(sess->out_ds()) {
                memcpy(reinterpret_cast<void*>(start + sess->offset() + Screen::COLS * 2),
                       reinterpret_cast<void*>(sess->out_ds()->virt() + sess->offset() +
                                               Screen::COLS * 2),
                       Screen::PAGE_SIZE - Screen::COLS * 2);
            }

            if(!tag_done) {
                // write tag into buffer
                memset(_buffer, 0, sizeof(_buffer));
                OStringStream os(_buffer, sizeof(_buffer));
                os << "Console " << sess->console() << ": " << sess->title() << " (" <<
                sess->id() << ")";

                // write console tag
                char *screen = reinterpret_cast<char*>(start + sess->offset());
                char *s = _buffer;
                for(uint x = 0; x < Screen::COLS; ++x, ++s) {
                    screen[x * 2] = *s ? *s : ' ';
                    screen[x * 2 + 1] = COLOR;
                }
                sess->activate();
                tag_done = true;
            }
        }
        catch(const Exception &e) {
            LOG(CONSOLE, e);
            // if the session is dead, stop switching to it
            until = 0;
            continue;
        }

        // wait 25ms
        LOG(CONSOLE, "Waiting until " << clock.source_time(REFRESH_DELAY) << "\n");
        timer.wait_until(clock.source_time(REFRESH_DELAY));
        LOG(CONSOLE, "Waiting done\n");
    }
}
