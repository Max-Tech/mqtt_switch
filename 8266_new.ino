#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <PubSubClient.h>
#include "DHT.h"
#include <WiFiUdp.h>
#include <ESP8266WiFiMulti.h>

// MQTT Server address
const char* mqtt_server = "192.168.1.180";
WiFiClient espClient;
PubSubClient client(espClient);

// DHT
#define DHTPIN 2
#define DHTTYPE DHT22

DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "**************";
const char* password = "**************";
String webSite,javaScript,XML;

// NTP

ESP8266WiFiMulti wifiMulti;      
WiFiUDP UDP;                     
IPAddress timeServerIP;          
const char* NTPServerName = "time.nist.gov";
const int NTP_PACKET_SIZE = 48; 
byte NTPBuffer[NTP_PACKET_SIZE]; 

unsigned long intervalNTP = 60000; // Request NTP time every minute
unsigned long prevNTP = 0;
unsigned long lastNTPResponse = millis();
uint32_t timeUNIX = 0;

unsigned long prevActualTime = 0;  
uint32_t actualTime;                             

ESP8266WebServer server(80);

void setup(void){

  delay(2);
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  Serial.println("");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  
  if (MDNS.begin("esp8266")) {
    Serial.println("MDNS responder started");
  }
  
  Serial.println("Starting UDP");
  UDP.begin(123);                          
  Serial.print("Local port:\t");
  Serial.println(UDP.localPort());
  Serial.println();
  
  if(!WiFi.hostByName(NTPServerName, timeServerIP)) { 
    Serial.println("DNS lookup failed. Rebooting.");
    Serial.flush();
    ESP.reset();
  }
  Serial.print("Time server IP:\t");
  Serial.println(timeServerIP);
  
  Serial.println("\r\nSending NTP request ...");
  sendNTPpacket(timeServerIP);
   
  server.on("/",handleWebsite);
  server.on("/xml",handleXML);
  server.onNotFound(handleNotFound);
  server.begin();
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);


}








void loop(void){ 


   
  server.handleClient();
  
  if (!client.connected()) {reconnect();}
  client.loop();

  time_now();

  if (actualTime != prevActualTime && timeUNIX != 0 && actualTime > prevActualTime+60) { // If a second has passed since last print
    prevActualTime = actualTime;
    Serial.printf("\rTime:\t%d:%d:%d   ", getHours(actualTime), getMinutes(actualTime), getSeconds(actualTime));
    Serial.println();
    getAndSendTemperatureAndHumidityData();
      
  }  

  if (Serial.available()){
    String in_data = Serial.readString();
    if (in_data == "1ON") {client.publish("cmnd/MT_1/POWER", "ON"); }
  }
}










// -----------------------------  NTP Functions Start ----------------------------------------
void time_now() {
unsigned long currentMillis = millis();

  if (currentMillis - prevNTP > intervalNTP) { // If a minute has passed since last NTP request
    prevNTP = currentMillis;
    Serial.println("\r\nSending NTP request ...");
    sendNTPpacket(timeServerIP);               // Send an NTP request
  }

  uint32_t time = getTime();                   // Check if an NTP response has arrived and get the (UNIX) time
  if (time) {                                  // If a new timestamp has been received
    timeUNIX = time;
    Serial.print("NTP response:\t");
    Serial.println(timeUNIX);
    lastNTPResponse = currentMillis;
  } else if ((currentMillis - lastNTPResponse) > 3600000) {
    Serial.println("More than 1 hour since last NTP response. Rebooting.");
    Serial.flush();
    ESP.reset();
  }

  actualTime = timeUNIX + (currentMillis - lastNTPResponse)/1000;
  
  
}

