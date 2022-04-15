#include <Arduino.h>        // for PlatformIO
#include <SimpleTimer.h>    //https://github.com/marcelloromani/Arduino-SimpleTimer/tree/master/SimpleTimer
#include <ESP8266WiFi.h>    //if you get an error here you need to install the ESP8266 board manager 
#include <ESP8266mDNS.h>    //if you get an error here you need to install the ESP8266 board manager 
#include <PubSubClient.h>   //https://github.com/knolleary/pubsubclient
#include <ArduinoOTA.h>     //https://github.com/esp8266/Arduino/tree/master/libraries/ArduinoOTA
#include <AH_EasyDriver.h>  //http://www.alhin.de/arduino/downloads/AH_EasyDriver_20120512.zip

#include <mdPushButton.h>   //https://github.com/sigmdel/mdPushButton

/*****************  START USER CONFIG SECTION *********************************/
/*****************  START USER CONFIG SECTION *********************************/
/*****************  START USER CONFIG SECTION *********************************/
/*****************  START USER CONFIG SECTION *********************************/

#define USER_SSID                 "willowiot_main"
#define USER_PASSWORD             "krn012gp"
#define USER_MQTT_SERVER          "192.168.1.250"
#define USER_MQTT_PORT            1883
#define USER_MQTT_USERNAME        "willow"
#define USER_MQTT_PASSWORD        "krn012gp"
#define USER_MQTT_CLIENT_NAME     "board1"         // Used to define MQTT topics, MQTT Client ID, and ArduinoOTA


#define BUTTON_UP_PIN       D3
#define BUTTON_DOWN_PIN     D4

#define RELAY_UP_PIN          D6
#define RELAY_DOWN_PIN        D7
#define RELAY_HOLD_DURATION   2500
#define RELAY_BURST_DURATION  100

/*****************  END USER CONFIG SECTION *********************************/
/*****************  END USER CONFIG SECTION *********************************/
/*****************  END USER CONFIG SECTION *********************************/
/*****************  END USER CONFIG SECTION *********************************/
/*****************  END USER CONFIG SECTION *********************************/

WiFiClient espClient;
PubSubClient mqttClient(espClient);
SimpleTimer mytimer;
mdPushButton upButton = mdPushButton(BUTTON_UP_PIN, HIGH, true);
mdPushButton downButton = mdPushButton(BUTTON_DOWN_PIN, HIGH, true);

//Global Variables
bool boot = true;
bool moving = false;
char charPayload[50];

int wifiRetryCounter = 0;

const char* ssid = USER_SSID ; 
const char* password = USER_PASSWORD ;
const char* mqtt_server = USER_MQTT_SERVER ;
const int mqtt_port = USER_MQTT_PORT ;
const char *mqtt_user = USER_MQTT_USERNAME ;
const char *mqtt_pass = USER_MQTT_PASSWORD ;
const char *mqtt_client_name = USER_MQTT_CLIENT_NAME ; 

enum relayState_t { RELAY_OFF, RELAY_UP, RELAY_DOWN} relayState = RELAY_OFF;

unsigned long relayDownUntil = 0;


//Functions
bool setup_wifi() {
  unsigned long abortWifiConnect = millis() + 120000;

  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while ((WiFi.status() != WL_CONNECTED) && (abortWifiConnect < millis()) ){
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
    wifiRetryCounter = 0;
  }
  else{
    Serial.println("");
    Serial.println("WiFi connecion FAILED");
    Serial.println("IP address: ???.???.???.???");
  }

  return  (WiFi.status() == WL_CONNECTED);
}

void reconnect() 
{

  if(wifiRetryCounter < 100)
  {
    Serial.print("Attempting MQTT connection...");
    if (mqttClient.connect(mqtt_client_name, mqtt_user, mqtt_pass)) 
    {
      wifiRetryCounter = 0;
      Serial.println("connected");
      if(boot == false)
      {
        mqttClient.publish(USER_MQTT_CLIENT_NAME"/checkIn","Reconnected"); 
      }
      if(boot == true)
      {
        mqttClient.publish(USER_MQTT_CLIENT_NAME"/checkIn","Rebooted");
      }
      // ... and resubscribe
      mqttClient.subscribe(USER_MQTT_CLIENT_NAME"/reclinerCommand");
    } 
    else 
    {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      wifiRetryCounter++;
    }
  }
  if(wifiRetryCounter > 98)
  {
    ESP.restart();
  }
  
}

