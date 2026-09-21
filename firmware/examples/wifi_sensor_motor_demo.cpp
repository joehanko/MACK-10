#include <Arduino.h>
#include <HX711.h>
#include <WebServer.h>
#include <WiFi.h>

// ============================================================
// MACK-10 REV B
// Web tensile tester
// ============================================================

// ------------------------------------------------------------
// HARDWARE
// ------------------------------------------------------------

// HX711
constexpr int HX_DT = D0;
constexpr int HX_SCK = D1;

// TMC2208
constexpr int STEP_PIN = D2;
constexpr int DIR_PIN = D3;
constexpr int EN_PIN = D6;

// ------------------------------------------------------------
// LOAD CELL
// ------------------------------------------------------------

HX711 scale;

long tareOffset = 0;

// TEMPORARY DEMO CALIBRATION
// Replace this once we calibrate the 10 kg S-cell.
constexpr float COUNTS_PER_NEWTON = 500.0f;

constexpr float ZERO_DEADBAND_N = 0.15f;

// ------------------------------------------------------------
// FRACTURE DETECTION
// ------------------------------------------------------------

constexpr float MIN_PEAK_FOR_BREAK_N = 5.0f;

// Current force must fall below 60% of peak
constexpr float BREAK_RATIO = 0.60f;

// Must happen for this many consecutive samples
constexpr int BREAK_CONFIRM_SAMPLES = 3;

// ------------------------------------------------------------
// MOTOR
// ------------------------------------------------------------

// Nice slow simulated tensile pull
constexpr float STEPS_PER_SECOND = 75.0f;

constexpr unsigned long STEP_INTERVAL_US =
    (unsigned long)(1000000.0f / STEPS_PER_SECOND);

// ------------------------------------------------------------
// WIFI
// ------------------------------------------------------------

const char* AP_SSID = "MACK10";
const char* AP_PASSWORD = "mack10test";

WebServer server(80);

// ------------------------------------------------------------
// TEST STATE
// ------------------------------------------------------------

enum TestState { READY, TARING, PULLING, FRACTURED, STOPPED };

TestState testState = READY;

float forceN = 0.0f;
float peakForceN = 0.0f;

int breakCounter = 0;

unsigned long testStartMs = 0;
unsigned long lastStepUs = 0;

uint32_t sampleId = 0;

// ============================================================
// STATE TEXT
// ============================================================

const char* getStateText() {
  switch (testState) {
    case READY:
      return "READY";

    case TARING:
      return "TARING";

    case PULLING:
      return "PULLING";

    case FRACTURED:
      return "FRACTURED";

    case STOPPED:
      return "STOPPED";
  }

  return "UNKNOWN";
}

// ============================================================
// MOTOR
// ============================================================

void disableMotor() {
  // Stop STEP pulses first
  digitalWrite(STEP_PIN, LOW);

  // TMC2208 EN is active LOW
  digitalWrite(EN_PIN, HIGH);
}

void enableMotor() { digitalWrite(EN_PIN, LOW); }

void updateMotor() {
  if (testState != PULLING) {
    return;
  }

  unsigned long now = micros();

  if (now - lastStepUs >= STEP_INTERVAL_US) {
    lastStepUs = now;

    digitalWrite(STEP_PIN, HIGH);
    delayMicroseconds(5);
    digitalWrite(STEP_PIN, LOW);
  }
}

// ============================================================
// FRACTURE
// ============================================================

void fractureDetected() {
  disableMotor();

  testState = FRACTURED;

  Serial.println();
  Serial.println("============================");
  Serial.println("*** SPECIMEN FRACTURED ***");

  Serial.print("Peak force: ");
  Serial.print(peakForceN, 2);
  Serial.println(" N");

  Serial.println("Motor disabled.");
  Serial.println("============================");
}

// ============================================================
// LOAD CELL
// ============================================================

