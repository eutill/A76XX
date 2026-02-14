#include "A76XX.h"

void SMSOnMessageRx::process(ModemSerial* serial) {
    // In this event-driven method, we don't want to further process the
    // message, because Control might be busy with some other stuff.
    // We only provide the message index to the callback function and
    // Control might evaluate the message at an appropriate point in time.

    // find index from URC
    // URC typically looks like this: +CMTI: <mem3>,<index>
    // We don't need to evaluate <mem3> (it should always be "SM"),
    // so we can discard it

    serial->find(',');
    uint8_t smsIdx = serial->parseInt();
    serial->find('\n');

    if(_smsEvtCb) _smsEvtCb(smsIdx);

}

A76XXSMSClient::A76XXSMSClient(A76XX& modem, smsEvtCb_t smsCallback)
    : A76XXBaseClient(modem)
    , _sms_cmds(_serial)
    , _on_message_rx_handler(smsCallback)
    , _messageReference(123) {
        _serial.registerEventHandler(&_on_message_rx_handler);
    }

bool A76XXSMSClient::begin() {
    int8_t retcode;
    
    // set standard storage to be SIM Card
    retcode = _sms_cmds.setStorage();
    A76XX_CLIENT_RETCODE_ASSERT_BOOL(retcode)
    
    // set SMS notification setting to be URC+save (AT+CNMI=2,1)
    retcode = _sms_cmds.setNotification();
    A76XX_CLIENT_RETCODE_ASSERT_BOOL(retcode)

    return true;
}

// Read the message at the given index.
// index [in] The index at the given memory location where the message resides.
// msg [in] A pointer to a message struct that the message is going to write to
// Returns true if read was successful and msg contains valid data.

