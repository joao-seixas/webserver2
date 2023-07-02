#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebSrv.h>
#include <WebSocketsServer.h>
#include <FS.h> // TODO - mudar o sistema de arquivos para o LittleFS

DNSServer dnsServer;
AsyncWebServer server(80); // declara a porta 80 para o WebServer
WebSocketsServer webSocket = WebSocketsServer(81); // declara a porta 81 para o WebSocketServer

String leds = "000"; // string utilizada para guardar os estados dos leds (0 apagado, 1 aceso)

// função para aplicar os estados dos leds nas portas correspondentes
// TODO - melhorar a lógica para remover as condicionais (converter o char para int)
void changeLedStatus() {
  leds.charAt(0) == '0' ? digitalWrite(D0, LOW) : digitalWrite(D0, HIGH); 
  leds.charAt(1) == '0' ? digitalWrite(D1, LOW) : digitalWrite(D1, HIGH); 
  leds.charAt(2) == '0' ? digitalWrite(D2, LOW) : digitalWrite(D2, HIGH); 
  leds.charAt(3) == '0' ? digitalWrite(D3, LOW) : digitalWrite(D3, HIGH); 
  leds.charAt(4) == '0' ? digitalWrite(D5, LOW) : digitalWrite(D5, HIGH); 
}

// função que trata os eventos do WebSocketServer
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  
  switch(type) {
    
// em caso de deconexão, apenas depura uma mensagem de desconexão
    case WStype_DISCONNECTED:
      Serial.printf("[%u] Desconectado!\n", num);
      break;

// assim que recebe uma nova conexão, envia o estado dos leds para o novo cliente
// TODO - verificar a consistência da informação recebida, para evitar erros
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        Serial.printf("[%u] Conexão estabelecida com %d.%d.%d.%d url: %s\n", num, ip[0], ip[1], ip[2], ip[3], payload);
        Serial.print("Enviando leds: ");
        Serial.println(leds);
        webSocket.sendTXT(num, leds); // envia o estado dos leds para o novo cliente (num)
      }
      break;

// quando recebe uma solicitação de alteração do estado dos leds por algum cliente
    case WStype_TEXT:
      Serial.printf("[%u] get Text: %s\n", num, payload);
// altera a variável que guarda o estado dos leds conforme solicitação do cliente
// TODO - melhorar a lógica para evitar as condicionais
      if (payload[0] == '0') leds[0] = payload[1];
      if (payload[0] == '1') leds[1] = payload[1];
      if (payload[0] == '2') leds[2] = payload[1];
      Serial.print("Informação recebida, enviando leds: ");
      Serial.println(leds);
      changeLedStatus();
      webSocket.broadcastTXT(leds); // faz um broadcast do novo estado dos leds para TODOS os clientes
      break;
  }
}

class CaptiveRequestHandler : public AsyncWebHandler {
public:
  CaptiveRequestHandler() {}
  virtual ~CaptiveRequestHandler() {}

  bool canHandle(AsyncWebServerRequest *request){
    //request->addInterestingHeader("ANY");
    return true;
  }

  void handleRequest(AsyncWebServerRequest *request) {
    String requestedFile = request->url();
    Serial.println(requestedFile);
  
    AsyncWebServerResponse *response = nullptr;  // declara variável que será usada na resposta

// verifica qual arquivo está sendo requisitado, e o envia de acordo
// TODO - melhorar a lógica para evitar o encadeamento de condicionais
    if (requestedFile.equals("/estilos.css")) {
      response = request->beginResponse(SPIFFS, "/estilos.css", "text/css");
    } else if (requestedFile.equals("/script.js")) {
      response = request->beginResponse(SPIFFS, "/script.js", "text/javascript");
    } else if (requestedFile.equals("/logo-fundo-escuro.png")) {
      response = request->beginResponse(SPIFFS, "/logo-fundo-escuro.png", "image/png");
    } else {
      response = request->beginResponse(SPIFFS, "/index.html", "text/html");
    }
  
    request->send(response); // envia o arquivo
  }
};

void setup(){

// pinagem das portas dos leds
// os números NÃO correspondem aos gravados na placa!!!
// TODO - listar todas as portas em constantes
  pinMode(D0, OUTPUT);
  pinMode(D1, OUTPUT);
  pinMode(D2, OUTPUT);
  pinMode(D3, OUTPUT);
  pinMode(D5, OUTPUT);

  Serial.begin(115200); // habilita a saída de depuração
  Serial.setDebugOutput(true); // habilita as saídas de depuração da biblioteca wi-fi (o padrão é true e ela seta para false)

  WiFi.softAP("MAQUETE"); // configura o Access Point
  dnsServer.start(53, "*", WiFi.softAPIP()); // configura o servidor de DNS (porta 53) apontando para o Access Point
  server.addHandler(new CaptiveRequestHandler()).setFilter(ON_AP_FILTER); // direciona todos os requests para o CaptiveRequestHandler

  webSocket.onEvent(webSocketEvent); // direciona todos os eventos do servidor WebSocket para o webSocketEvent
  webSocket.begin(); // inicia o sevidor WebSocket

// inicia o sistema de arquivos e aguarda sua montagem para só então iniciar o servidor web
// (para evitar que o servidor esteja disponível antes dos arquivos)
  if( !SPIFFS.begin()){
    Serial.println("Erro na montagem do sistema de arquivos...");
    while(1);
  } else {
    Serial.println("Sistema de arquivos montado com sucesso!");
    server.begin();
  }
}

void loop(){
  webSocket.loop();
  dnsServer.processNextRequest();
}
