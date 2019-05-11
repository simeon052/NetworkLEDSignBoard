#include <avr/pgmspace.h>
#include <LedControl.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <sdfonts.h>
#include <Time.h>
#include <TimeLib.h>

const int numDevices = 4;    // number of MAX7219s used
const long scrollDelay = 75; // adjust scrolling speed

unsigned long bufferLong[14] = {0};
LedControl lc = LedControl(11, 13, 8, numDevices);

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};

IPAddress ip(192, 168, 10, 226);
unsigned int localPort = 80;

char packetBuffer[UDP_TX_PACKET_MAX_SIZE]; //buffer to hold incoming packet,
EthernetUDP Udp;
// For NTP via Wifi
// Change this value to suit your Time Zone
const int UTC_offset = 9; // Japanese Standard Time

const int timeZone = 9; // Japan Standard Time

char timeServer[] = "ntp.nict.jp"; // NTP server

const int NTP_PACKET_SIZE = 48; // NTP timestamp is in the first 48 bytes of the message
const int UDP_TIMEOUT = 2000;   // timeout in miliseconds to wait for an UDP packet to arrive

byte packetBuffer4NTP[NTP_PACKET_SIZE]; // buffer to hold incoming and outgoing packets

void setup()
{
    Serial.begin(115200);

    for (int x = 0; x < numDevices; x++)
    {
        lc.shutdown(x, false); //The MAX72XX is in power-saving mode on startup
        lc.setIntensity(x, 2); // Set the brightness to default value
        lc.clearDisplay(x);    // and clear the display
    }
    Serial.println("LED matrix init is done.");

    // // disable w5100 SPI while setting up SD
    // pinMode(53,OUTPUT);
    // pinMode(10,OUTPUT);
    // pinMode(4,OUTPUT);
    // digitalWrite(53,HIGH);
    // digitalWrite(4, LOW);

    if (SDfonts.init(4))
    {
        Serial.println("SDFont init is done.");
        lc.setLed(0, 7, 7, true);
    }
    else
    {
        Serial.println("SDFont init is failed.");
        lc.setLed(0, 0, 0, true);
    }

    Serial.println("Start Ethernet initialize");
    // start the Ethernet connection:

    if (Ethernet.begin(mac) == 0)
    {
        Serial.println("Failed to configure Ethernet using DHCP");
        // try to congifure using IP address instead of DHCP:
    Ethernet.begin(mac, ip);
    }

    // give the Ethernet shield a second to initialize:
    delay(1000);

    scrollMessageUTF8("アドレス：");

    Serial.print("Local IP: ");
    Serial.println(Ethernet.localIP());
    Serial.print("subnetMask: ");
    Serial.println(Ethernet.subnetMask());
    Serial.print("gatewayIP: ");
    Serial.println(Ethernet.gatewayIP());
    Serial.print("dnsServerIP: ");
    Serial.println(Ethernet.dnsServerIP());

    // print your local IP address:
    ip = Ethernet.localIP();
    for (byte thisByte = 0; thisByte < 4; thisByte++)
    {
        // print the value of each byte of the IP address:
        char buffer[5];
        String(ip[thisByte], DEC).getBytes(buffer, 5, 0);
        scrollMessageUTF8(buffer);
        if (thisByte < 3)
        {
            scrollMessageUTF8(".");
        }
    }
    delay(5000);

    Udp.begin(localPort);
    clearAll();

    Serial.println("Start Ethernet is done.");

    setSyncProvider(getNtpTime);
    digitalClockDisplay();
    delay(3000);
    Serial.println("Initialize is done.");
}
// send an NTP request to the time server at the given address
void sendNTPpacket(char *ntpSrv)
{
    // set all bytes in the buffer to 0
    memset(packetBuffer4NTP, 0, NTP_PACKET_SIZE);
    // ialize values needed to form NTP request
    // (see URL above for details on the packets)

    packetBuffer4NTP[0] = 0b11100011; // LI, Version, Mode
    packetBuffer4NTP[1] = 0;          // Stratum, or type of clock
    packetBuffer4NTP[2] = 6;          // Polling Interval
    packetBuffer4NTP[3] = 0xEC;       // Peer Clock Precision
    // 8 bytes of zero for Root Delay & Root Dispersion
    packetBuffer4NTP[12] = 49;
    packetBuffer4NTP[13] = 0x4E;
    packetBuffer4NTP[14] = 49;
    packetBuffer4NTP[15] = 52;

    // all NTP fields have been given values, now
    // you can send a packet requesting a timestamp:
    Udp.beginPacket(ntpSrv, 123); //NTP requests are to port 123

    Udp.write(packetBuffer4NTP, NTP_PACKET_SIZE);

    Udp.endPacket();
}

