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

#include <stream/OStream.h>
#include <stream/IStream.h>
#include <ipc/ClientSession.h>
#include <services/Console.h>
#include <cstring>

namespace nre {

/**
 * This class provides means to read from or write to a VGA console.
 */
class VGAStream : public IStream, public OStream {
public:
    static const uint COLS              = 80;
    static const uint ROWS              = 25;
    static const uint TAB_WIDTH         = 4;
    static const size_t BUF_SIZE        = COLS * 2 + 1;
    static const size_t PAGES           = 32;
    static const size_t TEXT_OFF        = 0x18000;
    static const size_t TEXT_PAGES      = 8;
    static const size_t PAGE_SIZE       = 0x1000;

    /**
     * Constructor
     *
     * @param sess your console-session
     * @param page the console-page to write to
     * @param echo whether to echo read characters
     */
    explicit VGAStream(ConsoleSession &sess, uint page = 0, bool echo = true)
        : _sess(sess), _page(page), _pos(0), _color(0x0F), _echo(echo) {
    }

    /**
     * Clears the given page
     *
     * @param page the page
     */
    void clear(uint page) {
        assert(page < TEXT_PAGES);
        uintptr_t addr = _sess.screen().virt() + TEXT_OFF + page * PAGE_SIZE;
        memset(reinterpret_cast<void*>(addr), 0, PAGE_SIZE);
    }

    /**
     * @return the page
     */
    uint page() const {
        return _page;
    }

    /**
     * @return the current color
     */
    uint8_t color() const {
        return _color;
    }
    /**
     * Sets the color to given value
     *
     * @param color the color
     */
    void color(uint8_t color) {
        _color = color;
    }

    /**
     * @return the current x-position on the screen
     */
    uint x() const {
        return _pos % COLS;
    }
    /**
     * @return the current y-position on the screen
     */
    uint y() const {
        return _pos / ROWS;
    }

    /**
     * Sets the cursor position to <x>,<y>
     *
     * @param x the x-position
     * @param y the y-position
     */
    void pos(uint x, uint y) {
        _pos = y * COLS + x;
    }

    /**
     * Reads one character from the console
     *
     * @return the character
     */
    virtual char read();

    /**
     * Writes the given character to the console
     *
     * @param c the character
     */
    virtual void write(char c) {
        put((static_cast<ushort>(_color) << 8) | c, _pos);
    }

    /**
     * Writes the given character+colorcode to the given position and updates <pos> accordingly.
     *
     * @param value the character+color to write
     * @param pos the position (will be updated)
     */
    void put(ushort value, uint &pos) {
        uintptr_t addr = _sess.screen().virt() + TEXT_OFF + _page * PAGE_SIZE;
        put(value, reinterpret_cast<ushort*>(addr), pos);
    }

    /**
     * Writes the given character+colorcode to the given position and updates <pos> accordingly.
     *
     * @param value the character+color to write
     * @param base the base address of the console-page
     * @param pos the position (will be updated)
     */
    void put(ushort value, ushort *base, uint &pos);

private:
    ConsoleSession &_sess;
    uint _page;
    uint _pos;
    uint8_t _color;
    bool _echo;
};

}
