#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LovyanGFX.hpp>

#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_RED     0xF800
#define C_GREEN   0x07E0
#define C_YELLOW  0xFFE0
#define C_CYAN    0x07FF

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel_instance;
  lgfx::Bus_Parallel8 _bus_instance;
  lgfx::Light_PWM _light_instance;
public:
  LGFX(void) {
    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);
    auto cfg = _bus_instance.config();
    cfg.port = 0;
    cfg.freq_write = 20000000;
    cfg.pin_wr = 8;
    cfg.pin_rd = 9;
    cfg.pin_rs = 7;
    cfg.pin_d0 = 39;
    cfg.pin_d1 = 40;
    cfg.pin_d2 = 41;
    cfg.pin_d3 = 42;
    cfg.pin_d4 = 45;
    cfg.pin_d5 = 46;
    cfg.pin_d6 = 47;
    cfg.pin_d7 = 48;
    _bus_instance.config(cfg);
    _panel_instance.setBus(&_bus_instance);
    auto panel_cfg = _panel_instance.config();
    panel_cfg.pin_cs = 6;
    panel_cfg.pin_rst = 5;
    panel_cfg.pin_busy = -1;
    panel_cfg.panel_width = 170;
    panel_cfg.panel_height = 320;
    panel_cfg.offset_x = 35;
    panel_cfg.offset_y = 0;
    panel_cfg.readable = true;
    panel_cfg.invert = true;
    panel_cfg.rgb_order = false;
    panel_cfg.dlen_16bit = false;
    panel_cfg.bus_shared = false;
    _panel_instance.config(panel_cfg);
    auto light_cfg = _light_instance.config();
    light_cfg.pin_bl = 38;
    light_cfg.invert = false;
    light_cfg.freq = 44100;
    light_cfg.pwm_channel = 0;
    _light_instance.config(light_cfg);
    _panel_instance.setLight(&_light_instance);
    setPanel(&_panel_instance);
  }
};

LGFX lcd;
// Sprite pentru anti-flicker: desenam in memorie, punem pe ecran dintr-o data
LGFX_Sprite spr(&lcd);

#define BTN_IO0   0
#define BTN_IO14  14

#define RADAR_CX 85
#define RADAR_CY 105  // radar mutat mai sus, ca sa incapa detaliile jos
#define RADAR_R  68   // raza mai mica, radarul se termina la y = 173

WebServer server(80);
DNSServer dnsServer;
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 0, 253);

struct Target {
  char ssid[20];
  char mac[18];
  int rssi;
  int channel;
  float dist;
  float angle;
};

Target targets[12];
int targetCount = 0;
float sweepAngle = 0;
unsigned long lastScan = 0;
int maxRange = 150;
// Flag pentru scanare ceruta de butonul IO14 (citit de scanTask, nu de loop)
volatile bool scanCerut = false;

float rssiToDistance(int rssi) {
  // Calibrare dupa masuratori reale, pana la 150m:
  // -20 dBm = 0,3m, -33 dBm = sub 1m (0,8m), -50 dBm = 1,5m, -69 dBm = 3m,
  // -80 dBm = 12m, -90 dBm = 50m, -95 dBm = 100m, -100 dBm = 150m.
  // O singura formula nu ajunge la 150m, de aceea se interpoleaza intre punctele din tabel.
  // Intre 2 puncte, distanta se interpoleaza liniar in logaritm, ca sa nu sara brusc.
  // Cercurile nu se schimba, raman ca inainte.
  const int   calRssi[8] = {-20, -33, -50, -69, -80, -90, -95, -100};
  const float calDist[8] = {0.3, 0.8, 1.5, 3.0, 12.0, 50.0, 100.0, 150.0};
  if (rssi >= calRssi[0]) return calDist[0];
  if (rssi <= calRssi[7]) return calDist[7];
  for (int i = 0; i < 7; i++) {
    if (rssi <= calRssi[i] && rssi >= calRssi[i + 1]) {
      float f = (float)(calRssi[i] - rssi) / (float)(calRssi[i] - calRssi[i + 1]);
      float logD = log(calDist[i]) + f * (log(calDist[i + 1]) - log(calDist[i]));
      return exp(logD);
    }
  }
  return calDist[7];
}

