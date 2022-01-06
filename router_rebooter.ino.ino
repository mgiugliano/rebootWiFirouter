/*
 Router rebooter

 Jan 3rd 2022, Milan - Michele GIUGLIANO (mgiugliano@gmail.com)
*/

#include <WiFi.h>         // Standard WiFi library (from Arduino, ESP32 community)
#include <ESP32Ping.h>    // From https://github.com/marian-craciunescu/ESP32Ping
#include <HTTPClient.h>   // See https://techtutorialsx.com/2017/05/20/esp32-http-post-requests/
#include <ArduinoJson.h>  // From https://arduinojson.org/v6/doc/installation/
#include <NTPClient.h>    // See https://github.com/arduino-libraries/NTPClient and https://randomnerdtutorials.com/esp32-ntp-client-date-time-arduino-ide/

//const int relay = 26;   // Hardware PIN for controlling the relay


const char* ssid     = "??????";
const char* password = "??????";

IPAddress gateway;  // IP address of the WiFi router (gateway)...
IPAddress own;      // Own IP address, assigned by the router...

const char* URL_SLACK = "https://hooks.slack.com/services/????????";

// THESE ARE THE (EXTERNAL) HOSTS TO CHECK INTERNET CONNECTIVITY
const IPAddress remote1(8, 8, 8, 8);  // Host to check connectivity against
const IPAddress remote2(1, 1, 1, 1);  // Host to check connectivity against
const IPAddress remote3(1, 0, 0, 1);  // Host to check connectivity against
const IPAddress remote4(8, 8, 4, 4);  // Host to check connectivity against

int fail_counts;      // Counter of how many times (in a row) internet is uneachable...
int reboot_occurred;  // Boolean variable for (postponed) notification purposes... (as soon as Internet is accessible again).
String time_reboot;   // Time when the reboot occurred...

unsigned long waiting_time; // Time to wait until next check of internet connectivity.

WiFiUDP ntpUDP;               // Define NTP Client to get time
NTPClient timeClient(ntpUDP);

//-----------------------------------------------------------------
void reconnect_WiFi() {
  WiFi.disconnect();    // Disconnect in case I am connected..
  delay(100);           // Wait 100 ms before proceeding

  Serial.println();     // Print diagnostic information 
  Serial.print("(Re)connecting to home WiFi [");
  Serial.print(ssid);
  Serial.print("]");
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  own     = WiFi.localIP();
  gateway = WiFi.gatewayIP();
  
  Serial.println();
  Serial.println("WiFi connected!");
  Serial.print("IP:         ");  
  Serial.println(own);

  Serial.print("Gateway IP: ");   // print your gateway address:
  Serial.println(gateway);

  // WARNING!: This function returns if and only if home WiFi connected!
} // end reconnect_WiFi()
//---------------------------------------------------------------------


//---------------------------------------------------------------------
int internetConnectivity() {

  int test1, test2, test3, test4, test5, test6; // Temp (bool) variables..
  float avg_time_ms;                            // Average pint response time.

  Serial.println();     
  
  if(Ping.ping(gateway)) {                      // Let's first ping our (local) gateway...
    Serial.println("Pinging external servers... (please wait)");
    test1 = Ping.ping(remote1,5);
    avg_time_ms = Ping.averageTime();
    
    test2 = Ping.ping(remote2,5);
    avg_time_ms += Ping.averageTime();
    
    test3 = Ping.ping(remote3,5);
    avg_time_ms += Ping.averageTime();
    
    test4 = Ping.ping(remote4,5);
    avg_time_ms += Ping.averageTime();
    
    test5 = Ping.ping("www.google.com",5);    // Extra test, pinging google.com
    avg_time_ms += Ping.averageTime();
    
    test6 = Ping.ping("www.repubblica.it",5); // Extra test, pinging repubblica.it
    avg_time_ms += Ping.averageTime();

    Serial.print("Overall average response time has been (ms): ");
    Serial.println(avg_time_ms/6.);
    
    return test1 && test2 && test3 && test4 && test5 && test6; // && 0
    // returns 1 only if all tests pass, otherwise 0
  } else {
    Serial.println("ERROR: cannot reach (local) gateway. Is WiFi connected???");
    return 99;
  }
  Serial.println();     
} // end internetConnectivity()
//---------------------------------------------------------------------


