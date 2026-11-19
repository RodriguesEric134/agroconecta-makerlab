/*
  AgroConecta — Robô de Campo (Project-Based Maker Lab · Global Solution FIAP 2026.2)

  Evolução do carrinho autônomo: além de andar e desviar de obstáculos (ultrassônico),
  o robô agora monitora a lavoura com sensores ambientais e decide sozinho o que fazer.

  Sensores:  DHT22 (temperatura e umidade do ar) · umidade do solo · LDR (luminosidade)
             HC-SR04 (distância para desvio de obstáculos)
  Atuadores: 2 motores de passo (rodas) · servo da válvula de irrigação · LED RGB · buzzer · OLED
  IA:
    1. TinyML embarcado (modelo_tinyml.h): árvore de decisão treinada em Python que classifica
       a lavoura em NORMAL, IRRIGAR, ESTRESSE_TERMICO, RISCO_FUNGO ou RISCO_GEADA — roda no
       próprio ESP32, sem internet, e comanda as ações do robô.
    2. LLM na nuvem (n8n + GPT-4o mini): quando a classificação muda, o robô envia a telemetria;
       a IA devolve uma recomendação curta (exibida no OLED) e avisa o produtor no Telegram.
  Falhas tratadas: sensor com leitura inválida, Wi-Fi fora do ar (robô segue decidindo sozinho),
                   obstáculo no caminho e IA indisponível (recomendação local).

  Botões de teste (simulação): FALHA SENSOR (GPIO 19) e WI-FI ON/OFF (GPIO 4).
*/
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHTesp.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "modelo_tinyml.h"

// ----------------------------------------------------------------- configuração
const char* WIFI_SSID   = "Wokwi-GUEST";
const char* WIFI_SENHA  = "";
const char* URL_N8N     = "https://gsultimosemestreultimoano.app.n8n.cloud/webhook/robo-campo";
const char* ROBO_ID     = "robo-campo-01";

const uint32_t INTERVALO_LEITURA_MS   = 2000;    // lê os sensores ambientais
const uint32_t INTERVALO_HEARTBEAT_MS = 30000;   // envia telemetria mesmo sem mudança
const float    DIST_OBSTACULO_CM      = 25.0;    // abaixo disso, desvia
const float    SOLO_FECHA_VALVULA     = 40.0;    // histerese da irrigação (%)
const float    LUZ_NOITE              = 15.0;    // abaixo disso, modo noite (%)

// ----------------------------------------------------------------- pinos
const int PIN_DHT = 15, PIN_TRIG = 5, PIN_ECHO = 18, PIN_LDR = 34, PIN_SOLO = 35;
const int PIN_SERVO = 13, PIN_BUZZER = 14, PIN_LED_R = 25, PIN_LED_G = 26, PIN_LED_B = 27;
const int PIN_BTN_WIFI = 4, PIN_BTN_FALHA = 19;
const int PIN_ESQ_STEP = 32, PIN_ESQ_DIR = 33, PIN_DIR_STEP = 16, PIN_DIR_DIR = 17;

// ----------------------------------------------------------------- objetos
DHTesp dht;
Servo valvula;
Adafruit_SSD1306 oled(128, 64, &Wire, -1);
Eloquent::ML::Port::DecisionTree modelo;   // TinyML

// ----------------------------------------------------------------- estado
enum Modo { PATRULHA, DESVIANDO, PARADO_ALERTA, IRRIGANDO, NOITE, FALHA_SENSOR };
const char* NOME_MODO[] = { "PATRULHA", "DESVIANDO", "PARADO_ALERTA", "IRRIGANDO", "NOITE", "FALHA_SENSOR" };

struct Leitura { float temp, umidAr, solo, luz, dist; bool sensorOk; };
Leitura atual = { 0, 0, 0, 0, 400, false };
Modo modo = PATRULHA;
int classe = 0;
int classeEnviada = -1;
String acao = "Iniciando";
bool valvulaAberta = false;

