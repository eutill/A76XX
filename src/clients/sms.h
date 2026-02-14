#ifndef A76XX_SMS_CLIENT_H_
#define A76XX_SMS_CLIENT_H_

enum SMS_Encoding_t {
  SMS_GSM_CHAR = 0, // un-packed 7-bit GSM characters (i.e., one per byte)
  SMS_8BIT_DATA = 1, // 8-bit data – currently not supported
  SMS_UCS2_CHAR = 2  // 16-bit, big-endian UCS-2 chars
};

struct SMSMultipart_t {
    uint8_t reference;
    uint8_t total;
    uint8_t sequence;
};

struct SMS_UserData_t {
  SMS_Encoding_t encoding; // encoding of data
  uint8_t data[SMS_DATA_BUFFER_LEN]; // non-ASCII, doesn't have terminating null char!
  uint16_t length; // number of characters (GSM) or of bytes (UCS2, 8-bit data)
};

struct SMSMessage_t {
  SMSStatus_t status;
  char sender[SMS_SENDER_BUFFER_LEN];
  SMS_UserData_t raw;
  char decoded[SMS_DECODED_BUFFER_LEN];
};

typedef void (*smsEvtCb_t) (uint8_t smsIndex);


class SMSOnMessageRx : public EventHandler_t {
  public:
    SMSOnMessageRx(smsEvtCb_t smsEvtCb) :
      EventHandler_t("+CMTI: "), _smsEvtCb(smsEvtCb) {}

    void process(ModemSerial* serial);

  private:
    smsEvtCb_t _smsEvtCb;
          
};

class A76XXSMSClient : public A76XXBaseClient {
  private:
    SMSCommands                                 _sms_cmds;
    SMSOnMessageRx                 _on_message_rx_handler;
    uint8_t                             _messageReference;

  public:
    A76XXSMSClient(A76XX& modem, smsEvtCb_t smsCallback = NULL);

    bool begin();

    bool read(uint8_t index, SMSMessage_t* message);

    bool send(char* destination, char* text);
    bool sendWithComment(char* destination, SMS_UserData_t* message, char* comment);
    bool sendSingle(char* destination, SMS_UserData_t* message, SMSMultipart_t* multipart = NULL);
    
    uint8_t list(SMSPosition_t* positions, uint8_t positionsLen, SMSStatus_t statusFilter);

    bool remove(uint8_t index);


};


#endif /* A76XX_SMS_CLIENT_H_ */