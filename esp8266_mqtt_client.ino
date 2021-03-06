#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// Wifi Connection
const char* ssid = "Linksys_17211_2";
const char* password = "La794Vlh";

// MQTT Server address
const char* mqtt_server = "192.168.1.180";

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

char string[50];
char byteRead;


void setup() {
  
   
 // pinMode(2, OUTPUT);
 // digitalWrite(2, 1);

  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void setup_wifi() {

  delay(2);
  // We start by connecting to a WiFi network
   Serial.println();
   Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
 // Serial.print("Message arrived [");
 // Serial.print(topic);
 // Serial.print("] ");
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
   // Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
     // Serial.println("connected");
      // Once connected, publish an announcement...
      //client.publish("/switch/relay/state", "connected");
      // ... and resubscribe
      client.subscribe("stat/MT_1/POWER");
    } else {
     // Serial.print("failed, rc=");
      //Serial.print(client.state());
     // Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void loop() {

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;

    if (Serial.available()){

    int availableBytes = Serial.available();
    for(int i=0; i<availableBytes; i++)
    {
     string[i] = Serial.read();
    }
    snprintf (msg, 75, string, value);
   
    client.publish("cmnd/MT_1/POWER", msg);
    memset(string, 0, sizeof(string));
    }
    
  }
}
