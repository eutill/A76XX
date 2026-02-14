#ifndef A76XX_SMS_CMDS_H_
#define A76XX_SMS_CMDS_H_

/*
    @brief Commands in section 9 of the AT command manual

    Command         | Implemented | Function
    --------------- | ----------- | ------------------
    CSMS            |             |
    CPMS            |      y      | setStorage
    CMGF            |      y      | setMessageFormat
    CSCA            |             |
    CSCB            |             |
    CSMP            |      y      | setTextModeParam
    CSDH            |             |
    CNMA            |             |
    CNMI            |      y      | setNotification
    CGSMS           |             |
    CMGL            |      y      | list
    CMGR            |      y      | read
    CMGS            |      y      | send
    CMSS            |             |
    CMGW            |             |
    CMGD            |      y      | remove
    CMGMT           |             |
    CMVP            |             |
    CMGRD           |             |
    CMGSEX          |             |
    CMSSEX          |             |
    CCONCINDEX      |             |
*/

enum SMSStatus_t {
    SMS_STATUS_REC_UNREAD = 0,
    SMS_STATUS_REC_READ = 1,
    SMS_STATUS_STO_UNSENT = 2,
    SMS_STATUS_STO_SENT = 3,
    SMS_STATUS_ALL = 4
};

struct SMSPosition_t {
    uint8_t index;
    SMSStatus_t status;
    uint16_t length;
};

class SMSCommands {
  public:
    ModemSerial& _serial;

    SMSCommands(ModemSerial& serial)
        : _serial(serial) {}
    
    // CPMS
    int8_t setStorage() {
        // reset SMS storage by issuing command without parameters
        // -> default storage (SIM Card) is set
        _serial.sendCMD("AT+CPMS");
        Response_t rsp = _serial.waitResponse("+CPMS: ", 9000, false, true);

        switch(rsp) {
            case Response_t::A76XX_RESPONSE_MATCH_1ST:
            break;
            
            case Response_t::A76XX_RESPONSE_TIMEOUT:
            return A76XX_OPERATION_TIMEDOUT;
            break;

            case Response_t::A76XX_RESPONSE_ERROR:
            case Response_t:: A76XX_RESPONSE_OK:
            default:
            return A76XX_GENERIC_ERROR;
            break;
        }
        
        _serial.find('\n');
        rsp = _serial.waitResponse();

        A76XX_RESPONSE_PROCESS(rsp)
    }

    enum SMSMsgFormat_t {
        SMS_PDU_MODE = 0,
        SMS_TEXT_MODE = 1
    };

    // CMGF
    // Set the message format to be either PDU or text mode
    int8_t setMessageFormat(SMSMsgFormat_t msgFormat) {
        _serial.sendCMD("AT+CMGF=", (uint16_t) msgFormat);
        Response_t rsp = _serial.waitResponse(9000);
        A76XX_RESPONSE_PROCESS(rsp)
    }

    // CSMP
    // set text mode parameters.
    // DCS must be compatible with current AT+CSCS setting! (see characterSet cmd)
    int8_t setTextModeParam(uint8_t fo = 17, uint8_t vp = 167, uint8_t pid = 0, uint8_t dcs = 0) {
        _serial.sendCMD("AT+CSMP=", fo, ',', vp, ',', pid, ',', dcs);
        Response_t rsp = _serial.waitResponse("+CMS ERROR: ", 9000);
        A76XX_RESPONSE_PROCESS(rsp)
    }


    // CNMI
    // set new message notification settings to default values
    int8_t setNotification() {
        _serial.sendCMD("AT+CNMI");
        Response_t rsp = _serial.waitResponse(9000);

        A76XX_RESPONSE_PROCESS(rsp)
    }

    // CMGL
    // write into given array of SMSPosition_t
    // return value is not an error code, but number of found messages
    uint8_t list(SMSPosition_t* positions, uint8_t positionsLen, SMSStatus_t statusFilter) {
        if((positionsLen == 0) || (positions == NULL))
            return 0;
        
        // device must be in PDU mode
        if(this->setMessageFormat(SMS_PDU_MODE) != A76XX_OPERATION_SUCCEEDED)
            return 0;
        
        _serial.sendCMD("AT+CMGL=", (uint16_t) statusFilter);

        uint8_t foundMsg = 0;
        Response_t rsp;
        while(1) {
            rsp = _serial.waitResponse("+CMGL: ", 9000);
            if(rsp == Response_t::A76XX_RESPONSE_MATCH_1ST) {
                // found message
                positions[foundMsg].index = _serial.parseInt(); _serial.find(',');
                positions[foundMsg].status = (SMSStatus_t) _serial.parseInt(); _serial.find(',');
                _serial.find(','); // ignore <alpha>
                positions[foundMsg].length = _serial.parseInt(); _serial.find('\n');
                _serial.find('\n'); // ignore message PDU

                if(++foundMsg == positionsLen) {
                    // we cannot store more data
                    // discard all following messages, wait for OK
                    _serial.waitResponse(9000);
                    return foundMsg;
                }
            } else {
                // OK, ERROR or timeout
                return foundMsg;
            }
        }
    }

