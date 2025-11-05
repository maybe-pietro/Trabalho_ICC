#include <WiFi.h>
#include <Stepper.h>
#include <DS1302.h>
#include <EEPROM.h>

// --- CONFIGURAÇÃO DA REDE WI-FI ---
const char* ssid = "TP-LINK_0372";
const char* password = "72201671";

// --- CONFIGURAÇÃO DO MOTOR DE PASSO ---
const int passosPorRevolucao = 2048;
const int velocidadeMotorRPM = 15;
Stepper meuMotor(passosPorRevolucao, 13, 14, 12, 27);

WiFiServer server(80);

// Variáveis para controlar o motor
long passosRestantes = 0;
long passosTotais = 0;
unsigned long ultimoPasso = 0;
const int intervaloPasso = 60000 / (velocidadeMotorRPM * passosPorRevolucao); 

// --- CONFIG DS1302 (ajuste pinos se necessário) ---
#define DS1302_SCLK 2
#define DS1302_IO   3
#define DS1302_CE   4
DS1302 rtc(DS1302_SCLK, DS1302_IO, DS1302_CE);

// --- Schedules ---
struct Schedule {
  bool enabled;
  uint8_t hour;
  uint8_t minute;
  uint16_t durationSec; // tempo em segundos
  bool triggeredToday;
};

Schedule schedules[3];
int lastDay = -1;
const int EEPROM_SIZE = 64;

// Helper to convert seconds to motor steps
void startMotorForSeconds(uint16_t tempoEmSegundos) {
  Serial.printf("Iniciando motor por %d segundos\n", tempoEmSegundos);
  float revolucoes = (float)velocidadeMotorRPM / 60.0 * (float)tempoEmSegundos;
  passosTotais = round(revolucoes * passosPorRevolucao);
  passosRestantes = passosTotais;
}

// EEPROM helpers: layout: for i in 0..2 -> enabled(1), hour(1), minute(1), duration(2)
void saveSchedules() {
  int addr = 0;
  for (int i=0;i<3;i++){
    EEPROM.write(addr++, schedules[i].enabled ? 1 : 0);
    EEPROM.write(addr++, schedules[i].hour);
    EEPROM.write(addr++, schedules[i].minute);
    EEPROM.write(addr++, schedules[i].durationSec & 0xFF);
    EEPROM.write(addr++, (schedules[i].durationSec >> 8) & 0xFF);
  }
  EEPROM.commit();
}

void loadSchedules() {
  int addr = 0;
  for (int i=0;i<3;i++){
    schedules[i].enabled = EEPROM.read(addr++) == 1;
    schedules[i].hour = EEPROM.read(addr++);
    schedules[i].minute = EEPROM.read(addr++);
    uint16_t lo = EEPROM.read(addr++);
    uint16_t hi = EEPROM.read(addr++);
    schedules[i].durationSec = (hi << 8) | lo;
    schedules[i].triggeredToday = false;
  }
}

void setup() {
  Serial.begin(115200);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando ao WiFi...");
  }
  Serial.println("Conectado! Endereco IP: ");
  Serial.println(WiFi.localIP());

  server.begin();
  meuMotor.setSpeed(velocidadeMotorRPM);
  // Inicializa EEPROM e carrega schedules
  EEPROM.begin(EEPROM_SIZE);
  loadSchedules();
  // Inicializa RTC
  rtc.halt(false);
  rtc.writeProtect(false);
  // guarda dia atual
  lastDay = rtc.getDate().day;
}

