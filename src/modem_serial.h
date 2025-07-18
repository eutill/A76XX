#ifndef A76XX_MODEMUART_H_
#define A76XX_MODEMUART_H_

#include "A76XX.h"

class ModemSerial {
  protected:
    EventHandler_t*              _event_handlers[A76XX_MAX_EVENT_HANDLERS];
    uint8_t                                            _num_event_handlers;

  public:
    ModemSerial() : _num_event_handlers(0) {}

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
    virtual Response_t waitResponse(const char* match_1,
                                const char* match_2,
                                const char* match_3,
                                uint32_t timeout = 1000,
                                bool match_OK = true,
                                bool match_ERROR = true) = 0;

    /*
        @brief Wait for modem to respond.

        @detail Consume data from the serial port until a match is found with the 
            input string or before the operation times out. If the parameters
            `match_OK` and `match_ERROR` are true, we also check if the response
            matches with the default OK and ERROR strings, but precedence is given
            to matching the input string, which is useful for some commands.
        @param [IN] match_1 The first string to match.
        @param [IN] match_2 The second string to match.
        @param [IN] timeout Return if no match is found within this time in milliseconds.
            Default is 1000 milliseconds.
        @param [IN] match_OK Whether to match the string "OK\r\n". Default is true.
        @param [IN] match_ERROR Whether to match the string "ERROR\r\n". Default is true.

        @return A Response_t object.
    */
    Response_t waitResponse(const char* match_1,
                            const char* match_2,
                            uint32_t timeout = 1000,
                            bool match_OK = true,
                            bool match_ERROR = true) {
        return waitResponse(match_1, match_2, NULL, timeout, match_OK, match_ERROR);
    }

    /*
        @brief Wait for modem to respond.

        @detail Consume data from the serial port until a match is found with the 
            input string or before the operation times out. If the parameters
            `match_OK` and `match_ERROR` are true, we also check if the response
            matches with the default OK and ERROR strings, but precedence is given
            to matching the input string, which is useful for some commands.
        @param [IN] match_1 The string to match.
        @param [IN] timeout Return if no match is found within this time in milliseconds.
            Default is 1000 milliseconds.
        @param [IN] match_OK Whether to match the string "OK\r\n". Default is true.
        @param [IN] match_ERROR Whether to match the string "ERROR\r\n". Default is true.

        @return A Response_t object.
    */
    Response_t waitResponse(const char* match_1, 
                            uint32_t timeout = 1000,
                            bool match_OK = true,
                            bool match_ERROR = true) {
        return waitResponse(match_1, NULL, NULL, timeout, match_OK, match_ERROR);
    }

    /*
        @brief Wait for modem to respond.

        @detail Consume data from the serial port until a match with the default 
            OK and ERROR strings, if the parameters `match_OK` and `match_ERROR` are true.
        @param [IN] timeout Return if no match is found within this time in milliseconds.
            Default is 1000 milliseconds.
        @param [IN] match_OK Whether to match the string "OK\r\n". Default is true.
        @param [IN] match_ERROR Whether to match the string "ERROR\r\n". Default is true.

        @return A Response_t object.
    */
    Response_t waitResponse(uint32_t timeout = 1000,
                            bool match_OK = true,
                            bool match_ERROR = true) {
        return waitResponse(NULL, NULL, NULL, timeout, match_OK, match_ERROR);
    }

    /*
        @brief Listen for URCs from the serial connection with the module.

        @param [IN] timeout Wait up to this time in ms before returning.
    */
    void listen(uint32_t timeout = 100) {
        waitResponse(timeout, false, false);
    }

    /* 
        @brief Register a new event handler.

        @param [IN] Pointer to a subclass of EventHandler_t.
    */
    void registerEventHandler(EventHandler_t* handler) {
        _event_handlers[_num_event_handlers++] = handler;
    }

    /* 
        @brief Deregister an exisiting event handler.

        @param [IN] Pointer to a subclass of EventHandler_t.
    */
    void deRegisterEventHandler(EventHandler_t* handler) {
        // Search for the element of _event_handlers that points to the
        // same address of the input, and then shift the elements left
        // by one to `delete` the handler that needs to be de-registered.
        // This can be replaced by a linked list for efficient removal, 
        // but registration/deregistration is only done occasionally and 
        // _num_event_handlers will typically be small
        for (uint8_t i = 0; i < _num_event_handlers; i++) {
            if (_event_handlers[i] == handler) {
                for (uint8_t j = i; j < _num_event_handlers; j++) {
                    _event_handlers[j] = _event_handlers[j+1];
                }
                _num_event_handlers--;
                return;
            }
        }
    }

    /*
        @brief Send data to the command to the module, with a trailing carriage return,
            line feed characters.

        @param [IN] args Items (string, numbers, ...) to be sent.
        @param [IN] finish Whether to send a terminating "\r\n" to the module.
    */
    template <typename... ARGS>
    void sendCMD(ARGS... args) {
        printCMD(args..., "\r\n");
    }

    /*
        @brief Print data to the module's serial port.

        @detail This is analogus to ::sendCMD, but without the trailing carriage return, 
            line feed characters.

        @param [IN] args Items (string, numbers, ...) to be sent.
        @param [IN] finish Whether to send a terminating "\r\n" to the module.
    */
    template <typename HEAD, typename... TAIL>
    void printCMD(HEAD head, TAIL... tail) {
        printItem(head);
        printCMD(tail...); //recursively calls this function until base case: no arguments
    }

    // single argument case
    virtual void printItem(uint16_t val) = 0;
    virtual void printItem(const char* str) = 0;
    virtual void printItem(char chr) = 0;
    virtual void printItem(int val) = 0;
    virtual void printItem(long unsigned int val) = 0;
    virtual void printItem(unsigned int val) = 0;

    // base case: do nothing
    void printCMD(void) {}

    /*
        @brief Parse an integer number and then consume all data available in the 
            serial interface until the default OK or ERROR strings are found, or 
            until the operation times out.

        @param [IN] timeout Time out in milliseconds. Default is 50 milliseconds.
        @return The integer.
    */
    int32_t parseIntClear(uint32_t timeout = 500) {
        int32_t retcode = parseInt();
        clear(timeout);
        return retcode;
    }

    /*
        @brief Consume all data available in the stream, until the default
            OK or ERROR strings are found, or until the operation times out.
            
        @param [IN] timeout Time out in milliseconds. Default is 50 milliseconds.
    */ 
    void clear(uint32_t timeout = 500) {
        // TODO: this should not be used anywhere!
        waitResponse(timeout);
    }

    // The following functions are simply forwarding the calls to underlying stream 
    // object. If you need others, send a pull request!
    virtual int available() = 0;
    virtual long parseInt() = 0;
    virtual float parseFloat() = 0;
    virtual void flush() = 0;
    virtual int peek() = 0;
    virtual int read() = 0;
    virtual bool find(char terminator) = 0;
    virtual size_t write(const char* data) = 0;
    virtual size_t write(const char* data, size_t size) = 0;
    virtual size_t readBytesUntil(char terminator, char* buf, int len) = 0;
    virtual size_t readBytes(void* buf, int len) = 0;

    virtual ~ModemSerial() {}
};

#endif /* A76XX_MODEMUART_H_ */
