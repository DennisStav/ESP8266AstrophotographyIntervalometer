#include <stdio.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#define HTTP_REST_PORT 80
#define WIFI_RETRY_DELAY 500
#define MAX_WIFI_INIT_RETRY 20
#define RELAY 5 // relay shield connected to GPIO5 for D1 mini
//#define RELAY 0 // relay connected to GPIO0 for ESP01

#define BUILTINLED 2

#ifndef APSSID
#define APSSID "ESP8266Astro"
#define APPSK  "1qazxsw2"
#endif

/* Set these to your desired credentials. */
const char *apssid = APSSID;
const char *appassword = APPSK;

const char* client_ssid = "wewewe";
const char* client_passwd = "1qazxsw2";
String page("<html><head><link rel=\"stylesheet\" href=\"https://cdnjs.cloudflare.com/ajax/libs/mini.css/3.0.1/mini-default.min.css\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"><title>ESP8266AstroIntervalometer</title></head><body><div class=\"container\"><div class=\"row\"><form action=\"/astropic\" method=\"post\" target=\"curstatus\"><div class=\"row\">ExpTime:<input type=\"text\" name=\"exptime\"></div><div class=\"row\">WaitTime:<input type=\"text\" name=\"waittime\"></div><div class=\"row\">NumPics:<input type=\"text\" name=\"reps\"></div><div class=\"row\"><select name=\"action\"><option value=\"Start\">Start</option><option value=\"Stop\">Stop</option></select></div><div class=\"row\"><input type=\"submit\" value=\"Submit\"></div></form></div><div class=\"row\"><form action=\"/astropic\" method=\"get\" target=\"curstatus\"><input type=\"submit\" value=\"GetStatus\"></form></div><div class=\"row\"><iframe name=\"curstatus\" id=\"curstatus\"></iframe></div></body></html>");

unsigned long timenow=0, picstart=0, cooloffstart=0; 

enum astrostatus
{
  NOTREADY = 0,
  RUNNING = 1,
  STOPPED = 2
};

enum shutterstatus
{
  SCLOSED = 0,
  SOPEN = 1,
  COOLING=2
};

struct astroconf
{
  unsigned short exptime; 
  unsigned short cooloff;
  unsigned short numPics;
  unsigned short picscomp;
  astrostatus curstatus;
  shutterstatus curshutterstatus;
} astrojob;

ESP8266WebServer intwebserver(HTTP_REST_PORT);

String astroStatusToString(astrostatus dummyStat)
{
  String retVal;
  switch (dummyStat)
  {
    case NOTREADY:
      retVal="NOTREADY";
      break;
    case RUNNING:
      retVal="RUNNING";
      break;
    case STOPPED:
      retVal="STOPPED";
      break;
    default:
      retVal="Uknown Proc Status";
  }
  return retVal;
}

String shutterStatusToString(shutterstatus dummyStat)
{
  String retVal;
  switch (dummyStat)
  {
    case SOPEN:
      retVal="OPEN";
      break;
    case SCLOSED:
      retVal="CLOSED";
      break;
    case COOLING:
      retVal="COOLING";
      break;
    default:
      retVal="Uknown Shutter Status";
  }
  return retVal;
}

void init_resource()
{
    Serial.println("Initializing resources");
//    Serial.print("MyBoard is:");
//    Serial.println(MYBOARD);
    astrojob.exptime=0;
    astrojob.cooloff=0;
    astrojob.numPics=0;
    astrojob.picscomp=0;
    astrojob.curstatus=STOPPED;
    pinMode(RELAY,OUTPUT);
    pinMode(BUILTINLED,OUTPUT);
    digitalWrite(BUILTINLED,HIGH);
    digitalWrite(RELAY,LOW);
    closeShutter();
}

int init_wifi() {
    int retries = 0;

    Serial.println("Connecting to WiFi AP..........");
    WiFi.mode(WIFI_STA);
    WiFi.begin(client_ssid, client_passwd);
    // check the status of WiFi connection to be WL_CONNECTED
    while ((WiFi.status() != WL_CONNECTED) && (retries < MAX_WIFI_INIT_RETRY)) {
        retries++;
        delay(WIFI_RETRY_DELAY);
        Serial.print("#");
    }
    return WiFi.status(); // return the WiFi connection status
}

int init_wifiAP(){
  Serial.print("Configuring access point:"+String(apssid));
  /* You can remove the password parameter if you want the AP to be open. */
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apssid, appassword);  
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP statu:"+String(WiFi.status())+"\n");
  Serial.print("AP IP address:");
  Serial.println(myIP);
  return WiFi.status();
}

void get_astropic()
{  
  Serial.println("Get Intervalometer status called");
  String message = "GET Config\n";
  message += "Status:"+astroStatusToString(astrojob.curstatus)+"\n";
  message += "Shutter:"+shutterStatusToString(astrojob.curshutterstatus)+"\n";
  message += "Pics:"+String(astrojob.picscomp)+"/"+String(astrojob.numPics)+"\n";
  if(astrojob.curstatus==RUNNING){
    message += "Pic:"+String(astrojob.picscomp+1)+" Sec:"+String((timenow-picstart)/1000)+"/"+String(astrojob.exptime)+"\n";
  }
  intwebserver.send(200,"text/plain", message);
}

