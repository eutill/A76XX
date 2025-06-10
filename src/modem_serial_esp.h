#ifndef A76XX_MODEMUART_ESP_H_
#define A76XX_MODEMUART_ESP_H_

#include "modem_serial.h"
#include "utils/circbuf.h"
#include "driver/uart.h"
#include "esp_log.h"

void delay(unsigned long ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}

class TimeoutCalc {
public:
    TimeoutCalc(uint32_t timeoutMs) {
        _start = xTaskGetTickCount();
        _duration = pdMS_TO_TICKS(timeoutMs);
    }
    bool expired(void) {
        return(!(xTaskGetTickCount() - _start < _duration));
    }

private:
    TickType_t _start;
    TickType_t _duration;
};

class ModemSerialESP : public ModemSerial {
  private:
    uart_port_t _uart;
    CircBuf _buf;

  public:
    // The following functions are simply forwarding the calls to underlying stream
    // object. If you need others, send a pull request!

    /*
        @brief Construct a ModemSerial object.

        @details This is a standard Arduino Serial object on steroids, with
            additional functionality to send AT commands and parse the response
            from the module.

        @param [IN] stream The underlying serial object. It must be initialized
            externally by the user with, e.g., a call to begin, with the appropriate
            TX and RX pins. The modem cannot be used if the serial connection is
            not established.
    */
    ModemSerialESP(uart_port_t uart)
        : _uart(uart), _buf(200) {}


    /*
        @brief Wait for modem to respond.

        @detail Consume data from the serial port until a match is found with one of the
            two input string or before the operation times out. If the parameters
            `match_OK` and `match_ERROR` are true, we also check if the response
            matches with the default OK and ERROR strings, but precedence is given
            to matching the input strings, which is useful for some commands.
        @param [IN] match_1 The first string to match.
        @param [IN] match_2 The second string to match.
        @param [IN] match_3 The third string to match.
        @param [IN] timeout Return if no match is found within this time in milliseconds.
            Default is 1000 milliseconds.
        @param [IN] match_OK Whether to match the string "OK\r\n". Default is true.
        @param [IN] match_ERROR Whether to match the string "ERROR\r\n". Default is true.

        @return A Response_t object.
    */
    Response_t waitResponse(const char* match_1,
                            const char* match_2,
                            const char* match_3,
                            int timeout = 1000,
                            bool match_OK = true,
                            bool match_ERROR = true) override {

        TickType_t startTime = xTaskGetTickCount();
        TickType_t timeoutTicks = pdMS_TO_TICKS(timeout);

        //_buf.clear();
        const char* cmp_str[5] = {
                match_1,
                match_2,
                match_3,
                match_OK ? RESPONSE_OK : NULL,
                match_ERROR ? RESPONSE_ERROR : NULL
        };
        Response_t resp[5] = {
                A76XX_RESPONSE_MATCH_1ST,
                A76XX_RESPONSE_MATCH_2ND,
                A76XX_RESPONSE_MATCH_3RD,
                A76XX_RESPONSE_OK,
                A76XX_RESPONSE_ERROR
        };
        uint8_t cmp_idx;

        while(1) {
            //calculate remaining time
            TickType_t elapsedTime = xTaskGetTickCount() - startTime;
            if(elapsedTime > timeoutTicks) { //timeout
                _buf.clear();
                return A76XX_RESPONSE_TIMEOUT;
            }
            TickType_t remainingTime = timeoutTicks - elapsedTime;

            //wait and fetch a char from uart, with timeout
            uint8_t val;
            if(uart_read_bytes(_uart, &val, 1, remainingTime) > 0) {
                _buf.write(&val, 1);
            } else { //timeout
                _buf.clear();
                return A76XX_RESPONSE_TIMEOUT;
            }

            for(cmp_idx = 0; cmp_idx<5; cmp_idx++) {
                if(!cmp_str[cmp_idx]) continue;
                if(_buf.endsWith(cmp_str[cmp_idx])) {
                    //found match!
                    _buf.clear();
                    return resp[cmp_idx];
                }
            }
            //do the same with URCs
            for (uint8_t i=0; i<_num_event_handlers; i++) {
                if (_buf.endsWith(_event_handlers[i]->match_string)) {
                    //found match with registered URCs. Send event, then continue
                    _event_handlers[i]->process(this);
                    _buf.clear();
                    break;
                }
            }
        }
        return A76XX_RESPONSE_TIMEOUT; //execution won't reach here
    }

