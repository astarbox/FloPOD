// ----------------------------------------
// This will scan the I2C bus
// then will test the eeprom on the shield
// -----------------------------------------

#include <ETH.h>
#include <Network.h>
#include <Wire.h>

#include "../powerManagement.h"
#include "../environment.h"
#include "../motorCtrl.h"

// network interfaces
#define PodEthernet ETH

#define ETHERNET_CS     5
#define ETHERNET_INT	-1
#define ETHERNET_RESET  4

#define ETH_PHY_TYPE ETH_PHY_W5500
#define ETH_PHY_ADDR 1
#define ETH_PHY_CS   ETHERNET_CS
#define ETH_PHY_IRQ  -1
#define ETH_PHY_RST  ETHERNET_RESET

// SPI pins
#define ETH_SPI_SCK  14
#define ETH_SPI_MISO 12
#define ETH_SPI_MOSI 13


float fTemperature = 0.0f;
float fHumidity = 0.0f;
int32_t	rainSensorAdcValue = 0;
volatile bool ethernetPresent = false;
volatile bool bDhcpOk = false;

void setup()
{
    Wire.begin();
    Serial.begin(115200);
    Serial.println("\nI2C Scanner & FloPOD tester");
	if(!ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, -1, ETH_PHY_RST, SPI)) {
		Serial.println("No Thernet hardware detected");
	}
    else {
        ethernetPresent = true;
    }

    if(ethernetPresent) {
        bDhcpOk = PodEthernet.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0)); // all value set to the default 0 means use dhcp.

    }
}

void loop()
{
    int nDevices = 0;
    byte error, address;
    float v,a,p;
    float fDeg;
	TickType_t xDelay = 1000/portTICK_PERIOD_MS; // 1s

    Serial.println("Scanning...");
    for(address = 1; address < 127; address++ ) {
        // The i2c_scanner uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

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
        vTaskDelay(xDelay);
        taskYIELD();
        return;
    }

    // init power ports
    if(podPowerController == nullptr) {
        podPowerController = new powerPorts();
    }
    // init the sensor
    if (humTempSensor == nullptr) {
        humTempSensor = new HumTempSensor();
    }
    if (podRainSensor == nullptr) {
        podRainSensor = new RainSensor();
    }

    if(PodMotorController == nullptr) {
        PodMotorController = new motorCtrl();
    }

    if(podPowerController -> bMainInaPresent) {
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

    if(podPowerController -> bDC1InaPresent) {
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

    if(podPowerController -> bDC2InaPresent) {
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

    if(podPowerController -> bPWM1InaPresent) {
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

    if(podPowerController -> bPWM2InaPresent) {
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

    if(podPowerController -> bUSBCInaPresent) {
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

    if(podPowerController -> bBATInaPresent) {
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

    if(podPowerController -> bVMOTInaPresent) {
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
    humTempSensor->getTempAndHum(fTemperature, fHumidity);
    Serial.printf("HDC1080 : %f degC \n %f %% \n",fTemperature,fHumidity);

    rainSensorAdcValue = podRainSensor->getADCValue();
    Serial.println("Rain Sensor ADC : " + String(rainSensorAdcValue));

    if(PodMotorController) {
        PodMotorController->getEncoderPosition(fDeg);
        Serial.println("AMS encoder angle : " + String(fDeg));
    }

    if(ethernetPresent) {
        Serial.println("W5500 Ok.");
        if(PodEthernet.linkUp()) {
            Serial.println("W5500 IP = " + IpAddress2String(PodEthernet.localIP()));
        }
        else {
            Serial.println("W5500 waiting for link");

        }
    }

    vTaskDelay(xDelay);
    taskYIELD();
}
