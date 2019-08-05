#include <SPI.h>
#include <Ethernet.h>

const bool useSerial = false;

//timer interval
unsigned long previousMillis = 0;
long interval = 1000;

//analog voltage range
const float minVolt = 0;
const float maxVolt = 255;

//analog output calibration
float minValue = 0.00;
float maxValue = 0.01;

//cursor for current command
enum CommandTypeRunning { START, CYCLE };
CommandTypeRunning commandTypeRunning = START;
byte currentCommandIndex = 0;
bool commandRunning = false;

//the buffer string for concatenating the response
String bufferString = "";

//definitions for command structures
enum OutputType { NONE, DIGITAL, ANALOG };
struct MappedResponse
{
  bool flag;
  byte inputStrPos;
  byte outPinNumber;
  OutputType outPinType;
};
const byte mappedResponseCount = 7;
struct Command
{
  const char * command;
  MappedResponse mappedResponse[mappedResponseCount];
};
//startup check commands
struct Command startupCommands[] = {
  {"MSTART\r", {{true,0}}},
  {"RMODEANALOG\r", {{true,1},{true,2}}}
};
//commands for continually grabbing data
const byte numberOfCycleCommands = 3;
struct Command cycleCommands[numberOfCycleCommands] = {
  {"RMMEAS\r",{
    {true,1,3,ANALOG}
  }},
  {"RMMESSAGES\r",{
    {true,0,4,DIGITAL},//System error
    {true,1,5,DIGITAL},//Laser error
    {true,2,6,DIGITAL},//Flow error
    {true,3,7,DIGITAL},//Flow blocked error
    {true,4,8,DIGITAL},//Maximum concentration
    {true,5,9,DIGITAL},//STEL Alarmed
    {true,6,11,DIGITAL}//Filter concentration error
  }},
  {"RMALARM\r",{
    {true,0,12,DIGITAL},//Alarm 1
    {true,1,13,DIGITAL}//Alarm 2
  }}
};



// Enter a MAC address and IP address for your controller below.
// The IP address will be dependent on your local network:
byte mac[] = {
  0x90, 0xA2, 0xDA, 0x0F, 0xD4, 0xB7
};
IPAddress ip(192, 168, 0, 2);
IPAddress subnet(255,255,255,0);
// Enter the IP address of the server you're connecting to:
IPAddress server(192, 168, 0, 1);
// Initialize the Ethernet client library
EthernetClient client;

void setup() {
  // You can use Ethernet.init(pin) to configure the CS pin
  Ethernet.init(10);  // Most Arduino shields
  // start the Ethernet connection:
  Ethernet.begin(mac, ip, server, server, subnet);

  if (useSerial) {
    // Open serial communications and wait for port to open:
    Serial.begin(9600);
    while (!Serial) {
      ; // wait for serial port to connect. Needed for native USB port only
    } 
  }

  // Check for Ethernet hardware present
  if (Ethernet.hardwareStatus() == EthernetNoHardware) {
    if (useSerial) {
      Serial.println("Ethernet shield was not found.  Sorry, can't run without hardware. :(");
    }
    while (true) {
      delay(1); // do nothing, no point running without Ethernet hardware
    }
  }
  while (Ethernet.linkStatus() == LinkOFF) {
    if (useSerial) {
      Serial.println("Ethernet cable is not connected.");
    }
    delay(500);
  }

  if (useSerial) {
    Serial.println("connecting...");
  }
  
  // give the Ethernet shield a second to initialize:
  delay(5000);
  while (!client.connect(server, 3602)) {
    if (useSerial) {
      Serial.println("attempted connecting again...");
    }
    delay(5000);
  }

  if (useSerial) {
    Serial.println("connected");
  }
  //even after a successful connection, sleep for another 20 seconds
  delay(20000);
  //initialise all the pins being used for output
  for (int i=0; i<numberOfCycleCommands; i++) {
    for (int j=0; j<mappedResponseCount; j++){
      pinMode(cycleCommands[i].mappedResponse[j].outPinNumber, OUTPUT);
    }
  } 
}

void loop() {
  unsigned long currentMillis = millis();
  //timer that runs every interval
  if (((unsigned long)(currentMillis - previousMillis) >= interval) && (commandRunning == false)) {
    switch (commandTypeRunning) {
      case START:
      client.print(startupCommands[currentCommandIndex].command);
      break;
      case CYCLE:
      client.print(cycleCommands[currentCommandIndex].command);
      break;
    }
    commandRunning = true;
    currentCommandIndex ++;
    previousMillis = currentMillis;
  }

  //any received buffer data
  if (client.available()) {
    char inByte = client.read();
    if (useSerial) {
      Serial.print(inByte);  
    }
    //if an end of line character is detected
    if (inByte == '\n') {
      switch (commandTypeRunning) {
        case START:
        sendData(startupCommands[currentCommandIndex-1]);
        if (currentCommandIndex > 1) {
          currentCommandIndex = 0;
          commandTypeRunning = CYCLE;
        }
        break;
        case CYCLE:
        sendData(cycleCommands[currentCommandIndex-1]);
        if (currentCommandIndex > 2) {
          currentCommandIndex = 0;
        }      
        break;
      }
      commandRunning = false;
      bufferString = "";
    }
    else bufferString += inByte;
  }
}


void sendData(Command command){
  int commaIndex;
  byte offsetCommaCount = 0;
  for (int i=0; i<mappedResponseCount; i++) {
    //loop through for each comma
    for (int j=offsetCommaCount; j<=command.mappedResponse[i].inputStrPos; j++) {
      if (command.mappedResponse[i].flag) {
        commaIndex = bufferString.indexOf(',');
        if (j==command.mappedResponse[i].inputStrPos) {
          //specific function for handling start commands
          if (command.command == "RMODEANALOG") {
            switch (command.mappedResponse[i].inputStrPos) {
              case 1:
              minValue = bufferString.substring(0,commaIndex).toFloat();
              break;
              case 2:
              maxValue = bufferString.substring(0,commaIndex).toFloat();
              break;
            }
          } 
          //for handling all the digital and analog outputs
          switch (command.mappedResponse[i].outPinType) {
            case ANALOG:
              float tempValue;
              tempValue = bufferString.substring(0,commaIndex).toFloat();
              if (useSerial) {
                Serial.println((maxVolt-minVolt)/(maxValue-minValue)*(tempValue-minValue) + minVolt);
              }
              analogWrite(command.mappedResponse[i].outPinNumber,(maxVolt-minVolt)/(maxValue-minValue)*(tempValue-minValue) + minVolt);
            break;
            case DIGITAL:
              String tempStr;
              tempStr = bufferString.substring(0,commaIndex);
              if (tempStr == "1") {
                if (useSerial) {
                  Serial.println("HIGH");
                }
                digitalWrite(command.mappedResponse[i].outPinNumber,HIGH);
              }
              else if (tempStr == "0") {
                if (useSerial) {
                  Serial.println("LOW");
                }
                digitalWrite(command.mappedResponse[i].outPinNumber,LOW);
              }
            break;
          }          
        }
        bufferString = bufferString.substring(commaIndex+1);
      }
    }
    offsetCommaCount = command.mappedResponse[i].inputStrPos+1;
  }
}