void doScan() {
  // Scanare BLOCANTA - dar NU se cheama din loop(), se cheama doar din scanTask().
  // Asta e singura forma care gaseste retelele in mod AP_STA.
  // Cum ruleaza pe alt nucleu, loop-ul principal nu se mai blocheaza.
  int n = WiFi.scanNetworks(false, false, false, 300);
  Serial.printf("Scan: %d retele\n", n);
  targetCount = 0;
  for (int i = 0; i < n && targetCount < 12; i++) {
    String s = WiFi.SSID(i);
    if (s.length() == 0) s = "[ASCUNS]";
    if (s.length() > 18) s = s.substring(0, 18);
    strcpy(targets[targetCount].ssid, s.c_str());
    strcpy(targets[targetCount].mac, WiFi.BSSIDstr(i).c_str());
    targets[targetCount].rssi = WiFi.RSSI(i);
    targets[targetCount].channel = WiFi.channel(i);
    targets[targetCount].dist = rssiToDistance(WiFi.RSSI(i));
    String m = WiFi.BSSIDstr(i);
    int sum = 0;
    for (int k = 0; k < m.length(); k++) sum += m[k];
    targets[targetCount].angle = (sum % 360);
    targetCount++;
  }
  WiFi.scanDelete();
  lastScan = millis();
  // ===== ANTI-SUPRAPUNERE: unghiuri diferite de jur imprejur =====
  // Toate punctele raman pe aceeasi axa polara (centru + raza din distanta),
  // doar directia / unghiul se corecteaza ca sa nu stea unul peste altul.
  for (int i = 0; i < targetCount; i++) {
    for (int j = i + 1; j < targetCount; j++) {
      int incercari = 0;
      while (incercari < 14) {
        float dAng = fabs(targets[i].angle - targets[j].angle);
        if (dAng > 180) dAng = 360 - dAng;
        float dDist = fabs(targets[i].dist - targets[j].dist);
        // 28 grade distanta minima pe acelasi inel = punctele nu se mai ating
        if (dAng >= 28 || dDist > 18) break;
        targets[j].angle += 28;
        if (targets[j].angle >= 360) targets[j].angle -= 360;
        incercari++;
      }
    }
  }
  // ===== FARA PUNCTE PE AXELE DREPTE =====
  // Orientarea ramane cea din cod, din MAC, doar ca nu mai lasam punctul
  // la 0 / 90 / 180 / 270 grade, ca sa nu stea pe liniile crucii.
  // Daca e la mai putin de 12 grade de o axa, il impingem cu 15 grade in lateral.
  for (int i = 0; i < targetCount; i++) {
    float a = targets[i].angle;
    while (a >= 360) a -= 360;
    while (a < 0) a += 360;
    float rest = fmod(a, 90.0);
    if (rest < 12) a += 15;
    else if (rest > 78) a += 15;
    while (a >= 360) a -= 360;
    targets[i].angle = a;
  }
}

void scanTask(void *param) {
  // Task separat, ruleaza pe core 0, in paralel cu loop() de pe core 1.
  // Face scanare la fiecare 8 secunde sau imediat cand se apasa IO14.
  (void)param;
  doScan();
  for (;;) {
    for (int i = 0; i < 40; i++) {
      if (scanCerut) break;
      vTaskDelay(200 / portTICK_PERIOD_MS);
    }
    if (scanCerut || (millis() - lastScan > 8000)) {
      scanCerut = false;
      doScan();
    }
  }
}

