#ifndef POWERCONTROL_H
#define POWERCONTROL_H

#include <cstdint>

class PowerSupplyController {
public:
    PowerSupplyController();
    bool init();
    void setCurrent(float amperes);
    float getCurrent();
    void emergencyStop();

private:
    uint16_t card_handle;
    struct CAN_Frame {
        uint32_t id;
        uint8_t len;
        uint8_t data[8];
    };
    void sendFrame(const CAN_Frame& frame);
};

#endif