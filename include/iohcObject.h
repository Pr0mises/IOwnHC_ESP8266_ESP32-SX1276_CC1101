#pragma once

#include <Arduino.h>
#include <string>
#include <iohcPacket.h>
#include <iohcCryptoHelpers.h>

#define MAX_MANUFACTURER    13

namespace IOHC {
//    typedef uint8_t address[3];
    struct iohcObject_t {
        address     node;
        uint8_t     actuator[2];
        uint8_t     flags;
        uint8_t     io_manufacturer;
        address     backbone;
    };

    class iohcObject {
        public:
            iohcObject();
            iohcObject(address node, address backbone, uint8_t actuator[2], uint8_t manufacturer, uint8_t flags);
            explicit iohcObject(std::string serialized);
            ~iohcObject();

            address *getNode();
            address *getBackbone();
            std::tuple<uint16_t, uint8_t> getTypeSub();
            std::string serialize();
            void dump();

        protected:

        private:
            iohcObject_t iohcDevice{};
            std::vector<uint8_t> *_buffer{};
            char man_id[MAX_MANUFACTURER][15] = {"VELUX", "Somfy", "Honeywell", "Hörmann", "ASSA ABLOY", "Niko", "WINDOW MASTER", "Renson", "CIAT", "Secuyou", "OVERKIZ", "Atlantic Group", "Other"};
    };

}