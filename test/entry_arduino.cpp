#include <Arduino.h>
#include <Appliance/AirConditioner/StatusData.h>

#include <cassert>

void setup() {
  uint8_t status[24]{};
  status[0] = 0xC0;
  status[1] = 0x80;
  status[16] = 0x19;
  const dudanov::midea::ac::StatusData data(status, sizeof(status));
  assert(data.getApplianceError());
  assert(data.getErrorCode() == 0x19);
}
void loop() {}
