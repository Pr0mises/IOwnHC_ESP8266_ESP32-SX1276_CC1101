#include <Arduino.h>                // Is this not required?

#include <SX1276Helpers.h>
#if defined(SX1276)

#include <map>
#if defined(ESP8266)
    #include <TickerUs.h>
#elif defined(ESP32)  	
    #include <TickerUsESP32.h>
    #include <esp_task_wdt.h>
#endif

namespace Radio {
    SPISettings SpiSettings(SPI_CLOCK_DIV2, MSBFIRST, SPI_MODE0);
//    WorkingParams _params;

/*
    uint8_t bufferIndex = 0;


    uint32_t freqs[MAX_FREQS] = FREQS2SCAN;
    uint8_t next_freq = 0;
    uint8_t scanCounter = 0;
*/

    // Simplified bandwidth registries evaluation
    std::map<uint8_t, regBandWidth> __bw =
    {
        {25, {0x01, 0x04}},  // 25KHz
        {50, {0x01, 0x03}},
        {100, {0x01, 0x02}},
        {125, {0x00, 0x02}},
        {200, {0x01, 0x01}},
        {250, {0x00, 0x01}}  // 250KHz
    };

    void SPI_beginTransaction(void)
    {
        SPI.beginTransaction(Radio::SpiSettings);
        digitalWrite(RADIO_NSS, LOW);
    }

    void SPI_endTransaction(void)
    {
        digitalWrite(RADIO_NSS, HIGH);
        SPI.endTransaction();
    }

    void initHardware(void)
    {
        Serial.println("SPI Init");
        // SPI pins configuration
    #if defined(ESP8266)
        SPI.pins(RADIO_SCLK, RADIO_MISO, RADIO_MOSI, RADIO_NSS);
    #endif
        pinMode(RADIO_RESET, INPUT);    // Connected to Reset; floating for POR

        // Check the availability of the Radio
        do {
            delayMicroseconds(1);
    #if defined(ESP8266)
            wdt_reset();
    #elif defined(ESP32)
            esp_task_wdt_reset();        
    #endif
        } while (!digitalRead(RADIO_RESET));
        delayMicroseconds(BOARD_READY_AFTER_POR);
        Serial.printf("Radio Chip is ready\n");

        // Initialize SPI bus
    #if defined(ESP8266)
        SPI.begin();
    #elif defined(ESP32)
        SPI.begin(RADIO_SCLK, RADIO_MISO, RADIO_MOSI, RADIO_NSS);
    #endif
        // Disable SPI device
        // Disable device NRESET pin
        pinMode(RADIO_NSS, OUTPUT);
        pinMode(RADIO_RESET, OUTPUT);
        digitalWrite(RADIO_RESET, HIGH);
        digitalWrite(RADIO_NSS, HIGH);
        delayMicroseconds(BOARD_READY_AFTER_POR);

        SPI.beginTransaction(Radio::SpiSettings);
        SPI.endTransaction();
        writeByte(REG_OPMODE, RF_OPMODE_STANDBY);       // Put Radio in Standby mode

        pinMode(SCAN_LED, OUTPUT);
        digitalWrite(SCAN_LED, 1);
    }

