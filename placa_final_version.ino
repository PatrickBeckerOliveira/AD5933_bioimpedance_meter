/*
  ad5933-test
    Reads impedance values from the AD5933 over I2C and prints them serially.
*/

#include <Wire.h>
#include <math.h>
#include "AD5933.h"
//#include <MCP41_Simple.h> // biblioteca para controle do potenciometro digital MCP41010 não utilizado na versão final

#define START_FREQ  (1000)
#define FREQ_INCR   (1000)
#define NUM_INCR    (99)
#define REF_RESIST  (100)

/*
MCP41_Simple MyPot;

// Which pin is connected to CS

const uint8_t  CS_PIN      = A0;
const float    MAX_RESISTANCE   = 10000;
const uint16_t WIPER_RESISTANCE = 80;
*/

double gain[NUM_INCR + 1];
int phase[NUM_INCR + 1];

byte byteRead;

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"


class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};

void setup(void)
{
  // Begin I2C
  Wire.begin();

  // Begin serial at 9600 baud for output
  Serial.begin(9600);
  Serial.println("AD5933 Test Started!");

  // Perform initial configuration. Fail if any one of these fail.

  if (!(AD5933::reset()))
  {
    Serial.println("FAILED in reset!");
    while (true) ;
  }
  if (!(AD5933::setInternalClock(true)))
  {
    Serial.println("FAILED in internalClock!");
    while (true) ;
  }
  if (!(AD5933::setStartFrequency(START_FREQ)))
  {
    Serial.println("FAILED in startFreq!");
    while (true) ;
  }
  if (!(AD5933::setIncrementFrequency(FREQ_INCR)))
  {
    Serial.println("FAILED in freqInc!");
    while (true) ;
  }
  if (!(AD5933::setNumberIncrements(NUM_INCR)))
  {
    Serial.println("FAILED in numInc!");
    while (true) ;
  }
  if (!(AD5933::setPGAGain(PGA_GAIN_X1)))
  {
    Serial.println("FAILED in gain!");
    while (true) ;
  }
  if (!(AD5933::setRange(CTRL_OUTPUT_RANGE_2))) // 1 Vpp
  {
    Serial.println("FAILED in range!");
    while (true) ;
  }



  // Perform calibration sweep
  if (AD5933::calibrate(gain, phase, REF_RESIST, NUM_INCR + 1))
    Serial.println("Calibrated!");
  else
    Serial.println("Calibration failed...");



    // Create the BLE Device
  BLEDevice::init("ESP32");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  pCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");


}

void loop(void)
{
  while (Serial.available()) {
    byteRead = Serial.read();
    if(byteRead==97) // digite 'a' para executar
    {
      frequencySweepEasy();
      delay(1000);
    }
      
    if(byteRead==98) // digite 'b' para executar
    {
      frequencySweepRaw();
      delay(1000);
    }
      
  }
}

// Easy way to do a frequency sweep. Does an entire frequency sweep at once and
// stores the data into arrays for processing afterwards. This is easy-to-use,
// but doesn't allow you to process data in real time.
void frequencySweepEasy() {
  // Create arrays to hold the data
  int real[NUM_INCR + 1], imag[NUM_INCR + 1];

  // Perform the frequency sweep
  if (AD5933::frequencySweep(real, imag, NUM_INCR + 1)) {
    // Print the frequency data
    int cfreq = START_FREQ / 1000;
    for (int i = 0; i < NUM_INCR + 1; i++, cfreq += FREQ_INCR / 1000) {
      // Print raw frequency data
      Serial.print(cfreq);
      //Serial.print(": R=");
      //Serial.print(real[i]);
      //Serial.print("/I=");
      //Serial.print(imag[i]);

      // Compute impedance
      double magnitude = sqrt(pow(real[i], 2) + pow(imag[i], 2));
      double impedance = (magnitude * gain[i]); // originalmente a impedancia era calculada pelo inverso do produto ganho magnitude, a formula foi invertida pela influencia do hardware externo
      Serial.print("  |Z|=");
      Serial.print(impedance);

      double angle = (180/M_PI)*atan((double)imag[i]/(double)real[i]);
      if(real[i]<0)
        angle = angle+180;
      angle = angle - phase[i]; 
      angle = -angle; // angulo é trocado de sinal tambem por influencia do hardware externo 
      Serial.print(" phase=");
      Serial.println(angle);

      BLE_SEND_RESULT(impedance);
      BLE_SEND_RESULT(angle);
    }
    Serial.println("Frequency sweep complete!");
  } else {
    Serial.println("Frequency sweep failed...");
  }
}

// Removes the frequencySweep abstraction from above. This saves memory and
// allows for data to be processed in real time. However, it's more complex.
void frequencySweepRaw() {
  // Create variables to hold the impedance data and track frequency
  int real, imag, i = 0, cfreq = START_FREQ / 1000;

  // Initialize the frequency sweep
  if (!(AD5933::setPowerMode(POWER_STANDBY) &&          // place in standby
        AD5933::setControlMode(CTRL_INIT_START_FREQ) && // init start freq
        AD5933::setControlMode(CTRL_START_FREQ_SWEEP))) // begin frequency sweep
  {
    Serial.println("Could not initialize frequency sweep...");
  }

  // Perform the actual sweep
  while ((AD5933::readStatusRegister() & STATUS_SWEEP_DONE) != STATUS_SWEEP_DONE) {
    // Get the frequency data for this frequency point
    if (!AD5933::getComplexData(&real, &imag)) {
      Serial.println("Could not get raw frequency data...");
    }

    // Print out the frequency data
    Serial.print(cfreq);
    //Serial.print(": R=");
    //Serial.print(real);
    //Serial.print("/I=");
    //Serial.print(imag);

    // Compute impedance
    double magnitude = sqrt(pow(real, 2) + pow(imag, 2));
    double impedance = (magnitude * gain[i]); // originalmente a impedancia era calculada pelo inverso do produto ganho magnitude, a formula foi invertida pela influencia do hardware externo
    Serial.print(" |Z|=");
    Serial.print(impedance);

    double angle = (180/M_PI)*atan((double)imag/(double)real);
    if(real<0)
      angle = angle+180;
    angle = angle - phase[i];  
    angle = -angle; // angulo é trocado de sinal tambem por influencia do hardware externo
    Serial.print(" phase=");
    Serial.println(angle);

    BLE_SEND_RESULT(impedance);
    BLE_SEND_RESULT(angle);

    // Increment the frequency
    i++;
    cfreq += FREQ_INCR / 1000;
    AD5933::setControlMode(CTRL_INCREMENT_FREQ);
  }

  Serial.println("Frequency sweep complete!");

  // Set AD5933 power mode to standby when finished
  if (!AD5933::setPowerMode(POWER_STANDBY))
    Serial.println("Could not set to standby...");
}

void BLE_SEND_RESULT(double result)
{
      if (deviceConnected) {
          pCharacteristic->setValue((uint8_t*)&result, 8); // os valores armazenados no endereço da double são enviados como 8 inteiros de um byte
          pCharacteristic->notify();
          delay(3); // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
      }
      // disconnecting
      if (!deviceConnected && oldDeviceConnected) {
          delay(500); // give the bluetooth stack the chance to get things ready
          pServer->startAdvertising(); // restart advertising
          Serial.println("start advertising");
          oldDeviceConnected = deviceConnected;
      }
      // connecting
      if (deviceConnected && !oldDeviceConnected) {
          // do stuff here on connecting
          oldDeviceConnected = deviceConnected;
      }
}
