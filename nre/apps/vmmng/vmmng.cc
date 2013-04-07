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

#include <subsystem/ChildManager.h>
#include <services/Console.h>
#include <services/Timer.h>
#include <stream/Serial.h>
#include <stream/ConsoleStream.h>
#include <collection/Cycler.h>
#include <util/Clock.h>
#include <Hip.h>

#include "VMConfig.h"
#include "RunningVM.h"
#include "RunningVMList.h"
#include "VMMngService.h"

using namespace nre;

static const uint CUR_ROW_COLOR = 0x70;

static UserSm sm;
static size_t vmidx = 0;
static ConsoleSession cons("console", 0, "VMManager");
static SList<VMConfig> configs;
static ChildManager cm;
static Cycler<CPU::iterator> cpucyc(CPU::begin(), CPU::end());

static void refresh_console() {
    ScopedLock<UserSm> guard(&sm);
    ConsoleStream cs(cons, 0);
    cons.clear(0);
    cs << "Welcome to the interactive VM manager!\n\n";
    cs << "VM configurations:\n";
    size_t no = 1;
    for(auto it = configs.begin(); it != configs.end(); ++it, ++no)
        cs << "  [" << no << "] " << it->name() << "\n";
    cs << "\n";

    cs << "Running VMs:\n";
    RunningVM *vm;
    if(vmidx >= RunningVMList::get().count())
        vmidx = RunningVMList::get().count() - 1;
    for(size_t i = 0; (vm = RunningVMList::get().get(i)) != nullptr; ++i) {
        Reference<const Child> c = cm.get(vm->id());
        // detect crashed VMs
        if(!c.valid()) {
            RunningVMList::get().remove(vm);
            i--;
            continue;
        }

        uint8_t oldcol = cs.color();
        if(vmidx == i)
            cs.color(CUR_ROW_COLOR);
        size_t virt, phys;
        c->reglist().memusage(virt, phys);
        cs << "  [" << vm->console() << "] CPU:" << c->cpu() << " MEM:" << (phys / 1024);
        cs << "K CFG:" << vm->cfg()->name();
        while(cs.x() != 0)
            cs << ' ';
        if(vmidx == i)
            cs.color(oldcol);
    }
    cs << "\nPress R to reset or K to kill the selected VM";
}

static void input_thread(void*) {
    RunningVMList &vml = RunningVMList::get();
    while(1) {
        Console::ReceivePacket *pk = cons.consumer().get();
        switch(pk->keycode) {
            case Keyboard::VK_1 ... Keyboard::VK_9: {
                if(pk->flags & Keyboard::RELEASE) {
                    size_t idx = pk->keycode - Keyboard::VK_1;
                    auto it = configs.begin();
                    for(; it != configs.end() && idx-- > 0; ++it)
                        ;
                    if(it != configs.end()) {
                        try {
                            vml.add(cm, &*it, cpucyc.next()->log_id());
                        }
                        catch(const Exception &e) {
                            Serial::get() << "Start of '" << it->name() << "' failed: " << e.msg() << "\n";
                        }
                    }
                }
            }
            break;

            case Keyboard::VK_R: {
                ScopedLock<UserSm> guard(&sm);
                RunningVM *vm = vml.get(vmidx);
                if(vm && (pk->flags & Keyboard::RELEASE))
                    vm->execute(VMManager::RESET);
            }
            break;

            case Keyboard::VK_UP:
                if((~pk->flags & Keyboard::RELEASE) && vmidx > 0) {
                    vmidx--;
                    refresh_console();
                }
                break;

            case Keyboard::VK_DOWN:
                if((~pk->flags & Keyboard::RELEASE) && vmidx < vml.count()) {
                    vmidx++;
                    refresh_console();
                }
                break;

            case Keyboard::VK_K: {
                if(pk->flags & Keyboard::RELEASE) {
                    Child::id_type id = ObjCap::INVALID;
                    {
                        ScopedLock<UserSm> guard(&sm);
                        RunningVM *vm = vml.get(vmidx);
                        if(vm) {
                            id = vm->id();
                            vml.remove(vm);
                        }
                    }
                    if(id != ObjCap::INVALID)
                        cm.kill(id);
                }
            }
            break;
        }
        cons.consumer().next();
    }
}

static void refresh_thread(void*) {
    TimerSession timer("timer");
    Clock clock(1000);
    while(1) {
        timevalue_t next = clock.source_time(1000);
        refresh_console();

        // wait a second
        timer.wait_until(next);
    }
}

int main() {
    const Hip &hip = Hip::get();

    for(auto mem = hip.mem_begin(); mem != hip.mem_end(); ++mem) {
        if(strstr(mem->cmdline(), ".vmconfig")) {
            VMConfig *cfg = new VMConfig(mem->addr, mem->size, mem->cmdline());
            configs.append(cfg);
            Serial::get() << *cfg << "\n";
        }
    }

    GlobalThread::create(input_thread, CPU::current().log_id(), "vmmng-input")->start();
    GlobalThread::create(refresh_thread, CPU::current().log_id(), "vmmng-refresh")->start();

    VMMngService *srv = VMMngService::create("vmmanager");
    srv->start();
    return 0;
}
