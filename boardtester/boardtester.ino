// ----------------------------------------
// This will scan the I2C bus
// then will test the eeprom on the shield
// -----------------------------------------

#include <Wire.h>
#include "../powerManagement.h"
#include "../environment.h"

float fTemperature;
float fHumidity;
int32_t	rainSensorAdcValue;

void setup()
{
    Wire.begin();

    envSensor = new HumTempSensor();
    podRainSensor = new RainSensor();

    Serial.begin(115200);
    Serial.println("\nI2C Scanner & FloPOD tester");
}


void loop()
{
    int nDevices = 0;
    byte error, address;
    float v,a,p;

    Serial.println("Scanning...");
    for(address = 1; address < 127; address++ ) {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        Wire1.beginTransmission(address);
        error = Wire1.endTransmission();

        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address<16)
                Serial.print("0");
            Serial.print(address,HEX);
            Serial.println("  !");
            nDevices++;
        }
    }

    if (nDevices == 0) {
        Serial.println("No I2C devices found\n");
    }

    if (nDevices) {
        // init power ports
        if(podPowerController != nullptr) {
            podPowerController = new powerPorts();
        }
        if(podPowerController -> bMainInaPResent) {
            v = podPowerController->readVolts(INA260_MAIN);
            a = podPowerController->readAmps(INA260_MAIN);
            p = podPowerController->readPower(INA260_MAIN);
            Serial.println("Main :");
            Serial.println("\tVolts : " + String(v));
            Serial.println("\tAmps  : " + String(a));
            Serial.println("\tWatts : " + String(p));
        } else {
            Serial.println("Main input INA260 not found");
        }

        if(podPowerController -> bDC1InaPResent) {
            v = podPowerController->readVolts(INA260_DC_1);
            a = podPowerController->readAmps(INA260_DC_1);
            p = podPowerController->readPower(INA260_DC_1);
            Serial.println("DC1 :");
            Serial.println("\tVolts : " + String(v));
            Serial.println("\tAmps  : " + String(a));
            Serial.println("\tWatts : " + String(p));
        } else {
            Serial.println("DC1 INA260 not found");
        }

        if(podPowerController -> bDC2InaPResent) {
            v = podPowerController->readVolts(INA260_DC_2);
            a = podPowerController->readAmps(INA260_DC_2);
            p = podPowerController->readPower(INA260_DC_2);
            Serial.println("DC2 :");
            Serial.println("\tVolts : " + String(v));
            Serial.println("\tAmps  : " + String(a));
            Serial.println("\tWatts : " + String(p));
        } else {
            Serial.println("DC2 INA260 not found");
        }

        if(podPowerController -> bPWM1InaPResent) {
            v = podPowerController->readVolts(INA260_PWM1);
            a = podPowerController->readAmps(INA260_PWM1);
            p = podPowerController->readPower(INA260_PWM1);
            Serial.println("PWM1 :");
            Serial.println("\tVolts : " + String(v));
            Serial.println("\tAmps  : " + String(a));
            Serial.println("\tWatts : " + String(p));
        } else {
            Serial.println("PWM1 INA260 not found");
        }

        if(podPowerController -> bPWM2InaPResent) {
            v = podPowerController->readVolts(INA260_PWM2);
            a = podPowerController->readAmps(INA260_PWM2);
            p = podPowerController->readPower(INA260_PWM2);
            Serial.println("PWM2 :");
            Serial.println("\tVolts : " + String(v));
            Serial.println("\tAmps  : " + String(a));
            Serial.println("\tWatts : " + String(p));
        } else {
            Serial.println("PWM2 INA260 not found");
        }

        if(podPowerController -> bUSBCInaPResent) {
            v = podPowerController->readVolts(INA260_USB_C);
            a = podPowerController->readAmps(INA260_USB_C);
            p = podPowerController->readPower(INA260_USB_C);
            Serial.println("USB-C :");
            Serial.println("\tVolts : " + String(v));
            Serial.println("\tAmps  : " + String(a));
            Serial.println("\tWatts : " + String(p));
        } else {
            Serial.println("USB-C INA260 not found");
        }

        if(podPowerController -> bBATInaPResent) {
            v = podPowerController->readVolts(INA260_BAT);
            a = podPowerController->readAmps(INA260_BAT);
            p = podPowerController->readPower(INA260_BAT);
            Serial.println("Battery :");
            Serial.println("\tVolts : " + String(v));
            Serial.println("\tAmps  : " + String(a));
            Serial.println("\tWatts : " + String(p));
        } else {
            Serial.println("Battery INA260 not found");
        }

        if(podPowerController -> bVMOTInaPResent) {
            v = podPowerController->readVolts(INA260_VMOT);
            a = podPowerController->readAmps(INA260_VMOT);
            p = podPowerController->readPower(INA260_VMOT);
            Serial.println("Motor 1 :");
            Serial.println("\tVolts : " + String(v));
            Serial.println("\tAmps  : " + String(a));
            Serial.println("\tWatts : " + String(p));
        } else {
            Serial.println("Motor 1 INA260 not found");
        }

//        if(podPowerController -> bVMOT2naPResent) {
            // v = podPowerController->readVolts(INA260_VMOT2);
            // a = podPowerController->readAmps(INA260_VMOT2);
            // p = podPowerController->readPower(INA260_VMOT2);
            // Serial.println("Motor 2 :");
            // Serial.println("\tVolts : " + String(v));
            // Serial.println("\tAmps  : " + String(a));
            // Serial.println("\tWatts : " + String(p));
//        } else {
//            Serial.println("Motor 2 INA260 not found");
//        }

        // Environment sensors
        envSensor->getTempAndHum(fTemperature, fHumidity);
        Serial.printf("HDC1080 : %f degC \n %f %% \n",fTemperature,fHumidity);

        rainSensorAdcValue = podRainSensor->getADCValue();
        Serial.println("Rain Sensor ADC : " + String(rainSensorAdcValue));


    }
    delay(1000);
}