// motores (passo não bloqueante)
enum Mov { PARADO, FRENTE, RE, GIRA };
Mov mov = PARADO;
uint32_t ultimoPassoUs = 0, intervaloPassoUs = 2500;
bool nivelPasso = false;
uint32_t fimManobraMs = 0;
int etapaDesvio = 0;

// botões de simulação
bool wifiHabilitado = true, falhaSimulada = false;
int ultimoBtnWifi = HIGH, ultimoBtnFalha = HIGH;

// rede (tarefa no núcleo 0 para não travar o robô)
SemaphoreHandle_t trava;
String payloadPendente = "";
String recomendacao = "Aguardando IA...";
String fonteIA = "-";
bool online = false;

uint32_t ultimaLeituraMs = 0, ultimoEnvioMs = 0, ultimoBipMs = 0;

// ----------------------------------------------------------------- utilidades
void led(bool r, bool g, bool b) {
  static int ultimo = -1;                 // só escreve nos pinos quando a cor muda
  int cor = (r << 2) | (g << 1) | b;
  if (cor == ultimo) return;
  ultimo = cor;
  digitalWrite(PIN_LED_R, r); digitalWrite(PIN_LED_G, g); digitalWrite(PIN_LED_B, b);
}

void bip(int freq, int ms) { tone(PIN_BUZZER, freq, ms); }

float lerDistancia() {
  digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  unsigned long us = pulseIn(PIN_ECHO, HIGH, 30000);
  return us == 0 ? 400.0 : us / 58.0;
}

void mover(Mov m) {
  mov = m;
  // FRENTE: as duas rodas para frente · RE: as duas para trás · GIRA: esquerda frente, direita trás
  digitalWrite(PIN_ESQ_DIR, (m == RE) ? LOW : HIGH);
  digitalWrite(PIN_DIR_DIR, (m == FRENTE) ? HIGH : LOW);
  intervaloPassoUs = (m == GIRA) ? 1800 : 2500;
}

void tickMotores() {
  if (mov == PARADO) return;
  uint32_t agora = micros();
  if (agora - ultimoPassoUs >= intervaloPassoUs) {
    ultimoPassoUs = agora;
    nivelPasso = !nivelPasso;
    digitalWrite(PIN_ESQ_STEP, nivelPasso);
    digitalWrite(PIN_DIR_STEP, nivelPasso);
  }
}

void abrirValvula(bool abrir) {
  if (abrir == valvulaAberta) return;
  valvula.write(abrir ? 90 : 0);
  valvulaAberta = abrir;
  Serial.printf("[ATUADOR] Valvula de irrigacao %s\n", abrir ? "ABERTA" : "FECHADA");
}

// ----------------------------------------------------------------- sensores
void lerSensores() {
  TempAndHumidity th = dht.getTempAndHumidity();
  atual.temp = th.temperature;
  atual.umidAr = th.humidity;
  atual.solo = analogRead(PIN_SOLO) * 100.0 / 4095.0;
  atual.luz = 100.0 - analogRead(PIN_LDR) * 100.0 / 4095.0;   // no módulo LDR, mais luz = tensão menor
  // Validação: leitura ausente ou fisicamente impossível = falha do sensor
  atual.sensorOk = !falhaSimulada && !isnan(atual.temp) && !isnan(atual.umidAr)
                   && atual.temp > -20 && atual.temp < 60 && atual.umidAr >= 0 && atual.umidAr <= 100;
}

