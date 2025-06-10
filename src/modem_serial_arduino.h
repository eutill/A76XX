#ifndef A76XX_MODEMUART_ARDUINO_H_
#define A76XX_MODEMUART_ARDUINO_H_
//#define ARDUINO
#ifdef ARDUINO

#include "modem_serial.h"
#include "Arduino.h"

/*
    @brief Check if the last characters in the character buffer match with a given string.

    @param [IN] buf The character buffer.
    @param [IN] str The string to be matched.
    @return True in case of a match.
*/
template<size_t N>
bool endsWith(CircularBuffer<char, N>& buf, const char* str) {
    if (strlen(str) > buf.size()) { return false; }
    const char* m = str + strlen(str) - 1; // pointer to last character in str
    int i = 1;
    while (i <= buf.size() && i <= strlen(str) ) {
        if (buf[buf.size() - i] != *m) {
            return false;
        }
        m--;
        i++;
    }
    return true;
}

class TimeoutCalc {
public:
    TimeoutCalc(uint32_t timeoutMs) {
        _start = millis();
        _duration = timeoutMs;
    }
    bool expired(void) {
        return(!(millis() - _start < _duration));
    }

private:
    uint32_t _start;
    uint32_t _duration;
};

class ModemSerialArduino : public ModemSerial {
  private:
    Stream& _stream;

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
    ModemSerialArduino(Stream& stream)
        : _stream(stream) {_stream.setTimeout(A76XX_SERIAL_TIMEOUT_DEFAULT);}

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
                            bool match_ERROR = true) {
        // store data here
        CircularBuffer<char, 64> data;

        // start timer
        auto tstart = millis();

        while (millis() - tstart < timeout) {
            if (available() > 0) {
                data.push(static_cast<char>(read()));

                // parse modem output for any URCs that we need to process
                for (uint8_t i = 0; i < _num_event_handlers; i++) {
                    if (endsWith(data, _event_handlers[i]->match_string)) {
                        _event_handlers[i]->process(this);
                    }
                }

                if (match_1 != NULL && endsWith(data, match_1) == true)
                    return Response_t::A76XX_RESPONSE_MATCH_1ST;

                if (match_2 != NULL && endsWith(data, match_2) == true)
                    return Response_t::A76XX_RESPONSE_MATCH_2ND;

                if (match_3 != NULL && endsWith(data, match_3) == true)
                    return Response_t::A76XX_RESPONSE_MATCH_3RD;

                if (match_ERROR && endsWith(data, RESPONSE_ERROR) == true)
                    return Response_t::A76XX_RESPONSE_ERROR;

                if (match_OK && endsWith(data, RESPONSE_OK) == true)
                    return Response_t::A76XX_RESPONSE_OK;
            }
        }

        return Response_t::A76XX_RESPONSE_TIMEOUT;
    }

    void printItem(const char* str) override {
        _stream.print(str);
        flush();
    }

    void printItem(uint16_t val) override {
        _stream.print(val);
        flush();
    }

    int available() override {
        return _stream.available();
    }

    long parseInt() override {
        return _stream.parseInt();
    }

    float parseFloat() override {
        return _stream.parseFloat();
    }

    void flush() override {
        _stream.flush();
    }

    int peek() override {
        return _stream.peek();
    }

    int read() override {
        return _stream.read();
    }

    bool find(char terminator) override {
        return _stream.find(terminator);
    }

    size_t write(const char* data) override {
        return _stream.write(data);
    }

    size_t write(const char* data, size_t size) override {
        return _stream.write(data, size);
    }

    size_t readBytesUntil(char terminator, char* buf, int len) override {
        return _stream.readBytesUntil(terminator, buf, len);
    }

    size_t readBytes(void* buf, int len) override {
        return _stream.readBytes(buf, len);
    }
};

#endif /* ARDUINO */

#endif /* A76XX_MODEMUART_ARDUINO_H_ */
