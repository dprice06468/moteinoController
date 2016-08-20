#include <RFM69.h>    //get it here: https://www.github.com/lowpowerlab/rfm69
#include <RFM69_ATC.h>//get it here: https://www.github.com/lowpowerlab/rfm69
#include <SPI.h>
#include <SPIFlash.h> //get it here: https://www.github.com/lowpowerlab/spiflash

//*********************************************************************************************
//************ IMPORTANT SETTINGS - YOU MUST CHANGE/CONFIGURE TO FIT YOUR HARDWARE *************
//*********************************************************************************************
#define CONTROLLERNODEID  2    //must be unique for each node on same network (range up to 254, 255 is used for broadcast)
#define NETWORKID         100  //the same on all nodes that talk to each other (range up to 255)
#define RGBNODEID         1
#define RELAYNODEID       3
#define SOUNDNODEID       4
//Match frequency to the hardware version of the radio on your Moteino (uncomment one):
//#define FREQUENCY   RF69_433MHZ
//#define FREQUENCY   RF69_868MHZ
#define FREQUENCY     RF69_915MHZ
#define ENCRYPTKEY    "sampleEncryptKey" //exactly the same 16 characters/bytes on all nodes!
#define IS_RFM69HW    //uncomment only for RFM69HW! Leave out if you have RFM69W!
#define ENABLE_ATC    //comment out this line to disable AUTO TRANSMISSION CONTROL
//*********************************************************************************************

#ifdef __AVR_ATmega1284P__
  #define LED           15 // Moteino MEGAs have LEDs on D15
  #define FLASH_SS      23 // and FLASH SS on D23
#else
  #define LED           9 // Moteinos have LEDs on D9
  #define FLASH_SS      8 // and FLASH SS on D8
#endif

#define SERIAL_BAUD   115200

char payload[] = "$R";
char buff[20];
byte sendSize=0;
boolean requestACK = false;
SPIFlash flash(FLASH_SS, 0xEF30); //EF30 for 4mbit  Windbond chip (W25X40CL)

#ifdef ENABLE_ATC
  RFM69_ATC radio;
#else
  RFM69 radio;
#endif

typedef enum cmdType{
  CHAR,
  RGB,
  SOUND
};

//cmdType iCmdType;
String currColor = "$k";

//------------------------------------------------------------------
void setup() {
  Serial.begin(SERIAL_BAUD);
  Serial.println("moteinoController!");
  
  radio.initialize(FREQUENCY,CONTROLLERNODEID,NETWORKID);
  radio.setHighPower(); //uncomment only for RFM69HW!
  radio.encrypt(ENCRYPTKEY);
  //radio.setFrequency(919000000); //set frequency to some custom frequency

//Auto Transmission Control - dials down transmit power to save battery (-100 is the noise floor, -90 is still pretty good)
//For indoor nodes that are pretty static and at pretty stable temperatures (like a MotionMote) -90dBm is quite safe
//For more variable nodes that can expect to move or experience larger temp drifts a lower margin like -70 to -80 would probably be better
//Always test your ATC mote in the edge cases in your own environment to ensure ATC will perform as you expect
#ifdef ENABLE_ATC
  radio.enableAutoPower(-70);
#endif

  char buff[50];
  sprintf(buff, "\nTransmitting at %d Mhz...", FREQUENCY==RF69_433MHZ ? 433 : FREQUENCY==RF69_868MHZ ? 868 : 915);
  Serial.println(buff);

#ifdef ENABLE_ATC
  Serial.println("RFM69_ATC Enabled (Auto Transmission Control)\n");
#endif
}

//------------------------------------------------------------------
void Blink(byte PIN, int DELAY_MS)
{
  pinMode(PIN, OUTPUT);
  digitalWrite(PIN,HIGH);
  delay(DELAY_MS);
  digitalWrite(PIN,LOW);
}

//------------------------------------------------------------------
void loop() {
//  Serial.println(F("What would you like to do?"));
//  while (!Serial.available());
  
  //process any serial input
  if (Serial.available() > 0)
  {
    String strData = Serial.readString();
    String strCmd;
    byte i = 0;
    byte j = strData.indexOf(',');

    Serial.println("orig:" + strData);
//    Serial.print("j:"); Serial.println(j);
    
    while (j != 255) {
      strCmd = strData.substring(i,j);
      i = j+1;
//      Serial.print("i:"); Serial.println(i);
      j = strData.indexOf(',',i);
//      Serial.print("j:"); Serial.println(j);
      Serial.println(">" + strCmd);
      checkCmd(strCmd);
    }
    delay(1000);
    Serial.println(">" + strData.substring(i));
    checkCmd(strData.substring(i));
  }

  //check for any received packets from sensors
  if (radio.receiveDone())
  {
    char charData[20];  //to hold char data received
    String strData;     //to hold string data received
    byte i;
    
    Serial.print('[');Serial.print(radio.SENDERID, DEC);Serial.print("] ");
    for (i = 0; i < radio.DATALEN; i++) {
      Serial.print((char)radio.DATA[i]);
      charData[i] = (char)radio.DATA[i];
    }
    charData[i] = '\0';
    strData = String(charData);
    Serial.println("==> " + strData);
    
    Serial.print("   [RX_RSSI:");Serial.print(radio.RSSI);Serial.print("]");

    String strCmd;
    i = 0;
    byte j = strData.indexOf(',');

    while (j != 255) {
      strCmd = strData.substring(i,j);
      i = j+1;
      j = strData.indexOf(',',i);
      Serial.println(">" + strCmd);
      checkCmd(strCmd);
    }
    delay(1000);
    Serial.println(">" + strData.substring(i));
    checkCmd(strData.substring(i));
    
    Blink(LED,3);
    Serial.println();
  }
}