void loop() {
  // AQUI O ESP32 ATENDE AS REQUISIÇÕES DO NAVEGADOR
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        currentLine += c;
        if (c == '\n') {
          break;
        }
      }
    }

    // --- endpoints ---
    if (currentLine.indexOf("GET /setSchedule") >= 0) {
      // Exemplo: /setSchedule?slot=0&hour=10&minute=30&duration=10
      auto getParam = [&](const String &key)->String{
        int p = currentLine.indexOf((String)key + "=");
        if (p < 0) return String();
        p = p + key.length() + 1;
        int e = currentLine.indexOf(' ', p);
        if (e == -1) e = currentLine.length();
        String v = currentLine.substring(p, e);
        int amp = v.indexOf('&'); if (amp!=-1) v = v.substring(0, amp);
        return v;
      };
      String sSlot = getParam("slot");
      String sHour = getParam("hour");
      String sMinute = getParam("minute");
      String sDuration = getParam("duration");
      int slot = sSlot.length() ? sSlot.toInt() : -1;
      int hour = sHour.length() ? sHour.toInt() : -1;
      int minute = sMinute.length() ? sMinute.toInt() : -1;
      int duration = sDuration.length() ? sDuration.toInt() : 0;
      if (slot>=0 && slot<3 && hour>=0 && hour<24 && minute>=0 && minute<60 && duration>0 && duration<=600) {
        schedules[slot].enabled = true;
        schedules[slot].hour = hour;
        schedules[slot].minute = minute;
        schedules[slot].durationSec = duration;
        schedules[slot].triggeredToday = false;
        saveSchedules();
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:application/json");
        client.println("Access-Control-Allow-Origin: *");
        client.println("Connection: close");
        client.println();
        {
          String resp = String("{\"success\":true,\"slot\":") + slot + String("}\r\n");
          client.print(resp);
        }
        client.stop();
      } else {
        client.println("HTTP/1.1 400 Bad Request");
        client.println("Content-type:text/plain");
        client.println("Access-Control-Allow-Origin: *");
        client.println("Connection: close");
        client.println();
        client.println("Parâmetros inválidos. Use slot=0-2, hour=0-23, minute=0-59, duration=1-600");
        client.stop();
      }
    } else if (currentLine.indexOf("GET /getSchedules") >= 0) {
      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:application/json");
      client.println("Access-Control-Allow-Origin: *");
      client.println("Connection: close");
      client.println();
      String json = "[";
      for (int i=0;i<3;i++){
        if (i) json += ",";
        json += "{";
        json += String("\"enabled\":") + (schedules[i].enabled?"true":"false") + ",";
        json += String("\"hour\":") + schedules[i].hour + ",";
        json += String("\"minute\":") + schedules[i].minute + ",";
        json += String("\"duration\":") + schedules[i].durationSec;
        json += "}";
      }
      json += "]";
      client.println(json);
      client.stop();
    } else if (currentLine.indexOf("GET /clearSchedules") >= 0) {
      for (int i=0;i<3;i++) schedules[i] = {false,0,0,0,false};
      saveSchedules();
      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/plain");
      client.println("Access-Control-Allow-Origin: *");
      client.println("Connection: close");
      client.println();
      client.println("Schedules cleared");
      client.stop();
    } else if (currentLine.indexOf("GET /motor") >= 0) {
      // Validação do parâmetro tempo
      int tempoPos = currentLine.indexOf("tempo=");
      bool parametroValido = false;
      float tempoEmSegundos = 0;
      if (tempoPos > 0) {
        tempoPos += 6;
        int tempoEnd = currentLine.indexOf(' ', tempoPos);
        if (tempoEnd == -1) tempoEnd = currentLine.length();
        String tempoStr = currentLine.substring(tempoPos, tempoEnd);
        tempoEmSegundos = tempoStr.toFloat();
        if (tempoEmSegundos > 0 && tempoEmSegundos <= 30) {
          parametroValido = true;
        }
      }
      if (parametroValido) {
        // Resposta de sucesso
        client.println("HTTP/1.1 200 OK");
        client.println("Content-type:text/plain");
        client.println("Access-Control-Allow-Origin: *");
        client.println("Connection: close");
        client.println();
        client.println("Comando recebido com sucesso.");
        client.stop();
        // Processa o comando para mover o motor, mas só DEPOIS de responder
        Serial.print("Comando recebido: Motor de passo ira rodar por ");
        Serial.print(tempoEmSegundos);
        Serial.println(" segundos.");
        float revolucoes = (float)velocidadeMotorRPM / 60.0 * tempoEmSegundos;
        passosTotais = round(revolucoes * passosPorRevolucao);
        passosRestantes = passosTotais;
      } else {
        // Resposta de erro de parâmetro
        client.println("HTTP/1.1 400 Bad Request");
        client.println("Content-type:text/plain");
        client.println("Access-Control-Allow-Origin: *");
        client.println("Connection: close");
        client.println();
        client.println("Parâmetro 'tempo' inválido ou ausente. Use /motor?tempo=1-30");
        client.stop();
      }

    } else {
      // Responde para qualquer outra requisição (como a do IP puro)
      client.println("HTTP/1.1 200 OK");
      client.println("Content-type:text/plain");
      client.println("Access-Control-Allow-Origin: *");
      client.println("Connection: close");
      client.println();
      client.println("API do Motor de Passo está rodando. Use /motor?tempo=1-30 para enviar um comando.");
      client.stop(); // Paramos a conexão aqui também
    }
  }

  // AQUI O MOTOR É EXECUTADO DE FORMA NÃO-BLOQUEANTE
  if (passosRestantes > 0 && millis() - ultimoPasso >= intervaloPasso) {
    int direcao = (passosTotais > 0) ? 1 : -1;
    meuMotor.step(direcao);
    passosRestantes--;
    ultimoPasso = millis();
  }

  // --- Scheduler: verifica RTC e dispara schedules ---
  // Lê hora atual a partir do RTC (string) e faz parsing
  String timeStr = rtc.getTimeStr(); // esperado HH:MM:SS
  String dateStr = rtc.getDateStr(); // esperado DD/MM/YY
  int curHour = 0, curMinute = 0, curSecond = 0;
  int curDay = lastDay;
  if (timeStr.length() >= 8) {
    curHour = timeStr.substring(0,2).toInt();
    curMinute = timeStr.substring(3,5).toInt();
    curSecond = timeStr.substring(6,8).toInt();
  }
  if (dateStr.length() >= 2) {
    curDay = dateStr.substring(0,2).toInt();
  }
  // se dia mudou, reset triggeredToday
  if (curDay != lastDay) {
    for (int i=0;i<3;i++) schedules[i].triggeredToday = false;
    lastDay = curDay;
  }
  // verifica cada schedule: quando minuto e segundo são zero (ou segundo==0) e matches schedule minute/hour and not triggeredToday
  for (int i=0;i<3;i++){
    if (!schedules[i].enabled) continue;
    if (!schedules[i].triggeredToday && curHour == schedules[i].hour && curMinute == schedules[i].minute && curSecond == 0) {
      // dispara motor
      startMotorForSeconds(schedules[i].durationSec);
      schedules[i].triggeredToday = true;
    }
  }
}