/*
 * circbuf.h
 *
 *  Created on: 09.06.2025
 *      Author: leo
 */

#ifndef COMPONENTS_A76XX_SRC_UTILS_CIRCBUF_H_
#define COMPONENTS_A76XX_SRC_UTILS_CIRCBUF_H_

#include "A76XX.h"

typedef enum {
    CMP_NO_MATCH,
    CMP_MATCH_PART, //string is partly contained in ringbuffer
    CMP_ALL_MATCH //string is completely contained in ringbuffer
} cmp_match_t;

class CircBuf {
public:
    CircBuf(size_t sizeBytes);
    size_t write(uint8_t* source, uint16_t len);
    size_t read(uint8_t* dest, uint16_t maxLen);
    void clear(void);
    size_t getFree();
    size_t getUsed();
    cmp_match_t compare(const char* str);
    size_t consume(size_t n);
    bool endsWith(const char* str);
    bool peek(uint8_t* val);
    ~CircBuf();
private:
    unsigned char* _buf;
    size_t _size;
    size_t _head;
    size_t _tail;
};



#endif /* COMPONENTS_A76XX_SRC_UTILS_CIRCBUF_H_ */
