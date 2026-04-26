// ----------------------------------------
// This will scan the I2C bus
// then will test the eeprom on the shield
// -----------------------------------------
#include "../config.h"
#include "../powerManagement.h"
#include "../environment.h"
#include "../motorCtrl.h"

float fTemperature = 0.0f;
float fHumidity = 0.0f;
int32_t	rainSensorValuePf = 0;

volatile bool bPortOn = false;
int nbLoop = 0;


#ifdef USE_ETHERNET
volatile bool ethernetPresent = false;
volatile bool bDhcpOk = false;
bool initEthernet();
#endif // #ifdef USE_ETHERNET


void setup()
{
    // Create Pod controller. This configure all GPIO
    globalPodConfig = new PodConfig();

    int nTimeout;
    Wire.begin();
    Serial.begin(115200);
    Serial.println("\nI2C Scanner & FloPOD tester");

	Network.begin();
#ifdef USE_ETHERNET
    initEthernet();
#endif

    // init power ports
    podPowerController = new powerPorts();

    // set all ports on
    podPowerController->setPortState(USB_C, true);
    podPowerController->setPortState(DC1, true);
    podPowerController->setPortState(DC2, true);
    // set pwm to 50% (should output 6.xx V)
    podPowerController->setPwmPortState(PWM1PwmChannel, 50);
    podPowerController->setPwmPortState(PWM2PwmChannel, 50);
    bPortOn = true;

}

void loop()
{
    int nDevices = 0;
    byte error, address;
    float v,a,p;
    float fDeg;
	static TickType_t xDelay = 2000/portTICK_PERIOD_MS; // 2s


#ifdef USE_ETHERNET
    if(ethernetPresent) {
        Serial.println("W5500 Ok.");
        if(PodEthernet.linkUp()) {
            Serial.println("W5500 IP = " + IpAddress2String(PodEthernet.localIP()));
        }
        else {
            Serial.println("W5500 waiting for link");

        }
    }
#endif

    Serial.println("Scanning I2C bus ...");
    for(address = 8; address < 127; address++ ) {
        // This uses the return value of
        // the Write.endTransmisstion to see if
        // a device did acknowledge to the address.
        // addresses 0 to 7 are reserved so we start scanning at 8
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
        vTaskDelay(10); // wait 10ms
        taskYIELD();

    }

    if (nDevices == 0) {
        Serial.println("No I2C devices found\n");
        vTaskDelay(xDelay);
        taskYIELD();
        return;
    }
    Serial.println("");
    // init the sensor
    if (humTempSensor == nullptr) {
        humTempSensor = new HumTempSensor();
    }
    if (podRainSensor == nullptr) {
        podRainSensor = new RainSensor();
        podRainSensor->enableHeater(true);
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
    Serial.println("");

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
    Serial.println("");

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
    Serial.println("");

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
    Serial.println("");

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
    Serial.println("");

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
    Serial.println("");

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
    Serial.println("");

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
    Serial.println("");

    if(podPowerController -> bVMOT2InaPresent) {
        v = podPowerController->readVolts(INA260_VMOT2);
        a = podPowerController->readAmps(INA260_VMOT2);
        p = podPowerController->readPower(INA260_VMOT2);
        Serial.println("Motor 2 :");
        Serial.println("\tVolts : " + String(v));
        Serial.println("\tAmps  : " + String(a));
        Serial.println("\tWatts : " + String(p));
    } else {
        Serial.println("Motor 2 INA260 not found");
    }
    Serial.println("");


    // Environment sensors
    humTempSensor->getTempAndHum(fTemperature, fHumidity);
    Serial.printf("HDC1080 : %f degC \n %f %% \n",fTemperature,fHumidity);
    Serial.println("");

    if(podRainSensor->isPresent()) {
        rainSensorValuePf = podRainSensor->getValue();
        Serial.println("Rain Sensor  : " + String(rainSensorValuePf) + "pF");
        Serial.println("");
    } else {
        Serial.println("No FDC1004 sensor detected");
    }

    PodMotorController->getEncoderPosition(fDeg);
    Serial.println("AMS encoder angle : " + String(fDeg));
    Serial.println("");
/*
    if(nbLoop == 10) {
        if(bPortOn) {
            Serial.println("Switching port Off\n");
            bPortOn = false;
            podPowerController->setPortState(USB_C, false);
            podPowerController->setPortState(DC1, false);
            podPowerController->setPortState(DC2, false);
            podPowerController->setPwmPortState(PWM1PwmChannel, 0);
            podPowerController->setPwmPortState(PWM2PwmChannel, 0);
        }
        else {
            Serial.println("Switching port On\n");
            bPortOn = true;
            podPowerController->setPortState(USB_C, true);
            podPowerController->setPortState(DC1, true);
            podPowerController->setPortState(DC2, true);
            podPowerController->setPwmPortState(PWM1PwmChannel, 50);
            podPowerController->setPwmPortState(PWM2PwmChannel, 50);
        }
        nbLoop = 0;
    }
*/
    nbLoop++;
    vTaskDelay(xDelay);
    taskYIELD();
}


#ifdef USE_ETHERNET
bool initEthernet()
{
	char macBuffer[20];
	bool bDhcpOk = false;
	int nTimeout = 0;

	Serial.println("========== Init Ethernet ==========");
	// resetChip(ETHERNET_RESET);
	SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
	// network configuration
	if(!ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI)) {
		Serial.println("No Ethernet hardware detected");
		return false;
	}
    ethernetPresent = true;
	// set an ip so we can get the link status
	PodEthernet.config("192.168.0.100", "192.168.0.1", "255.255.255.0");
	while(!PodEthernet.linkUp() ) {
		vTaskDelay(250 / portTICK_PERIOD_MS);
		nTimeout++;
		if(nTimeout == 10) {
			return false;
		}
	}

	PodEthernet.macAddress(MAC_Address);
	PodEthernet.setHostname("FLO-Pod");

	Serial.println("========== Setting IP config ==========");
    bDhcpOk = PodEthernet.config(IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0), IPAddress(0, 0, 0, 0)); // all value set to the default 0 means use dhcp.
    if(bDhcpOk) {
        nTimeout = 0;
        while(PodEthernet.localIP() == IPAddress(0,0,0,0) ) {
            vTaskDelay(250 / portTICK_PERIOD_MS);
            nTimeout++;
            if(nTimeout == 30) {
                break;
            }
        }
    }

	if(PodEthernet.localIP() == IPAddress(0,0,0,0)) {
			PodEthernet.config("192.168.0.100", "192.168.0.1", "255.255.255.0");
            PodEthernet.dnsIP(0,"1.1.1.1");
            vTaskDelay(250 / portTICK_PERIOD_MS);
	}

	PodEthernet.setDefault();

	Serial.println("========== Checking hardware status ==========");
	Serial.println("W5500 Ok.");
	Serial.println("W5500 IP = " + IpAddress2String(PodEthernet.localIP()));
	snprintf(macBuffer,20,"%02x:%02x:%02x:%02x:%02x:%02x",
		MAC_Address[0],
		MAC_Address[1],
		MAC_Address[2],
		MAC_Address[3],
		MAC_Address[4],
		MAC_Address[5]);
	Serial.println("Dome MAC : " + String(macBuffer));
	return true;
}
#endif