//------------------------------------------------------------------
void checkCmd(String str) {
  char charData[20];
  str.toCharArray(charData, 20);

  switch (charData[0]) {
    case '$': {
      if (charData[1] == 'z') {
        Serial.println("lightening...");
        sendCmd("$k", RGBNODEID);
        delay(100);
        sendCmd("$w", RGBNODEID);
        delay(200);
        sendCmd("$k", RGBNODEID);
        delay(100);
        sendCmd("$w", RGBNODEID);
        delay(200);
        sendCmd("$k", RGBNODEID);
        delay(600);
        sendCmd(currColor, RGBNODEID);
        break;
      }
      else {
        Serial.println("sending color..." + str);
        sendCmd(str, RGBNODEID);
        currColor = str;
        currColor.toUpperCase();
      }
      break;
    }
    
    case '%': {
      Serial.println("sending rgb color..." + str);
      sendCmd(str, RGBNODEID);
      currColor = str;
      break;
    }
    
    case '#': {
      Serial.println("starting sound..." + str);
//      sendCmd(str, SOUNDNODEID);
      sendCmd(str, RGBNODEID);
      break;
    }
    
    case '@': {
      Serial.println("sending to relay..." + str);
      sendCmd(str, RELAYNODEID);
      break;
    }
  }
}

//------------------------------------------------------------------
void sendCmd(String strCmd, int rcvrID) {
  char cmd[20];
  strCmd.toCharArray(cmd, 20);
  
  Serial.println("Sending: " + strCmd);
  sendSize = strCmd.length();

//  if (radio.sendWithRetry(RGBNODEID, cmd, sendSize))
//   Serial.print(" ok!");
//  else Serial.println(" nothing...");
  radio.send(rcvrID, cmd, sendSize);
}

//------------------------------------------------------------------
/*void checkCmd(char input) {
    switch (input) {
      case '$': {
        iCmdType = CHAR;
        break;
      }
      
      case '%': {
        iCmdType = RGB;
        break;
      }
      
      case '#': {
        iCmdType = SOUND;
        break;
      }
      
      case 'c': {
         Serial.println("close relay...");
         sendCmd("$c", RELAYNODEID);
         break;
      }
    
      case 'o': {
         Serial.println("open relay...");
         sendCmd("$o", RELAYNODEID);
         break;
      }
    
      case 'R': {
         Serial.println("fade to red...");
         sendCmd("$R", RGBNODEID);
         currColor = "$R";
         break;
      }

      case 'r': {
        if (iCmdType == RGB) {
         Serial.println("instant red...");
         sendCmd("$r", RGBNODEID);
         currColor = "$R";
        }
        else
          radio.readAllRegs();
  
      }
      
      case 'g': {
         Serial.println("instant green...");
         sendCmd("$g", RGBNODEID);
         currColor = "$G";
         break;
      }
    
      case 'G': {
         Serial.println("fade to green...");
         sendCmd("$G", RGBNODEID);
         currColor = "$G";
         break;
      }
    
      case 'b': {
         Serial.println("instant blue...");
         sendCmd("$b", RGBNODEID);
         currColor = "$B";
         break;
      }
    
      case 'B': {
         Serial.println("fade to blue...");
         sendCmd("$B", RGBNODEID);
         currColor = "$B";
         break;
      }
    
      case 'w': {
         Serial.println("instant white...");
         sendCmd("$w", RGBNODEID);
         currColor = "$W";
         break;
      }
    
      case 'W': {
         Serial.println("fade to white...");
         sendCmd("$W", RGBNODEID);
         currColor = "$W";
         break;
      }
    
      case 'k': {
         Serial.println("instant black...");
         sendCmd("$k", RGBNODEID);
         currColor = "$K";
         break;
      }
    
      case 'K': {
         Serial.println("fade to black...");
         sendCmd("$K", RGBNODEID);
         currColor = "$K";
         break;
      }
    
      case 'z': {
         Serial.println("lightening...");
         sendCmd("$k", RGBNODEID);
         delay(100);
         sendCmd("$w", RGBNODEID);
         delay(200);
         sendCmd("$k", RGBNODEID);
         delay(100);
         sendCmd("$w", RGBNODEID);
         delay(200);
         sendCmd("$k", RGBNODEID);
         delay(600);
         sendCmd(currColor, RGBNODEID);
         break;
      }
    }
    
    if (input == 'r') //d=dump register values
      radio.readAllRegs();
    //if (input == 'E') //E=enable encryption
    //  radio.encrypt(KEY);
    //if (input == 'e') //e=disable encryption
    //  radio.encrypt(null);

    if (input == 'd') //d=dump flash area
    {
      Serial.println("Flash content:");
      uint16_t counter = 0;

      Serial.print("0-256: ");
      while(counter<=256){
        Serial.print(flash.readByte(counter++), HEX);
        Serial.print('.');
      }
      while(flash.busy());
      Serial.println();
    }
    if (input == 'e')
    {
      Serial.print("Erasing Flash chip ... ");
      flash.chipErase();
      while(flash.busy());
      Serial.println("DONE");
    }
    if (input == 'i')
    {
      Serial.print("DeviceID: ");
      word jedecid = flash.readDeviceId();
      Serial.println(jedecid, HEX);
    }
}*/

