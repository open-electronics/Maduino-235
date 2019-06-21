

#define TINY_GSM_MODEM_SIM808

//#define GO_LOWPOWER


#include <TinyGsmClient.h>
#include <PubSubClient.h>

#ifdef GO_LOWPOWER 
  #include <ArduinoLowPower.h>
#endif 

#include <ArduinoJson.h>


// PIN INTERRUPT
#define PIN_INT 3 

// PIN LED
#define PIN_LED 7 

// PIN  POWER GPRS
#define PIN_GPRS 9 


#define SerialMon SerialUSB
#define SerialAT Serial1

TinyGsm modem(SerialAT);


int send_data_every = 120000;



// Your GPRS credentials
// Leave empty, if missing user or pass
const char apn[]  = "TM";
const char user[] = "";
const char pass[] = "";

TinyGsmClient client(modem);


/*###########################################################################################*/
/*                               MQTT                                                        */

#define TOKEN "JGOUm2D94RIShq7vpwnx"

char thingsboardServer[] = "demo.thingsboard.io";

// N.B. questi valori sovrascrivono quelli di default del file PubSubClient.h
#define MQTT_VERSION = 3 //  valori possibili 3 o 4
#define MQTT_KEEPALIVE 9
#define MQTT_SOCKET_TIMEOUT 30
/*###########################################################################################*/


PubSubClient mqtt_client(client);

StaticJsonDocument<200> doc;
StaticJsonDocument<200> doc2;
char buffer[200];

long lastReconnectAttempt = 0;
unsigned long lastSend;

bool mqtt_set = false;
bool subscribed_to_RCP = false;
bool subscribed_to_Shared_Attribute = false;
bool client_Attribute_sent = false;
bool subscribed_to_Attribute = false;


volatile bool data_sent = false;
volatile bool force_send_data = false;
volatile bool gprs_connected = false;
volatile bool shared_attribute_received = false;




void setup() {
  
  SerialMon.begin(9600);

  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_GPRS, OUTPUT);
  pinMode(PIN_INT, INPUT_PULLUP);


  digitalWrite(PIN_LED, HIGH); 
  delay(6000); 
  digitalWrite(PIN_LED, LOW);


  SerialMon.println("##################################");
  #ifdef GO_LOWPOWER 
    LowPower.attachInterruptWakeup(PIN_INT, interrupt_routine, LOW);
    LowPower.attachInterruptWakeup(RTC_ALARM_WAKEUP, interrupt_routine , CHANGE);  
    SerialMon.println("MODE: LOWPOWER");  
  #else
    attachInterrupt(PIN_INT,interrupt_routine2,LOW);
    SerialMon.println("MODE: NORMAL");
  #endif
  SerialMon.print("Send DataEvery: ");
  SerialMon.print(send_data_every / 1000 );
  SerialMon.println(" Seconds ");  
  SerialMon.println("##################################");
  SerialMon.println();

     
}