    void printCMD(const char* str) override {
        uart_write_bytes(_uart, str, strlen(str));
        flush();
    }

    void printCMD(uint16_t val) override {
        char buf[6];
        int len = snprintf(buf, 6, "%u", val);
        uart_write_bytes(_uart, buf, len);
        flush();
    }

    int available() override {
        int availBytes;
        if(uart_get_buffered_data_len(_uart, (size_t *) &availBytes) != ESP_OK) {
            availBytes = 0;
        }
        return availBytes + _buf.getUsed();
    }

    long parseInt() override {
        //ignores all invalid characters before first valid one
        //returns on timeout (and evaluates all valid chars up until then)
        //or on first invalid char after valid ones.

        TickType_t startTime = xTaskGetTickCount();
        TickType_t timeoutTicks = pdMS_TO_TICKS(A76XX_SERIAL_TIMEOUT_DEFAULT);

        _buf.clear(); //discard all values as ringbuffer is used as intermediate buffer
        bool validNumber = false;
        char validChars[] = "0123456789-";
        char numberBuf[20];
        uint8_t val, numberLen;

        while(1) {
            //calculate remaining time
            TickType_t elapsedTime = xTaskGetTickCount() - startTime;
            if(elapsedTime > timeoutTicks) { //timeout
                _buf.clear();
                return 0L;
            }
            TickType_t remainingTime = timeoutTicks - elapsedTime;

            if(validNumber) {
                if(uart_read_bytes(_uart, &val, 1, remainingTime) != 1) { //timeout
                    numberLen = _buf.read((uint8_t*) numberBuf, 20);
                    if(numberLen == 20) {
                        _buf.clear();
                        return 0L;
                    } else {numberBuf[numberLen] = '\0';}

                    break; //go to evaluation
                }
                //check if char is valid
                if(strchr(validChars, val)) {
                    //store in ringbuffer
                    _buf.write(&val, 1);
                } else { //invalid digit after valid one(s)
                    //transfer valid chars from ringbuffer to numberBuf
                    numberLen = _buf.read((uint8_t*) numberBuf, 20);
                    if(numberLen == 20) {
                        _buf.clear();
                        _buf.write(&val, 1);
                        return 0L;
                    } else {numberBuf[numberLen] = '\0';}
                    //store invalid digit in ringbuffer
                    _buf.write(&val, 1);
                    //proceed to evaluation
                    break;
                }
            } else { // !validNumber
                //look for first occurrence of a valid char
                if(uart_read_bytes(_uart, &val, 1, remainingTime) != 1) { //timeout
                    return 0L;
                }
                //check if char is valid
                if(strchr(validChars, val)) {
                    //store in ringbuffer
                    _buf.write(&val, 1);
                    //from now on, minus (-) is not a valid character
                    validChars[10] = '\0';
                    validNumber = true;
                }
            }
        }
        //turn valid characters into 'long' variable
        return strtol(numberBuf, NULL, 10);
    }

