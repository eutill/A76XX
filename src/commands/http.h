#ifndef A76XX_HTTP_CMDS_H_
#define A76XX_HTTP_CMDS_H_

/*
    @brief Commands in section 16 of the AT command manual version 1.09

    Command     | Implemented | Method | Function(s)
    ----------- | ----------- | ------ |-----------------
    HTTPINIT    |      y      | EXEC   | init
    HTTPTERM    |      y      | EXEC   | term
    HTTPPARA    |      y      | WRITE  | configHttp*
    HTTPACTION  |      y      | WRITE  | action
    HTTPHEAD    |      y      | EXEC   | readHeader
    HTTPREAD    |      y      | R/W    | getContentLength, readResponseBody
    HTTPDATA    |      y      | WRITE  | inputData
    HTTPPOSTFILE|             |        |
    HTTPREADFILE|             |        |
*/

class HTTPCommands {
  public:
    ModemSerial& _serial;

    HTTPCommands(ModemSerial& serial)
        : _serial(serial) {}

    // HTTPINIT
    int8_t init() {
        _serial.sendCMD("AT+HTTPINIT");
        A76XX_RESPONSE_PROCESS(_serial.waitResponse(120000))
    }

    // HTTPTERM
    int8_t term() {
        _serial.sendCMD("AT+HTTPTERM");
        A76XX_RESPONSE_PROCESS(_serial.waitResponse(120000))
    }

    // HTTPPARA URL
    int8_t configHttpURL(const char* server, uint16_t port, const char* path, bool use_ssl) {
        // add the protocol if not present
        if (strstr(server, "https://") == NULL && strstr(server, "http://") == NULL) {
            if (use_ssl == true) {
                _serial.sendCMD("AT+HTTPPARA=\"URL\",", "\"https://", server, ":", port, "/", path, "\"");
            } else {
                _serial.sendCMD("AT+HTTPPARA=\"URL\",", "\"http://", server, ":", port, "/", path, "\"");
            }
        } else {
            _serial.sendCMD("AT+HTTPPARA=\"URL\",", "\"", server, ":", port, "/", path, "\"");
        }
        A76XX_RESPONSE_PROCESS(_serial.waitResponse(120000))
    }

    // HTTPPARA CONNECTTO
    int8_t configHttpConnTimeout(int conn_timeout) {
        _serial.sendCMD("AT+HTTPPARA=\"CONNECTTO\",", conn_timeout);
        A76XX_RESPONSE_PROCESS(_serial.waitResponse(120000))
    }

    // HTTPPARA RECVTO
    int8_t configHttpRecvTimeout(int recv_timeout) {
        _serial.sendCMD("AT+HTTPPARA=\"RECVTO\",", recv_timeout);
        A76XX_RESPONSE_PROCESS(_serial.waitResponse(120000))
    }

    // HTTPPARA CONTENT
    int8_t configHttpContentType(const char* content_type) {
        _serial.sendCMD("AT+HTTPPARA=\"CONTENT\",\"", content_type, "\"");
        A76XX_RESPONSE_PROCESS(_serial.waitResponse(120000))
    }

    // HTTPPARA ACCEPT
    int8_t configHttpAccept(const char* accept) {
        _serial.sendCMD("AT+HTTPPARA=\"ACCEPT\",\"", accept, "\"");
        A76XX_RESPONSE_PROCESS(_serial.waitResponse(120000))
    }

    // HTTPPARA SSLCFG
    int8_t configHttpSSLCfgId(uint8_t sslcfg_id) {
        _serial.sendCMD("AT+HTTPPARA=\"SSLCFG\",", sslcfg_id);
        A76XX_RESPONSE_PROCESS(_serial.waitResponse(120000))
    }

    // HTTPPARA USERDATA
    int8_t configHttpUserData(const char* header, const char* value) {
        _serial.sendCMD("AT+HTTPPARA=\"USERDATA\",\"", header, ":", value, "\"");
        A76XX_RESPONSE_PROCESS(_serial.waitResponse(120000))
    }

    // HTTPPARA READMODE
    int8_t configHttpReadMode(uint8_t readmode) {
        _serial.sendCMD("AT+HTTPPARA=\"READMODE\",", readmode);
        A76XX_RESPONSE_PROCESS(_serial.waitResponse(120000))
    }