time_t getNtpTime()
{
    while (Udp.parsePacket() > 0)
        ; // discard any previously received packets
    Serial.println("Transmit NTP Request");
    sendNTPpacket(timeServer);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500)
    {
        int size = Udp.parsePacket();
        if (size >= NTP_PACKET_SIZE)
        {
            Serial.println("Receive NTP Response");
            Udp.read(packetBuffer4NTP, NTP_PACKET_SIZE); // read packet into the buffer
            unsigned long secsSince1900;
            // convert four bytes starting at location 40 to a long integer
            secsSince1900 = (unsigned long)packetBuffer4NTP[40] << 24;
            secsSince1900 |= (unsigned long)packetBuffer4NTP[41] << 16;
            secsSince1900 |= (unsigned long)packetBuffer4NTP[42] << 8;
            secsSince1900 |= (unsigned long)packetBuffer4NTP[43];
            clearAll();
            return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
        }else{
            Serial.println(".");
            delay(1000);
        }
    }
    Serial.println("No NTP Response :-(");
    return 0; // return 0 if unable to get the time
}

void clearAll()
{
    scrollMessageUTF8("　　　　");
    for (int x = 0; x < numDevices; x++)
    {
        lc.clearDisplay(x);   // and clear the display
        lc.shutdown(x, true); //The MAX72XX is in power-saving mode on startup
    }
}

void powerOnAll()
{
    for (int x = 0; x < numDevices; x++)
    {
        lc.shutdown(x, false); //The MAX72XX is in power-saving mode on startup
        lc.clearDisplay(x);    // and clear the display
    }
}

void digitalClockOnLED()
{
    // digital clock display of the time
    digitalClockDisplay();

    //scrollMessageUTF8("　　　　");
    String dateTime = ""; //String(year()) + "/" + printDigitsOnLED(month()) + "/" + printDigitsOnLED(day()) + " ";
    dateTime += printDigitsOnLED(hour()) + ":";
    dateTime += printDigitsOnLED(minute());

    scrollMessageUTF8(convertHanNum2ZenNum(dateTime.c_str(), dateTime.length()));
    scrollMessageUTF8("　　　　");
}

char *convertHanNum2ZenNum(char *src, int num){
    char zenNumStr[24];

    for (int c = 0; c < 24;c+=3){
        zenNumStr[c] = 0x00;
    }
    for (int c = 0, i = 0; i < num; i++, c += 3)
    {
        Serial.print(i);
        Serial.print(" - ");
        Serial.println(c);
            zenNumStr[c] = 0xEF;
            zenNumStr[c + 1] = 0xBC;
            zenNumStr[c + 2] = src[i] - 0x30 + 0x90;
    }

    return zenNumStr;
}

String printDigitsOnLED(int digits)
{
    String digitStr = "";
    if (digits < 10)
        digitStr += "0";
    digitStr += String(digits);

    return digitStr;
}
void digitalClockDisplay()
{
    // digital clock display of the time
    Serial.print(year());
    Serial.print("/");
    Serial.print(month());
    Serial.print("/");
    Serial.print(day());
    Serial.print(" ");
    Serial.print(hour());
    printDigits(minute());
    printDigits(second());
    Serial.println();

}

void printDigits(int digits)
{
    // utility for digital clock display: prints preceding colon and leading 0
    Serial.print(":");
    if (digits < 10)
        Serial.print('0');
    Serial.print(digits);
}