void updateLoadCell() {
  if (testState != PULLING) {
    return;
  }

  if (!scale.is_ready()) {
    return;
  }

  long raw = scale.read();

  long counts = raw - tareOffset;

  float measuredForce = -(float)counts / COUNTS_PER_NEWTON;

  if (fabs(measuredForce) < ZERO_DEADBAND_N) {
    measuredForce = 0.0f;
  }

  // Tensile only
  if (measuredForce < 0.0f) {
    measuredForce = 0.0f;
  }

  forceN = measuredForce;

  sampleId++;

  // ----------------------------------------------------------
  // Peak tracking
  // ----------------------------------------------------------

  if (forceN > peakForceN) {
    peakForceN = forceN;

    // We're still building load.
    breakCounter = 0;
  }

  // ----------------------------------------------------------
  // Fracture detection
  // ----------------------------------------------------------

  if (peakForceN >= MIN_PEAK_FOR_BREAK_N) {
    float fractureThreshold = peakForceN * BREAK_RATIO;

    if (forceN < fractureThreshold) {
      breakCounter++;

      Serial.print("Possible break: ");
      Serial.print(forceN, 2);
      Serial.print(" N / peak ");
      Serial.print(peakForceN, 2);
      Serial.print(" N  ");
      Serial.print(breakCounter);
      Serial.print("/");
      Serial.println(BREAK_CONFIRM_SAMPLES);

    } else {
      breakCounter = 0;
    }

    if (breakCounter >= BREAK_CONFIRM_SAMPLES) {
      fractureDetected();

      return;
    }
  }

  Serial.print("Force: ");
  Serial.print(forceN, 2);

  Serial.print(" N   Peak: ");
  Serial.print(peakForceN, 2);

  Serial.println(" N");
}

// ============================================================
// START TEST
// ============================================================

void startTest() {
  disableMotor();

  testState = TARING;

  Serial.println();
  Serial.println("Starting new tensile test...");
  Serial.println("Taring load cell...");

  if (!scale.wait_ready_timeout(2000)) {
    Serial.println("HX711 NOT READY");

    testState = STOPPED;

    return;
  }

  tareOffset = scale.read_average(20);

  Serial.print("Tare offset: ");
  Serial.println(tareOffset);

  forceN = 0.0f;
  peakForceN = 0.0f;

  breakCounter = 0;
  sampleId = 0;

  // Pull direction
  digitalWrite(DIR_PIN, HIGH);

  testStartMs = millis();

  lastStepUs = micros();

  testState = PULLING;

  enableMotor();

  Serial.println("TEST STARTED");
  Serial.println("Motor pulling...");
}

// ============================================================
// STOP TEST
// ============================================================

void stopTest() {
  disableMotor();

  if (testState == PULLING || testState == TARING) {
    testState = STOPPED;
  }

  Serial.println("Test stopped.");
}

// ============================================================
// NEW TEST / RESET
// ============================================================

void resetTest() {
  disableMotor();

  forceN = 0.0f;
  peakForceN = 0.0f;

  breakCounter = 0;
  sampleId = 0;

  testState = READY;

  Serial.println("Ready for new test.");
}

// ============================================================
// WEB PAGE
// ============================================================

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
      content="width=device-width, initial-scale=1">

<title>MACK-10 Load Tester</title>

<style>

* {
    box-sizing: border-box;
}

body {
    margin: 0;
    padding: 22px;
    background: #0f1726;
    color: #f5f7fa;
    font-family:
        -apple-system,
        BlinkMacSystemFont,
        "Segoe UI",
        Arial,
        sans-serif;
}

.container {
    max-width: 1000px;
    margin: auto;
}

.header {
    margin-bottom: 22px;
}

h1 {
    margin: 0;
    font-size: 34px;
}

.subtitle {
    margin-top: 5px;
    color: #8f9bad;
}

.state {
    display: inline-block;
    margin-top: 14px;
    padding: 7px 13px;
    border-radius: 20px;
    background: #253044;
    font-weight: 600;
    font-size: 14px;
}

.cards {
    display: grid;
    grid-template-columns:
        repeat(auto-fit, minmax(180px, 1fr));
    gap: 14px;
    margin-bottom: 18px;
}

.card {
    background: #1e2939;
    border-radius: 14px;
    padding: 20px;
}

.label {
    color: #98a3b5;
    font-size: 14px;
}

.value {
    margin-top: 6px;
    font-size: 38px;
    font-weight: 700;
}

.graphCard {
    background: white;
    border-radius: 14px;
    padding: 10px;
}

canvas {
    width: 100%;
    height: 350px;
    display: block;
}

.controls {
    display: flex;
    flex-wrap: wrap;
    gap: 10px;
    margin-top: 18px;
}

