//
// Board Settings (Tools menu):
// --------------------------------
// Board:            ESP32S3 Dev Module
// Flash Size:       8MB
// Partition Scheme: 8MB  (3MB APP/1.5MB SPIFFS)
// --------------------------------
//

#include "../config.h"
#include "../powerManagement.h"
#include "../environment.h"
#include "../motorCtrl.h"

float fTemperature = 0.0f;
float fHumidity = 0.0f;
int32_t	rainSensorValue = 0;

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
    DebugPort.begin(115200);
    DBPrintln("\nI2C Scanner & FloPOD tester");

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

	// create object to access IO Expanders
	IoExpanders = new ioExpander();

}

void loop()
{
    int nDevices = 0;
    byte error, address;
    float v,a,p;
    float fDeg;
	static TickType_t xDelay = 2000/portTICK_PERIOD_MS; // 2s

    DBPrintln("========================");
#ifdef USE_ETHERNET
    if(ethernetPresent) {
        DBPrintln("W5500 Ok.");
        if(PodEthernet.linkUp()) {
            DBPrintln("W5500 IP = " + IpAddress2String(PodEthernet.localIP()));
        }
        else {
            DBPrintln("W5500 waiting for link");

        }
    }
    else {
        DBPrintln("No W5500 detected");
    }
#endif
    DBPrintln("Scanning I2C bus ...");
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
            DBPrintln("  !");
            nDevices++;
        }
        vTaskDelay(10/portTICK_PERIOD_MS); // wait 10ms
        taskYIELD();

    }

    if (nDevices == 0) {
        DBPrintln("No I2C devices found\n");
        vTaskDelay(xDelay);
        taskYIELD();
        return;
    }
    DBPrintln("");

    if(IoExpanders->isIoExpander1Present()) {
        DBPrintln("TCAL6408A side 1 detected");
        DBPrintln("Mag Switch side 1 = " + String(IoExpanders->checkMagSwitch1()?"On":"Off"));
    }
    else {
        DBPrintln("TCAL6408A side 1 not found");
    }
    if(IoExpanders->isIoExpander2Present()) {
        DBPrintln("TCAL6408A side 2 detected");
        DBPrintln("Mag Switch side 2 = " + String(IoExpanders->checkMagSwitch2()?"On":"Off"));
    }
    else {
        DBPrintln("TCAL6408A side 2 not found");
    }

    // init the sensor
    if (humTempSensor == nullptr) {
        humTempSensor = new HumTempSensor();
    }
    if (podRainSensor == nullptr) {
        podRainSensor = new RainSensor();
        podRainSensor->enableHeater(false); // make sure this is off or we cook the temp sensor.
    }
    if(PodMotorController == nullptr) {
        PodMotorController = new motorCtrl();
    }
    if(podPowerController -> bMainInaPresent) {
        v = podPowerController->readVolts(INA260_MAIN);
        a = podPowerController->readAmps(INA260_MAIN);
        p = podPowerController->readPower(INA260_MAIN);
        DBPrintln("Main :");
        DBPrintln("\tVolts : " + String(v));
        DBPrintln("\tAmps  : " + String(a));
        DBPrintln("\tWatts : " + String(p));
    } else {
        DBPrintln("Main input INA260 not found");
    }
    DBPrintln("");

    if(podPowerController -> bDC1InaPresent) {
        v = podPowerController->readVolts(INA260_DC_1);
        a = podPowerController->readAmps(INA260_DC_1);
        p = podPowerController->readPower(INA260_DC_1);
        DBPrintln("DC1 :");
        DBPrintln("\tVolts : " + String(v));
        DBPrintln("\tAmps  : " + String(a));
        DBPrintln("\tWatts : " + String(p));
    } else {
        DBPrintln("DC1 INA260 not found");
    }
    DBPrintln("");

    if(podPowerController -> bDC2InaPresent) {
        v = podPowerController->readVolts(INA260_DC_2);
        a = podPowerController->readAmps(INA260_DC_2);
        p = podPowerController->readPower(INA260_DC_2);
        DBPrintln("DC2 :");
        DBPrintln("\tVolts : " + String(v));
        DBPrintln("\tAmps  : " + String(a));
        DBPrintln("\tWatts : " + String(p));
    } else {
        DBPrintln("DC2 INA260 not found");
    }
    DBPrintln("");

    if(podPowerController -> bPWM1InaPresent) {
        v = podPowerController->readVolts(INA260_PWM1);
        a = podPowerController->readAmps(INA260_PWM1);
        p = podPowerController->readPower(INA260_PWM1);
        DBPrintln("PWM1 :");
        DBPrintln("\tVolts : " + String(v));
        DBPrintln("\tAmps  : " + String(a));
        DBPrintln("\tWatts : " + String(p));
    } else {
        DBPrintln("PWM1 INA260 not found");
    }
    DBPrintln("");

    if(podPowerController -> bPWM2InaPresent) {
        v = podPowerController->readVolts(INA260_PWM2);
        a = podPowerController->readAmps(INA260_PWM2);
        p = podPowerController->readPower(INA260_PWM2);
        DBPrintln("PWM2 :");
        DBPrintln("\tVolts : " + String(v));
        DBPrintln("\tAmps  : " + String(a));
        DBPrintln("\tWatts : " + String(p));
    } else {
        DBPrintln("PWM2 INA260 not found");
    }
    DBPrintln("");

    if(podPowerController -> bUSBCInaPresent) {
        v = podPowerController->readVolts(INA260_USB_C);
        a = podPowerController->readAmps(INA260_USB_C);
        p = podPowerController->readPower(INA260_USB_C);
        DBPrintln("USB-C :");
        DBPrintln("\tVolts : " + String(v));
        DBPrintln("\tAmps  : " + String(a));
        DBPrintln("\tWatts : " + String(p));
    } else {
        DBPrintln("USB-C INA260 not found");
    }
    DBPrintln("");

    if(podPowerController -> bBATInaPresent) {
        v = podPowerController->readVolts(INA260_BAT);
        a = podPowerController->readAmps(INA260_BAT);
        p = podPowerController->readPower(INA260_BAT);
        DBPrintln("Battery :");
        DBPrintln("\tVolts : " + String(v));
        DBPrintln("\tAmps  : " + String(a));
        DBPrintln("\tWatts : " + String(p));
    } else {
        DBPrintln("Battery INA260 not found");
    }
    DBPrintln("");

    if(podPowerController -> bVMOTInaPresent) {
        v = podPowerController->readVolts(INA260_VMOT);
        a = podPowerController->readAmps(INA260_VMOT);
        p = podPowerController->readPower(INA260_VMOT);
        DBPrintln("Motor 1 :");
        DBPrintln("\tVolts : " + String(v));
        DBPrintln("\tAmps  : " + String(a));
        DBPrintln("\tWatts : " + String(p));
    } else {
        DBPrintln("Motor 1 INA260 not found");
    }
    DBPrintln("");

    if(podPowerController -> bVMOT2InaPresent) {
        v = podPowerController->readVolts(INA260_VMOT2);
        a = podPowerController->readAmps(INA260_VMOT2);
        p = podPowerController->readPower(INA260_VMOT2);
        DBPrintln("Motor 2 :");
        DBPrintln("\tVolts : " + String(v));
        DBPrintln("\tAmps  : " + String(a));
        DBPrintln("\tWatts : " + String(p));
    } else {
        DBPrintln("Motor 2 INA260 not found");
    }
    DBPrintln("");
    // Environment sensors
    if(humTempSensor->isPresent()) {
        humTempSensor->getTempAndHum(fTemperature, fHumidity);
        Serial.printf("SHT31 : %f degC \n %f %% \n",fTemperature,fHumidity);
        DBPrintln("");
    } else {
        DBPrintln("Temperature and humidity sensor not found");
    }

    if(podRainSensor->isPresentSideOne()) {
         DBPrintln("Rain sensor side 1 detected");
    }
    else {
         DBPrintln("Rain sensor side 1 not found");
    }
    if(podRainSensor->isPresentSideTwo()) {
         DBPrintln("Rain sensor side 2 detected");
    }
    else {
         DBPrintln("Rain sensor side 2 not found");
    }

    if(podRainSensor->isPresentSideOne() || podRainSensor->isPresentSideTwo()) {
        rainSensorValue = podRainSensor->getValue();
        DBPrintln("Rain Sensor  : " + String(rainSensorValue));
        DBPrintln("");
    } else {
        DBPrintln("No rain sensor detected");
    }

    if(PodMotorController->isEncoder1Present()){
        PodMotorController->getEncoder1Position(fDeg);
        DBPrintln("AMS encoder 1 angle : " + String(fDeg));
        DBPrintln("");
    }
    else {
        DBPrintln("AMS encoder 1 not present");
        DBPrintln("");

    }

    if(PodMotorController->isEncoder2Present()){
        PodMotorController->getEncoder2Position(fDeg);
        DBPrintln("AMS encoder 2 angle : " + String(fDeg));
        DBPrintln("");
    }
    else {
        DBPrintln("AMS encoder 2 not present");
        DBPrintln("");

    }
    if(bPortOn) {
        DBPrintln("Switching port Off\n");
        bPortOn = false;
        podPowerController->setPortState(USB_C, false);
        podPowerController->setPortState(DC1, false);
        podPowerController->setPortState(DC2, false);
        podPowerController->setPwmPortState(PWM1PwmChannel, 0);
        podPowerController->setPwmPortState(PWM2PwmChannel, 0);
        if(IoExpanders->isIoExpander1Present()) {
            DBPrintln("swithing side 1  LED off");
            IoExpanders->setLedSide1(false);
        }
        if(IoExpanders->isIoExpander2Present()) {
            DBPrintln("swithing side 2  LED off");
            IoExpanders->setLedSide2(false);
        }
    }
    else {

        //DBPrintln("Switching port On\n");
        //bPortOn = true;
        podPowerController->setPortState(USB_C, true);
        podPowerController->setPortState(DC1, true);
        podPowerController->setPortState(DC2, true);
        podPowerController->setPwmPortState(PWM1PwmChannel, 50);
        podPowerController->setPwmPortState(PWM2PwmChannel, 50);
        if(IoExpanders->isIoExpander1Present()) {
            DBPrintln("swithing side 1  LED on");
            IoExpanders->setLedSide1(true);
        }
        if(IoExpanders->isIoExpander2Present()) {
            DBPrintln("swithing side 2  LED on");
            IoExpanders->setLedSide2(true);
        }
    }

    nbLoop++;
    vTaskDelay(xDelay);
    taskYIELD();
}


