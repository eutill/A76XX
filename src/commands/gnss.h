#ifndef A76XX_GNSS_CMDS_H_
#define A76XX_GNSS_CMDS_H_

/*
    @brief Commands in section 24 of the AT command manual version 1.09

    Command         | Implemented | Method     | Function(s)
    --------------- | ----------- | ---------- | -----------
    CGNSSPWR        |      y      |     W      | powerControl
    CGPSCOLD        |      y      |     E      | start
    CGPSWARM        |      y      |     E      | start
    CGPSHOT         |      y      |     E      | start
    CGNSSIPR        |      y      |     W      | setUART3BaudRate
    CGNSSMODE       |      y      |     W      | setSupportMode
    CGNSSNMEA       |      y      |     W      | setNMEASentence
    CGPSNMEARATE    |      y      |     W      | setNMEARate
    CGPSFTM         |      y      |     W      | startTestMode, stopTestMode
    CGPSINFO        |      y      |     E      | getGPSInfo
    CGNSSINFO       |      y      |     E      | getGNSSInfo
    CGNSSCMD        |      y      |     W      | sendGNSSCommand
    CGNSSTST        |      y      |     W      | enableNMEAOutput
    CGNSSPORTSWITCH |      y      |     W      | selectOutputPort
    CAGPS           |      y      |     E      | getAGPSData
    CGNSSPROD       |      y      |     E      | getGPSProductInfo
*/

// flags for filtering NMEA messages
#define A76XX_GNSS_nGGA 0b00000001
#define A76XX_GNSS_nGLL 0b00000010
#define A76XX_GNSS_nGSA 0b00000100
#define A76XX_GNSS_nGSV 0b00001000
#define A76XX_GNSS_nRMC 0b00010000
#define A76XX_GNSS_nVTG 0b00100000
#define A76XX_GNSS_nZDA 0b01000000
#define A76XX_GNSS_nGST 0b10000000

/*
    @brief Data structure of GNSS info returned by the command AT+CGNSSINFO.
*/ 
struct GNSSInfo_t {
    bool  hasfix = false;             // whether a fix is available or not
    int   mode = 0;                   // Fix mode 2=2D fix 3=3D fix
    int   GPS_SVs = 0;                // GPS satellite visible numbers
    int   GLONASS_SVs = 0;            // GLONASS satellite visible numbers
    int   BEIDOU_SVs = 0;             // BEIDOU satellite visible numbers
    float lat = 0.0;                  // Latitude of current position. Output format is dd.ddddd
    char  NS = '0';                   // N/S Indicator, N=north or S=south.
    float lon = 0.0;                  // Longitude of current position. Output format is ddd.ddddd 
    char  EW = '0';                   // E/W Indicator, E=east or W=west.
    char  date[7] = "000000";         // Date. Output format is ddmmyy.
    char  UTC_TIME[10] = "000000.00"; // UTC Time. Output format is hhmmss.ss.
    float alt = 0.0;                  // MSL Altitude. Unit is meters.
    float speed = 0.0;                // Speed Over Ground. Unit is knots.
    float course = 0.0;               // Course. Degrees.
    float PDOP = 0.0;                 // Position Dilution Of Precision.
    float HDOP = 0.0;                 // Horizontal Dilution Of Precision.
    float VDOP = 0.0;                 // Vertical Dilution Of Precision.
};

/*
    @brief Data structure of GPS info returned by the command AT+CGPSINFO.
*/ 
struct GPSInfo_t {
    bool  hasfix       = false;       // whether a fix is available or not
    float lat          = 0.0;         // Latitude of current position. Output format is dd.ddddd
    char  NS           = '0';         // N/S Indicator, N=north or S=south.
    float lon          = 0.0;         // Longitude of current position. Output format is ddd.ddddd 
    char  EW           = '0';         // E/W Indicator, E=east or W=west.
    char  date[7]      = "000000";    // Date. Output format is ddmmyy.
    char  UTC_TIME[10] = "000000.00"; // UTC Time. Output format is hhmmss.ss.
    float alt          = 0.0;         // MSL Altitude. Unit is meters.
    float speed        = 0.0;         // Speed Over Ground. Unit is knots.
    float course       = 0.0;         // Course. Degrees.
};

