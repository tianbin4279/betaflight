/*
 * This file is part of Cleanflight.
 *
 * Cleanflight is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Cleanflight is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Cleanflight.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdbool.h>
#include <stdint.h>

#include "platform.h"

#include "common/axis.h"

#include "drivers/light_led.h"

#include "fc/config.h"
#include "fc/runtime_config.h"

#include "sensors/sensors.h"
#include "sensors/boardalignment.h"
#include "sensors/compass.h"

#ifdef USE_HARDWARE_REVISION_DETECTION
#include "hardware_revision.h"
#endif

mag_t mag;                   // mag access functions

#ifdef MAG

static int16_t magADCRaw[XYZ_AXIS_COUNT];
static uint8_t magInit = 0;

void compassInit(void)
{
    // initialize and calibration. turn on led during mag calibration (calibration routine blinks it)
    LED1_ON;
    mag.dev.init();
    LED1_OFF;
    magInit = 1;
}

void compassUpdate(uint32_t currentTime, flightDynamicsTrims_t *magZero)
{
    static uint32_t tCal = 0;
    static flightDynamicsTrims_t magZeroTempMin;
    static flightDynamicsTrims_t magZeroTempMax;
    uint32_t axis;

    mag.dev.read(magADCRaw);
    for (axis = 0; axis < XYZ_AXIS_COUNT; axis++) mag.magADC[axis] = magADCRaw[axis];  // int32_t copy to work with
    alignSensors(mag.magADC, mag.dev.magAlign);

    if (STATE(CALIBRATE_MAG)) {
        tCal = currentTime;
        for (axis = 0; axis < 3; axis++) {
            magZero->raw[axis] = 0;
            magZeroTempMin.raw[axis] = mag.magADC[axis];
            magZeroTempMax.raw[axis] = mag.magADC[axis];
        }
        DISABLE_STATE(CALIBRATE_MAG);
    }

    if (magInit) {              // we apply offset only once mag calibration is done
        mag.magADC[X] -= magZero->raw[X];
        mag.magADC[Y] -= magZero->raw[Y];
        mag.magADC[Z] -= magZero->raw[Z];
    }

    if (tCal != 0) {
        if ((currentTime - tCal) < 30000000) {    // 30s: you have 30s to turn the multi in all directions
            LED0_TOGGLE;
            for (axis = 0; axis < 3; axis++) {
                if (mag.magADC[axis] < magZeroTempMin.raw[axis])
                    magZeroTempMin.raw[axis] = mag.magADC[axis];
                if (mag.magADC[axis] > magZeroTempMax.raw[axis])
                    magZeroTempMax.raw[axis] = mag.magADC[axis];
            }
        } else {
            tCal = 0;
            for (axis = 0; axis < 3; axis++) {
                magZero->raw[axis] = (magZeroTempMin.raw[axis] + magZeroTempMax.raw[axis]) / 2; // Calculate offsets
            }

            saveConfigAndNotify();
        }
    }
}
#endif