// ----------------------------------------------------------------- decisão (autonomia)
void decidir() {
  if (!atual.sensorOk) {
    modo = FALHA_SENSOR; mover(PARADO); abrirValvula(false);
    acao = "Parou: falha no sensor DHT22";
    return;
  }
  float x[4] = { atual.temp, atual.umidAr, atual.solo, atual.luz };
  classe = modelo.predict(x);   // inferência TinyML no próprio ESP32

  if (modo == DESVIANDO) return;   // termina a manobra antes de mudar de comportamento

  switch (classe) {
    case 1:   // IRRIGAR
      if (atual.solo < SOLO_FECHA_VALVULA) {
        modo = IRRIGANDO; mover(PARADO); abrirValvula(true);
        acao = "Parou e abriu a valvula de irrigacao";
      }
      break;
    case 2:   // ESTRESSE_TERMICO
      modo = PARADO_ALERTA; mover(PARADO); abrirValvula(false);
      acao = "Parou e emitiu alerta de calor";
      break;
    case 3:   // RISCO_FUNGO
      modo = PATRULHA; mover(FRENTE); abrirValvula(false);
      acao = "Segue patrulha e marca area com risco de fungo";
      break;
    case 4:   // RISCO_GEADA
      modo = PARADO_ALERTA; mover(PARADO); abrirValvula(false);
      acao = "Parou e soou alarme de geada";
      break;
    default:  // NORMAL
      if (valvulaAberta && atual.solo < SOLO_FECHA_VALVULA) break;   // histerese: termina de irrigar
      abrirValvula(false);
      if (atual.luz < LUZ_NOITE) { modo = NOITE; mover(PARADO); acao = "Modo noite: patrulha pausada"; }
      else { modo = PATRULHA; mover(FRENTE); acao = "Patrulhando a lavoura"; }
  }
}

void sinalizar() {
  // LED: verde normal · azul irrigando · laranja calor · roxo fungo · branco geada · vermelho piscando falha
  bool pisca = (millis() / 400) % 2;
  switch (modo) {
    case FALHA_SENSOR: led(pisca, false, false); break;
    case IRRIGANDO:    led(false, false, true); break;
    case NOITE:        led(false, false, pisca); break;
    case DESVIANDO:    led(true, true, false); break;
    default:
      if (classe == 2) led(true, pisca, false);
      else if (classe == 3) led(true, false, true);
      else if (classe == 4) led(true, true, true);
      else led(false, true, false);
  }
  // Buzzer: bips periódicos nos alertas críticos
  if (millis() - ultimoBipMs > 5000) {
    if (classe == 4 && modo == PARADO_ALERTA) { bip(1800, 400); ultimoBipMs = millis(); }
    else if (classe == 2 && modo == PARADO_ALERTA) { bip(1200, 150); ultimoBipMs = millis(); }
    else if (modo == FALHA_SENSOR) { bip(400, 200); ultimoBipMs = millis(); }
  }
}

// ----------------------------------------------------------------- desvio de obstáculo
void controlarDesvio() {
  if (modo == DESVIANDO) {
    if (millis() < fimManobraMs) return;
    if (etapaDesvio == 1) { mover(GIRA); fimManobraMs = millis() + 900; etapaDesvio = 2; acao = "Girando para desviar"; }
    else { etapaDesvio = 0; modo = PATRULHA; mover(FRENTE); acao = "Desvio concluido"; }
    return;
  }
  if (modo == PATRULHA && atual.dist < DIST_OBSTACULO_CM) {
    modo = DESVIANDO; etapaDesvio = 1; mover(RE); fimManobraMs = millis() + 600;
    acao = "Obstaculo a " + String(atual.dist, 0) + " cm: dando re";
    bip(900, 120);
    Serial.printf("[DESVIO] Obstaculo a %.0f cm\n", atual.dist);
  }
}

