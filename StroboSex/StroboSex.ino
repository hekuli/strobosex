#include <WiFi.h>
#include <WebServer.h>
#include <FastLED.h>

#define LED_CHIPSET NEOPIXEL
#define NUM_LEDS 55
#define DATA_PIN 16
#define MAX_VOLTS 5
#define MAX_AMPS 3500

IPAddress apIP(192, 168, 4, 1);
IPAddress netMsk(255, 255, 255, 0);
WebServer server(80);

// set up the block of memory that will be used for storing and manipulating the led data
CRGB leds[NUM_LEDS];

volatile bool isEnabled = true;       // [true, false]: enables/disables all lights
volatile uint8_t brightness = 128;    // [1, 255]
volatile uint16_t freq = 100;         // [100, 6000]: in centi-herz (1 Hz = 100 cHz)
volatile uint8_t dutyPct = 50;        // [1, 99]: Percentage of time the lights are on vs off in a single cycle
uint32_t lastToggleTime = 0;              // Used to track cyle timing
uint32_t onTimeMs = 500;              // Calculated value in Milliseconds to determine how long the lights should be ON in a cycle.
uint32_t offTimeMs = 500;             // Calculated value in Milliseconds to determine how long the lights should be OFF in a cycle.

void setup() {
  Serial.begin(9600);   
  setupWiFi();
  setupLEDs();
  setupWebserver();
  // Turn on internal LED to indicate functioning ok.
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

void setupWiFi() {
  WiFi.mode(WIFI_AP);
  // configure the AP to use a fixed IP
  WiFi.softAPConfig(apIP, apIP, netMsk);
  WiFi.softAP("StroboSex");
  Serial.print("AP IP = ");
  Serial.println(WiFi.softAPIP());
}

void setupLEDs() {
  FastLED.addLeds<LED_CHIPSET, DATA_PIN>(leds, NUM_LEDS);
  FastLED.setBrightness(brightness);
  // just in case, cap the total power draw
  FastLED.setMaxPowerInVoltsAndMilliamps(MAX_VOLTS, MAX_AMPS);
}

void setupWebserver() {
  server.on("/", handleRoot);
  server.on("/params", handleParams);  // e.g.  http://<ip>/params?hz=12&duty=25
  server.begin();
}

// Parse requests from the web console.
void handleParams() {
  if (server.hasArg("isEnabled")) {
    isEnabled = server.arg("isEnabled").toInt() != 0;
  }

  if (server.hasArg("brightness")) {
    brightness = constrain(server.arg("brightness").toInt(), 1, 255);
    FastLED.setBrightness(brightness);
  }

  // parse & clamp frequency
  if (server.hasArg("freq")) {
    freq = constrain(server.arg("freq").toInt(), 1, 6000);
  }
  // parse & clamp duty cycle
  if (server.hasArg("duty")) {
    dutyPct = constrain(server.arg("duty").toInt(), 1, 99);
  }

  // Recompute on/off durations
  uint32_t period = 100000UL / freq;
  onTimeMs  = (period * dutyPct) / 100;
  offTimeMs = period - onTimeMs;

  String statusMsg = "brightness=" + String(brightness) + ", freq=" + String(freq) + ", duty=" + String(dutyPct);
  server.send(200, "text/plain", statusMsg);
  Serial.println(statusMsg);
}

void loop() {
  // Handle any web console input.
  server.handleClient();

  // skip if disabled
  if (!isEnabled) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
    return;
  }

  uint32_t now = millis();
  static bool ledOn = false;
  uint32_t targetDuration = ledOn ? onTimeMs : offTimeMs;

  if (now - lastToggleTime >= targetDuration) {
    ledOn = !ledOn;
    fill_solid(leds, NUM_LEDS, ledOn ? CRGB::White : CRGB::Black);
    FastLED.show();
    lastToggleTime = now;
  }

}