void post_astropic(){
  Serial.println("Post Intervalometer Config called");
  if (intwebserver.args() == 0) {
    intwebserver.send(200,"text/plain", "ERROR: No Arguments");
  } else {
    String message = "POST CONFIGUIRING Number of args received:";
    message += intwebserver.args()+"\n";
    for (uint8_t i = 0; i < intwebserver.args(); i++) {
      message += " " + intwebserver.argName(i) + ": " + intwebserver.arg(i) + "\n";
    }
    Serial.println(intwebserver.arg(4));

    if(intwebserver.arg(3).equals("Start"))
    {
      astrojob.curstatus=RUNNING;
      astrojob.picscomp=0;
      astrojob.exptime=intwebserver.arg(0).toInt();
      astrojob.cooloff=intwebserver.arg(1).toInt();
      astrojob.numPics=intwebserver.arg(2).toInt();
    }
    else
    {
      astrojob.curstatus=STOPPED;
    }
    intwebserver.send(200,"text/plain", message);
  }
  return;
}

void get_configpage(){
  Serial.println("Serving main page");
  intwebserver.send(200, "text/html", page);
  return;
}

void config_rest_server_routing() {
    Serial.println("Registering web server paths");

    intwebserver.on("/", HTTP_GET, get_configpage);

    intwebserver.on("/astropic", HTTP_GET, get_astropic);
    intwebserver.on("/astropic", HTTP_POST, post_astropic);    
}

void setup(void) {
    Serial.begin(115200);
    Serial.println("Initial Setup");
    init_resource();
    if (init_wifi() == WL_CONNECTED) {
        Serial.print("Connetted to ");
        Serial.print(client_ssid);
        Serial.print("--- IP: ");
        Serial.println(WiFi.localIP());
    } else {
        //Couldn't connect as client, setup our own AP.
        Serial.print("Error connecting to: ");
        Serial.println(client_ssid);
        init_wifiAP();
    }
    // Set up mDNS responder:
    // - first argument is the domain name, in this example
    //   the fully-qualified domain name is "esp8266.local"
    // - second argument is the IP address to advertise
    //   we send our IP address on the WiFi network
    if (!MDNS.begin("esp8266astro")) {
      Serial.println("Error setting up MDNS responder!");
      while (1) {
        delay(1000);
      }
    }
    Serial.println("mDNS responder started");
    config_rest_server_routing();
    intwebserver.begin();
    Serial.println("HTTP REST Server Started");

    // Add service to MDNS-SD
    MDNS.addService("http", "tcp", 80);
    astrojob.curstatus=STOPPED;
}

void openShutter()
{
    Serial.println("Open Shutter");
    digitalWrite(RELAY, HIGH);
    digitalWrite(BUILTINLED, LOW); //on the Wemos D1 mini this turn on the led
    astrojob.curshutterstatus=SOPEN;
    picstart=millis();
}

void closeShutter()
{
    Serial.println("Close Shutter");
    digitalWrite(RELAY, LOW);
    digitalWrite(BUILTINLED, HIGH); //on the Wemos D1 mini this turn off the led
    astrojob.curshutterstatus=SCLOSED;
    if(astrojob.curstatus==RUNNING){
      //Just finished another picture, count it.
      astrojob.picscomp=astrojob.picscomp+1;
    }//No need to cound if we are stopping
    cooloffstart=millis();
}

void doPics()
{
  //put pic logic here.
  timenow=millis();

  if(astrojob.curstatus==RUNNING) //We are in the middle of a run
  {
    if(astrojob.picscomp<astrojob.numPics){ //Have we finished the pics?
      if(astrojob.curshutterstatus==SOPEN) //the shutter is currently open
      {
        //Need to convert millis to sec - multiply by 1000
        if((timenow-picstart)>=1000*astrojob.exptime) //we have finished the exposure
        {
          Serial.println("Start Cool-off after "+String(timenow-picstart)+"ms");
          closeShutter();
          astrojob.curshutterstatus=COOLING; //start the cooloff
        }
      }
      else if(astrojob.curshutterstatus==COOLING) //we are currently cooling
      {
        //Need to convert millis to sec - multiply by 1000
        if((timenow-cooloffstart)>=1000*astrojob.cooloff) //Cooloff has finished, start another picture
        {
          Serial.println("Start Pic after cool off after "+String(timenow-cooloffstart)+"ms");
          openShutter();
        }
      }else{ //Shutter is closed, but we are in the middle of a run, start pic
        Serial.println("Start First Pic");
        openShutter();     
      }
    }
    else //we hace finished a run, let's put status to stopped so we can stop everything
    {
      Serial.println("Stopping");
      astrojob.curstatus=STOPPED;
    }
  }
  else //We are in STOPPED mode - close shutter if open.
  {
      if(astrojob.curshutterstatus!=SCLOSED)
      {
        Serial.println("Make sure everything is closed");
        closeShutter();
      }
  }
}

void loop(void) {
    MDNS.update();
    intwebserver.handleClient();
    doPics();
}
