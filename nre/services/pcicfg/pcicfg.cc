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
#include <services/PCIConfig.h>
#include <Logging.h>

#include "HostPCIConfig.h"
#include "HostMMConfig.h"

using namespace nre;

static HostPCIConfig *pcicfg;
static HostMMConfig *mmcfg;

static Config *find(BDF bdf, size_t offset) {
    if(pcicfg->contains(bdf, offset))
        return pcicfg;
    if(mmcfg && mmcfg->contains(bdf, offset))
        return mmcfg;
    VTHROW(Exception, E_NOT_FOUND, bdf << "+" << fmt(offset, "#x") << " not found");
}

PORTAL static void portal_pcicfg(void*) {
    UtcbFrameRef uf;
    try {
        BDF bdf;
        size_t offset = 0;
        PCIConfig::Command cmd;
        uf >> cmd;

        Config *cfg = nullptr;
        if(cmd == PCIConfig::READ || cmd == PCIConfig::WRITE || cmd == PCIConfig::ADDR) {
            uf >> bdf >> offset;
            cfg = find(bdf, offset);
        }

        switch(cmd) {
            case PCIConfig::READ: {
                uf.finish_input();
                PCIConfig::value_type res = cfg->read(bdf, offset);
                LOG(PCICFG, cfg->name() << "::READ " << bdf << " off=" << fmt(offset, "#x")
                                        << ": " << fmt(res, "#x") << "\n");
                uf << E_SUCCESS << res;
            }
            break;

            case PCIConfig::WRITE: {
                PCIConfig::value_type value;
                uf >> value;
                uf.finish_input();
                cfg->write(bdf, offset, value);
                LOG(PCICFG, cfg->name() << "::WRITE " << bdf << " off=" << fmt(offset, "#x")
                                        << ": " << fmt(value, "#x") << "\n");
                uf << E_SUCCESS;
            }
            break;

            case PCIConfig::ADDR: {
                uf.finish_input();
                uintptr_t res = mmcfg->addr(bdf, offset);
                LOG(PCICFG, "MMConfig::ADDR " << bdf << " off=" << fmt(offset, "#x")
                                              << ": " << fmt(res, "p") << "\n");
                uf << E_SUCCESS << res;
            }
            break;

            case PCIConfig::SEARCH_DEVICE: {
                PCIConfig::value_type theclass, subclass, inst;
                uf >> theclass >> subclass >> inst;
                uf.finish_input();
                BDF bdf = pcicfg->search_device(theclass, subclass, inst);
                LOG(PCICFG, "PCIConfig::SEARCH_DEVICE" << " class=" << fmt(theclass, "#x")
                                                       << " subclass=" << fmt(subclass, "#x")
                                                       << " inst=" << fmt(inst, "#x")
                                                       << " => " << bdf << "\n");
                uf << E_SUCCESS << bdf;
            }
            break;

            case PCIConfig::SEARCH_BRIDGE: {
                PCIConfig::value_type bridge;
                uf >> bridge;
                uf.finish_input();
                BDF bdf = pcicfg->search_bridge(bridge);
                LOG(PCICFG, "PCIConfig::SEARCH_BRIDGE bridge=" << fmt(bridge, "#x")
                                                               << " => " << bdf << "\n");
                uf << E_SUCCESS << bdf;
            }
            break;

            case PCIConfig::REBOOT: {
                uf.finish_input();
                pcicfg->reset();
                uf << E_SUCCESS;
            }
            break;
        }
    }
    catch(const Exception &e) {
        uf.clear();
        uf << e;
    }
}

int main() {
    pcicfg = new HostPCIConfig();
    try {
        mmcfg = new HostMMConfig();
    }
    catch(const Exception &e) {
        Serial::get() << e.name() << ": " << e.msg() << "\n";
    }

    Service *srv = new Service("pcicfg", CPUSet(CPUSet::ALL), portal_pcicfg);
    srv->start();
    return 0;
}
