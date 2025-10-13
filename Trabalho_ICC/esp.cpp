#include <WiFi.h>
#include <Stepper.h>

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

    if (currentLine.indexOf("GET /motor") >= 0) {
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
}