bool A76XXSMSClient::read(uint8_t index, SMSMessage_t* msg) {
    uint8_t pduBuf[200];
    uint16_t pduLen;

    if(_sms_cmds.read(index, pduBuf, 200, &pduLen, &msg->status) != A76XX_OPERATION_SUCCEEDED) {
        return false;
    }
    
    // Decode PDU
    uint16_t pduIdx = 0;

    // Read SMSC information length, then skip it
    pduIdx += 1 + pduBuf[0];

    // Read first octet
    uint8_t firstOctet = pduBuf[pduIdx];
    if(firstOctet & 0x03) {
        // Message Type Indicator other than SMS-DELIVER (0x00) – not supported here
        return false;
    }
    bool containsUDH = firstOctet & 0x40; // Flag for User Data Header
    
    uint8_t addrLen = pduBuf[++pduIdx];
    uint8_t typeOfAddr = pduBuf[++pduIdx];

    switch(typeOfAddr & 0x70) {
        case 0x50:
            {
            // Type of Number is 7-bit alphanumeric
            // determine number of significant 7-bit characters from addrLen
            uint16_t charLen = addrLen*4 / 7;
            if(charLen > (SMS_SENDER_BUFFER_LEN - 1)) {
                return false;
            }
            unpack7Bit(pduBuf+pduIdx+1, charLen, (uint8_t*) msg->sender);
            decodeGSM((uint8_t*) msg->sender, charLen);
            msg->sender[charLen] = '\0'; // append terminating null char
            break;
            }
        case 0x10:
            {
            // Type of Number is International
            // prepend a plus sign
            msg->sender[0] = '+';
            if((addrLen+1) > (SMS_SENDER_BUFFER_LEN - 1)) {
                return false;
            }
            extractBcdDigits(pduBuf+pduIdx+1, addrLen, msg->sender+1);
            msg->sender[addrLen+1] = '\0'; // terminating null char
            break;
            }
        default:
            {
            // Type of Number is a regular number, represented as half-byte, inverse BCD code
            if(addrLen > (SMS_SENDER_BUFFER_LEN - 1 )) {
                return false;
            }
            extractBcdDigits(pduBuf+pduIdx+1, addrLen, msg->sender);
            msg->sender[addrLen] = '\0'; // terminating null char
            break;
            }
    }

    pduIdx += 1 + ((addrLen+1)>>1);
    pduIdx++; // ignore TP-PID

    uint8_t dcs = pduBuf[pduIdx];
    pduIdx += 8; // ignore TP-SCTS

    uint8_t udl = pduBuf[pduIdx++]; // User Data Length

    uint8_t headerLen = 0;
    if(containsUDH) {
        headerLen = pduBuf[pduIdx] + 1; // UDHL plus length byte itself
    }

    switch(dcs) {
        case 0x00:
        {
            // 7-bit GSM encoding
            uint8_t fillBits = 0;

            if(containsUDH) {
                fillBits = (7 - (headerLen % 7)) % 7;
                udl -= (8*headerLen + fillBits) / 7;
            }

            if(udl > SMS_DATA_BUFFER_LEN) {
                return false;
            }
            
            unpack7Bit(pduBuf+pduIdx+headerLen, udl, msg->raw.data, fillBits);
            msg->raw.length = udl;
            msg->raw.encoding = SMS_GSM_CHAR;

            if(udl > (SMS_DECODED_BUFFER_LEN - 1)) {
                return false;
            }

            decodeGSM(msg->raw.data, udl, false, msg->decoded);
            msg->decoded[udl] = '\0'; // terminating null char
            break;
        }
        case 0x08:
        {
            // 16-bit UCS-2 encoding
            if(containsUDH) {
                udl -= headerLen;
            }

            if(udl > SMS_DATA_BUFFER_LEN) {
                return false;
            }

            memcpy(msg->raw.data, pduBuf+pduIdx+headerLen, udl);

            msg->raw.length = udl;
            msg->raw.encoding = SMS_UCS2_CHAR;
            
            uint8_t decodedLen = udl >> 1; // counts characters rather than bytes
            if(decodedLen > (SMS_DECODED_BUFFER_LEN - 1)) {
                return false;
            }
            
            decodeUCS2(pduBuf+pduIdx+headerLen, decodedLen, msg->decoded);
            msg->decoded[decodedLen] = '\0'; // terminating null char
            break;
        }
        default:
            return false;
            break;
    }
    
    return true;
}


/*
    @brief Send an SMS
    @detail Send a regular SMS, using the 7-bit GSM encoding. No special characters! Splits up text into multipart SMS, if necessary.
    @param [IN] destination contains a null-terminated char array with the destination address (e.g. telephone number prepended with +)
    @param [IN] text contains the ASCII-encoded message text. 459 characters maximum! Null-terminated.
*/
bool A76XXSMSClient::send(char* destination, char* text) {
    // Send 7-bit GSM encoded single or multiparted message.
    // Provide destination and text in normal ASCII format (char array).
    uint8_t totalParts = 1;
    bool multipart = false;
    uint8_t maxBytes;
    uint16_t textLen = strlen(text);

    if(textLen > 459) return false; // text too long for three-parted message (3*153 chars)
    if(textLen > 160) { // multipart message
        totalParts = (textLen + 152) / 153;
        multipart = true;
        maxBytes = 153;
    } else {
        maxBytes = 160;
    }

    SMSMultipart_t multipartInfo;
    if(multipart) {
        multipartInfo.reference = _messageReference++;
        multipartInfo.total = totalParts;
    }

    uint16_t messageIdx = 0;
    uint16_t sendBytes;

    SMS_UserData_t partMsg;
    partMsg.encoding = SMS_GSM_CHAR;


    for(uint8_t part=1; part<=totalParts; part++) {
        if(messageIdx >= textLen) {
            return false; // This shouldn't happen
        }
        
        sendBytes = textLen - messageIdx;
        sendBytes = (sendBytes < maxBytes) ? sendBytes : maxBytes;

        encodeGSM(text + messageIdx, false, partMsg.data, sendBytes);
        messageIdx += sendBytes;

        partMsg.length = sendBytes;
        if(multipart) multipartInfo.sequence = part;

        if(!this->sendSingle(destination, &partMsg, multipart ? &multipartInfo : NULL)) return false;
    }
    
    return true;
}