button {

    border: none;
    border-radius: 9px;

    padding: 13px 18px;

    font-size: 16px;

    cursor: pointer;
}

.start {
    background: #22c55e;
    color: #07120b;
}

.stop {
    background: #ef4444;
    color: white;
}

.new {
    background: #475569;
    color: white;
}

.export {
    background: #2563eb;
    color: white;
}

.note {
    color: #7f8a9c;
    margin-top: 12px;
    font-size: 13px;
}

</style>

</head>


<body>

<div class="container">

<div class="header">

<h1>MACK-10</h1>

<div class="subtitle">
Rev B Tensile Tester
</div>

<div class="state">
State:
<span id="state">READY</span>
</div>

</div>


<div class="cards">

<div class="card">

<div class="label">
Live Force
</div>

<div class="value">
<span id="force">0.00</span> N
</div>

</div>


<div class="card">

<div class="label">
Peak Force
</div>

<div class="value">
<span id="peak">0.00</span> N
</div>

</div>


<div class="card">

<div class="label">
Elapsed
</div>

<div class="value">
<span id="time">0.0</span> s
</div>

</div>

</div>


<div class="graphCard">

<canvas
    id="graph"
    width="1000"
    height="400">
</canvas>

</div>


<div class="controls">

<button
    class="start"
    onclick="startTest()">
START TEST
</button>

<button
    class="stop"
    onclick="stopTest()">
STOP
</button>

<button
    class="new"
    onclick="newTest()">
NEW TEST
</button>

<button
    class="export"
    onclick="exportCSV()">
EXPORT CSV
</button>

</div>


<div class="note">

Graph shows the most recent
<span id="windowText">15</span>
seconds.

Full test data is retained for CSV export.

</div>

</div>


<script>

// ============================================================
// SETTINGS
// ============================================================

// Number of seconds visible on screen.
//
// Change this to 10, 20, 30, etc.
const GRAPH_WINDOW_SECONDS = 15;


// ============================================================
// DATA STORAGE
// ============================================================

// Entire test.
//
// Used ONLY for CSV export.
let allSamples = [];


// Rolling visible data.
//
// Old points are removed as time advances.
let graphSamples = [];


let lastSampleId = -1;


// ============================================================
// CANVAS
// ============================================================

const canvas =
    document.getElementById("graph");

const ctx =
    canvas.getContext("2d");

document.getElementById("windowText")
    .textContent =
    GRAPH_WINDOW_SECONDS;


// ============================================================
// NICE Y AXIS
// ============================================================

function niceYMaximum(value) {

    if (value <= 10)
        return 10;

    if (value <= 20)
        return 20;

    if (value <= 50)
        return 50;

    if (value <= 100)
        return 100;

    if (value <= 200)
        return Math.ceil(value / 20) * 20;

    if (value <= 500)
        return Math.ceil(value / 50) * 50;

    return Math.ceil(value / 100) * 100;
}


// ============================================================
// DRAW GRAPH
// ============================================================