void mqttCallback(char* topic, byte* payload, unsigned int length) 
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

  if (newTopic == USER_MQTT_CLIENT_NAME"/reclinerCommand") 
  {
    if (newPayload == "OPEN")
    {
      relayState = RELAY_UP;
      relayDownUntil = millis() + RELAY_HOLD_DURATION;
    }
    else if (newPayload == "CLOSE")
    {   
      relayState = RELAY_DOWN;
      relayDownUntil = millis() + RELAY_HOLD_DURATION;
    }
    else if (newPayload == "STOP")
    {
      relayState = RELAY_OFF;
      relayDownUntil = 0;
    }
  }
}

void checkIn()
{
  mqttClient.publish(USER_MQTT_CLIENT_NAME"/checkIn","OK"); 
}


/*******************************************************/
/***  SETUP PROCESS                                  ***/
/*******************************************************/
void setup() {
  digitalWrite(RELAY_UP_PIN,   LOW);
  digitalWrite(RELAY_DOWN_PIN, LOW);
  
  //Start the serial Comms
  Serial.begin(115200);

  //Start the Wifi
  WiFi.mode(WIFI_STA);

  if (setup_wifi()){
    //Connect the MQTT
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    ArduinoOTA.setHostname(USER_MQTT_CLIENT_NAME);
    ArduinoOTA.begin(); 
    delay(10);
    
    mytimer.setInterval(90000, checkIn); 
  }
}


void processRelays(){

  if(relayDownUntil < millis()){
    relayState = RELAY_OFF;
    Serial.println("Relay off due to time");
    //mqttClient.publish(USER_MQTT_CLIENT_NAME"/movement","stop");
  }

  switch (relayState)
  {
  case RELAY_UP:
      digitalWrite(RELAY_UP_PIN,   HIGH);
      digitalWrite(RELAY_DOWN_PIN, LOW);
    break;
  case RELAY_DOWN:
      digitalWrite(RELAY_UP_PIN,   LOW);
      digitalWrite(RELAY_DOWN_PIN, HIGH);
    break;
  
  default:
      digitalWrite(RELAY_UP_PIN,   LOW);
      digitalWrite(RELAY_DOWN_PIN, LOW); 
  }
}


void processButtons(){
  
  int clicks = 0;

  //Do the processing for the butons
  switch (clicks = upButton.status()) {
    case  0: 
      /* ignore this case */; 
      break;

    case  2: 
      Serial.println("Button UP double-click"); 
      relayDownUntil = millis() + RELAY_HOLD_DURATION;
      relayState = RELAY_UP;
      break;
            
    default  : 
      Serial.print("Button UP pressed "); Serial.print(clicks); Serial.println(" times"); break;
      relayDownUntil = millis() + RELAY_BURST_DURATION;
      relayState = RELAY_UP;
  }

  switch (clicks = downButton.status()) {
    case  0: 
      /* ignore this case */; 
      break;

    case  2: 
      Serial.println("Button DOWN double-click"); 
      relayDownUntil = millis() + RELAY_HOLD_DURATION;
      relayState = RELAY_DOWN;
      break;
            
    default  : 
      Serial.print("Button DOWN pressed "); Serial.print(clicks); Serial.println(" times"); break;
      relayDownUntil = millis() + RELAY_BURST_DURATION;
      relayState = RELAY_DOWN;
  }
  
  delay(40 + random(21)); // rest of loop takes 40 to 60 ms to execute 
}
/*******************************************************/
/***  MAIN LOOP                                      ***/
/*******************************************************/
void loop() 
{

  if (!mqttClient.connected() && (relayState == RELAY_OFF)) 
  {
    reconnect();
  }

  //if the counter is Zero, we have connection and can do the MQTT and other suff, 
  //but untill then, we need to focus on the hard work of reading buttons and 
  //switching relays
  if (wifiRetryCounter == 0){
    mqttClient.loop();    
    mytimer.run();
    ArduinoOTA.handle();
  }  

  //do the processing of the Buttons
  processButtons();

  //Do the processing for the Relays
  processRelays();
}