#ifdef USE_ETHERNET
void onEvent(arduino_event_id_t event, arduino_event_info_t info)
{
  switch (event) {
    case ARDUINO_EVENT_ETH_START:
      DBPrintln("ETH Started");
      //set eth hostname here
      DBPrintln("esp32-eth0");
      break;
    case ARDUINO_EVENT_ETH_CONNECTED:
      DBPrintln("ETH Connected");
      break;
    case ARDUINO_EVENT_ETH_GOT_IP:
      DBPrintln("ETH Got IP: '" + String(esp_netif_get_desc(info.got_ip.esp_netif)) +"'");
      DBPrintln(ETH);
      break;
    case ARDUINO_EVENT_ETH_LOST_IP:
      DBPrintln("ETH Lost IP");
      break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
      DBPrintln("ETH Disconnected");
      break;
    case ARDUINO_EVENT_ETH_STOP:
      DBPrintln("ETH Stopped");
      break;
    default:
      break;
  }
}

bool initEthernet()
{
	char macBuffer[20];
	bool bDhcpOk = false;
	int nTimeout = 0;

	DBPrintln("========== Init Ethernet ==========");
	Network.onEvent(onEvent); // this is just for debugging
	// resetChip(ETHERNET_RESET);
	SPI.begin(ETH_SPI_SCK, ETH_SPI_MISO, ETH_SPI_MOSI);
	// network configuration
	if(!ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_CS, ETH_PHY_IRQ, ETH_PHY_RST, SPI)) {
		DBPrintln("No Ethernet hardware detected");
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

	DBPrintln("========== Setting IP config ==========");
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

	DBPrintln("========== Checking hardware status ==========");
	DBPrintln("W5500 Ok.");
	DBPrintln("W5500 IP = " + IpAddress2String(PodEthernet.localIP()));
	snprintf(macBuffer,20,"%02x:%02x:%02x:%02x:%02x:%02x",
		MAC_Address[0],
		MAC_Address[1],
		MAC_Address[2],
		MAC_Address[3],
		MAC_Address[4],
		MAC_Address[5]);
	DBPrintln("Dome MAC : " + String(macBuffer));
	return true;
}
#endif
