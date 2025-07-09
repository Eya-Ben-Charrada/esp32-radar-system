#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ESP32Servo.h>

#define TRIG_PIN 5
#define ECHO_PIN 18
#define SERVO_PIN 13

const char* ssid = "GNET_WIFI";
const char* password = "19971997";

Servo myServo;
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

int angle = 0;
bool increasing = true;

float measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  return duration * 0.034 / 2; // distance in cm
}

String getHTML() {
  return R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>ESP32 Radar</title>
  <style>
    body {
      margin: 0; background: #000; color: #0f0; font-family: monospace;
      display: flex; flex-direction: column; align-items: center;
      user-select: none;
    }
    canvas { background: #111; margin-top: 20px; }
    #distance { margin-top: 10px; font-size: 18px; }
  </style>
</head>
<body>
  <h1>ESP32 Radar</h1>
  <canvas id="radar" width="400" height="400"></canvas>
  <div id="distance">Distance: -- cm</div>

  <script>
    const canvas = document.getElementById('radar');
    const ctx = canvas.getContext('2d');
    const centerX = canvas.width / 2;
    const centerY = canvas.height / 2;
    const radius = 180;
    let lastPoints = [];

    function drawRadarBackground() {
      ctx.fillStyle = '#111';
      ctx.fillRect(0, 0, canvas.width, canvas.height);

      ctx.strokeStyle = '#0f0';
      ctx.lineWidth = 1;
      ctx.beginPath();
      for(let r = radius; r > 0; r -= radius / 4) {
        ctx.moveTo(centerX + r, centerY);
        ctx.arc(centerX, centerY, r, 0, 2 * Math.PI);
      }
      ctx.stroke();

      // Range labels
      ctx.fillStyle = '#0f0';
      ctx.font = '12px monospace';
      for(let i = 1; i <= 4; i++) {
        let r = (radius / 4) * i;
        ctx.fillText(`${r * 1} cm`, centerX + r + 5, centerY - 5);
      }
    }

    function drawSweep(angle) {
      ctx.fillStyle = 'rgba(0, 255, 0, 0.15)';
      ctx.beginPath();
      ctx.moveTo(centerX, centerY);
      let startAngle = (angle - 10) * Math.PI / 180;
      let endAngle = (angle + 10) * Math.PI / 180;
      ctx.arc(centerX, centerY, radius, startAngle, endAngle);
      ctx.closePath();
      ctx.fill();
    }

    function drawPoint(angle, distance) {
      let rad = angle * Math.PI / 180;
      let d = Math.min(distance, radius);
      let x = centerX + d * Math.cos(rad);
      let y = centerY - d * Math.sin(rad);

      // Color code based on distance
      let color = distance < radius * 0.33 ? 'red' :
                  distance < radius * 0.66 ? 'yellow' : 'lime';

      ctx.fillStyle = color;
      ctx.beginPath();
      ctx.arc(x, y, 6, 0, 2 * Math.PI);
      ctx.fill();

      // Distance label
      ctx.fillStyle = '#0f0';
      ctx.font = '14px monospace';
      ctx.fillText(`${distance.toFixed(1)} cm`, x + 10, y);
    }

    const ws = new WebSocket(`ws://${location.host}/ws`);
    ws.onmessage = e => {
      let [angle, distance] = e.data.split(',');
      angle = parseInt(angle);
      distance = parseFloat(distance);

      drawRadarBackground();
      drawSweep(angle);
      drawPoint(angle, distance);

      document.getElementById('distance').textContent = `Distance: ${distance.toFixed(1)} cm`;
    };

    // Initial draw
    drawRadarBackground();
  </script>
</body>
</html>

)rawliteral";
}

void notifyClients(int angle, float distance) {
  String msg = String(angle) + "," + String(distance, 2);
  ws.textAll(msg);
}

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  myServo.setPeriodHertz(50);           // Standard servo PWM frequency
  myServo.attach(SERVO_PIN, 500, 2400); // Attach servo on GPIO 13 with pulse width range

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP: " + WiFi.localIP().toString());

  ws.onEvent([](AsyncWebSocket *server, AsyncWebSocketClient *client,
    AwsEventType type, void *arg, uint8_t *data, size_t len) {
    // No events needed here
  });

  server.addHandler(&ws);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", getHTML());
  });
  server.begin();
}

void loop() {
  if (increasing) {
    angle += 2;
    if (angle >= 180) increasing = false;
  } else {
    angle -= 2;
    if (angle <= 0) increasing = true;
  }

  myServo.write(angle);
  delay(50);

  float distance = measureDistance();
  notifyClients(angle, distance);
}
