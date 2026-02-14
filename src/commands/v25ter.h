#ifndef A76XX_V25TER_CMDS_H_
#define A76XX_V25TER_CMDS_H_


/*
    @brief Commands in section 2 of the AT command manual version 1.09

    Command  | Implemented | Method | Function(s)
    -------- | ----------- | ------ |-----------------------
    ATD      |             |        |
    ATA      |             |        |
    ATH      |             |        |
    ATS0     |             |        |
    +++      |             |        |
    ATO      |             |        |
    ATI      |             |        |
    ATE      |     y       | WRITE  | commandEcho
    AT&V     |             |        |
    ATV      |             |        |
    AT&F     |             |        |
    ATQ      |             |        |
    ATX      |             |        |
    AT&W     |             |        |
    ATZ      |             |        |
    AT+CGMI  |             |        |
    AT+CGMM  |     y       | READ   | modelIdentification
    AT+CGMR  |     y       | READ   | revisionIdentification
    AT+CGSN  |             |        |
    AT+CSCS  |     y       | WRITE  | characterSet
    AT+GCAP  |             |        |
*/

typedef enum {
    A76XX_CHARSET_IRA = 0,
    A76XX_CHARSET_UCS2 = 1,
    A76XX_CHARSET_HEX = 2,
    A76XX_CHARSET_GSM = 3
} characterSet_t;

class V25TERCommands {
  public:
    ModemSerial& _serial;

    V25TERCommands(ModemSerial& serial)
        : _serial(serial) {}

    /*
        @brief Implementation for ATE - WRITE Command.
        @detail Enable/Disable command echo.
        @param [IN] enable whether to enable command echo.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR
    */
    int8_t commandEcho(bool enable) {
        _serial.sendCMD("ATE", enable ? "1" : "0");
        switch(_serial.waitResponse(120000)) {
            case Response_t::A76XX_RESPONSE_OK : {
                return A76XX_OPERATION_SUCCEEDED;
            }
            case Response_t::A76XX_RESPONSE_TIMEOUT : {
                return A76XX_OPERATION_TIMEDOUT;
            }
            default : {
                return A76XX_GENERIC_ERROR;
            }
        }
    }

    /*
        @brief Implementation for CGMM - WRITE Command.
        @detail Get model identification string
        @param [IN] model will contain the model string on successful execution.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR
    */
    int8_t modelIdentification(char* buf, size_t len) {
        // clear stream before sending command, then get rid of the first line
        _serial.clear();
        _serial.sendCMD("AT+CGMM");
        _serial.find('\n');
        size_t readLen = _serial.readBytesUntil('\r', buf, len);
        _serial.clear();
        if(readLen > 0) {
            if(readLen == len) return A76XX_OUT_OF_MEMORY;
            //add terminating null char
            buf[readLen] = '\0';
            return A76XX_OPERATION_SUCCEEDED;
        }

        /*
        TimeoutCalc tc(5000);
        while (!tc.expired()) {
            if (_serial.available() > 0) {
                char c = static_cast<char>(_serial.read());
                if (c == '\r') {
                    _serial.clear();
                    return A76XX_OPERATION_SUCCEEDED;
                }
                model += c;
            }
        }*/
        return A76XX_OPERATION_TIMEDOUT;
    }

    /*
        @brief Implementation for CGMR - WRITE Command.
        @detail Get model revision string
        @param [IN] model will contain the revision string on successful execution.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR
    */
    int8_t revisionIdentification(char* buf, size_t len) {
        _serial.sendCMD("AT+CGMR");
        switch (_serial.waitResponse("+CGMR: ", 9000, false, false)) {
            case Response_t::A76XX_RESPONSE_MATCH_1ST : {
                /*TimeoutCalc tc(5000);
                while (!tc.expired()) {
                    if (_serial.available() > 0) {
                        char c = static_cast<char>(_serial.read());
                        if (c == '\r') {
                            _serial.clear();
                            return A76XX_OPERATION_SUCCEEDED;
                        }
                        revision += c;
                    }
                }*/
                size_t readLen = _serial.readBytesUntil('\r', buf, len);
                _serial.clear();
                if(readLen > 0) {
                    if(readLen == len) return A76XX_OUT_OF_MEMORY;
                    //add terminating null char
                    buf[readLen] = '\0';
                    return A76XX_OPERATION_SUCCEEDED;
                }
                return A76XX_OPERATION_TIMEDOUT;
            }
            case Response_t::A76XX_RESPONSE_TIMEOUT : {
                return A76XX_OPERATION_TIMEDOUT;
            }
            default : {
                return A76XX_GENERIC_ERROR;
            }
        }
    }

    /*
        @brief Implementation for CSCS - WRITE Command.
        @detail Set the character set for string input/output operations such as SMS r/w, phonebook etc.
        @param [IN] charset contains the desired character set.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR
    */
    int8_t characterSet(characterSet_t charset = A76XX_CHARSET_IRA) {
        const char* charSetStr[] = {
            "IRA",
            "UCS2",
            "HEX",
            "GSM"
        };
        _serial.sendCMD("AT+CSCS=\"", charSetStr[charset], '\"');
        Response_t rsp = _serial.waitResponse();
        A76XX_RESPONSE_PROCESS(rsp)
   }

};

#endif /* A76XX_V25TER_CMDS_H_ */