    // HTTPACTION
    // 0 =>    GET
    // 1 =>   POST
    // 2 =>   HEAD
    // 3 => DELETE
    // 4 =>    PUT
    int8_t action(uint8_t method, uint16_t* status_code, uint32_t* length) {
        _serial.sendCMD("AT+HTTPACTION=", method);
        Response_t rsp = _serial.waitResponse("+HTTPACTION: ", 120000, false, true);
        switch (rsp) {
            case Response_t::A76XX_RESPONSE_MATCH_1ST : {
                _serial.parseInt();
                _serial.find(',');
                *status_code = _serial.parseInt();
                _serial.find(',');
                *length = _serial.parseInt();
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

    // HTTPHEAD
    int8_t readHeader(char* header, size_t max_len) {
        _serial.sendCMD("AT+HTTPHEAD");
        Response_t rsp = _serial.waitResponse("+HTTPHEAD: ", 120000, false, true);
        switch (rsp) {
            case Response_t::A76XX_RESPONSE_MATCH_1ST : {
                // get length of header
                int header_length = _serial.parseInt();

                // reserve space for the string
                if (max_len-1 < header_length) {
                    return A76XX_OUT_OF_MEMORY;
                }

                // advance till we start with the actual content
                _serial.find('\n');

                // read as many bytes as we said
                size_t readLen = _serial.readBytes(header, header_length);
                header[readLen] = '\0';
                /*
                for (uint32_t i = 0; i < header_length; i++) {
                    while (_serial.available() == 0) {}
                    header += static_cast<char>(_serial.read());
                }*/

                if (_serial.waitResponse() == Response_t::A76XX_RESPONSE_OK) {
                    if(readLen != header_length) return A76XX_GENERIC_ERROR;
                    return A76XX_OPERATION_SUCCEEDED;
                } else {
                    return A76XX_GENERIC_ERROR;
                }
            }
            case Response_t::A76XX_RESPONSE_TIMEOUT : {
                return A76XX_OPERATION_TIMEDOUT;
            }
            default : {
                return A76XX_GENERIC_ERROR;
            }
        }
    }

    int8_t getContentLength(uint32_t* len) {
        _serial.sendCMD("AT+HTTPREAD?");
        Response_t rsp = _serial.waitResponse("+HTTPREAD: LEN,", 120000, false, true);
        switch (rsp) {
            case Response_t::A76XX_RESPONSE_MATCH_1ST : {
                *len = _serial.parseIntClear();
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

    // HTTPREAD - read entire response. When calling, be sure that the provided array
    // can host up to body_length + 1 characters!
    int8_t readResponseBody(char* body, uint32_t body_length) {
        _serial.sendCMD("AT+HTTPREAD=", 0, ",", body_length);
        Response_t rsp = _serial.waitResponse("+HTTPREAD: ", 120000, false, true);
        switch (rsp) {
            case Response_t::A76XX_RESPONSE_MATCH_1ST : {
                // this should match with body_length
                if (_serial.parseInt() != body_length) {
                    return A76XX_GENERIC_ERROR;
                }
                /*
                // check we can allocate the string
                if (body.reserve(body_length) == 0) {
                    return A76XX_OUT_OF_MEMORY;
                }*/

                // advance till we start with the actual content
                _serial.find('\n');

                // read as many bytes as we said
                size_t readLen = _serial.readBytes(body, body_length);
                body[readLen] = '\0';
                /*
                for (uint32_t i = 0; i < body_length; i++) {
                    while (_serial.available() == 0) {}
                    body += static_cast<char>(_serial.read());
                }*/

                // clear stream
                if (_serial.waitResponse("+HTTPREAD: 0") == Response_t::A76XX_RESPONSE_MATCH_1ST) {
                    if(readLen != body_length) return A76XX_GENERIC_ERROR;
                    return A76XX_OPERATION_SUCCEEDED;
                } else {
                    return A76XX_GENERIC_ERROR;
                }
            }
            case Response_t::A76XX_RESPONSE_TIMEOUT : {
                return A76XX_OPERATION_TIMEDOUT;
            }
            default : {
                return A76XX_GENERIC_ERROR;
            }
        }
    }

    // HTTPDATA
    int8_t inputData(const char* data, uint32_t length) {
        // use 30 seconds timeout
        _serial.sendCMD("AT+HTTPDATA=", length, ",", 30);

        // timeout after 10 seconds
        Response_t rsp = _serial.waitResponse("DOWNLOAD", 10000, false, true);

        switch (rsp) {
            case Response_t::A76XX_RESPONSE_MATCH_1ST : {
                _serial.write(data, length);
                _serial.flush();
                switch (_serial.waitResponse()) {
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
            case Response_t::A76XX_RESPONSE_TIMEOUT : {
                return A76XX_OPERATION_TIMEDOUT;
            }
            default : {
                return A76XX_GENERIC_ERROR;
            }
        }
    }
};

#endif /* A76XX_HTTP_CMDS_H_ */
