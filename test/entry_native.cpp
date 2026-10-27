#include <Appliance/AirConditioner/StatusData.h>

#include <cassert>
#include <cstdint>

extern "C" int main() {
  uint8_t status[24]{};
  status[0] = 0xC0;
  status[1] = 0x80;
  status[16] = 0x19;

  const dudanov::midea::ac::StatusData data(status, sizeof(status));
  assert(data.getApplianceError());
  assert(data.getErrorCode() == 0x19);

  status[1] = 0x00;
  status[16] = 0x00;
  const dudanov::midea::ac::StatusData clear(status, sizeof(status));
  assert(!clear.getApplianceError());
  assert(clear.getErrorCode() == 0x00);
}
