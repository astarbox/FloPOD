//
// Board Settings (Tools menu):
// --------------------------------
// Board:            ESP32S3 Dev Module
// Flash Size:       8MB
// Partition Scheme: Default 8MB  (3MB APP/1.5MB SPIFFS)
// --------------------------------
//


#include "../config.h"
#include "../powerManagement.h"

int nbLoops = 0;

void setup()
{
    // Create Pod controller. This configure all GPIO
    globalPodConfig = new PodConfig();

    int nTimeout;
    Wire.begin();
    Serial.begin(115200);
    Serial.println("\nI2C Scanner & FloPOD tester");

	Network.begin();

    // init power ports
    podPowerController = new powerPorts();

    // set all ports off
    podPowerController->setPortState(USB_C, false);
    podPowerController->setPortState(DC1, false);
    podPowerController->setPortState(DC2, false);
    // set pwm to 50% (should output 6.xx V)
    podPowerController->setPwmPortState(PWM1PwmChannel, 50);
    podPowerController->setPwmPortState(PWM2PwmChannel, 50);

}

void loop()
{
    float v,a,p;
	static TickType_t xDelay = 10/portTICK_PERIOD_MS; // 50ms
    // wait for keypress
	if(nbLoops == 0) {
        while(Serial.available()<1) {
            vTaskDelay(xDelay);
            taskYIELD();
        }
	    if(podPowerController -> bDC1InaPresent) {
			// start linear actuator
			podPowerController->setPortState(DC1, true);
		}
        Serial.println("Port,Volts,Amps,Power");
		nbLoops++;
	}
/*
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
    Serial.println("");
 */

    if(podPowerController -> bDC1InaPresent) {
        v = podPowerController->readVolts(INA260_DC_1);
        a = podPowerController->readAmps(INA260_DC_1);
        p = podPowerController->readPower(INA260_DC_1);
        Serial.println("DC1," +String(v) + "," + String(a) + "," + String(p));
    }
    vTaskDelay(xDelay);
    taskYIELD();
}