bool A76XXSMSClient::sendWithComment(char* destination, SMS_UserData_t* message, char* comment) {
    // Prepend ASCII-encoded comment to message and send them, if necessary in multiple segments.
    uint8_t totalParts = 1;
    uint16_t commentLen = strlen(comment);
    bool multipart = false;
    uint8_t maxBytes;
    uint16_t resLen;
    
    // Calculate total length and length of each segment
    switch(message->encoding) {
        case SMS_GSM_CHAR:
            resLen = message->length + commentLen; // Number of chars
            if(resLen > 459) // 3 * 153 chars (160 chars - 7 chars of Multipart Header)
                return false;
            if(resLen > 160) { // multipart
                totalParts = (resLen + 152) / 153;
                multipart = true;
                maxBytes = 153;
            } else {
                maxBytes = 160;
            }
            break;
        case SMS_UCS2_CHAR:
            commentLen <<= 1; // One char in ASCII is two bytes in UCS2
            resLen = message->length + commentLen; // Number of bytes, not UCS2 chars!
            if(resLen > 402) // 3 * 134 Bytes (140 Bytes - 6 Bytes of Multipart Header)
                return false;
            if(resLen > 140) { // multipart
                totalParts = (resLen + 133) / 134;
                multipart = true;
                maxBytes = 134;
            } else {
                maxBytes = 140;
            }
            break;
        case SMS_8BIT_DATA:
        default:
            // not supported
            return false;
            break;
    }
    
    SMSMultipart_t multipartInfo;
    if(multipart) {
        multipartInfo.reference = _messageReference++;
        multipartInfo.total = totalParts;
    }

    uint8_t partIdx;
    uint16_t commentIdx = 0, messageIdx = 0;
    SMS_UserData_t partMsg;
    partMsg.encoding = message->encoding;
    uint16_t sendBytes, freeBytes;

    for(uint8_t part=1; part<=totalParts; part++) {
        partIdx = 0;

        if(commentIdx < commentLen) { // There is still (part of) comment to send
            sendBytes = commentLen - commentIdx;
            sendBytes = (sendBytes < maxBytes) ? sendBytes : maxBytes;
            
            // we need to yet encode the comment
            switch(message->encoding) {
                case SMS_GSM_CHAR:
                    encodeGSM(comment + commentIdx, false, partMsg.data, sendBytes);
                    commentIdx += sendBytes;
                    partIdx += sendBytes;
                    break;
                case SMS_UCS2_CHAR:
                    for(uint16_t i=0; i<(sendBytes>>1); i++) { // Acting on characters
                        partMsg.data[partIdx++] = 0x00;
                        partMsg.data[partIdx++] = comment[commentIdx>>1];
                        commentIdx += 2;
                    }
                    break;
                default:
                    return false;
            }
        }

        if((messageIdx < message->length) && (partIdx < maxBytes)) {
            // Message hasn't been sent completely yet and there's still space in this segment
            sendBytes = message->length - messageIdx;
            freeBytes = maxBytes - partIdx;
            sendBytes = (sendBytes < freeBytes) ? sendBytes : freeBytes;

            memcpy(partMsg.data + partIdx, message->data + messageIdx, sendBytes);
            messageIdx += sendBytes;
            partIdx += sendBytes;
        }

        partMsg.length = partIdx;
        if(multipart) multipartInfo.sequence = part;

        if(!this->sendSingle(destination, &partMsg, multipart ? &multipartInfo : NULL)) return false;
    }

    return true;
}

