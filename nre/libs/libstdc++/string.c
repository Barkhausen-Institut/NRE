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

#include <arch/Defines.h>
#include <cstring>

void* memcpy(void *dest, const void *src, size_t len) {
    uchar *bdest = (uchar*)dest;
    uchar *bsrc = (uchar*)src;
    // copy bytes for alignment
    if(((uintptr_t)bdest % sizeof(word_t)) == ((uintptr_t)bsrc % sizeof(word_t))) {
        while(len > 0 && (uintptr_t)bdest % sizeof(word_t)) {
            *bdest++ = *bsrc++;
            len--;
        }
    }

    word_t *ddest = (word_t*)bdest;
    word_t *dsrc = (word_t*)bsrc;
    // copy words with loop-unrolling
    while(len >= sizeof(word_t) * 8) {
        *ddest = *dsrc;
        *(ddest + 1) = *(dsrc + 1);
        *(ddest + 2) = *(dsrc + 2);
        *(ddest + 3) = *(dsrc + 3);
        *(ddest + 4) = *(dsrc + 4);
        *(ddest + 5) = *(dsrc + 5);
        *(ddest + 6) = *(dsrc + 6);
        *(ddest + 7) = *(dsrc + 7);
        ddest += 8;
        dsrc += 8;
        len -= sizeof(word_t) * 8;
    }

    // copy remaining words
    while(len >= sizeof(word_t)) {
        *ddest++ = *dsrc++;
        len -= sizeof(word_t);
    }

    // copy remaining bytes
    bdest = (uchar*)ddest;
    bsrc = (uchar*)dsrc;
    while(len-- > 0)
        *bdest++ = *bsrc++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t count) {
    uchar *s, *d;
    // nothing to do?
    if((uchar*)dest == (uchar*)src || count == 0)
        return dest;

    // moving forward
    if((uintptr_t)dest > (uintptr_t)src) {
        word_t *dsrc = (word_t*)((uintptr_t)src + count - sizeof(word_t));
        word_t *ddest = (word_t*)((uintptr_t)dest + count - sizeof(word_t));
        while(count >= sizeof(word_t)) {
            *ddest-- = *dsrc--;
            count -= sizeof(word_t);
        }
        s = (uchar*)dsrc + (sizeof(word_t) - 1);
        d = (uchar*)ddest + (sizeof(word_t) - 1);
        while(count-- > 0)
            *d-- = *s--;
    }
    // moving backwards
    else
        memcpy(dest, src, count);

    return dest;
}

void *memset(void *addr, int value, size_t count) {
    uchar *baddr = (uchar*)addr;
    // align it
    while(count > 0 && (uintptr_t)baddr % sizeof(word_t)) {
        *baddr++ = value;
        count--;
    }

    // set with dwords
    uint dwval = (value << 24) | (value << 16) | (value << 8) | value;
    uint *dwaddr = (uint*)baddr;
    while(count >= sizeof(uint)) {
        *dwaddr++ = dwval;
        count -= sizeof(uint);
    }

    // set remaining bytes
    baddr = (uchar*)dwaddr;
    while(count-- > 0)
        *baddr++ = value;
    return addr;
}

size_t strlen(const char *src) {
    size_t len = 0;
    while(*src++)
        len++;
    return len;
}

char *strcpy(char *dst, const char *src) {
    char *res = dst;
    while(*src)
        *dst++ = *src++;
    return res;
}

int memcmp(const void *str1, const void *str2, size_t count) {
    const uchar *s1 = (const uchar*)str1;
    const uchar *s2 = (const uchar*)str2;
    while(count-- > 0) {
        if(*s1++ != *s2++)
            return s1[-1] < s2[-1] ? -1 : 1;
    }
    return 0;
}

int strcmp(const char *str1, const char *str2) {
    char c1 = *str1, c2 = *str2;
    while(c1 && c2) {
        // different?
        if(c1 != c2) {
            if(c1 > c2)
                return 1;
            return -1;
        }
        c1 = *++str1;
        c2 = *++str2;
    }
    // check which strings are finished
    if(!c1 && !c2)
        return 0;
    if(!c1 && c2)
        return -1;
    return 1;
}

int strncmp(const char *str1, const char *str2, size_t count) {
    ssize_t rem = count;
    while(*str1 && *str2 && rem-- > 0) {
        if(*str1++ != *str2++)
            return str1[-1] < str2[-1] ? -1 : 1;
    }
    if(rem <= 0)
        return 0;
    if(*str1 && !*str2)
        return 1;
    return -1;
}

char *strchr(const char *str, int ch) {
    while(*str) {
        if(*str++ == ch)
            return (char*)(str - 1);
    }
    return nullptr;
}

char *strstr(const char *str1, const char *str2) {
    char *res = nullptr;
    char *sub;

    // handle special case to prevent looping the string
    if(!*str2)
        return nullptr;
    while(*str1) {
        // matching char?
        if(*str1++ == *str2) {
            res = (char*)--str1;
            sub = (char*)str2;
            // continue until the strings don't match anymore
            while(*sub && *str1 == *sub) {
                str1++;
                sub++;
            }
            // complete substring matched?
            if(!*sub)
                return res;
        }
    }
    return nullptr;
}

size_t strcspn(const char *s, const char *reject) {
    size_t res = 0;
    while(s[res] && !strchr(reject, s[res]))
        res++;
    return res;
}

static int isspace(int c) {
    return c == ' ' || c == '\f' || c == '\n' || c == '\r' || c == '\t' || c == '\v';
}

unsigned long strtoul(char const *ptr, char const **end, int base) {
    while(isspace(*ptr))
        ptr++;

    if(!base) {
        if(*ptr != '0')
            base = 10;
        else if(*(ptr + 1) == 'x')
            ptr += 2, base = 16;
        else
            ptr += 1, base = 8;
    }

    unsigned long val = 0;
    unsigned char c;
    while((c = *ptr)) {
        int x = (c >= 'a') ? c - 'a' + 10 :
                (c >= 'A') ? c - 'A' + 10 :
                (c >= '0') ? c - '0' : 0xff;
        if(x >= base)
            break;

        val = val * base + x;
        ptr++;
    }

    if(end)
        *end = ptr;
    return val;
}
