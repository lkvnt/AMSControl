#ifndef POWERCONTROL_H
#define POWERCONTROL_H

#include <cstdint>
#include <vector>

class PowerSupplyController {
public:
    PowerSupplyController(uint8_t deviceId = 1);
    ~PowerSupplyController();

    bool init();
    void setPowerState(bool turnOn);
    void setCurrent(float amperes);
    void emergencyStop();
    
    // Метод для вызова в цикле опроса, читает CAN буфер
    void processCanMessages(); 

    float getCurrent() const { return current_actual; }
    uint8_t getStatusFlags() const { return status_flags; }

private:
    int card_handle;
    uint8_t dev_id;
    float current_actual;
    uint8_t status_flags;

    uint32_t getTargetId() const;
    void sendCanCommand(uint8_t cmd, const std::vector<uint8_t>& payload);
    void requestActualCurrent();
    void requestStatus();
};

#endif