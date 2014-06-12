/*
Module GSM EFCOM v1.2 (dupa pinii GSM_ON -> D6 si GSM_RESET -> D5)
 (versiunea 1.0 foloseste D9 si D10 ptr software control )
 http://elecfreaks.com/store/download/datasheet/wireless/EFcom_v1.2.pdf
 
 Am modificat in GSM.h ptr ca libraria sa initializeze corect shield-ul
 #define GSM_ON              6 // EFCOM module ON
 #define GSM_RESET           5 // EFCOM module RESET
 
 GSM TX -> pin 2
 GSM RX -> pin 3
 
 ******** WEB *************
 $ ping is-giro-01  (10.220.10.171)
 
 */

#include <SPI.h>
#include <Ethernet.h>

#include "SIM900.h"
#include "sms.h"
#include <SoftwareSerial.h>

#define LINE_BUFFER_SIZE 25

byte MAC[] = { 
    0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED};
		
IPAddress DNS(10, 220, 10, 1);
IPAddress GW(10, 220, 0, 1);
IPAddress MASK(255, 255, 0, 0);  
IPAddress IP(10, 220, 10, 171);       //is-giro-01

EthernetServer server(80);

SMSGSM sms;
boolean started=false;
String phone = "";
String message = "";
String lineBuffer = "";

void setup() {
  lineBuffer.reserve(LINE_BUFFER_SIZE);
  phone.reserve(10);
  message.reserve(160);

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  initGSM();
  initEthernet();
}


void loop() {
  // listen for incoming clients
  EthernetClient client = server.available();
  if (client) {
    Serial.println(F("\n# new client"));
    boolean isPostRequest = false;

    if (client.connected() && client.available()) {                        
      readLine(client);
      isPostRequest = lineBuffer.startsWith("POST");        
    }

    if (isPostRequest) {
      Serial.println(F("# POST request"));
    } 
    else {
      Serial.println(F("# GET request"));
    }

    while (client.connected() && client.available()) {
      readLine(client);

      if (lineBuffer.equals("\n")) { //http GET request ends with a blank line                    
        if (isPostRequest) {      // but POST transmits parameters after a blank line                               
          readPhone(client);            
          readMessage(client);
          char responseCode = sendSMS(); 
          sendResponse(client, getMessage(responseCode));
        } 
        else {
          sendResponse(client, NULL);
        }                   
        break;          
      }
    }
    // give the web browser time to receive the data
    delay(1);
    // close the connection:
    client.stop();
    Serial.println(F("# client disconnected"));
    Serial.print(F("# Free SRAM: "));
    Serial.println(freeRam());
  }
}


void initGSM() {
  if (gsm.begin(19200)) {
    Serial.println(F("\nstatus=READY"));
    started=true;
  } 
  else {
    Serial.println(F("\nstatus=IDLE"));
  }
}


void initEthernet() {
  Ethernet.begin(MAC, IP, DNS, GW, MASK);
  server.begin();
  Serial.print(F("# server is at: "));
  Serial.println(Ethernet.localIP());
}


String getMessage(char c) {
  if (c == 1) {
    return "Message delivered.";
  } 
  else {
    return "Error delivering message.";
  } 
  //   return "Service unavailable.";    
}

char sendSMS() {  
  if (started) {    
    char p[phone.length() + 1];  // +1 extra for '\0'
    char m[message.length() + 1];
    phone.toCharArray(p, phone.length() + 1);
    message.toCharArray(m, message.length() + 1);
    return sms.SendSMS(p, m);
  } 
  return -1;  
}

void readPhone(EthernetClient client) {
  char c; 
  phone = "";
  while(client.read() != '=');  

  c = client.read();
  while(c != '&') {
    phone += c;   
    c = client.read();
  }  
  Serial.print(F("# Phone: "));
  Serial.println(phone);
}

void readMessage(EthernetClient client) {
  message = "";
  char c, x;
  while(client.read() != '=');  
  while(client.available()) {
    c = client.read();

    if (c == '+') {
      c = ' ';
    }
    if (c == '%') {
      x = h2int(client.read()) << 4;
      c = x | h2int(client.read());
    }
    message += c;
  } 
  Serial.print(F("# Msg: "));
  Serial.println(message);   
}


void readLine(EthernetClient client) {
  int i = 1;
  lineBuffer = "";
  char c = client.read();
  while (c != '\r') {
    if (i < LINE_BUFFER_SIZE){
      lineBuffer += c;
    }
    c = client.read();
    i++;
  }
  client.read(); // reading \n after \r (13, 10)

  if (lineBuffer.length() == 0) {
    lineBuffer = "\n";
  }  
  //      Serial.println(lineBuffer);
}


void sendResponse(EthernetClient client, String msg) {
  Serial.println(F("# sending response"));
  // send a standard http response header
  client.println(F("HTTP/1.1 200 OK"));
  client.println(F("Content-Type: text/html"));
  client.println(F("Connection: close"));  // the connection will be closed after completion of the response
  client.println();
  client.println(F("<!DOCTYPE HTML>"));
  client.println(F("<html>"));
  if (msg != NULL) {
    client.print(F("<p style='color:gray'>Note: "));
    client.print(msg);
    client.println(F("</p>"));
  }
  client.println(F("<form name='send-sms' method='post'>"));
  client.println(F("Phone:<br/>"));
  client.println(F("<input type='text' name='phone' pattern='[0-9]{10,10}' required/>"));
  client.println(F("<font color='gray'>(10 digits)</font><br/>"));
  client.println(F("Message:<br/>"));
  client.println(F("<textarea name='msg' maxlength='159' required></textarea>"));
  client.println(F("<font color='gray'>(max 159 chars)</font><br/>"));
  client.println(F("<input type='submit' value='Send SMS'/>"));
  client.println(F("</form>"));
  client.println(F("</html>"));
}


// convert a single hex digit character to its integer value
unsigned char h2int(char c){
  if (c >= '0' && c <='9'){
    return((unsigned char)c - '0');
  }
  if (c >= 'a' && c <='f'){
    return((unsigned char)c - 'a' + 10);
  }
  if (c >= 'A' && c <='F'){
    return((unsigned char)c - 'A' + 10);
  }
  return(0);
} 


int freeRam () 
{
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}
















