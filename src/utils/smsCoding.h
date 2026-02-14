#ifndef A76XX_UTILS_SMS_CODING_H_
#define A76XX_UTILS_SMS_CODING_H_

void hexPairToByte(char high, char low, uint8_t* out);

void byteTohexPair(const uint8_t input, char* highNibble, char* lowNibble);

bool checkHexDec(char c);

void unpack7Bit(const uint8_t* input, uint16_t evalChars, uint8_t* output, uint8_t fillbits = 0);

uint8_t pack7Bit(const uint8_t* input, uint8_t numChars, uint8_t* output, uint8_t fillbits = 0);

void decodeGSM(uint8_t* input, uint16_t numChars, bool overwrite = true, char* output = NULL);

uint16_t encodeGSM(char* input, bool overwrite = true, uint8_t* output = NULL, uint16_t length = 0);

void extractBcdDigits(const uint8_t* input, int numDigits, char* output);

void storeBcdDigits(char* input, uint8_t numDigits, uint8_t* output);

void decodeUCS2(const uint8_t* input, int evalChars, char* output);

#endif /* A76XX_UTILS_SMS_CODING_H_ */