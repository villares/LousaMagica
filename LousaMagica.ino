#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
#include <Hash.h>
#include <Wire.h>
#include <Adafruit_ADS1015.h>

// Conexões no NodeMCU/WeMos
static const uint8_t tilt_pin = D0;
// ADS1115 CLK: D1
// ADS1115 SDA: D2
// Potenciômetro Opacidade: A0

// Configurações
static const char ssid[] = "seu-ssid";
static const char senha[] = "sua-senha";
static const char nome[] = "lousa-magica";
static const int porta = 81;
static const unsigned int intervaloAmostra = 50;  // Em milisegundos

WebSocketsServer webSocket = WebSocketsServer(porta);

// Pode ser necessário recalibrar conforme tensão máxima
// aplicada nas portas analógicas.
// Tensão máxima suportada: +/- 6.144V / resolução 1 bit: 0.1875mV
static const int ads_max_valor = 3.30 / 0.1875 * 1000; 
Adafruit_ADS1115 ads;

uint64_t ultimaAmostra = 0;
bool conectado = false;

void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] desconectado\n", num);
      conectado = false;
      break;
    case WStype_CONNECTED:
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] %s conectado\n", num, WiFi.localIP().toString().c_str());
      conectado = true;
      break;
  }
}

void setup() {
  Serial.begin(74880);
  Serial.println();
  Serial.println();
  Serial.println("Iniciando Lousa Mágica");

  pinMode(tilt_pin, INPUT);

  ads.setGain(GAIN_TWOTHIRDS);
  ads.begin();

  Serial.print("Iniciando access point... ");
  Serial.println(WiFi.softAP(nome) ? "ok" : "erro!");

  Serial.print("Endereço IP do access point... ");
  Serial.println(WiFi.softAPIP());  
  
  Serial.print("Conectando a ");
  Serial.print(ssid);
  WiFi.begin(ssid, senha);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println(" ok");

  if (MDNS.begin(nome)) {
    Serial.println("MDNS iniciado");
  }
  else {
    Serial.println("MDNS falhou");
  }

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
  MDNS.addService("ws", "tcp", 81);

  Serial.printf("WebService disponível em ws://%s.local:%d ou ws://%s:%d\n",
                nome, porta, WiFi.localIP().toString().c_str(), porta);
  Serial.println("Aguardando conexões");
}

uint16_t readADC(uint8_t channel, uint16_t max_valor) {
  // Cuidado! constraint é uma macro.
  int16_t value = ads.readADC_SingleEnded(channel);
  return map(constrain(value, 0, ads_max_valor), 0, ads_max_valor, 0, max_valor);
}

void loop() {
  webSocket.loop();
  if (conectado) {
    uint64_t agora = millis();
    if (agora - ultimaAmostra > intervaloAmostra) {
      ultimaAmostra = agora;

      bool tilt = digitalRead(tilt_pin);
      uint16_t X = readADC(0, 1023);
      uint16_t Y = readADC(1, 1023);
      uint16_t tam = readADC(2, 100);
      uint16_t sat = readADC(3, 255);
      int opa = map(analogRead(0), 0, 1023, 0, 255);

      String dado = String(tilt) + " " +
                    String(X) + " " + String(Y) + " " +
                    String(tam) + " " + String(sat) + " " + String(opa);
      webSocket.broadcastTXT(dado);
    }
  }
}