//---------------------------------------------------------------------
int reboot_router() {       // We are on our own: isolated from the Internet!
    Serial.println("Rebooting the router....");
//    
//    digitalWrite(relay, LOW);   // Power is cut...
//    delay(5000);               // for 10 seconds and then... 
//    digitalWrite(relay, HIGH);  // it is restored.
//
    reboot_occurred = 1;        // A reboot actually just occurred.
    time_reboot = timeClient.getFormattedTime();
    
    Serial.println("Reboot done!");
} // end reboot_router()
//---------------------------------------------------------------------


//-----------------------------------------------------------------
void notify_slack(String message) {

  HTTPClient http;
  String json;
  DynamicJsonDocument doc(2048);      // Prepare JSON document
  
  doc["username"]   = "webhookbot";
  doc["text"]       = message;
  doc["icon_emoji"] = ":house:";

  serializeJson(doc, json);         // Serialize JSON document
  
  http.begin(URL_SLACK);                                //Specify destination for HTTP request
  http.addHeader("Content-Type", "application/json");   //Specify content-type header
  int httpCode = http.POST(json);                       //Send the request
 
  if (httpCode > 0) {                                   //Check the returning code
       String payload = http.getString();               //Get the request response payload
      Serial.println("Slack webhook returned: " + payload); //Print the response payload
  }

  http.end();                                           //Close connection
} // end notify_slack()
//---------------------------------------------------------------------


//---------------------------------------------------------------------
void setup() {          // This is excuted only once - at "boot"
  
  fail_counts     = 0;  // Init counter of failed "ping" checks...
  reboot_occurred = 0;  // Init of a boolean variable for (delayed) notification purposes..
   
  Serial.begin(115200); // Set the Serial port baud rate...

  //pinMode(relay, OUTPUT); // Init of the pin controlling the relay...
  
  delay(10);            // Add a delay for stability...
  
  reconnect_WiFi();     // Connect to the home WiFi network 
  notify_slack("ESP32 watchdog is online."); 

  waiting_time = 60000;           // 60 s (when everything is ok)

  timeClient.begin();             // Initialize a NTPClient to get the current time
  timeClient.setTimeOffset(3600); // GMT +1 = 3600 (offset in seconds)
  
} // end setup()
//---------------------------------------------------------------------


//---------------------------------------------------------------------
void loop() {           // This is constantly executed, over and over again..

  //while(!timeClient.update()) {
  //  timeClient.forceUpdate();
  //}
  if (!timeClient.update()) {     // I do not want to block the execution, in case of no internet connectivity!
    timeClient.forceUpdate();
  }

    
  switch (internetConnectivity()) {
    case 0:    
      fail_counts++;    // The counter is incremented... (later we will intervene if needed).
      Serial.println("ERROR: no internet connectivity!!");
      Serial.print("It has been the ");
      Serial.print(fail_counts);
      Serial.println("-th time in a row!");
      waiting_time = 10000;           // 10 s (when a problem seems to have occurred)
      break;
      
    case 1:    
      Serial.println("Internet OK.");
      fail_counts = 0;          // The counter is reset...
      if (reboot_occurred) {    // Maybe a reboot just occurred and a notification can now be sent!
        reboot_occurred = 0;
        notify_slack("ESP32 has rebooted the router at " + time_reboot);  
      }
      waiting_time = 60000;           // 60 s (when everything is ok)
      break;
      
    case 99:    
      Serial.println("WiFi not connected - reconnecting...");
      reconnect_WiFi();
      //fail_counts = 0;  // The counter is reset...
      break;
  }


  if (fail_counts > 5) {
    Serial.println("Too many failures logged!");
    //reboot_router();
    delay(30000);           // This should probably be set to 30s and not 3s
    reconnect_WiFi();       // This is blocking! (Router must be broken if no WiFi reconnection possible)
    fail_counts = 0;        // The counter is reset...
  }

  Serial.println("Waiting a bit for the next check");
  delay(waiting_time);        // delay in between tests, for stability...
} // end loop()
