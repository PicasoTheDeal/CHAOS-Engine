#pragma once
// power state, read from sysfs. no display, no toolkit: safe in the app and inside plasmashell alike.
namespace chaos::power {
bool onBattery();                   // any /sys/class/power_supply/*/status == Discharging. never throws
}