// Serve the web control panel.
void handleRoot() {
  server.send(200, "text/html; charset=utf-8", R"rawliteral(

<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="viewport-fit=cover, width=device-width, initial-scale=1.0, minimum-scale=1.0, maximum-scale=1.0, user-scalable=no"/>
  <title>Strobo Sex</title>
  <style>
    :root {
      --slider-height: 250px;
      --slider-width: 75px;
    }
    /* prevent double-tap and pinch zoom on modern browsers */
    html, body {
      touch-action: pan-y;
    }
    body {
      font-size: 1.2em;
      padding: 0;
      margin: 0;
      font-family: helvetica, arial;
    }
    label { display: block; margin: 1em 0 .2em; }
    input[type=number] {
      font-size: 1.2em;
      width: var(--slider-width);
      padding: 5px;
      text-align: right;
    }
    h3 { margin: 0; }
    #status {
      display: inline-block;
      background-color: #eee;
      padding: 10px;
    }
    .faders {
      display: flex;
      justify-content: space-between;  /* even spacing */
      align-items: flex-end;           /* bottoms aligned */
      width: 100%;
      box-sizing: border-box;
    }
    .fader {
      display: flex;
      flex-direction: column;
      align-items: center;
      flex: 1;                         /* all three take equal space */
    }
    .fader label {
      margin-bottom: 0.5em;
    }
    .fader > * {
      margin: 0;
      margin-bottom: 10px;
      padding: 0;
    }
    /* the wrapper that *defines* the visible slider’s box */
    .slidecontainer {
      position: relative;
      width: var(--slider-width);
      height: var(--slider-height);
    }
    /* the actual <input>, rotated inside its container */
    .slidecontainer .slider {
      position: absolute;
      top: var(--slider-height);
      left: 0;
      width: var(--slider-height);
      height: var(--slider-width);
      transform: rotate(-90deg);
      transform-origin: 0 0;
      -webkit-appearance: none;
      appearance: none;
      background: #eee;
      outline: none;
      border-radius: 10px;
      margin: 0;
    }
    .slidecontainer .slider::-webkit-slider-thumb {
      -webkit-appearance: none;
      width: 50px;  /* actually the height b/c of rotation */
      height: var(--slider-width);
      background: #999;
      cursor: pointer;
      border-radius: 10px;
    }
    .fader button {
      background: #999;
      border-radius: 10px;
      height: 50px;
      width: var(--slider-width);
      color: #fff;
      font-size: 35px;
    }
    .fader button:active {
      background: #aaa;
      transform: translateY(2px);
    }
    #heading {
      padding: 1rem;
    }
    #freq {
      border: none;
      color: #999;
    }
  </style>