void loop() {

  // Non rimuovere questo ritardo!!
  delay(200);

 lastSend = 0; 


  if (! gprs_connected){
      gprs_connected = initGPRS();
      client_Attribute_sent =false;
      subscribed_to_RCP =false;
      subscribed_to_Shared_Attribute =false;
      subscribed_to_Attribute=false;
  } else {

      
      
      mqtt_client.loop();

      if (!mqtt_set){
       mqtt_client.setServer( thingsboardServer, 1883 );
       mqtt_client.setCallback(on_message);
       mqtt_set =true;
      }
      
      if ( !mqtt_client.connected() ) {
        reconnect();
      } else {

        if (! client_Attribute_sent){
           // invio un attributo Client        
           doc["firmware_version"] = "1.0.3";   
           serializeJson(doc, buffer);    
           SerialMon.print("Sending Client Attribute: ");
           client_Attribute_sent =mqtt_client.publish("v1/devices/me/attributes", buffer);
           SerialMon.print("result -> ");      
           SerialMon.println(client_Attribute_sent);
        }


        if (! subscribed_to_RCP){
          SerialMon.print("Subscribe to RPC: ");
          // Sottoscrivo richieste RPC
          subscribed_to_RCP = mqtt_client.subscribe("v1/devices/me/rpc/request/+"); 
          SerialMon.print("result -> ");
          SerialMon.println(subscribed_to_RCP);
        }
        
        
        if (! subscribed_to_Shared_Attribute){
          // Sottoscrivo alla modifica degli attributi
          SerialMon.print("Subscribe Attribute Modification:");
          subscribed_to_Shared_Attribute = mqtt_client.subscribe("v1/devices/me/attributes");
          SerialMon.print("result -> ");
          SerialMon.println(subscribed_to_Shared_Attribute);
        }


        if (! subscribed_to_Attribute){
          //Sottoscrivo alla richiesta di attributi
          SerialMon.print("Subscribe Attribute Topic:");
          
          mqtt_client.subscribe("v1/devices/me/attributes/response/+"); 
   
          doc2["sharedKeys"] = "update_frequency,mobile_number";
          serializeJson(doc2, buffer);
        
          subscribed_to_Attribute = mqtt_client.publish( "v1/devices/me/attributes/request/1" , buffer);
                         
          SerialMon.print("result -> ");
          SerialMon.println(subscribed_to_Attribute);
          
        }




        #ifdef GO_LOWPOWER
          if (! data_sent){
             data_sent = send_Data();
          } 
        #else
          if ( (millis() - lastSend > send_data_every) ||  force_send_data ) {  
            force_send_data=false; 
            data_sent = send_Data();
            lastSend = millis();
          }
        #endif



        #ifdef GO_LOWPOWER
        if ( data_sent && shared_attribute_received  ) { 
          data_sent = false;
          shared_attribute_received = false; 
          subscribed_to_Attribute = false;
          subscribed_to_Shared_Attribute = false; 
          subscribed_to_RCP = false;  
          client_Attribute_sent = false;  
          Toggle_Power_SIM808();          
          USBDevice.detach();          
          interrupts();     
          LowPower.sleep(send_data_every);  
        }
       #endif




      } 

          
  }



 
}




void Toggle_Power_SIM808(){
    SerialMon.println("Toggle_Power_SIM808()");     
    digitalWrite(PIN_GPRS, HIGH);
    delay(3000); 
    digitalWrite(PIN_GPRS, LOW);    
    delay(3000);
}







void blink(int ms) {
  digitalWrite(PIN_LED, HIGH);
  delay(ms);
  digitalWrite(PIN_LED, LOW);
  delay(ms);
}




void interrupt_routine() {
  data_sent=false;
  //gprs_connected =false;
  shared_attribute_received = false;
}



void interrupt_routine2() {
  force_send_data=true;
  //shared_attribute_received = false;
}




bool initGPRS(){
  bool result = false;
  
  SerialAT.begin(9600);
  
  Toggle_Power_SIM808();

  //modem.restart();
  modem.init();
  
  String modemInfo = modem.getModemInfo();

  SerialMon.print("Modem Iniziatization: ");
  if (!modem.waitForNetwork()) {
    SerialMon.println(" -> Failed");
    while (true);
  }else{
    SerialMon.println(" -> OK");
  }

  SerialMon.print("GPRS Connection: ");
  if (!modem.gprsConnect(apn, user, pass)) {
    SerialMon.println("-> Failed");
    while (true);
  }

  if (modem.isNetworkConnected()) {
    SerialMon.println("-> Connected");
    SerialMon.println();
    result= true;
  }


  return result;
 
}





