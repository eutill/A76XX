/*
 * byteringbuf.cpp
 *
 *  Created on: 09.06.2025
 *      Author: leo
 */

#include "A76XX.h"

ByteRingBuf::ByteRingBuf(size_t sizeBytes) : _size(sizeBytes), _head(0), _tail(0) {
    _buf = new unsigned char[sizeBytes];
}

ByteRingBuf::~ByteRingBuf(void) {
    delete[] _buf;
}

size_t ByteRingBuf::getFree() {
    if(_tail > _head) {
        return _tail - _head - 1;
    }
    return _size - _head + _tail - 1;
}

size_t ByteRingBuf::getUsed() {
    return (_tail > _head) ? (_size - _tail + _head) : (_head - _tail);
}

size_t ByteRingBuf::write(uint8_t* source, uint16_t len) {
    size_t writeLen = len < (_size - 1) ? len : (_size - 1);
    //determine whether we are going to overwrite valid data
    bool overwrite = writeLen > getFree();
    //First copy
    size_t copyLen = _size - _head;
    //determine whether we are going to wrap around
    bool wrapAround = writeLen > copyLen;
    copyLen = wrapAround ? copyLen : writeLen;
    memcpy(_buf + _head, source, copyLen);
    if(wrapAround) {
        size_t copyLen2 = writeLen - copyLen;
        memcpy(_buf, source + copyLen, copyLen2);
    }
    _head = (_head + writeLen) % _size;
    if(overwrite) _tail = (_head + 1) % _size;
    return writeLen;
}

size_t ByteRingBuf::read(uint8_t* dest, uint16_t maxLen) {
    size_t readLen = getUsed();
    readLen = maxLen < readLen ? maxLen : readLen;
    size_t copyLen = _size - _tail;
    bool wrapAround = readLen > copyLen;
    copyLen = wrapAround ? copyLen : readLen;
    memcpy(dest, _buf + _tail, copyLen);
    if(wrapAround) {
        size_t copyLen2 = readLen - copyLen;
        memcpy(dest + copyLen, _buf, copyLen2);
    }
    _tail = (_tail + readLen) % _size;
    return readLen;
}

void ByteRingBuf::clear(void) {
    _head = 0;
    _tail = 0;
}

cmp_match_t ByteRingBuf::compare(const char* str) {
    if(!str) return CMP_NO_MATCH;
    size_t availLen = getUsed();
    if(availLen == 0) return CMP_NO_MATCH;
    size_t strLen = strlen(str);
    if(!strLen) return CMP_NO_MATCH;
    size_t cmpLen = strLen < availLen ? strLen : availLen;

    for(int i=0;i<cmpLen;i++) {
        if(str[i] != _buf[(_tail + i) % _size]) return CMP_NO_MATCH;
    }

    if(strLen != cmpLen) return CMP_MATCH_PART;
    return CMP_ALL_MATCH;
}

size_t ByteRingBuf::consume(size_t n) {
    size_t availLen = getUsed();
    size_t consumeLen = n < availLen ? n : availLen;

    _tail = (_tail + consumeLen) % _size;
    return consumeLen;
}

bool ByteRingBuf::endsWith(const char* str) {
    if(!str) return false;
    size_t strLen = strlen(str);
    if(!strLen) return false;

    size_t availLen = getUsed();
    if(availLen < strLen) return false;

    for(int i=0;i<strLen;i++) {
        if(str[strLen-i-1] != _buf[(_tail+availLen-1-i) % _size]) return false;
    }
    return true;
}

bool ByteRingBuf::peek(uint8_t* val) {
    if(getUsed() > 0) {
        *val = _buf[_tail];
        return true;
    }
    return false;
}

bool ByteRingBuf::pop(uint8_t* val) {
    if(peek(val)) {
        _tail = (_tail + 1) % _size;
        return true;
    }
    return false;
}