void handleData() {
  char buf[900];
  int pos = 0;
  pos += snprintf(buf + pos, sizeof(buf) - pos, "{\"range\":%d,\"targets\":[", maxRange);
  for (int i = 0; i < targetCount; i++) {
    if (i > 0) pos += snprintf(buf + pos, sizeof(buf) - pos, ",");
    pos += snprintf(buf + pos, sizeof(buf) - pos,
      "{\"ssid\":\"%s\",\"rssi\":%d,\"ch\":%d,\"dist\":%.1f,\"ang\":%.0f}",
      targets[i].ssid, targets[i].rssi, targets[i].channel, targets[i].dist, targets[i].angle);
    if (pos > 750) break;
  }
  pos += snprintf(buf + pos, sizeof(buf) - pos, "]}");
  server.send(200, "application/json", buf);
}

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<style>body{background:#020a02;color:#0f4;font-family:monospace;text-align:center;margin:0;padding:8px}
button{background:#031503;color:#0f4;border:1px solid #0f4;padding:8px 14px;border-radius:20px;font-weight:bold}
button.active{background:#0f4;color:#000}#radar{width:min(92vw,400px);border:2px solid #0f4;border-radius:50%;background:#000}
#list{text-align:left;max-width:400px;margin:10px auto;background:#031503;padding:10px;border:1px solid #0f4;border-radius:10px}
</style></head><body><h2>ESP32 RADAR</h2>
<div><button id="b50" onclick="setRange(50)">50m</button> <button id="b100" onclick="setRange(100)">100m</button> <button id="b150" class="active" onclick="setRange(150)">150m</button></div>
<canvas id="radar" width="400" height="400"></canvas><div id="status">Se scaneaza...</div><div id="list"></div>
<script>const c=document.getElementById('radar'),x=c.getContext('2d'),cx=200,cy=200,R=190;let s=0,t=[],mr=150;
function setRange(r){mr=r;document.querySelectorAll('button').forEach(b=>b.classList.remove('active'));document.getElementById('b'+r).classList.add('active');fetch('/range?val='+r);}
function draw(){x.clearRect(0,0,400,400);[25,50,75,100,125,150].forEach(d=>{if(d>mr)return;let r=d/mr*R;x.strokeStyle='rgba(0,255,65,.5)';x.beginPath();x.arc(cx,cy,r,0,6.29);x.stroke();x.fillStyle='#0f4';x.fillText(d+'m',cx+6,cy-r+11);});
x.beginPath();x.moveTo(cx-R,cy);x.lineTo(cx+R,cy);x.moveTo(cx,cy-R);x.lineTo(cx,cy+R);x.stroke();s+=0.05;if(s>6.29)s=0;
x.strokeStyle='#0f4';x.beginPath();x.moveTo(cx,cy);x.lineTo(cx+Math.cos(s)*R,cy+Math.sin(s)*R);x.stroke();
t.forEach(o=>{let r=Math.min(o.dist/mr*R,R);if(r<28)r=28;a=o.ang*Math.PI/180,px=cx+Math.cos(a)*r,py=cy+Math.sin(a)*r,col=o.dist<25?'#f44':o.dist<75?'#fa0':'#0f4';x.fillStyle=col;x.beginPath();x.arc(px,py,3,0,6.29);x.fill();x.fillStyle='white';x.fillText(o.ssid,px+9,py-5);});
x.fillStyle='#fff';x.beginPath();x.arc(cx,cy,5,0,6.29);x.fill();requestAnimationFrame(draw);}
async function fetchData(){try{let r=await fetch('/data'),j=await r.json();t=j.targets;mr=j.range;document.getElementById('status').innerHTML='Detectate: <b>'+t.length+'</b> | '+new Date().toLocaleTimeString();let h='';[...t].sort((a,b)=>a.dist-b.dist).forEach(o=>{h+='<div><b>'+o.ssid+'</b> '+o.dist.toFixed(1)+'m - '+o.rssi+' dBm CH '+o.ch+'</div>';});document.getElementById('list').innerHTML=h;}catch(e){}}
setInterval(fetchData,3000);fetchData();draw();</script></body></html>
)rawliteral";

void drawRadarFrame() {
  // ===== LAYOUT ECRAN 170x320 =====
  //  y  0 -  35 : titlu + raza
  //  y 37 - 173 : radar (centru 105, raza 68)
  //  y 180 - 310 : lista detalii tinte
  //  y 310 - 320 : footer butoane
  // Anti-flicker: desenam totul in sprite, nu direct pe lcd, si il punem o data pe ecran
  spr.fillScreen(C_BLACK);
  spr.setTextColor(C_GREEN, C_BLACK);
  spr.setTextSize(2);
  spr.setCursor(10, 2);
  spr.println("WIFI RADAR");
  spr.setTextSize(1);
  spr.setCursor(10, 22);
  spr.print("Raza: ");
  spr.print(maxRange);
  spr.print("m  Tinte: ");
  spr.print(targetCount);
  int ranges[6] = {25, 50, 75, 100, 125, 150};
  for (int i = 0; i < 6; i++) {
    if (ranges[i] > maxRange) continue;
    int r = (ranges[i] * RADAR_R) / maxRange;
    spr.drawCircle(RADAR_CX, RADAR_CY, r, C_GREEN);
  }
  spr.drawLine(RADAR_CX - RADAR_R, RADAR_CY, RADAR_CX + RADAR_R, RADAR_CY, C_GREEN);
  spr.drawLine(RADAR_CX, RADAR_CY - RADAR_R, RADAR_CX, RADAR_CY + RADAR_R, C_GREEN);
  sweepAngle += 0.25;
  if (sweepAngle > 6.283) sweepAngle = 0;
  int sx = RADAR_CX + cos(sweepAngle) * RADAR_R;
  int sy = RADAR_CY + sin(sweepAngle) * RADAR_R;
  spr.drawLine(RADAR_CX, RADAR_CY, sx, sy, C_CYAN);
  for (int i = 0; i < targetCount; i++) {
    float r = (targets[i].dist * RADAR_R) / maxRange;
    if (r > RADAR_R) r = RADAR_R;
    // Doar punctul, nu cercul: daca e prea aproape de centru, il punem la 14px,
    // ca sa nu stea sub punctul alb din centru. Cercurile raman neschimbate.
    if (r < 14) r = 14;
    float a = targets[i].angle * 0.0174533;
    int x = RADAR_CX + cos(a) * r;
    int y = RADAR_CY + sin(a) * r;
    uint16_t col = C_GREEN;
    if (targets[i].dist < 25) col = C_RED;
    else if (targets[i].dist < 75) col = C_YELLOW;
    // Punct mai mic (raza 2, nu 3) si fara cercul exterior de 5 care le facea sa se suprapuna
    spr.fillCircle(x, y, 2, col);
  }
  spr.fillCircle(RADAR_CX, RADAR_CY, 3, C_WHITE);

  // ===== DETALII JOS - incap acum 5 tinte, fiecare pe 2 randuri =====
  //  Linie separatoare intre radar si lista
  spr.drawLine(0, 178, 170, 178, C_GREEN);
  spr.setTextColor(C_GREEN, C_BLACK);
  spr.setTextSize(1);
  spr.setCursor(5, 183);
  spr.print("TOP TINTE :");

  spr.setTextColor(C_WHITE, C_BLACK);
  if (targetCount > 0) {
    // Sortare dupa distanta: cea mai aproape prima
    int idx[12];
    for (int i = 0; i < targetCount; i++) idx[i] = i;
    for (int i = 0; i < targetCount - 1; i++)
      for (int j = i + 1; j < targetCount; j++)
        if (targets[idx[j]].dist < targets[idx[i]].dist) { int t = idx[i]; idx[i] = idx[j]; idx[j] = t; }
    // Afisam 5 tinte, cate 2 randuri fiecare = 10 randuri x 11px = 110px
    //  y start 198, pas 22px (11px randul 1 + 11px randul 2)
    for (int k = 0; k < targetCount && k < 5; k++) {
      int ii = idx[k];
      int y = 198 + k * 22;
      // Randul 1: nr. + SSID + distanta + RSSI
      spr.setCursor(5, y);
      spr.print(k + 1);
      spr.print(". ");
      spr.print(targets[ii].ssid);
      spr.print(" ");
      spr.print(targets[ii].dist, 0);
      spr.print("m ");
      spr.print(targets[ii].rssi);
      spr.print("dBm");
      // Randul 2: MAC + canal, cu culoare gri-verde
      spr.setTextColor(C_CYAN, C_BLACK);
      spr.setCursor(15, y + 11);
      spr.print(targets[ii].mac);
      spr.print(" CH");
      spr.print(targets[ii].channel);
      spr.setTextColor(C_WHITE, C_BLACK);
    }
  } else {
    spr.setCursor(5, 210);
    spr.print("Se scaneaza...");
  }
  // Footer butoane
  spr.setTextColor(C_GREEN, C_BLACK);
  spr.setCursor(5, 310);
  spr.print("BUTON raza<<  >>BUTON scan");
  // Punem cadrul complet pe ecran dintr-o data, fara flicker
  spr.pushSprite(0, 0);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  pinMode(15, OUTPUT);
  pinMode(38, OUTPUT);
  pinMode(BTN_IO0, INPUT_PULLUP);
  pinMode(BTN_IO14, INPUT_PULLUP);
  lcd.begin();
  lcd.setRotation(0);
  // Cream sprite-ul de 170x320 pentru anti-flicker, inainte de primul desen
  spr.createSprite(170, 320);
  digitalWrite(15, HIGH);
  digitalWrite(38, HIGH);
  lcd.fillScreen(C_RED); delay(600);
  lcd.fillScreen(C_GREEN); delay(600);
  lcd.fillScreen(C_CYAN); delay(600);
  lcd.fillScreen(C_BLACK);
  lcd.setTextColor(C_GREEN, C_BLACK);
  lcd.setTextSize(2);
  lcd.setCursor(20, 130);
  lcd.println("RADAR");
  lcd.setTextSize(1);
  lcd.setCursor(20, 155);
  lcd.println("Pornire WiFi...");
  delay(500);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP("WIFI-RADAR", "12345678");
  WiFi.setSleep(false);
  dnsServer.start(DNS_PORT, "*", apIP);
  server.on("/", [](){ server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/data", handleData);
  server.on("/range", [](){
    if (server.hasArg("val")) maxRange = server.arg("val").toInt();
    if (maxRange != 50 && maxRange != 100 && maxRange != 150) maxRange = 150;
    server.send(200, "text/plain", "OK");
  });
  server.on("/generate_204", [](){ server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/hotspot-detect.html", [](){ server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/connecttest.txt", [](){ server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/fwlink", [](){ server.send_P(200, "text/html", INDEX_HTML); });
  server.onNotFound([](){ server.send_P(200, "text/html", INDEX_HTML); });
  server.begin();
  Serial.println("AP: ESP32-RADAR / 12345678");
  Serial.println("Web: http://192.168.0.253");
  // Pornim task-ul de scanare pe core 0. Prima scanare o face el singur.
  xTaskCreatePinnedToCore(scanTask, "scanTask", 4096, NULL, 1, NULL, 0);
}

void loop() {
  server.handleClient();
  dnsServer.processNextRequest();
  digitalWrite(15, HIGH);
  digitalWrite(38, HIGH);
  static unsigned long lastBtn0 = 0;
  if (digitalRead(BTN_IO0) == LOW && millis() - lastBtn0 > 300) {
    lastBtn0 = millis();
    if (maxRange == 50) maxRange = 100;
    else if (maxRange == 100) maxRange = 150;
    else maxRange = 50;
  }
  static unsigned long lastBtn14 = 0;
  if (digitalRead(BTN_IO14) == LOW && millis() - lastBtn14 > 300) {
    lastBtn14 = millis();
    scanCerut = true; // nu scanam aici, doar cerem task-ului sa scaneze acum
  }
  // Scanul automat la 8 secunde il face scanTask() pe core 0, nu loop().
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw > 120) {
    lastDraw = millis();
    drawRadarFrame();
  }
  delay(10);
}