// ----------------------------------------------------------------- rede
void prepararEnvio(bool evento) {
  JsonDocument doc;
  doc["robo_id"] = ROBO_ID;
  if (atual.sensorOk) {                    // leitura inválida vai como null (NaN não é JSON válido)
    doc["temp_ar"] = round(atual.temp * 10) / 10.0;
    doc["umid_ar"] = round(atual.umidAr);
  } else {
    doc["temp_ar"] = nullptr;
    doc["umid_ar"] = nullptr;
  }
  doc["umid_solo"] = round(atual.solo * 10) / 10.0;
  doc["luz"] = round(atual.luz * 10) / 10.0;
  doc["distancia_cm"] = round(atual.dist);
  doc["classe"] = atual.sensorOk ? modelo.idxToLabel(classe) : "SENSOR_INVALIDO";
  doc["acao"] = acao;
  doc["modo"] = NOME_MODO[modo];
  doc["sensor_ok"] = atual.sensorOk;
  doc["irrigando"] = valvulaAberta;
  doc["evento"] = evento;
  String corpo; serializeJson(doc, corpo);
  xSemaphoreTake(trava, portMAX_DELAY);
  payloadPendente = corpo;
  xSemaphoreGive(trava);
}

void tarefaRede(void*) {
  WiFiClientSecure cliente;
  cliente.setInsecure();   // simulação acadêmica: sem validação de certificado
  for (;;) {
    if (wifiHabilitado && WiFi.status() != WL_CONNECTED) {
      WiFi.begin(WIFI_SSID, WIFI_SENHA, 6);
      Serial.println("[REDE] Conectando ao Wi-Fi Wokwi-GUEST...");
      for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; i++) vTaskDelay(pdMS_TO_TICKS(250));
      if (WiFi.status() == WL_CONNECTED) Serial.printf("[REDE] Wi-Fi conectado, IP %s\n", WiFi.localIP().toString().c_str());
      else Serial.println("[REDE] Wi-Fi indisponivel, nova tentativa em seguida.");
    }
    online = wifiHabilitado && WiFi.status() == WL_CONNECTED;

    String corpo;
    xSemaphoreTake(trava, portMAX_DELAY);
    corpo = payloadPendente; payloadPendente = "";
    xSemaphoreGive(trava);

    if (corpo.length() && online) {
      HTTPClient http;
      http.setTimeout(15000);
      http.begin(cliente, URL_N8N);
      http.addHeader("Content-Type", "application/json");
      Serial.println("[REDE] Enviando telemetria ao n8n (HTTPS)...");
      int codigo = http.POST(corpo);
      if (codigo == 200) {
        JsonDocument resp;
        if (!deserializeJson(resp, http.getString())) {
          xSemaphoreTake(trava, portMAX_DELAY);
          recomendacao = resp["recomendacao"] | "Sem recomendacao";
          fonteIA = resp["fonte"] | "-";
          xSemaphoreGive(trava);
        }
        Serial.printf("[NUVEM] Telemetria enviada. IA (%s): %s\n", fonteIA.c_str(), recomendacao.c_str());
      } else {
        Serial.printf("[NUVEM] Falha no envio (HTTP %d). Robo segue com decisao local.\n", codigo);
      }
      http.end();
    } else if (corpo.length()) {
      Serial.println("[NUVEM] Offline: telemetria nao enviada, robo segue com decisao local (TinyML).");
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
}

// ----------------------------------------------------------------- display
void desenharTela() {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  oled.printf("AgroConecta   %s\n", online ? "ONLINE" : "OFFLN");
  if (atual.sensorOk) oled.printf("T %.1fC  Ar %.0f%%\n", atual.temp, atual.umidAr);
  else oled.println("DHT22: FALHA!");
  oled.printf("Solo %.0f%% Luz %.0f%%\n", atual.solo, atual.luz);
  oled.printf("Dist %.0fcm\n", atual.dist);
  oled.printf("IA: %s\n", atual.sensorOk ? modelo.idxToLabel(classe) : "-");
  oled.printf("%s\n", NOME_MODO[modo]);
  xSemaphoreTake(trava, portMAX_DELAY);
  String rec = online ? recomendacao : String("Offline: decisao local");
  xSemaphoreGive(trava);
  oled.println(rec.substring(0, 21));
  if (rec.length() > 21) oled.print(rec.substring(21, 42));
  oled.display();
}

// ----------------------------------------------------------------- botões
void lerBotoes() {
  int bw = digitalRead(PIN_BTN_WIFI), bf = digitalRead(PIN_BTN_FALHA);
  if (bw == LOW && ultimoBtnWifi == HIGH) {
    wifiHabilitado = !wifiHabilitado;
    if (!wifiHabilitado) WiFi.disconnect();
    Serial.printf("[TESTE] Wi-Fi %s\n", wifiHabilitado ? "HABILITADO" : "DESLIGADO (simulando queda)");
  }
  if (bf == LOW && ultimoBtnFalha == HIGH) {
    falhaSimulada = !falhaSimulada;
    Serial.printf("[TESTE] Falha do sensor %s\n", falhaSimulada ? "ATIVADA" : "DESATIVADA");
  }
  ultimoBtnWifi = bw; ultimoBtnFalha = bf;
}

// ----------------------------------------------------------------- setup / loop
void setup() {
  Serial.begin(115200);
  pinMode(PIN_TRIG, OUTPUT); pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_LED_R, OUTPUT); pinMode(PIN_LED_G, OUTPUT); pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_BTN_WIFI, INPUT_PULLUP); pinMode(PIN_BTN_FALHA, INPUT_PULLUP);
  pinMode(PIN_ESQ_STEP, OUTPUT); pinMode(PIN_ESQ_DIR, OUTPUT);
  pinMode(PIN_DIR_STEP, OUTPUT); pinMode(PIN_DIR_DIR, OUTPUT);
  dht.setup(PIN_DHT, DHTesp::DHT22);
  valvula.attach(PIN_SERVO, 500, 2400);
  valvula.write(0);
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("[ERRO] OLED nao encontrado");
  trava = xSemaphoreCreateMutex();
  WiFi.mode(WIFI_STA);
  // O handshake TLS (HTTPS) leva vários segundos de CPU sem ceder o núcleo 0; sem isto o watchdog
  // do núcleo 0 reinicia o ESP32 no meio do envio. O loop de controle (núcleo 1) segue protegido.
  disableCore0WDT();
  xTaskCreatePinnedToCore(tarefaRede, "rede", 12288, nullptr, 1, nullptr, 0);
  led(false, true, false);
  bip(1000, 150);
  Serial.println("[ROBO] AgroConecta Robo de Campo iniciado. Modelo TinyML carregado.");
  mover(FRENTE);
}