</head>
<body>
  <div id="heading">
    <h3>Strobo Sex</h3>
    Centi-Herz Value: <input type="number" id="freq" min="1" max="6000" readonly/>
    <label style="position: absolute; top: 20px; right: 20px;">
      <input type="checkbox" id="isEnabledToggle" checked onchange="syncAndSend(true)">
      Enabled
    </label>
    <div id="status"></div>
  </div>
  <div class="faders">
    <div class="fader">
      <label for="sliderHz">Hz</label>
      <input id="numHz" type="number" min="1" max="60" value="1" oninput="syncAndSend(false)">
      <div class="slidecontainer"><input id="sliderHz" class="slider" type="range" min="1" max="60" value="1" oninput="syncAndSend(true)"/></div>
      <button type="button" onclick="step('Hz', 1)">▲</button>
      <button type="button" onclick="step('Hz', -1)">▼</button>
    </div>
    <div class="fader">
      <label for="sliderDHz">Hz Decimal</label>
      <input id="numDHz" type="number" min="0" max="99" value="0" oninput="syncAndSend(false)">
      <div class="slidecontainer"><input id="sliderDHz" class="slider" type="range" min="0" max="99" value="0" oninput="syncAndSend(true)"/></div>
      <button type="button" onclick="step('DHz', 1)">▲</button>
      <button type="button" onclick="step('DHz', -1)">▼</button>
    </div>
    <div class="fader">
      <label for="sliderDuty">Duty Cycle</label>
      <input id="numDuty" type="number" min="10" max="90" value="50" oninput="syncAndSend(false)">
      <div class="slidecontainer"><input id="sliderDuty"class="slider" type="range" min="10" max="90" value="50" oninput="syncAndSend(true)"/></div>
      <button type="button" onclick="step('Duty', 1)">▲</button>
      <button type="button" onclick="step('Duty', -1)">▼</button>
    </div>
    <div class="fader">
      <label for="sliderBrightness">Brightness</label>
      <input id="numBrightness" type="number" min="1" max="100" value="50" oninput="syncAndSend(false)">
      <div class="slidecontainer"><input id="sliderBrightness"class="slider" type="range" min="1" max="100" value="50" oninput="syncAndSend(true)"/></div>
      <button type="button" onclick="step('Brightness', 1)">▲</button>
      <button type="button" onclick="step('Brightness', -1)">▼</button>
    </div>
  </div>

  <script>
  window.addEventListener('load', function() {
    const freq             = document.getElementById('freq');
    const status           = document.getElementById('status');
    const isEnabledToggle  = document.getElementById('isEnabledToggle');
    // sliders
    const sliderHz         = document.getElementById('sliderHz');
    const numHz            = document.getElementById('numHz');
    const sliderDHz        = document.getElementById('sliderDHz');
    const numDHz           = document.getElementById('numDHz');
    const sliderDuty       = document.getElementById('sliderDuty');
    const numDuty          = document.getElementById('numDuty');
    const sliderBrightness = document.getElementById('sliderBrightness');
    const numBrightness    = document.getElementById('numBrightness');

    function parseInput(val, min, max) {
      const numVal = parseInt(val) || 0;
      const minVal = parseInt(min);
      const maxVal = parseInt(max);
      return Math.max( Math.min(numVal, maxVal), minVal );
    }

    function step(sliderSuffix, delta) {
      const slider = document.getElementById('slider' + sliderSuffix);
      const sliderVal = parseInt(slider.value);
      const deltaVal = parseInt(delta);
      slider.value = sliderVal + deltaVal;
      syncAndSend(true);
    }

    function syncAndSend(touchedSlider) {
      if (touchedSlider) {
        numHz.value   = sliderHz.value;
        numDHz.value  = sliderDHz.value;
        numDuty.value = sliderDuty.value;
        numBrightness.value = sliderBrightness.value;
      } else {
        sliderHz.value   = parseInput(numHz.value, sliderHz.min, sliderHz.max);
        sliderDHz.value  = parseInput(numDHz.value, sliderDHz.min, sliderDHz.max);
        sliderDuty.value = parseInput(numDuty.value, sliderDuty.min, sliderDuty.max);
        sliderBrightness.value = parseInput(numBrightness.value, sliderBrightness.min, sliderBrightness.max);

        // force back to valid range
        numHz.value = sliderHz.value;
        numDHz.value = sliderDHz.value;
        numDuty.value = sliderDuty.value;
        numBrightness.value = sliderBrightness.value;
      }
      // update combined value display
      freq.value = parseInt(sliderHz.value) * 100 + parseInt(sliderDHz.value);
      const isEnabled = isEnabledToggle.checked ? 1 : 0;
      const brightness = parseInt(sliderBrightness.value)/100 * 255;

      const url = `/params?freq=${freq.value}&duty=${numDuty.value}&brightness=${brightness}&isEnabled=${isEnabled}`;
      // fire the update and handle response
      fetch(url, { method: 'GET', cache: 'no-store', mode: 'same-origin', })
        .then(resp => {
          if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
          return resp.text();
        })
        .then(txt => {
          status.textContent = `😊: ${txt.trim()}`;
          status.style.color = 'green';
        })
        .catch(err => {
          status.textContent = `😵 ERROR: ${err.message}`;
          status.style.color = 'red';
        });
    }

    window.syncAndSend = syncAndSend;
    window.step = step;
    syncAndSend(false);  // initial sync (no fetch on load)
  });
  </script>
</body>
</html>

  )rawliteral");
}