// Scroll Message
void scrollMessageUTF8(char *pUTF8)
{
    powerOnAll();
    int count = 8;
    uint8_t buf[8] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    SDfonts.open();         // フォントのオープン
    SDfonts.setFontSize(8); // フォントサイズ 8に固定
    int i = 0;
    uint16_t currentCharUtf16;
    SDfonts.charUFT8toUTF16(&currentCharUtf16, pUTF8);
    //    Serial.println(currentCharUtf16);
    while (pUTF8 = SDfonts.getFontData(buf, pUTF8)) // フォントの取得
    {
        if (currentCharUtf16 <= 0x007F)
        {
            // ASCII chars have 4 dot width
            count = 4;
        }
        else
        {
            count = 8;
        }
        SDfonts.charUFT8toUTF16(&currentCharUtf16, pUTF8);

        i++;
        for (int a = 0; a < 8; a++)
        {                                        // Loop 8 times
            unsigned long c = buf[a];            // Index into character table to get row data
            unsigned long x = bufferLong[a * 2]; // Load current scroll buffer
            x = x | c;                           // OR the new character onto end of current
            bufferLong[a * 2] = x;               // Store in buffer
        }

        for (byte x = 0; x < count; x++)
        {
            rotateBufferLong();
            printBufferLong();
            delay(scrollDelay);
        }
        for (int i = 0; i < 8; i++)
        {
            // Clear font buf
            buf[i] = 0x00;
        }
    }
    SDfonts.close();
    Serial.print(i);
    Serial.println(" characters are sent.");
    return i;
}

// Rotate the buffer
void rotateBufferLong()
{
    for (int a = 0; a < 8; a++)
    {                                        // Loop 8 times
        unsigned long x = bufferLong[a * 2]; // Get low buffer entry
        byte b = bitRead(x, 31);             // Copy high order bit that gets lost in rotation
        x = x << 1;                          // Rotate left one bit
        bufferLong[a * 2] = x;               // Store new low buffer
        x = bufferLong[a * 2 + 1];           // Get high buffer entry
        x = x << 1;                          // Rotate left one bit
        bitWrite(x, 0, b);                   // Store saved bit
        bufferLong[a * 2 + 1] = x;           // Store new high buffer
    }
}
// Display Buffer on LED matrix
void printBufferLong()
{
    for (int a = 0; a < 8; a++)
    {                                            // Loop 7 times for a 5x7 font
        unsigned long x = bufferLong[a * 2 + 1]; // Get high buffer entry
        byte y = x;                              // Mask off first character
        lc.setRow(3, a, y);                      // Send row to relevent MAX7219 chip
        x = bufferLong[a * 2];                   // Get low buffer entry
        y = (x >> 24);                           // Mask off second character
        lc.setRow(2, a, y);                      // Send row to relevent MAX7219 chip
        y = (x >> 16);                           // Mask off third character
        lc.setRow(1, a, y);                      // Send row to relevent MAX7219 chip
        y = (x >> 8);                            // Mask off forth character
        lc.setRow(0, a, y);                      // Send row to relevent MAX7219 chip
    }
}

bool clockIsEnabled = true;
//----------------------------------------------------------------
// 日時 <-> 1970/1/1 00:00:00からの秒数(UNIXの日付) 相互変換
//----------------------------------------------------------------
#define SECONDS_IN_DAY   86400   // 24 * 60 * 60
#define SECONDS_IN_HOUR  3600    // 60 * 60

typedef struct {
  int  year;
  byte month;
  byte day;
  byte hour;
  byte min;
  byte sec;
} date_time;

date_time tm;
void unix_time_to_date( unsigned long unix_datetime, date_time *tm );

