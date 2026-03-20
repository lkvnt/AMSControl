#ifndef COOLCONTROL_H
#define COOLCONTROL_H

#include <cstdint>

class CoolingController {
public:
    CoolingController();
    void setPumpState(bool start);
    void setCoolerState(bool start);
    float getTemperature();
    float getFlowRate();
    void emergencyStop();

private:
    struct CAN_Frame {
        uint32_t id;
        uint8_t len;
        uint8_t data[8];
    };
    void sendFrame(const CAN_Frame& frame);
};

#endif