void loop() {
  tickMotores();
  lerBotoes();

  static uint32_t ultimaDistMs = 0;
  if (millis() - ultimaDistMs > 150) { ultimaDistMs = millis(); atual.dist = lerDistancia(); controlarDesvio(); }

  if (millis() - ultimaLeituraMs >= INTERVALO_LEITURA_MS) {
    ultimaLeituraMs = millis();
    lerSensores();
    decidir();
    int codigoEstado = atual.sensorOk ? classe : 99;
    bool mudou = codigoEstado != classeEnviada;
    if (mudou || millis() - ultimoEnvioMs > INTERVALO_HEARTBEAT_MS) {
      prepararEnvio(mudou);
      classeEnviada = codigoEstado;
      ultimoEnvioMs = millis();
    }
    Serial.printf("[SENSORES] T=%.1fC UA=%.0f%% Solo=%.0f%% Luz=%.0f%% Dist=%.0fcm | TinyML=%s | Modo=%s | %s\n",
                  atual.temp, atual.umidAr, atual.solo, atual.luz, atual.dist,
                  atual.sensorOk ? modelo.idxToLabel(classe) : "-", NOME_MODO[modo], acao.c_str());
    desenharTela();
  }
  sinalizar();
  delay(1);   // libera a CPU (o passo dos motores é de 1,8–2,5 ms, então 1 ms não atrapalha)
}
