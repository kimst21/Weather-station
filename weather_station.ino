// 필요한 라이브러리 로드
#include <WiFi.h>
#include "SD.h"
#include "DHT.h"
#include <Wire.h>
#include <Adafruit_BME280.h>

// 네트워크 자격 증명으로 대체
const char* ssid     = ""; // 공유기 ID입력
const char* password = ""; // 공유기 비밀번호 입

// 사용 중인 DHT 센서 유형에 대해 정의
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321

// DHT가 연결된 GPIO
const int DHTPin = 21;
//DHT 센서 초기화
DHT dht(DHTPin, DHTTYPE);

// bme 개체 만들기
Adafruit_BME280 bme; // I2C

// SD 카드에 저장된 웹 페이지 파일
File webFile; 

// 트리머 GPIO 설정
const int potPin = 2;
const int LDRPin = 6;

// 온도와 습도를 저장할 변수
float tempC;
float tempF;
float humi;

// HTTP 요청을 저장할 변수
String header;

//웹 서버 포트 번호를 80으로 설정
WiFiServer server(80);

void setup(){    
  // 직렬 포트 초기화
  Serial.begin(115200); 

  // DHT 센서 초기화
  dht.begin();

  // BME280 센서 초기화
  if (!bme.begin(0x76)){
    Serial.println("Could not find BME280 sensor");
    while (1) {}
  }

  // SD 카드 초기화
  if(!SD.begin()){
      Serial.println("Card Mount Failed");
      return;
  }
  uint8_t cardType = SD.cardType();
  if(cardType == CARD_NONE){
      Serial.println("No SD card attached");
      return;
  }
  // SD 카드 초기화
  Serial.println("Initializing SD card...");
  if (!SD.begin()) {
      Serial.println("ERROR - SD card initialization failed!");
      return;    // int 실패
  }

  // SSID 및 암호로 Wi-Fi 네트워크에 연결
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // 로컬 IP 주소를 인쇄하고 웹 서버를 시작합니다
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  server.begin();
}

void loop(){
  WiFiClient client = server.available();   //들어오는 클라이언트 듣기

  if (client) {  // 새 클라이언트가 연결되는 경우
    boolean currentLineIsBlank = true;
    while (client.connected()) {
      if (client.available()) {   // 읽을 수 있는 클라이언트 데이터
        char c = client.read(); // 클라이언트에서 1바이트(문자) 읽기
        header += c;
        // 현재 행이 비어 있으면 두 줄의 새 행 문자가 연속으로 표시됩니다.
        // 클라이언트 HTTP 요청은 이것으로 끝이므로 응답을 보냅니다:
        if (c == '\n' && currentLineIsBlank) {
          // 표준 http 응답 헤더를 보냅니다
          client.println("HTTP/1.1 200 OK");
          // XML 파일 또는 웹 페이지 보내기
          // 클라이언트가 웹 페이지에 이미 있는 경우 브라우저가 가장 최근에 AJAX로 요청합니다
          // 센서 판독값(ESP32가 XML 파일을 전송함)
          if (header.indexOf("update_readings") >= 0) {
            // HTTP 헤더의 나머지를 보냅니다
            client.println("Content-Type: text/xml");
            client.println("Connection: keep-alive");
            client.println();
            // 센서 판독값과 함께 XML 파일 전송
            sendXMLFile(client);
          }
          // 클라이언트가 처음으로 연결되면 index.html 파일을 보냅니다
          // 마이크로SD 카드에 저장된
          else {  
            //HTTP 헤더의 나머지를 보냅니다
            client.println("Content-Type: text/html");
            client.println("Connection: keep-alive");
            client.println();
            // 마이크로SD카드에 저장된 웹페이지를 보냅니다
            webFile = SD.open("/index.html");
            if (webFile) {
              while(webFile.available()) {
                // 클라이언트에 웹 페이지 보내기
                client.write(webFile.read()); 
              }
              webFile.close();
            }
          }
          break;
        }
        // 고객으로부터 받은 텍스트의 모든 행은 \r\n으로 끝납니다
        if (c == '\n') {
          // 받은 텍스트 행의 마지막 문자
          // 새 행을 다음 문자 읽기로 시작하기
          currentLineIsBlank = true;
        } 
        else if (c != '\r') {
          //고객으로부터 문자를 받았습니다
          currentLineIsBlank = false;
        }
        } // end if (client.available())
    } // end while (client.connected())
    // 헤더 변수 지우기
    header = "";
    // 연결 닫기
    client.stop();
    Serial.println("Client disconnected.");
  } // end if (client)
}

//최신 센서 판독값과 함께 XML 파일 전송
void sendXMLFile(WiFiClient cl){
  // DHT 센서 판독 및 변수 업데이트
  readDHT();

  // XML 파일 준비
  cl.print("<?xml version = \"1.0\" ?>");
  cl.print("<inputs>");

  cl.print("<reading>");
  cl.print(tempC);
  cl.println("</reading>");
  
  cl.print("<reading>");
  cl.print(tempF);
  cl.println("</reading>");
  
  cl.print("<reading>");
  cl.print(humi);
  cl.println("</reading>");
  
  float currentTemperatureC = bme.readTemperature();
  cl.print("<reading>");
  cl.print(currentTemperatureC);
  cl.println("</reading>");
  float currentTemperatureF = (9.0/5.0)*currentTemperatureC+32.0;
  cl.print("<reading>");
  cl.print(currentTemperatureF);
  cl.println("</reading>");
  
  cl.print("<reading>");
  cl.print(bme.readPressure());
  cl.println("</reading>");
  
  cl.print("<reading>");
  cl.print(analogRead(potPin));
  cl.println("</reading>");

  // 중요: 핀 과제에서 GPIO 4에 대한 참고 사항을 읽습니다 
  cl.print("<reading>");
  cl.print(analogRead(LDRPin));
  cl.println("</reading>");
  
  cl.print("</inputs>");
}

void readDHT(){
  //센서 수치가 최대 2초 '오래됨'일 수도 있습니다(센서가 매우 느림)
  humi = dht.readHumidity();
  // 온도를 섭씨(기본값)로 읽습니다
  tempC = dht.readTemperature();
  // 온도를 화씨(화씨 = 참)로 읽습니다
  tempF = dht.readTemperature(true);

  // 실패한 읽기가 있는지 확인하고 일찍 종료(다시 시도)합니다.
  if (isnan(humi) || isnan(tempC) || isnan(tempF)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
}