//----------------------------------------------------------------
// 西暦０年１月１日からの日数を返す
//----------------------------------------------------------------
unsigned long calc_0_days(int year, byte month, byte day) {
  unsigned long days;
  int daysinmonth_ruiseki[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
  
  year--; // 当年は含まず 
  days = (unsigned long)year * 365;
  days += year / 4;  // 閏年の日数を足しこむ
  days -= year/100;  // 閏年で無い日数を差し引く
  days += year/400;  // 差し引きすぎた日数を足しこむ
  days += (unsigned long)daysinmonth_ruiseki[month-1];
  if( is_leapyear( year ) &&  3 <= month ) {
    day++;
  }
  days += (unsigned long)day;
  
  return days;
}

//----------------------------------------------------------------
// 西暦2018年1月11日からの日数を返す
//----------------------------------------------------------------
unsigned long calc_MakoRikoBirth_days(int year, byte month, byte day) {
  unsigned long days;

  return calc_0_days(year, month, day) - calc_0_days(2018, 1, 11);
}

//----------------------------------------------------------------
// 閏年か否かを返す
//----------------------------------------------------------------
boolean is_leapyear( int year )
{
  if( (year % 400) == 0 || ((year % 4) == 0 && (year % 100) != 0)) {
    return true;
  } else {
    return false;
  }
}

//----------------------------------------------------------------
// Unixの日付を日時に変換　ループ処理しているので2msec程度かかる
//----------------------------------------------------------------
void unix_time_to_date( unsigned long unix_datetime, date_time *tm )
{
  tm->year  = 1970;
  tm->month = 1;
  tm->day   = 1;
  tm->hour  = 0;
  tm->min   = 0;
  tm->sec   = 0;
  
  unsigned long yearindate;
  unsigned long seconds_year;
  unsigned long seconds_month;
  byte daysinmonth[12] = {31,28,31,30,31,30,31,31,30,31,30,31};

  unix_datetime += 9 * SECONDS_IN_HOUR;
  while( unix_datetime > 0 ) {
    if( is_leapyear(tm->year) ) {
      yearindate = 366;
      daysinmonth[1] = 29;
    } else {
      yearindate = 365;
      daysinmonth[1] = 28;
    }
    seconds_year  = yearindate * SECONDS_IN_DAY;
    seconds_month = daysinmonth[tm->month - 1] * SECONDS_IN_DAY;
    if( unix_datetime >= seconds_year ) {
      tm->year++;
      unix_datetime -= seconds_year;
    } else if( unix_datetime >= seconds_month ) {
      tm->month++;
      unix_datetime -= seconds_month;
    } else if( unix_datetime >= SECONDS_IN_DAY ) {
      tm->day++;
      unix_datetime -= SECONDS_IN_DAY;  
    } else if( unix_datetime >= SECONDS_IN_HOUR ) {
      tm->hour++;
      unix_datetime -= SECONDS_IN_HOUR; 
    } else if( unix_datetime >= 60 ) {
      tm->min++;
      unix_datetime -= 60;
    } else {
      tm->sec = (byte)unix_datetime;
      unix_datetime= 0;
    }
  }
}

void Dispaly_MakoRiko(){
    int days = calc_MakoRikoBirth_days(year(), month(), day());
    String bufPre = String("真子 莉子 誕生から ");
    bufPre.concat(String(days));
    bufPre.concat(" 日");
    Serial.print(bufPre);
//    char[] byteBuf = bufPre.toCharArray();
    //bufPre.getBytes(byteBuf,64);
    scrollMessageUTF8(bufPre.c_str());
    delay(1000);
    clearAll();
}
void loop()
{
    // if there's data available, read a packet
    int packetSize = Udp.parsePacket();
    if (packetSize)
    {
        int len = Udp.read(packetBuffer, UDP_TX_PACKET_MAX_SIZE);
        Serial.println(len);
        //終端文字設定
        if (len > 0)
            packetBuffer[len] = '\0';

            if(packetBuffer[0] == 'c'){
                Serial.println("Clock display is enabled");
                clockIsEnabled = true;
            }else if(packetBuffer[0] == 'o'){
                Serial.println("Clock display is diabled");
                clockIsEnabled = false;
            }else{
                if(len > 2){
                    if((packetBuffer[0] == 's') &&(packetBuffer[1] == 't') &&(packetBuffer[2] == 'r')){
                        for(int c = 0; c < 3;c++){
                            packetBuffer[c] = ' ';
                        }
                        Serial.println(packetBuffer);
                        scrollMessageUTF8(packetBuffer);
                    }
                }
            }
            delay(3000);
            clearAll();
    }
    else
    {
        if(clockIsEnabled){
            digitalClockOnLED();
            delay(1000);
            Dispaly_MakoRiko();

        }
        delay(500);
    }
//    digitalClockOnLED();
//    delay(2000);
}