    void initRegisters(uint8_t maxPayloadLength = 0xff)
    {
        // Firstly put radio in StandBy mode as some parameters cannot be changed differently
        writeByte(REG_OPMODE, (readByte(REG_OPMODE) & RF_OPMODE_MASK) | RF_OPMODE_STANDBY);

        // ---------------- Common Register init section ---------------- 
        // Switch-off clockout
        writeByte(REG_OSC, RF_OSC_CLKOUT_OFF);

        // Variable packet lenght, generates working CRC !!!
        // Packet mode, IoHomeOn, IoHomePowerFrame to be added (0x10) to avoid rx to newly detect the preamble during tx radio shutdown 
        writeByte(REG_PACKETCONFIG1, RF_PACKETCONFIG1_PACKETFORMAT_VARIABLE | RF_PACKETCONFIG1_DCFREE_OFF | RF_PACKETCONFIG1_CRC_ON | RF_PACKETCONFIG1_CRCAUTOCLEAR_ON | RF_PACKETCONFIG1_ADDRSFILTERING_OFF | RF_PACKETCONFIG1_CRCWHITENINGTYPE_CCITT);
        writeByte(REG_PACKETCONFIG2, RF_PACKETCONFIG2_DATAMODE_PACKET | RF_PACKETCONFIG2_IOHOME_ON | RF_PACKETCONFIG2_IOHOME_POWERFRAME);   // Is IoHomePowerFrame useful ?

        // Preamble shall be set to AA for packets to be received by appliances. Sync word shall be set with different values if Rx or Tx
        writeByte(REG_SYNCCONFIG, RF_SYNCCONFIG_AUTORESTARTRXMODE_WAITPLL_ON | RF_SYNCCONFIG_PREAMBLEPOLARITY_AA | RF_SYNCCONFIG_SYNC_ON);

        // Set Sync word to 0xff33 both for rx and tx
        writeByte(REG_SYNCVALUE1, SYNC_BYTE_1);
        writeByte(REG_SYNCVALUE2, SYNC_BYTE_2);

        // Mapping of pins DIO0 to DIO3
        // DIO0: PayloadReady|PacketSent    DIO1: FIFO empty    DIO2: Sync   | DIO3: TxReady
        // Mapping of pins DIO4 and DIO5
        // DIO4: PreambleDetect  DIO5: Data
        writeByte(REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_00 | RF_DIOMAPPING1_DIO1_01 | RF_DIOMAPPING1_DIO2_11 | RF_DIOMAPPING1_DIO3_01); // Org
//        writeByte(REG_DIOMAPPING1, RF_DIOMAPPING1_DIO0_00 | RF_DIOMAPPING1_DIO1_01 | RF_DIOMAPPING1_DIO2_10 | RF_DIOMAPPING1_DIO3_01); // timeout on DIO2 for test
        writeByte(REG_DIOMAPPING2, RF_DIOMAPPING2_DIO4_11 | RF_DIOMAPPING2_DIO5_10 | RF_DIOMAPPING2_MAP_PREAMBLEDETECT); // Preamble on DIO4

        // Enable Fast Hoping (frequency change)
        writeByte(REG_PLLHOP, readByte(RF_PLLHOP_FASTHOP_ON) | RF_PLLHOP_FASTHOP_ON);

/*---*/
        // ---------------- TX Register init section ---------------- 

        // PA boost maximum power
        writeByte(REG_PACONFIG, RF_PACONFIG_PASELECT_MASK | RF_PACONFIG_PASELECT_PABOOST);
        // PA Ramp: No Shaping, Ramp up/down 15us
        writeByte(REG_PARAMP, RF_PARAMP_MODULATIONSHAPING_00 | RF_PARAMP_0015_US);
        // Setting Preamble Length
        writeByte(REG_PREAMBLEMSB, PREAMBLE_MSB);
        writeByte(REG_PREAMBLELSB, PREAMBLE_LSB);
        // FIFO Threshold - currently useless
        writeByte(REG_FIFOTHRESH, RF_FIFOTHRESH_TXSTARTCONDITION_FIFONOTEMPTY);

/*---*/
        // ---------------- RX Register init section ---------------- 

        // Set lenght checking if passed as parameter
        writeByte(REG_PAYLOADLENGTH, 0xff); // the use of maxPayloadLength is not working. Prevents generating PayloadReady signal

        // RSSI precision +-2dBm
//        writeByte(REG_RSSICONFIG, RF_RSSICONFIG_SMOOTHING_32);

        // Activates Timeout interrupt on Preamble
        writeByte(REG_RXCONFIG, RF_RXCONFIG_AFCAUTO_ON | RF_RXCONFIG_AGCAUTO_ON | RF_RXCONFIG_RXTRIGER_PREAMBLEDETECT);
        // 250KHz BW with AFC
        writeByte(REG_AFCBW, RF_AFCBW_MANTAFC_16 | RF_AFCBW_EXPAFC_1);
        // Enables Preamble Detect, 2 bytes
        writeByte(REG_PREAMBLEDETECT, RF_PREAMBLEDETECT_DETECTOR_ON | RF_PREAMBLEDETECT_DETECTORSIZE_2 | RF_PREAMBLEDETECT_DETECTORTOL_10);
/*---*/
    }