    float parseFloat() override {
        //ignores all invalid characters before first valid one
        //returns on timeout (and evaluates all valid chars up until then)
        //or on first invalid char after valid ones.

        TickType_t startTime = xTaskGetTickCount();
        TickType_t timeoutTicks = pdMS_TO_TICKS(A76XX_SERIAL_TIMEOUT_DEFAULT);

        _buf.clear(); //discard all values as ringbuffer is used as intermediate buffer
        bool validNumber = false;
        char validChars[] = "0123456789.-";
        char numberBuf[20];
        uint8_t val, numberLen;

        while(1) {
            //calculate remaining time
            TickType_t elapsedTime = xTaskGetTickCount() - startTime;
            if(elapsedTime > timeoutTicks) { //timeout
                _buf.clear();
                return 0.0f;
            }
            TickType_t remainingTime = timeoutTicks - elapsedTime;

            if(validNumber) {
                if(uart_read_bytes(_uart, &val, 1, remainingTime) != 1) { //timeout
                    numberLen = _buf.read((uint8_t*) numberBuf, 20);
                    if(numberLen == 20) {
                        _buf.clear();
                        return 0.0f;
                    } else {numberBuf[numberLen] = '\0';}

                    break; //go to evaluation
                }
                //check if char is valid
                if(strchr(validChars, val)) {
                    //store in ringbuffer
                    _buf.write(&val, 1);
                    if(val == '.') { // all remaining chars can only be digits
                        validChars[10] = '\0';
                    }
                } else { //invalid digit after valid one(s)
                    //transfer valid chars from ringbuffer to numberBuf
                    numberLen = _buf.read((uint8_t*) numberBuf, 20);
                    if(numberLen == 20) {
                        _buf.clear();
                        _buf.write(&val, 1);
                        return 0.0f;
                    } else {numberBuf[numberLen] = '\0';}
                    //store invalid digit in ringbuffer
                    _buf.write(&val, 1);
                    //proceed to evaluation
                    break;
                }
            } else {
                //look for first occurrence of a valid char
                if(uart_read_bytes(_uart, &val, 1, remainingTime) != 1) { //timeout
                    return 0.0f;
                }
                //check if char is valid
                if(strchr(validChars, val)) {
                    //store in ringbuffer
                    _buf.write(&val, 1);
                    if(val == '.') { //first character was a dot, remaining ones can only be digits
                        validChars[10] = '\0';
                    } else {
                        validChars[11] = '\0'; //remaining ones can be digits or dot
                    }
                    validNumber = true;
                }
            }
        }
        //turn valid characters into 'float' variable
        return strtof(numberBuf, NULL);
    }

    void flush() override {
        uart_flush(_uart);
    }

    int peek() override {
        uint8_t val;
        //first, peek into ringbuffer if it contains data
        if(_buf.peek(&val)) return val;
        //if it doesn't, transfer one byte from UART input buffer to ringbuffer
        if(uart_read_bytes(_uart, &val, 1, 0) != 1) return -1;
        _buf.write(&val, 1);
        return val;
    }

    int read() override {
        uint8_t val;
        if(_buf.getUsed() > 0) {
            if(_buf.read(&val, 1) == 1) {
                return val;
            }
        }

        if(uart_read_bytes(_uart, (void*) &val, 1, 0) == 1) {
            return val;
        }
        return -1;
    }

    bool find(char terminator) override {
        //first, go fishing in the ringbuffer
        char val;
        while(_buf.pop((uint8_t*) &val)) {
            if(val == terminator) {
                return true;
            }
        }
        //when ringbuffer empty, go to UART input buffer
        while(uart_read_bytes(_uart, &val, 1, pdMS_TO_TICKS(A76XX_SERIAL_TIMEOUT_DEFAULT)) == 1) {
            if(val == terminator) {
                return true;
            }
        }
        //timeout
        return false;
    }

    size_t write(const char* data) override {
        return uart_write_bytes(_uart, data, strlen(data));
    }

    size_t write(const char* data, size_t size) override {
        return uart_write_bytes(_uart, data, size);
    }

    size_t readBytesUntil(char terminator, char* buf, int len) override {
        //first, fetch from ringbuffer
        char val;
        size_t writeLen = 0;
        while(_buf.pop((uint8_t*) &val)) {
            if(val == terminator) {
                return writeLen;
            }
            //save value to buffer
            buf[writeLen++] = val;
            if(writeLen == len) return len;
        }
        //now, fetch from UART input buffer
        while(uart_read_bytes(_uart, &val, 1, pdMS_TO_TICKS(A76XX_SERIAL_TIMEOUT_DEFAULT)) == 1) {
            if(val == terminator) {
                return writeLen;
            }
            buf[writeLen++] = val;
            if(writeLen == len) return len;
        }
        //timeout
        return 0;
    }

    size_t readBytes(void* buf, int len) override {
        //first, use data left in ringbuffer
        size_t ringDataLen = _buf.getUsed();
        size_t readLen = 0;
        if(ringDataLen) {
            readLen = _buf.read((uint8_t*) buf, len);
            if(readLen == len) return readLen;
        }
        //read remaining data directly from UART
        size_t readLen2 = uart_read_bytes(_uart, (uint8_t*)buf+readLen, len-readLen, pdMS_TO_TICKS(A76XX_SERIAL_TIMEOUT_DEFAULT));
        if(readLen2 > 0) readLen += readLen2;
        return readLen;
    }
};





#endif /* A76XX_MODEMUART_ESP_H_ */
