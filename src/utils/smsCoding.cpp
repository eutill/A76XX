#include "A76XX.h"

uint8_t hexToNibble(char c) {
    return (c & 0xf) + (c >= 'A' ? 9 : 0);
}

void hexPairToByte(char high, char low, uint8_t* out) {
    *out = (hexToNibble(high) << 4) | hexToNibble(low);
    return;
}

char nibble2hex(uint8_t nibble) {
    return (nibble < 10) ? (nibble + '0') : (nibble - 10 + 'A');
}

void byteTohexPair(const uint8_t input, char* high, char* low) {
    *high = nibble2hex(input >> 4);
    *low = nibble2hex(input & 0x0F);
}

bool checkHexDec(char c) {
    if('0' <= c && c <= '9') return true;
    if('A' <= c && c <= 'F') return true;
    return false;
}

void unpack7Bit(const uint8_t* input, uint16_t evalChars, uint8_t* output, uint8_t fillbits) {
    uint32_t bitBuffer = 0;
    uint8_t bitsInCount = 0;
    uint16_t inputIdx = 0;
    uint16_t charsExtracted = 0;

    if(fillbits) {
        bitBuffer = (uint32_t) input[inputIdx++];
        bitsInCount = 8 - fillbits;
        bitBuffer >>= fillbits;
    }

    while (charsExtracted < evalChars) {
        // If no sufficient bits for next character: load next byte
        if (bitsInCount < 7) {
            bitBuffer |= ((uint32_t)input[inputIdx++] << bitsInCount);
            bitsInCount += 8;
        }

        // Extract char
        output[charsExtracted++] = (uint8_t)(bitBuffer & 0x7F);
        bitBuffer >>= 7;
        bitsInCount -= 7;
    }
    
    // don't append null terminator because this isn't an ASCII string
    // resulting output shouldn't be treated as one – rather use evalChars as length
    // 0x00 is a valid 7-bit GSM char (@)!
}

uint8_t pack7Bit(const uint8_t* input, uint8_t numChars, uint8_t* output, uint8_t fillbits) {
    uint16_t bitBuffer = 0;
    uint8_t bitsInCount = fillbits;
    uint16_t inputIdx = 0;
    uint8_t packedBytes = 0;

    do {
        while((bitsInCount < 8) && (inputIdx < numChars)) {
            bitBuffer |= ((uint16_t) (input[inputIdx++] & 0x7F)) << bitsInCount;
            bitsInCount += 7;
        }
        output[packedBytes++] = (uint8_t) (bitBuffer & 0xFF);
        bitBuffer >>= 8;
        bitsInCount -= (bitsInCount < 8) ? bitsInCount : 8;
    } while((bitsInCount > 0) || (inputIdx < numChars));

    return packedBytes;
}

void decodeGSM(uint8_t* input, uint16_t numChars, bool overwrite, char* output) {
    if(!(overwrite || output)) return;
    
    char replaceChar, extractedChar;
    for(uint16_t i=0; i<numChars; i++) {
        extractedChar = input[i];

        // Map 7-bit GSM 03.38 encoding to ASCII, replace unsupported chars with '?'
        if(extractedChar == 0x24 || extractedChar == 0x40) {replaceChar = '?';}
        else if((extractedChar >= 0x20 && extractedChar <= 0x5A) || (extractedChar >= 0x61 && extractedChar <= 0x7A) || (extractedChar == 0x0A) || (extractedChar == 0x0D)) {
            // ASCII is equivalent to GSM 03.38 except edge cases already covered
            replaceChar = '\0';
        }
        else if(extractedChar == 0x00) {replaceChar = '@';}
        else if(extractedChar == 0x02) {replaceChar = '$';}
        else if(extractedChar == 0x11) {replaceChar = '_';}
        else {replaceChar = '?';}

        if(overwrite) {
            if(replaceChar) input[i] = replaceChar;
        } else if(replaceChar) {
            output[i] = replaceChar;
        } else {
            output[i] = extractedChar;
        }
    }
}

uint16_t encodeGSM(char* input, bool overwrite, uint8_t* output, uint16_t length) {
    if(!(overwrite || output)) return 0;

    char extractedChar, replaceChar;
    uint16_t inputLen = length ? length : strlen(input);
    for(uint16_t i=0; i<inputLen; i++) {
        extractedChar = input[i];

        // Map ASCII to GSM 03.38 encoding, replace unsupported chars with '?'
        if(extractedChar == '$') {replaceChar = 0x02;}
        else if(extractedChar == '@') {replaceChar = 0x00;}
        else if((extractedChar >= 0x20 && extractedChar <= 0x5A) || (extractedChar >= 0x61 && extractedChar <= 0x7A) || (extractedChar == 0x0A) || (extractedChar == 0x0D)) {
            replaceChar = 0xFF; // don't replace
        }
        else if(extractedChar == '_') {replaceChar = 0x11;}
        else {replaceChar = '?';}

        if(overwrite) {
            if(replaceChar != 0xFF) input[i] = replaceChar;
        } else if(replaceChar != 0xFF) {
            output[i] = replaceChar;
        } else {
            output[i] = extractedChar;
        }
    }
    
    return inputLen;
}

void extractBcdDigits(const uint8_t* input, int numDigits, char* output) {
    for(int i=0; i<numDigits; i++) {
        output[i] = (char) (((input[i>>1] >> ((i&1) << 2)) & 0x0F) + '0');
    }
}

void storeBcdDigits(char* input, uint8_t numDigits, uint8_t* output) {
    uint8_t numBytes = (numDigits+1) >> 1;
    for(int i=0; i<numBytes; i++) {
        output[i] = (input[i*2] - '0') | ((input[i*2+1] - '0') << 4);
    }
    if(numDigits & 0x01) // odd number of digits, mark last nibble as 0xF
        output[numBytes-1] |= 0xF0;
}

void decodeUCS2(const uint8_t* input, int evalChars, char* output) {
    char decoded;
    uint16_t val;
    
    for(int i=0;i<evalChars;i++) {
        // UCS2 values are big endian
        val = (input[2*i] << 8) | input[2*i+1];

        if(val < 0x0080) {
            decoded = (char) (val & 0xFF);
        } else {
            decoded = '?';
        }
        output[i] = decoded;
    }
}