    void calibrate(void)
    {
        // RC Calibration (only call after setting correct frequency band)
        writeByte(REG_OSC, RF_OSC_RCCALSTART);
        // Start image and RSSI calibration
        writeByte(REG_IMAGECAL, (RF_IMAGECAL_AUTOIMAGECAL_MASK & RF_IMAGECAL_IMAGECAL_MASK) | RF_IMAGECAL_IMAGECAL_START);
        // Wait end of calibration
        do {} while (readByte(REG_IMAGECAL) & RF_IMAGECAL_IMAGECAL_RUNNING);
    }

    void setStandby(void)
    {
        writeByte(REG_OPMODE, (readByte(REG_OPMODE) & RF_OPMODE_MASK) | RF_OPMODE_STANDBY);
    }

    void setTx(void)    // Uncommon and incompatible settings
    {
        // Enabling Sync word - Size must be set to SYNCSIZE_2 (0x01 in header file)
        writeByte(REG_SYNCCONFIG, (readByte(REG_SYNCCONFIG) & RF_SYNCCONFIG_SYNCSIZE_MASK) | RF_SYNCCONFIG_SYNCSIZE_2);

        writeByte(REG_OPMODE, (readByte(REG_OPMODE) & RF_OPMODE_MASK) | RF_OPMODE_TRANSMITTER);
        TxReady;
    }

    void setRx(void)    // Uncommon and incompatible settings
    {
        writeByte(REG_SYNCCONFIG, (readByte(REG_SYNCCONFIG) & RF_SYNCCONFIG_SYNCSIZE_MASK) | RF_SYNCCONFIG_SYNCSIZE_3);

        writeByte(REG_OPMODE, (readByte(REG_OPMODE) & RF_OPMODE_MASK) | RF_OPMODE_RECEIVER);
        RxReady;
/*
        // Start Sequencer
        writeByte(REG_OPMODE, (readByte(REG_OPMODE) & RF_OPMODE_MASK) | RF_OPMODE_RECEIVER);
        writeByte(REG_SEQCONFIG1, readByte(REG_SEQCONFIG1 | RF_SEQCONFIG1_SEQUENCER_START));
*/
    }

    void clearBuffer(void)
    {
        for (uint8_t idx=0; idx <= 64; ++idx)   // Clears FIFO at startup to avoid dirty reads
            readByte(REG_FIFO);
    }

    void clearFlags(void)
    {
        uint8_t out[2] = {0xff, 0xff};
        writeBytes(REG_IRQFLAGS1, out, 2);
    }

    bool preambleDetected(void)
    {
        return (readByte(REG_IRQFLAGS1) & RF_IRQFLAGS1_PREAMBLEDETECT);
    }

    bool syncedAddress(void)
    {
        return (readByte(REG_IRQFLAGS1) & RF_IRQFLAGS1_SYNCADDRESSMATCH);
    }

    bool dataAvail(void)
    {
        return ((readByte(REG_IRQFLAGS2) & RF_IRQFLAGS2_FIFOEMPTY)?false:true);
    }

    uint8_t readByte(uint8_t regAddr)
    {
        uint8_t getByte;
        readBytes(regAddr, &getByte, 1);

        return (getByte);
    }

    void readBytes(uint8_t regAddr, uint8_t *out, uint8_t len)
    {
        SPI_beginTransaction();
        SPI.transfer(regAddr);                  // Send Address
        for (uint8_t idx=0; idx < len; ++idx)
            out[idx] = SPI.transfer(regAddr);   // Get data
        SPI_endTransaction();

        return;
    }

    bool writeByte(uint8_t regAddr, uint8_t data, bool check)
    {
        return writeBytes(regAddr, &data, 1, check);
    }

    bool writeBytes(uint8_t regAddr, uint8_t *in, uint8_t len, bool check)
    {
        SPI_beginTransaction();
        SPI.write(regAddr | SPI_Write);      // Send Address with Write flag
        for (uint8_t idx=0; idx < len; ++idx)
            SPI.write(in[idx]);              // Send data
        SPI_endTransaction();

        if (check)
        {
            uint8_t getByte;

            SPI_beginTransaction();
            SPI.transfer(regAddr);                  // Send Address
            for (uint8_t idx=0; idx < len; ++idx)
            {
                getByte = SPI.transfer(regAddr);    // Get data
                if (in[idx] != getByte)
                {
                    SPI_endTransaction();
                    return false;
                }
            }
            SPI_endTransaction();
        }

        return true;
    }

