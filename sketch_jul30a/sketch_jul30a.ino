#include <esp_now.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include <Arduino_JSON.h>

// Replace with your network credentials (STATION)
const char* ssid = "iPhone (8)";
const char* password = "1678namning";

// Structure example to receive data
// Must match the sender structure
typedef struct struct_message {
  int id;
  float temp;
  float hum;
  unsigned int readingId;
} struct_message;

struct_message incomingReadings;

JSONVar board;

AsyncWebServer server(80);
AsyncEventSource events("/events");

const int ledPin = 2; // Built-in LED pin
const int buttonPin = 4; // GPIO 4 used as the button input
bool ledState = false; // LED state (OFF by default)

// Variables to implement hold button functionality
bool buttonState = false;
bool ledHoldState = false;
unsigned long buttonPressStartTime = 0;
const unsigned long HOLD_DURATION_MS = 1000;

// callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) {
  // ... (existing callback code)
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP-NOW DASHBOARD</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    /* ... (existing CSS styles) ... */
  </style>
</head>
<body>
  <div class="topnav">
    <h3>ESP-NOW DASHBOARD</h3>
  </div>
  <div class="content">
    <div class="cards">
      <!-- New card for the LED control -->
      <div class="card">
        <h4><i class="fas fa-lightbulb sensor-icon"></i> LED Control</h4>
        <p class="led-status">LED Status: <span id="ledStatus">OFF</span></p>
        <button class="led-btn" onmousedown="startHoldLED()" onmouseup="stopHoldLED()" onmouseleave="stopHoldLED()">Hold to Turn On</button>
      </div>
    </div>
  </div>
  <script>
    // Function to update LED status when an event is received
    function updateLEDStatus(status) {
      document.getElementById("ledStatus").innerHTML = status;
    }

    // Function to handle LED state change event
    function handleLEDState(state) {
      updateLEDStatus(state);
    }

    // Function to start holding the button (turning on the LED)
    function startHoldLED() {
      fetch('/holdled/start')
        .then(response => response.text())
        .then(data => {
          console.log(data);
          updateLEDStatus(data);
        });
    }

    // Function to stop holding the button (turning off the LED)
    function stopHoldLED() {
      fetch('/holdled/stop')
        .then(response => response.text())
        .then(data => {
          console.log(data);
          updateLEDStatus(data);
        });
    }

    // Establish a server-sent events (SSE) connection with the server
    const eventSource = new EventSource('/events');
    eventSource.onmessage = function(event) {
      const eventData = JSON.parse(event.data);
      if (eventData.event === "led_state") {
        handleLEDState(eventData.data);
      }
    };
  </script>
</body>
</html>)rawliteral";

void setup() {
  Serial.begin(115200);

  pinMode(ledPin, OUTPUT);
  digitalWrite(ledPin, LOW);

  pinMode(buttonPin, INPUT_PULLUP);

  WiFi.mode(WIFI_AP_STA);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Setting as a Wi-Fi Station..");
  }
  Serial.print("Station IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });

  server.on("/toggleled", HTTP_GET, [](AsyncWebServerRequest *request){
    // Toggle the LED state when the button is released after a hold
    if (buttonState && ledHoldState) {
//      ledState = !ledState;
      digitalWrite(ledPin, ledState ? HIGH : LOW);
      events.send(String(ledState ? "ON" : "OFF").c_str(), "led_state", millis());
      ledHoldState = false;
      request->send(200, "text/plain", ledState ? "ON" : "OFF");
//      digitalWrite(2,LOW);
    } else {
      // If the button is not being held, respond with the current LED state
      request->send(200, "text/plain", ledState ? "ON" : "OFF");
//      digitalWrite(2,LOW);
    }
  });

  // New route to handle starting the hold (turning on the LED)
  server.on("/holdled/start", HTTP_GET, [](AsyncWebServerRequest *request){
    if (!buttonState) {
      // Start the button press timing
      buttonState = true;
      buttonPressStartTime = millis();
      ledHoldState = false;
    }
    request->send(200, "text/plain", "Holding LED...");
    digitalWrite(2,HIGH);
  });

  // New route to handle stopping the hold (turning off the LED)
  server.on("/holdled/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    if (buttonState) {
      // Check the button press duration to determine if it's a hold
      unsigned long buttonPressDuration = millis() - buttonPressStartTime;
      if (buttonPressDuration >= HOLD_DURATION_MS) {
        // Hold duration detected, set the LED hold state
        ledHoldState = true;
      }
      buttonState = false;
    }
    request->send(200, "text/plain", ledHoldState ? "ON" : "OFF");
    digitalWrite(2,LOW);
  });

  events.onConnect([](AsyncEventSourceClient *client){
    // Send the initial LED state when a client connects
    events.send(String(ledState ? "ON" : "OFF").c_str(), "led_state", millis());
  });
  server.addHandler(&events);
  server.begin();
}

void loop() {
  // No need to handle LED control here since it's already handled in the server code
}