void on_message(char* topic, byte* payload, unsigned int len) {

  String str_topic;
  long mobile_number;
  long update_frequency;
  const char* method; 
  String str_method;
  bool enabled;
  int pin;

  str_topic = String(topic);

  SerialMon.println();
  SerialMon.println("+++++++++++++++++ RECEIVED TOPIC START +++++++++++++++++");

  char json[len + 1];
  strncpy (json, (char*)payload, len);
  json[len] = '\0';

  SerialMon.print("Topic: ");
  SerialMon.println(topic);
  
  SerialMon.print("Message: ");
  SerialMon.println(json);

  StaticJsonDocument<200> doc;

  DeserializationError error = deserializeJson(doc, payload);

  // Test if parsing succeeds.
  if (error) {
    SerialMon.println("deserialization Failed");
    SerialMon.println(error.c_str());
    return;
  }




if(str_topic.startsWith("v1/devices/me/attributes/response/")){
  //Topic: v1/devices/me/attributes/response/1
  //Message: {"shared":{"mobile_number":123141218,"update_frequency":40}}
  
  mobile_number = doc["shared"]["mobile_number"];
  update_frequency = doc["shared"]["update_frequency"];

  if (mobile_number !=0 ){
    SerialMon.print("mobile_number: ");
    SerialMon.println(mobile_number);
  }
  
  if (update_frequency !=0 ){
   SerialMon.print("update_frequency: ");
   SerialMon.println(update_frequency);
  }

  shared_attribute_received = true;

} 
  

if(str_topic.startsWith("v1/devices/me/rpc/request/")){
  
  //Topic: v1/devices/me/rpc/request/854
  //Message: {"method":"setGpioStatus","params":{"pin":7,"enabled":true"}}

  //Topic: v1/devices/me/rpc/request/858
  //Message: {"method":"getGpioStatus","params":{"pin":7}}
 
  method = doc["method"];
  
  SerialMon.print("method: ");
  SerialMon.println(method);

  str_method = String(method);

  String responseTopic = str_topic;
  responseTopic.replace("request", "response"); 

 if (str_method.equals("getGpioStatus")) { 
     pin = doc["params"]["pin"];
     SerialMon.println(get_gpio_status(pin).c_str());
     mqtt_client.publish(responseTopic.c_str(), get_gpio_status(pin).c_str()); 
 }

 if(str_method.equals("setGpioStatus")) {
     pin    = doc["params"]["pin"];
     enabled = doc["params"]["enabled"];
     SerialMon.print("pin: ");
     SerialMon.println(pin);
     SerialMon.print("enabled: ");
     SerialMon.println(enabled);
     // Update GPIO status and reply
     set_gpio_status(pin, enabled);  
     mqtt_client.publish(responseTopic.c_str(), get_gpio_status(pin).c_str());     
  }

}


  
if(str_topic.equals("v1/devices/me/attributes")){
    
    //Topic: v1/devices/me/attributes
    //Message: {"mobile_number":123141216}

    //Topic: v1/devices/me/attributes
    //Message: {"update_frequency":40}
    
    mobile_number = doc["mobile_number"];
    update_frequency = doc["update_frequency"];

   if (mobile_number !=0 ){
      SerialMon.print("mobile_number: ");
      SerialMon.println(mobile_number);
   }

   if (update_frequency !=0 ){
      SerialMon.print("update_frequency: ");
      SerialMon.println(update_frequency);
   }
      
}
  
  SerialMon.println("+++++++++++++++++ RECEIVED TOPIC END +++++++++++++++++");
  SerialMon.println();
}
  
  




String get_gpio_status(int pin) {
  StaticJsonDocument<200> doc;
  
  char buffer[100]; 
  
  doc[String(pin)] = digitalRead(pin) ? true : false;   
  
  serializeJson(doc, buffer);

  return buffer;
}


void set_gpio_status(int pin, boolean enabled) {
    digitalWrite(pin, enabled ? HIGH : LOW);    
}



void reconnect(){

  while (!mqtt_client.connected()) {

    //modem.init();
    
    SerialMon.print("Connecting to ThingsBoard node ...");
    
    // Create a random client ID
    String clientId;
    clientId += String(random(0xffff), HEX);

    if ( mqtt_client.connect(clientId.c_str(), TOKEN, NULL) ) {
      
      SerialMon.println( " -> OK");
   
    } else {
      
      SerialMon.println( "[FAILED] [ State= " );
      
      translateMQTT_State(mqtt_client.state());
      SerialMon.println( "]" );
      
      //SerialMon.println( " : retrying in 2 seconds]" );
      
      // Wait 2 seconds before retrying
      delay( 2000 );
      
    }

 
  }
 
}







double randomDouble(double minf, double maxf){
  // use 1ULL<<63 for max double values)
  return minf + random(1UL << 31) * (maxf - minf) / (1UL << 31);  
}