/*
    @brief Flag to start the GPS module in different modes. 
*/
enum GPSStart_t {
    COLD,
    WARM,
    HOT
};

class GNSSCommands {
  public:
    ModemSerial& _serial;

    GNSSCommands(ModemSerial& serial)
        : _serial(serial) {}

    /*
        @brief Implementation for CGNSSPWR - Write Command.
        @detail GNSS power control.
        @param [IN] enable_GNSS Set to true to power on the GNSS module.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT, A76XX_GENERIC_ERROR 
            or A76XX_GNSS_NOT_READY.
    */
    int8_t powerControl(bool enable_GNSS) {
        _serial.sendCMD("AT+CGNSSPWR=", enable_GNSS ? 1 : 0);
        switch (_serial.waitResponse("+CGNSSPWR: READY!", 9000, !enable_GNSS, true)) {
            case Response_t::A76XX_RESPONSE_MATCH_1ST : {
                return A76XX_OPERATION_SUCCEEDED;
            }
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
        @brief Implementation for CGPSCOLD/WARM/HOT - Write Command.
        @detail Cold/Warm/Hot start GPS.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR.
    */
    int8_t start(GPSStart_t _start) {
        switch (_start) {
            case COLD: { _serial.sendCMD("AT+CGPSCOLD"); break; }
            case WARM: { _serial.sendCMD("AT+CGPSWARM"); break; }
            case HOT:  { _serial.sendCMD("AT+CGPSHOT");  break; }
        }
        switch (_serial.waitResponse(9000)) {
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
        @brief Implementation for CGNSSIPR - Write Command.
        @detail Configure the baud rate of UART3 and GPS module.
        @param [IN] baud_rate The baud rate to set.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR.
    */
    int8_t setUART3BaudRate(uint32_t baud_rate) {
        _serial.sendCMD("AT+CGNSSIPR=", baud_rate);
        switch (_serial.waitResponse(9000)) {
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
        @brief Implementation for CGNSSMODE - Write Command.
        @detail Configure GNSS support mode.
        @param [IN] mode The suport mode, from 1 to 7.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR.
    */
    int8_t setSupportMode(uint8_t mode) {
        _serial.sendCMD("AT+CGNSSMODE=", mode);
        switch (_serial.waitResponse(9000)) {
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
        @brief Implementation for CGNSSNMEA - Write Command.
        @detail Configure NMEA sentence type. A mask can be constructed by `|`-ing
            several of the A76XX_GNSS_nXXX flags. For instance, to only output GGA 
            and RMC NMEA sentences set nmea_mask to A76XX_GNSS_nGGA | A76XX_GNSS_nRMC.
        @param [IN] nmea_mask An 8 bit mask to select the sentences to output.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR.
    */
    int8_t setNMEASentence(uint8_t nmea_mask) {
        uint8_t nGGA = (nmea_mask & A76XX_GNSS_nGGA) >> 0;
        uint8_t nGLL = (nmea_mask & A76XX_GNSS_nGLL) >> 1;
        uint8_t nGSA = (nmea_mask & A76XX_GNSS_nGSA) >> 2; 
        uint8_t nGSV = (nmea_mask & A76XX_GNSS_nGSV) >> 3;
        uint8_t nRMC = (nmea_mask & A76XX_GNSS_nRMC) >> 4; 
        uint8_t nVTG = (nmea_mask & A76XX_GNSS_nVTG) >> 5;
        uint8_t nZDA = (nmea_mask & A76XX_GNSS_nZDA) >> 6; 
        uint8_t nGST = (nmea_mask & A76XX_GNSS_nGST) >> 7;
        _serial.sendCMD("AT+CGNSSNMEA=", nGGA, ",", nGLL, ",", nGSA, ",", nGSV, ",",
                                         nRMC, ",", nVTG, ",", nZDA, ",", nGST);
        switch (_serial.waitResponse(9000)) {
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
        @brief Implementation for CGNSSNMEA - Write Command.
        @detail Set NMEA output rate.
        @param [IN] nmea_rate Rate in outputs per second - 1, 2, 4, 5 or 10
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR.
    */
    int8_t setNMEARate(uint8_t nmea_rate) {
        _serial.sendCMD("AT+CGPSNMEARATE=", nmea_rate);
        switch (_serial.waitResponse(9000)) {
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
        @brief Implementation for CGPSFTM - Write Command.
        @detail Start GPS test mode.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR.
    */
    int8_t startTestMode() {
        _serial.sendCMD("AT+CGPSFTM=1");
        switch (_serial.waitResponse(9000)) {
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
        @brief Implementation for CGPSFTM - Write Command.
        @detail Stop GPS test mode.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR.
    */
    int8_t stopTestMode() {
        _serial.sendCMD("AT+CGPSFTM=0");
        switch (_serial.waitResponse(9000)) {
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
        @brief Implementation for CGNSSINFO - Write Command.
        @detail Get GNSS fixed position information.
        @param [OUT] info A GNSSInfo_t structure.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR.
    */
    int8_t getGNSSInfo(GNSSInfo_t& info) {
        _serial.sendCMD("AT+CGNSSINFO");
        switch (_serial.waitResponse("+CGNSSINFO:", 9000, false, true)) {
            case Response_t::A76XX_RESPONSE_MATCH_1ST : {
                // when we do not have a fix there is a space
                if (_serial.peek() == ' ') { 
                    info.hasfix = false;
                } else {
                    info.hasfix = true;
                    info.mode        = _serial.parseInt();   _serial.find(',');
                    info.GPS_SVs     = _serial.parseInt();   _serial.find(',');
                    info.GLONASS_SVs = _serial.parseInt();   _serial.find(',');
                    info.BEIDOU_SVs  = _serial.parseInt();   _serial.find(',');
                    info.lat         = _serial.parseFloat(); _serial.find(',');
                    info.NS          = _serial.read();       _serial.find(',');
                    info.lon         = _serial.parseFloat(); _serial.find(',');
                    info.EW          = _serial.read();       _serial.find(',');
                    _serial.readBytes(info.date, 6);         _serial.find(',');
                    _serial.readBytes(info.UTC_TIME, 9);     _serial.find(',');
                    info.alt         = _serial.parseFloat(); _serial.find(',');
                    info.speed       = _serial.parseFloat(); _serial.find(',');
                    info.course      = _serial.parseFloat(); _serial.find(',');
                    info.PDOP        = _serial.parseFloat(); _serial.find(',');
                    info.HDOP        = _serial.parseFloat(); _serial.find(',');
                    info.VDOP        = _serial.parseFloat();
                }
                // get last OK in any case
                if (_serial.waitResponse(9000) == Response_t::A76XX_RESPONSE_OK) {
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

    /*
        @brief Implementation for CGPSSINFO - Write Command.
        @detail Get GPS fixed position information.
        @param [OUT] info A GPSInfo_t structure.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR.
    */
    int8_t getGPSInfo(GPSInfo_t& info) {
        _serial.sendCMD("AT+CGPSINFO");
        switch (_serial.waitResponse("+CGPSINFO: ", 9000, false, true)) {
            case Response_t::A76XX_RESPONSE_MATCH_1ST : {
                // when we do not have a fix there should be an empty space after the : - not true in my case
                if (_serial.peek() == ',') {
                    info.hasfix = false;
                } else {
                    info.hasfix = true;
                    char degreesBuf[4];

                    if(_serial.peek() != ',') {
                        //get 2 digits of lat degrees
                        _serial.readBytes(degreesBuf, 2); degreesBuf[2] = '\0';
                        //get lat minutes and combine them with degrees
                        info.lat = (_serial.parseFloat() / 60.0f) + strtof(degreesBuf, NULL);
                    } else {info.lat = 0.0f;}
                     _serial.find(',');

                     if(_serial.peek() != ',') {
                         info.NS = _serial.read();
                     } else {info.NS = 'N';}
                    _serial.find(',');

                    if(_serial.peek() != ',') {
                        //same procedure with lon degrees and minutes, but lon degrees have 3 digits
                        _serial.readBytes(degreesBuf, 3); degreesBuf[3] = '\0';
                        info.lon = (_serial.parseFloat() / 60.0f) + strtof(degreesBuf, NULL);
                    } else {info.lon = 0.0f;}
                    _serial.find(',');

                    if(_serial.peek() != ',') {
                        info.EW = _serial.read();
                    } else {info.EW = 'E';}
                    _serial.find(',');

                    if(_serial.peek() != ',') {
                        _serial.readBytes(info.date, 6);
                    } else {info.date[0] = '\0';}
                    _serial.find(',');

                    if(_serial.peek() != ',') {
                        _serial.readBytes(info.UTC_TIME, 9);
                    } else {info.UTC_TIME[0] = '\0';}
                    _serial.find(',');

                    if(_serial.peek() != ',') {
                        info.alt = _serial.parseFloat();
                    } else {info.alt = 0.0f;}
                    _serial.find(',');

                    if(_serial.peek() != ',') {
                        info.speed = _serial.parseFloat();
                    } else {info.speed = 0.0f;}
                    _serial.find(',');

                    if(_serial.peek() != '\r') {
                        info.course = _serial.parseFloat();
                    } else {info.course = 0.0f;}
                }
                // get last OK in any case
                if (_serial.waitResponse(9000) == Response_t::A76XX_RESPONSE_OK) {
                    return A76XX_OPERATION_SUCCEEDED;
                } else {
                    return A76XX_GNSS_GENERIC_ERROR;
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

    /*
        @brief Implementation for CGNSSCMD - Write Command.
        @detail Send command to GNSS module.
        @param [in] cmd The command to send.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR.
    */
    int8_t sendGNSSCommand(const char* cmd) {
        _serial.sendCMD("AT+CGNSSCMD=0,", "\"", cmd, "\"");
        switch (_serial.waitResponse(9000, true, true)) {
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
        @brief Implementation for CGNSSTST - Write Command.
        @detail Send data received from GNSS module to the serial port.
        @param [IN] enabled Whether to send NMEA data to the serial port.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR.
    */
    int8_t enableNMEAOutput(bool enabled) {
        _serial.sendCMD("AT+CGNSSTST=", enabled == true ? "1" : "0");
        switch (_serial.waitResponse(9000)) {
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
        @brief Implementation for CGNSSPORTSWITCH - Write Command.
        @detail Select the output port for NMEA sentence.
        @param [IN] output_parsed_data If true, output parsed data to the serial port.
        @param [IN] output_nmea_data If true, output raw nmea data to the serial port.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR.
    */
    int8_t selectOutputPort(bool output_parsed_data, bool output_nmea_data) {
        _serial.sendCMD("AT+CGNSSPORTSWITCH=", 
                        output_parsed_data == true ? "1" : "0", ",",
                        output_nmea_data   == true ? "1" : "0");
        switch (_serial.waitResponse(9000)) {
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
        @brief Implementation for CAGPS - EXEC Command.
        @detail Get AGPS data from the AGNSS server for assisted positioning.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR.
    */
    int8_t getAGPSData() {
        _serial.sendCMD("AT+CAGPS");
        switch (_serial.waitResponse("+AGPS: ", 9000, false, true)) {
            case Response_t::A76XX_RESPONSE_MATCH_1ST : {
                // when the output is "+AGPS: success"
                if (_serial.peek() == 's') {
                    _serial.clear();
                    return A76XX_OPERATION_SUCCEEDED;
                }
                // error case
                return _serial.parseIntClear();
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
        @brief Implementation for CGNSSPROD - EXEC Command.
        @detail Get GNSS receiver product information.
        @param [OUT] info Buffer to store the the GPS device information.
        @param [IN] len Length of the buffer.
        @return A76XX_OPERATION_SUCCEEDED, A76XX_OPERATION_TIMEDOUT or A76XX_GENERIC_ERROR.
    */
    int8_t getGPSProductInfo(char* info, size_t len) {
        _serial.sendCMD("AT+CGNSSPROD");
        switch (_serial.waitResponse("PRODUCT: ", 9000, false, true)) {
            case Response_t::A76XX_RESPONSE_MATCH_1ST : {
                _serial.readBytesUntil('\r', info, len);
                _serial.clear();
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
};

#endif /* A76XX_GNSS_CMDS_H_ */