function drawGraph() {

    const W = canvas.width;
    const H = canvas.height;

    const left = 75;
    const right = 20;
    const top = 20;
    const bottom = 55;

    const graphW =
        W - left - right;

    const graphH =
        H - top - bottom;


    // Background
    ctx.fillStyle = "#ffffff";

    ctx.fillRect(
        0,
        0,
        W,
        H
    );


    // --------------------------------------------------------
    // Determine force scale
    // --------------------------------------------------------

    let visibleMaxForce = 0;

    graphSamples.forEach(p => {

        if (p.force > visibleMaxForce)
            visibleMaxForce = p.force;
    });


    let yMax =
        niceYMaximum(
            visibleMaxForce * 1.10
        );


    // --------------------------------------------------------
    // Time window
    // --------------------------------------------------------

    let latestTime = 0;

    if (graphSamples.length > 0) {

        latestTime =
            graphSamples[
                graphSamples.length - 1
            ].time;
    }


    let timeStart =
        Math.max(
            0,
            latestTime -
            GRAPH_WINDOW_SECONDS
        );

    let timeEnd =
        timeStart +
        GRAPH_WINDOW_SECONDS;


    // --------------------------------------------------------
    // GRID
    // --------------------------------------------------------

    ctx.lineWidth = 1;

    ctx.font =
        "14px Arial";

    ctx.textBaseline =
        "middle";


    const horizontalLines = 5;


    for (
        let i = 0;
        i <= horizontalLines;
        i++
    ) {

        let fraction =
            i /
            horizontalLines;

        let y =
            top +
            graphH -
            fraction * graphH;

        let force =
            fraction * yMax;


        ctx.strokeStyle =
            "#e5e7eb";

        ctx.beginPath();

        ctx.moveTo(
            left,
            y
        );

        ctx.lineTo(
            left + graphW,
            y
        );

        ctx.stroke();


        ctx.fillStyle =
            "#374151";

        ctx.textAlign =
            "right";

        ctx.fillText(
            force.toFixed(0) + " N",
            left - 10,
            y
        );
    }


    // --------------------------------------------------------
    // VERTICAL TIME GRID
    // --------------------------------------------------------

    const verticalLines = 5;


    for (
        let i = 0;
        i <= verticalLines;
        i++
    ) {

        let fraction =
            i /
            verticalLines;

        let x =
            left +
            fraction * graphW;

        let t =
            timeStart +
            fraction *
            GRAPH_WINDOW_SECONDS;


        ctx.strokeStyle =
            "#f0f1f3";

        ctx.beginPath();

        ctx.moveTo(
            x,
            top
        );

        ctx.lineTo(
            x,
            top + graphH
        );

        ctx.stroke();


        ctx.fillStyle =
            "#374151";

        ctx.textAlign =
            "center";

        ctx.textBaseline =
            "top";

        ctx.fillText(
            t.toFixed(1) + " s",
            x,
            top +
            graphH +
            12
        );
    }


    // --------------------------------------------------------
    // FORCE TRACE
    // --------------------------------------------------------

    if (graphSamples.length < 2)
        return;


    ctx.strokeStyle =
        "#2563eb";

    ctx.lineWidth = 3;

    ctx.lineJoin =
        "round";

    ctx.lineCap =
        "round";

    ctx.beginPath();


    let started = false;


    graphSamples.forEach(p => {

        if (
            p.time < timeStart ||
            p.time > timeEnd
        ) {
            return;
        }


        let x =
            left +
            (
                (p.time - timeStart) /
                GRAPH_WINDOW_SECONDS
            ) *
            graphW;


        let y =
            top +
            graphH -
            (
                p.force /
                yMax
            ) *
            graphH;


        if (!started) {

            ctx.moveTo(
                x,
                y
            );

            started = true;

        } else {

            ctx.lineTo(
                x,
                y
            );
        }
    });


    ctx.stroke();
}


// ============================================================
// LIVE DATA
// ============================================================

async function updateData() {

    try {

        const response =
            await fetch(
                "/data",
                {
                    cache:
                    "no-store"
                }
            );


        const data =
            await response.json();


        document
            .getElementById("force")
            .textContent =
            data.force.toFixed(2);


        document
            .getElementById("peak")
            .textContent =
            data.peak.toFixed(2);


        document
            .getElementById("time")
            .textContent =
            data.time.toFixed(1);


        document
            .getElementById("state")
            .textContent =
            data.state;


        // Only add a point when the ESP32
        // has actually produced a new HX711 sample.
        //
        // This prevents browser polling from creating
        // duplicate points.

        if (
            data.state === "PULLING" &&
            data.sampleId !== lastSampleId
        ) {

            lastSampleId =
                data.sampleId;


            const sample = {

                time:
                    data.time,

                force:
                    data.force
            };


            // Save complete test
            allSamples.push(
                sample
            );


            // Save visible graph data
            graphSamples.push(
                sample
            );


            // ------------------------------------------------
            // ROLLING WINDOW
            // ------------------------------------------------

            const cutoff =
                data.time -
                GRAPH_WINDOW_SECONDS;


            while (
                graphSamples.length > 0 &&
                graphSamples[0].time <
                cutoff
            ) {

                graphSamples.shift();
            }
        }


        drawGraph();

    }

    catch (err) {

        console.log(
            "Connection error:",
            err
        );
    }
}


// ============================================================
// START
// ============================================================

async function startTest() {

    allSamples = [];

    graphSamples = [];

    lastSampleId = -1;

    drawGraph();


    await fetch(
        "/start",
        {
            method:
            "POST"
        }
    );
}