uint32_t getTime() {
  if (UDP.parsePacket() == 0) { // If there's no response (yet)
    return 0;
  }
  UDP.read(NTPBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
  // Combine the 4 timestamp bytes into one 32-bit number
  uint32_t NTPTime = (NTPBuffer[40] << 24) | (NTPBuffer[41] << 16) | (NTPBuffer[42] << 8) | NTPBuffer[43];
  // Convert NTP time to a UNIX timestamp:
  // Unix time starts on Jan 1 1970. That's 2208988800 seconds in NTP time:
  const uint32_t seventyYears = 2208988800UL;
  // subtract seventy years:
  uint32_t UNIXTime = NTPTime - seventyYears;
  return UNIXTime-(3600*5);
}

void sendNTPpacket(IPAddress& address) {
  memset(NTPBuffer, 0, NTP_PACKET_SIZE);  // set all bytes in the buffer to 0
  // Initialize values needed to form NTP request
  NTPBuffer[0] = 0b11100011;   // LI, Version, Mode
  // send a packet requesting a timestamp:
  UDP.beginPacket(address, 123); // NTP requests are to port 123
  UDP.write(NTPBuffer, NTP_PACKET_SIZE);
  UDP.endPacket();
}

inline int getSeconds(uint32_t UNIXTime) {
  return UNIXTime % 60;
}

inline int getMinutes(uint32_t UNIXTime) {
  return UNIXTime / 60 % 60;
}

inline int getHours(uint32_t UNIXTime) {
  return UNIXTime / 3600 % 24;
}

//---------------------------------- SENSOR ------------------------------------

void getAndSendTemperatureAndHumidityData()
{
  Serial.print("Collecting temperature data. - ");
  Serial.println(actualTime);

 
  float h = dht.readHumidity();
  float t = dht.readTemperature();

  if (isnan(h) || isnan(t)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }


  String temperature = String(t);
  String humidity = String(h);

  String payload = "{";
  payload +="\"Time\":\"00:00:00\",";
  payload +="\"AM2301\":";
  payload +="{";
  payload += "\"Temperature\":"; payload += temperature; payload += ",";
  payload += "\"Humidity\":"; payload += humidity;
  payload += "}";
  payload += ",";
  payload += "\"TempUnit\":\"C\"}";
  char attributes[100];
  payload.toCharArray( attributes, 100 );
  client.publish( "tele/MT/SENSOR", attributes );
  Serial.println( attributes );

}

//---------------------------------- MQTT -------------------------

void callback(char* topic, byte* payload, unsigned int length) {
 Serial.print(topic);
 Serial.print("/");
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}

void reconnect() {
  while (!client.connected()) {
    if (client.connect("ESP8266Client")) {
      client.subscribe("tele/MT/SENSOR");
      client.subscribe("cmnd/MT_1/POWER");
    } else {
      delay(5000);
    }
  }
}

//------------------------------HTTP ---------------------------

void buildWebsite(){
  buildJavascript();
  webSite="<!DOCTYPE HTML>\n";
  webSite+=javaScript;
  webSite+="<BODY onload='process()'>\n";
  webSite+="<BR>esp8266 data info<BR>\n";
  webSite+="Time = <A id='runtime'></A>\n";
  webSite+="</BODY>\n";
  webSite+="</HTML>\n";
}

void buildJavascript(){
  javaScript="<SCRIPT>\n";
  javaScript+="var xmlHttp=createXmlHttpObject();\n";

  javaScript+="function createXmlHttpObject(){\n";
  javaScript+=" if(window.XMLHttpRequest){\n";
  javaScript+="    xmlHttp=new XMLHttpRequest();\n";
  javaScript+=" }else{\n";
  javaScript+="    xmlHttp=new ActiveXObject('Microsoft.XMLHTTP');\n";
  javaScript+=" }\n";
  javaScript+=" return xmlHttp;\n";
  javaScript+="}\n";

  javaScript+="function process(){\n";
  javaScript+=" if(xmlHttp.readyState==0 || xmlHttp.readyState==4){\n";
  javaScript+="   xmlHttp.open('PUT','xml',true);\n";
  javaScript+="   xmlHttp.onreadystatechange=handleServerResponse;\n"; // no brackets?????
  javaScript+="   xmlHttp.send(null);\n";
  javaScript+=" }\n";
  javaScript+=" setTimeout('process()',1000);\n";
  javaScript+="}\n";
  
  javaScript+="function handleServerResponse(){\n";
  javaScript+=" if(xmlHttp.readyState==4 && xmlHttp.status==200){\n";
  javaScript+="   xmlResponse=xmlHttp.responseXML;\n";
  javaScript+="   xmldoc = xmlResponse.getElementsByTagName('response');\n";
  javaScript+="   message = xmldoc[0].firstChild.nodeValue;\n";
  javaScript+="   document.getElementById('runtime').innerHTML=message;\n";
  javaScript+=" }\n";
  javaScript+="}\n";
  javaScript+="</SCRIPT>\n";
}

void buildXML(){
  XML="<?xml version='1.0'?>";
  XML+="<response>";
  XML+=DataFromArduino();;
  XML+="</response>";
}

String DataFromArduino(){
  String coming;
  coming=actualTime;
  return coming;
  }

void handleWebsite(){
buildWebsite();
server.send(200,"text/html",webSite);
}

void handleXML(){
  buildXML();
  server.send(200,"text/xml",XML);
}


void handleNotFound(){
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET)?"GET":"POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i=0; i<server.args(); i++){
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}