    bool inStdbyOrSleep(void)
    {
        uint8_t data = readByte(REG_OPMODE);
        data &= ~RF_OPMODE_MASK;
        if ((data == RF_OPMODE_SLEEP) || (data == RF_OPMODE_STANDBY))
            return true;

        return false;
    }

    bool setCarrier(Carrier param, uint32_t value)
    {
        uint32_t tmpVal;
        uint8_t out[4];
        regBandWidth bw;

//  Change of Frequency can be done while the radio is working thanks to Freq Hopping
        if (!inStdbyOrSleep())
            if (param != Carrier::Frequency)
                return false;

        switch (param)
        {
            case Carrier::Frequency:
                tmpVal = (uint32_t)(((float_t)value/FXOSC)*(1<<19));
                out[0] = (tmpVal & 0x00ff0000) >> 16;
                out[1] = (tmpVal & 0x0000ff00) >> 8;
                out[2] = (tmpVal & 0x000000ff); // If Radio is active writing LSB triggers frequency change
                writeBytes(REG_FRFMSB, out, 3);
                break;
            case Carrier::Bandwidth:
                bw = bwRegs(value);
                writeByte(REG_RXBW, bw.Mant | bw.Exp);
                writeByte(REG_AFCBW, bw.Mant | bw.Exp);
                break;
            case Carrier::Deviation:
                tmpVal = (uint32_t)(((float_t)value/FXOSC)*(1<<19));
                out[0] = (tmpVal & 0x0000ff00) >> 8;
                out[1] = (tmpVal & 0x000000ff);
                writeBytes(REG_FDEVMSB, out, 2);
                break;
            case Carrier::Modulation:
                switch (value)
                {
                    case Modulation::FSK:
                        uint8_t rfOpMode = readByte(REG_OPMODE);
                        rfOpMode &= RF_OPMODE_LONGRANGEMODE_MASK;
                        rfOpMode |= RF_OPMODE_LONGRANGEMODE_OFF;
                        rfOpMode &= RF_OPMODE_MODULATIONTYPE_MASK;
                        rfOpMode |= RF_OPMODE_MODULATIONTYPE_FSK;
                        rfOpMode &= RF_OPMODE_MASK;
                        rfOpMode |= RF_OPMODE_STANDBY;
                        rfOpMode &= ~0x08;
                        writeByte(REG_OPMODE, rfOpMode);
                        break;
                }
                break;
            case Carrier::Bitrate:
                tmpVal = FXOSC/value;
                out[0] = (tmpVal & 0x0000ff00) >> 8;
                out[1] = (tmpVal & 0x000000ff);
                writeBytes(REG_BITRATEMSB, out, 2);
                break;
        }

        return true;
    }

    regBandWidth bwRegs(uint8_t bandwidth)
    {
        for (auto it = __bw.begin(); it != __bw.end(); it++)
            if (it->first == bandwidth)
                return it->second;

        return __bw.rbegin()->second;
    }

    void dump()
    {
        uint8_t idx = 1;

        Serial.printf("*********************** Radio registers ***********************\n");
        do       
        {
            Serial.printf("*%2.2x=%2.2x\t", idx, readByte(idx)); idx += 1;
            Serial.printf("*%2.2x=%2.2x\t", idx, readByte(idx)); idx += 1;
            Serial.printf("*%2.2x=%2.2x\t", idx, readByte(idx)); idx += 1;
            Serial.printf("*%2.2x=%2.2x\t", idx, readByte(idx)); idx += 1;
            Serial.printf("*%2.2x=%2.2x\t", idx, readByte(idx)); idx += 1;
            Serial.printf("*%2.2x=%2.2x\t", idx, readByte(idx)); idx += 1;
            Serial.printf("*%2.2x=%2.2x\t", idx, readByte(idx)); idx += 1;
            Serial.printf("*%2.2x=%2.2x\t", idx, readByte(idx)); idx += 1;
            Serial.printf("\n");
        }
        while (idx < 0x70);
        Serial.printf("***************************************************************\n");
        Serial.printf("\n");
    }
}
#endif