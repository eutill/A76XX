#ifndef A76XX_MODEMUART_ESP_H_
#define A76XX_MODEMUART_ESP_H_

#include "modem_serial.h"
#include "utils/circbuf.h"
#include "driver/uart.h"
#include "esp_log.h"

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

        _buf.clear();
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
            if(elapsedTime > timeoutTicks) return A76XX_RESPONSE_TIMEOUT;
            TickType_t remainingTime = timeoutTicks - elapsedTime;

            //wait and fetch a char from uart, with timeout
            uint8_t val;
            if(uart_read_bytes(_uart, &val, 1, remainingTime) > 0) {
                _buf.write(&val, 1);
            } else {
                return A76XX_RESPONSE_TIMEOUT;
            }

            for(cmp_idx = 0; cmp_idx<5; cmp_idx++) {
                if(!cmp_str[cmp_idx]) continue;
                if(_buf.endsWith(cmp_str[cmp_idx])) return resp[cmp_idx];
            }
            //do the same with URCs
            for (uint8_t i=0; i<_num_event_handlers; i++) {
                if (_buf.endsWith(_event_handlers[i]->match_string)) {
                    _event_handlers[i]->process(this);
                    _buf.clear();
                    break;
                }
            }
        }
        return A76XX_RESPONSE_TIMEOUT;
    }

    int available() override {
        int availBytes;
        if(uart_get_buffered_data_len(_uart, (size_t *) &availBytes) != ESP_OK) {
            return 0;
        }
        return availBytes;
    }
/*
    long parseInt() override {
        return _stream.parseInt();
    }

    float parseFloat() override {
        return _stream.parseFloat();
    }
*/
    void flush() override {
        uart_flush(_uart);
    }
/*
    char peek() override {
        return _stream.peek();
    }
*/
    int read() override {
        uint8_t val;
        if(uart_read_bytes(_uart, (void*) &val, 1, 0) == 1) {
            return val;
        }
        return -1;
    }
};

#endif /* A76XX_MODEMUART_ESP_H_ */