// ============================================================
// STOP
// ============================================================

async function stopTest() {

    await fetch(
        "/stop",
        {
            method:
            "POST"
        }
    );
}


// ============================================================
// NEW TEST
// ============================================================

async function newTest() {

    await fetch(
        "/reset",
        {
            method:
            "POST"
        }
    );


    allSamples = [];

    graphSamples = [];

    lastSampleId = -1;

    drawGraph();
}


// ============================================================
// CSV EXPORT
// ============================================================

function exportCSV() {

    if (
        allSamples.length === 0
    ) {

        alert(
            "No test data to export."
        );

        return;
    }


    let csv =
        "time_s,force_N\n";


    allSamples.forEach(p => {

        csv +=
            p.time.toFixed(3) +
            "," +
            p.force.toFixed(4) +
            "\n";
    });


    const blob =
        new Blob(
            [csv],
            {
                type:
                "text/csv"
            }
        );


    const url =
        URL.createObjectURL(
            blob
        );


    const a =
        document.createElement(
            "a"
        );


    a.href = url;

    a.download =
        "MACK10_tensile_test.csv";


    document.body.appendChild(a);

    a.click();

    document.body.removeChild(a);

    URL.revokeObjectURL(url);
}


// ============================================================
// INITIAL GRAPH
// ============================================================

drawGraph();

setInterval(
    updateData,
    100
);

</script>

</body>

</html>
)rawliteral";

// ============================================================
// WEB ROUTES
// ============================================================

void handleRoot() { server.send_P(200, "text/html", INDEX_HTML); }

void handleData() {
  float elapsed = 0.0f;

  if (testState == PULLING || testState == FRACTURED || testState == STOPPED) {
    elapsed = (millis() - testStartMs) / 1000.0f;
  }

  String json = "{";

  json += "\"time\":";
  json += String(elapsed, 3);

  json += ",\"force\":";
  json += String(forceN, 3);

  json += ",\"peak\":";
  json += String(peakForceN, 3);

  json += ",\"sampleId\":";
  json += String(sampleId);

  json += ",\"state\":\"";
  json += getStateText();
  json += "\"";

  json += "}";

  server.send(200, "application/json", json);
}

void handleStart() {
  // Acknowledge browser first
  server.send(200, "text/plain", "STARTING");

  startTest();
}

void handleStop() {
  stopTest();

  server.send(200, "text/plain", "STOPPED");
}

void handleReset() {
  resetTest();

  server.send(200, "text/plain", "READY");
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);

  delay(500);

  Serial.println();
  Serial.println("MACK-10 REV B WEB TESTER");

  // ----------------------------------------------------------
  // MOTOR
  // ----------------------------------------------------------

  pinMode(STEP_PIN, OUTPUT);

  pinMode(DIR_PIN, OUTPUT);

  pinMode(EN_PIN, OUTPUT);

  digitalWrite(STEP_PIN, LOW);

  digitalWrite(DIR_PIN, HIGH);

  // IMPORTANT:
  // keep driver disabled until user
  // intentionally starts a test
  disableMotor();

  // ----------------------------------------------------------
  // HX711
  // ----------------------------------------------------------

  scale.begin(HX_DT, HX_SCK);

  // ----------------------------------------------------------
  // WIFI
  // ----------------------------------------------------------

  WiFi.mode(WIFI_AP);

  WiFi.softAP(AP_SSID, AP_PASSWORD);

  IPAddress ip = WiFi.softAPIP();

  Serial.println();

  Serial.print("Wi-Fi network: ");

  Serial.println(AP_SSID);

  Serial.print("Dashboard: http://");

  Serial.println(ip);

  // ----------------------------------------------------------
  // WEB SERVER
  // ----------------------------------------------------------

  server.on("/", HTTP_GET, handleRoot);

  server.on("/data", HTTP_GET, handleData);

  server.on("/start", HTTP_POST, handleStart);

  server.on("/stop", HTTP_POST, handleStop);

  server.on("/reset", HTTP_POST, handleReset);

  server.begin();

  Serial.println("Web server started.");

  Serial.println("MACK-10 READY");
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  // Keep the web server responsive
  server.handleClient();

  // Generate motor steps
  updateMotor();

  // Read force + detect fracture
  updateLoadCell();
}