bool A76XXSMSClient::sendSingle(char* destination, SMS_UserData_t* message, SMSMultipart_t* multipart) {
    // create PDU and send it
    uint8_t pduBuf[SMS_PDU_LEN];
    uint8_t pduIdx = 0;

    // omit SMSC address – set length 0
    pduBuf[pduIdx++] = 0x00;

    // set first octet / PDU type to be SMS-SUBMIT and factor in existence of User Data Header
    if(multipart) {pduBuf[pduIdx++] = 0x41;}
    else {pduBuf[pduIdx++] = 0x01;}

    // set MR (message reference) to be 0x00 – will be set by modem
    pduBuf[pduIdx++] = 0x00;

    // Destination Address.
    // First, determine length
    uint8_t daLen = strlen(destination);

    // If starts with a plus (+) sign, set a flag
    bool international = destination[0] == '+';
    if(international) {
        daLen--;
        destination++;
    }

    // Address length
    pduBuf[pduIdx++] = daLen;

    // Address type – national or international number
    if(international) {pduBuf[pduIdx++] = 0x91;}
    else {pduBuf[pduIdx++] = 0x81;}

    // Address itself – half-byte BCDs
    storeBcdDigits(destination, daLen, pduBuf+pduIdx);
    pduIdx += (daLen+1) >> 1;

    // Protocol identifier (PID)
    pduBuf[pduIdx++] = 0x00;

    // Data coding scheme (DCS) depends on user data type
    switch(message->encoding) {
        case SMS_8BIT_DATA:
            pduBuf[pduIdx++] = 0x04;
            break;
        case SMS_UCS2_CHAR:
            pduBuf[pduIdx++] = 0x08;
            break;
        case SMS_GSM_CHAR:
        default:
            pduBuf[pduIdx++] = 0x00;
            break;
    }

    // TP-VP (Validity Period) field not present (as set in first octet)

    // TP-UDL (User Data Length) in characters (GSM) or bytes (UCS2, 8-bit data)
    uint8_t udl = message->length;
    uint8_t fillbits = 0;
    if(multipart) {
        switch(message->encoding) {
            case SMS_GSM_CHAR:
                // For 6-byte UDH (and one fill bit), length increases by 7 equivalent 7-bit characters
                pduBuf[pduIdx++] = udl + 7;
                fillbits = 1;
                break;
            case SMS_UCS2_CHAR:
            case SMS_8BIT_DATA:
            default:
                pduBuf[pduIdx++] = udl + 6;
                break;
        }

        // create UDH
        // TP-UDHL (header length excluding this byte)
        pduBuf[pduIdx++] = 0x05;

        // IEI (Info Element ID) – 0x00 for multipart messages
        pduBuf[pduIdx++] = 0x00;

        // IEDL (Info Element Data Length)
        pduBuf[pduIdx++] = 0x03;

        // Multipart Message Reference Number
        pduBuf[pduIdx++] = multipart->reference;

        // Multipart Total parts
        pduBuf[pduIdx++] = multipart->total;

        // Multipart Sequence
        pduBuf[pduIdx++] = multipart->sequence;

    } else {
        pduBuf[pduIdx++] = udl;
    }

    if(message->encoding == SMS_GSM_CHAR) {
        pduIdx += pack7Bit(message->data, udl, pduBuf + pduIdx, fillbits);
    } else {
        memcpy(pduBuf + pduIdx, message->data, udl);
        pduIdx += udl;
    }

    int8_t ret = _sms_cmds.send(pduBuf, pduIdx);
    A76XX_CLIENT_RETCODE_ASSERT_BOOL(ret)

    return true;
}

uint8_t A76XXSMSClient::list(SMSPosition_t* positions, uint8_t positionsLen, SMSStatus_t statusFilter) {
    return _sms_cmds.list(positions, positionsLen, statusFilter);
}

bool A76XXSMSClient::remove(uint8_t index) {
    int8_t retcode = _sms_cmds.remove(index);
    A76XX_CLIENT_RETCODE_ASSERT_BOOL(retcode)
    return true;
}