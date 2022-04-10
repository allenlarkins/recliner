#include <SimpleTimer.h>    //https://github.com/marcelloromani/Arduino-SimpleTimer/tree/master/SimpleTimer
#include <ESP8266WiFi.h>    //if you get an error here you need to install the ESP8266 board manager 
#include <ESP8266mDNS.h>    //if you get an error here you need to install the ESP8266 board manager 
#include <PubSubClient.h>   //https://github.com/knolleary/pubsubclient
#include <ArduinoOTA.h>     //https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA
#include <AH_EasyDriver.h>  //http://www.alhin.de/arduino/downloads/AH_EasyDriver_20120512.zip

/*****************  START USER CONFIG SECTION *********************************/
/*****************  START USER CONFIG SECTION *********************************/
/*****************  START USER CONFIG SECTION *********************************/
/*****************  START USER CONFIG SECTION *********************************/

#define USER_SSID                 "ssid"
#define USER_PASSWORD             "wifipw"
#define USER_MQTT_SERVER          "192.168.1.1"
#define USER_MQTT_PORT            1883
#define USER_MQTT_USERNAME        "mqttname"
#define USER_MQTT_PASSWORD        "mqttpassw"
#define USER_MQTT_CLIENT_NAME     "board1"         // Used to define MQTT topics, MQTT Client ID, and ArduinoOTA

#define BUTTON_UP           D6
#define RELAY_UP            D7
#define BUTTON_DOWN         D8
#define RELAY_DOWN          D9
 
/*****************  END USER CONFIG SECTION *********************************/
/*****************  END USER CONFIG SECTION *********************************/
/*****************  END USER CONFIG SECTION *********************************/
/*****************  END USER CONFIG SECTION *********************************/
/*****************  END USER CONFIG SECTION *********************************/

WiFiClient espClient;
PubSubClient client(espClient);
SimpleTimer mytimer;

//Global Variables
bool boot = true;

char positionPublish[50];
bool moving = false;
char charPayload[50];

const char* ssid = USER_SSID ; 
const char* password = USER_PASSWORD ;
const char* mqtt_server = USER_MQTT_SERVER ;
const int mqtt_port = USER_MQTT_PORT ;
const char *mqtt_user = USER_MQTT_USERNAME ;
const char *mqtt_pass = USER_MQTT_PASSWORD ;
const char *mqtt_client_name = USER_MQTT_CLIENT_NAME ; 

int downButtonState;
int lastDownButtonState = LOW;

int upButtonState;
int lastUpButtonState   = LOW;

int currentDownRelayState = LOW;
int currentUPRelayState = LOW;

unsigned long lastDebounceTime = 0;    // the last time the output pin was toggled
unsigned long debounceDelay = 50; 

unsigned long lastDebouncedPress = 0;       // the last time a debounced press was active
unsigned long doubleClickDelay = 800;       // the time before a second click is concidered to be a double click
unsigned long doubleClickDuration = 2500;   // the time the double click effect will last on the relay  

//Functions
void setup_wifi() {
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
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

void reconnect() 
{
  int retries = 0;
  while (!client.connected()) {
    if(retries < 150)
    {
      Serial.print("Attempting MQTT connection...");
      if (client.connect(mqtt_client_name, mqtt_user, mqtt_pass)) 
      {
        Serial.println("connected");
        if(boot == false)
        {
          client.publish(USER_MQTT_CLIENT_NAME"/checkIn","Reconnected"); 
        }
        if(boot == true)
        {
          client.publish(USER_MQTT_CLIENT_NAME"/checkIn","Rebooted");
        }
        // ... and resubscribe
        client.subscribe(USER_MQTT_CLIENT_NAME"/blindsCommand");
        client.subscribe(USER_MQTT_CLIENT_NAME"/positionCommand");
      } 
      else 
      {
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        retries++;
        // Wait 5 seconds before retrying
        delay(5000);
      }
    }
    if(retries > 149)
    {
    ESP.restart();
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("Message arrived [");
  String newTopic = topic;
  Serial.print(topic);
  Serial.print("] ");
  payload[length] = '\0';
  String newPayload = String((char *)payload);
  int intPayload = newPayload.toInt();
  Serial.println(newPayload);
  Serial.println();
  newPayload.toCharArray(charPayload, newPayload.length() + 1);
  if (newTopic == USER_MQTT_CLIENT_NAME"/blindsCommand") 
  {
    if (newPayload == "OPEN")
    {
      client.publish(USER_MQTT_CLIENT_NAME"/positionCommand", "0", true);
    }
    else if (newPayload == "CLOSE")
    {   
      int stepsToClose = STEPS_TO_CLOSE;
      String temp_str = String(stepsToClose);
      temp_str.toCharArray(charPayload, temp_str.length() + 1);
      client.publish(USER_MQTT_CLIENT_NAME"/positionCommand", charPayload, true);
    }
    else if (newPayload == "STOP")
    {
      String temp_str = String(currentPosition);
      temp_str.toCharArray(positionPublish, temp_str.length() + 1);
      client.publish(USER_MQTT_CLIENT_NAME"/positionCommand", positionPublish, true); 
    }
  }
  if (newTopic == USER_MQTT_CLIENT_NAME"/positionCommand")
  {
    if(boot == true)
    {
      newPosition = intPayload;
      currentPosition = intPayload;
      boot = false;
    }
    if(boot == false)
    {
      newPosition = intPayload;
    }
  }
  
}


void checkIn()
{
  client.publish(USER_MQTT_CLIENT_NAME"/checkIn","OK"); 
}


//Run once setup
void setup() {
  pinMode(BUTTON_UP,   INPUT);
  pinMode(BUTTON_DOWN, INPUT);
  pinMode(RELAY_UP,    OUTPUT);
  pinMode(RELAY_DOWN,  OUTPUT);

  digitalWrite(RELAY_UP,   LOW);
  digitalWrite(RELAY_DOWN, LOW);
  
  Serial.begin(115200);


  WiFi.mode(WIFI_STA);
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  ArduinoOTA.setHostname(USER_MQTT_CLIENT_NAME);
  ArduinoOTA.begin(); 
  delay(10);
  
  mytimer.setInterval(90000, checkIn); 
}


void processButtons(){
  // read the state of the switch into a local variable:
  int reading = digitalRead(BUTTON_UP);

  // If the switch changed, due to noise or pressing:
  if (reading != lastButtonState) {
    // reset the debouncing timer
    lastDebounceTime = millis();
    Serial.print("-");
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    Serial.print("CLICK");
    // whatever the reading is at, it's been there for longer than the debounce
    // delay, so take it as the actual current state:

    // if the button state has changed:
    if (reading != buttonState) {
      buttonState = reading;
      currentDownRelayState = LOW;
      currentUPRelayState = LOW;


      //Now, if this button is being pressed again within the double-click debounce time, the activate the long click relay
      if ((millis() - lastDebouncedPress) > doubleClickDelay){
         // if the button has been pressed again within the double-debounce time
        currentDownRelayState = HIGH;
        currentUPRelayState = LOW;
        s 
      }
 

      
    }
  }

  reading = lastButtonState;

}

void processRelays(){
  // TODO
  digitalWrite(RELAY_UP,   LOW);
  digitalWrite(RELAY_DOWN, LOW);

  
int currentDownRelayState = LOW;
int currentUPRelayState = LOW;

  
}


void loop() 
{
  if (!client.connected()) 
  {
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();
  mytimer.run();
}