    // CMGR
    // reads the message at given index in PDU mode
    // and converts the hexadecimal format into binary representation
    int8_t read(uint8_t index, uint8_t* buffer, uint16_t bufLen, uint16_t* msgLen, SMSStatus_t* msgStatus = NULL) {
        if((buffer == NULL) || (bufLen == 0) || (msgLen == NULL)) {
            return A76XX_OUT_OF_MEMORY;
        }

        // first, make sure we are in PDU mode
        if(this->setMessageFormat(SMS_PDU_MODE) != A76XX_OPERATION_SUCCEEDED) {
            return A76XX_GENERIC_ERROR;
        }

        // send read command
        _serial.sendCMD("AT+CMGR=", index);

        // evaluate response
        Response_t rsp = _serial.waitResponse("+CMGR: ", "+CMS ERROR: ", 9000);
        switch(rsp) {
            case Response_t::A76XX_RESPONSE_MATCH_1ST:
            {
                if(msgStatus) {
                    *msgStatus = (SMSStatus_t) _serial.parseInt();
                }
                _serial.find(',');
                _serial.find(','); // skip <alpha>
                
                uint16_t pduLen = _serial.parseInt(); _serial.find('\n');
                
                // pduLen is in bytes
                // pduLen doesn't take SMSC info at PDU beginning into account.
                // read first two bytes (SMSC len) to calculate memory requirements for output buffer
                char high, low;
                uint16_t bufCount = 1;

                _serial.readBytes(&high, 1);
                _serial.readBytes(&low, 1);

                if(!(checkHexDec(high) && checkHexDec(low))) {
                    return A76XX_GENERIC_ERROR;
                }
                
                hexPairToByte(high, low, buffer);
                
                *msgLen = buffer[0] + 1 + pduLen;
                if(*msgLen > bufLen) {
                    return A76XX_OUT_OF_MEMORY;
                }

                for(;bufCount < *msgLen; bufCount++) {
                    _serial.readBytes(&high, 1);
                    _serial.readBytes(&low, 1);

                    // check for valid hexdec data
                    if(!(checkHexDec(high) && checkHexDec(low))) {
                        return A76XX_GENERIC_ERROR;
                    }

                    hexPairToByte(high, low, buffer + bufCount);
                }

                // wait for terminal OK
                rsp = _serial.waitResponse(9000);
                A76XX_RESPONSE_PROCESS(rsp)

                break;
            }
            default:
                return A76XX_GENERIC_ERROR;
                break;
        }
    }

    // CMGS
    // Takes a PDU in binary format, hex-encodes it and attempts to send it in PDU mode.
    int8_t send(uint8_t* pdu, uint8_t length) {
        if(pdu == NULL || length == 0) return A76XX_GENERIC_ERROR;
        int8_t retcode;

        // Set PDU mode
        retcode = this->setMessageFormat(SMS_PDU_MODE);
        A76XX_RETCODE_ASSERT_RETURN(retcode)

        _serial.sendCMD("AT+CMGS=", length - 1); // don't count SMSC length byte in PDU length
        _serial.find('>');

        char highByte, lowByte;

        for(int i=0; i<length; i++) {
            byteTohexPair(pdu[i], &highByte, &lowByte);
            _serial.printCMD(highByte, lowByte);
        }
        _serial.sendCMD((char) 0x1A);
        
        Response_t rsp = _serial.waitResponse("+CMGS: ", "+CMS ERROR: ", 40000);
        if(rsp == Response_t::A76XX_RESPONSE_MATCH_1ST) {
            // await OK
            rsp = _serial.waitResponse(40000);
            A76XX_RESPONSE_PROCESS(rsp)
        } else {
            A76XX_RESPONSE_PROCESS(rsp)
        }

    }

    // CMGD
    // Delete a message
    int8_t remove(uint8_t index, uint8_t delflag = 0) {
        _serial.sendCMD("AT+CMGD=", index, ',', delflag);
        Response_t rsp = _serial.waitResponse("+CMS ERROR: ", 9000);

        A76XX_RESPONSE_PROCESS(rsp)
    }
};


#endif /*A76XX_SMS_CMDS_H_*/