bool send_Data(){
  bool result = false;
  char *array[10];
  int i=0;
  char string[50];
  char data[300];

  String temperature;
  String humidity;
  String weight;
  String battery;
  String latitude;
  String longitude;
  String segnal_level;

  randomSeed(analogRead(0));

  SerialMon.println();
  SerialMon.println("################# START Collecting Data ###########################");
  
  // Temperature
  float t = randomDouble(20, 26);

  // Humidity
  float h = randomDouble(50, 80);

  // Weight
  float w = randomDouble(50, 65);

  // Battery
  //float b = randomDouble(4, 5);

  // Latitude
  float lt = randomDouble(50, 65);

  // Longitude
  float lg = randomDouble(50, 65);

  float b = modem.getBattVoltage() / 1000.0F;
  //SerialMon.print("battLevel:");
  //SerialMon.println(b);

  int csq = modem.getSignalQuality();
  //SerialMon.print("Signal quality: ");
  //SerialMon.println (csq);

  String gsmLoc = modem.getGsmLocation();
  //SerialMon.print("gsmLoc: ");
  //SerialMon.println (gsmLoc);


  modem.enableGPS();
  String gps_raw = modem.getGPSraw();
  //SerialMon.print("GPSraw: ");
  modem.disableGPS();
  //SerialMon.println (gps_raw);


  gsmLoc.toCharArray(string, 50);
  array[i] = strtok(string,",");
  while(array[i]!=NULL){
    array[++i] = strtok(NULL,",");  
  }
  longitude = array[1];
  latitude = array[2];

 
  SerialMon.println("################# STOP Collecting Data ###########################");
  SerialMon.println();


  temperature = String(t);
  humidity = String(h);
  weight = String(w);
  battery = String(b);
  segnal_level = String(csq);
  

  SerialMon.println("################# START Send Data ##############################");
 
  // Payload per device singolo , Non richiede timestamp
  String payload = "{";
  payload += "\"T\":"; payload += "\""; payload +=  temperature ; payload += "\""; payload += ",";
  payload += "\"H\":"; payload += "\"";  payload += humidity; payload += "\""; payload += ",";
  payload += "\"W\":"; payload += "\"";  payload += weight; payload += "\""; payload += ",";
  payload += "\"B\":"; payload += "\"";  payload += battery; payload += "\""; payload += ",";
  payload += "\"LT\":"; payload += "\"";  payload += latitude; payload += "\""; payload += ",";
  payload += "\"LG\":"; payload += "\"";  payload += longitude; payload += "\""; payload += ",";
  payload += "\"S\":"; payload += "\"";  payload += segnal_level ; payload += "\"";
  payload += "}";

  
  payload.toCharArray(data, 300 );

  
  if (mqtt_client.publish("v1/devices/me/telemetry", data )){
    result = true;
  } else{
    result = false;
  }

  
  SerialMon.print( data );
  SerialMon.print ("  -> Result: ");
  SerialMon.println (result);
  
  SerialMon.println("################# END Send Data ##############################");
  SerialMon.println();

  return result;

}





void translateMQTT_State (int state){

switch (state) {
  case -4:
    SerialMon.println("MQTT_CONNECTION_TIMEOUT");
    break;
  case -3:
    SerialMon.println("MQTT_CONNECTION_LOST");
    break;

  case -2:
   SerialMon.println("MQTT_CONNECT_FAILED");
    break;

   case -1:
    SerialMon.println("MQTT_DISCONNECTED");
    break;

   case -0:
    SerialMon.println("MQTT_CONNECTED");
    break;

   case 1:
    SerialMon.println("MQTT_CONNECTION_TIMEOUT");
    break;
    
  case 2:
    SerialMon.println("MQTT_CONNECT_BAD_CLIENT_ID");
    break;

  case 3:
    SerialMon.println("MQTT_CONNECT_UNAVAILABLE");
    break;

  case 4:
    SerialMon.println("MQTT_CONNECT_BAD_CREDENTIALS");
    break;

  case 5:
    SerialMon.println("MQTT_CONNECT_UNAUTHORIZED ");
    break; 
     
  default:
    // statements
    break;
}
 
}
