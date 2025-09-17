// ********* 간단한 ESP32 Weather Station - 최종 버전 *********
// ESP32를 사용한 웹 기반 날씨 모니터링 시스템
// 기능: BME280 센서로 온도/습도/기압 측정, NeoPixel LED로 시각적 표시, OLED 화면 출력, 웹서버로 원격 모니터링

#include <WiFi.h>                  // ESP32 WiFi 연결과 웹서버 기능 제공
#include <Adafruit_BME280.h>        // BME280 온도/습도/기압 센서 제어 라이브러리
#include <Adafruit_Sensor.h>        // Adafruit 센서들의 통합 인터페이스 제공
#include <Adafruit_NeoPixel.h>      // WS2812B RGB LED 스트립 제어 라이브러리
#include <Adafruit_GFX.h>           // Adafruit 그래픽 라이브러리 (OLED 텍스트/그래픽 출력용)
#include <Adafruit_SSD1306.h>       // SSD1306 OLED 디스플레이 제어 라이브러리

// ********* WiFi 설정 (직접 입력) *********
// 실제 사용할 WiFi 네트워크 정보로 변경 필요
const char* ssid = "WeVO_2.4G";      // 연결할 WiFi 네트워크 SSID (2.4GHz 대역 권장)
const char* password = "WEVO8358"; // WiFi 네트워크 비밀번호

// ********* 웹서버 설정 *********
WiFiServer server(80);             // HTTP 포트 80에서 웹서버 객체 생성 (기본 웹 포트)

// ********* BME280 센서 객체 *********
Adafruit_BME280 bme;              // I2C 통신으로 BME280 센서를 제어할 객체 생성

// ********* OLED 디스플레이 설정 *********
#define SCREEN_WIDTH 128          // OLED 디스플레이 가로 해상도 (픽셀)
#define SCREEN_HEIGHT 64          // OLED 디스플레이 세로 해상도 (픽셀)
#define OLED_RESET    -1          // OLED 리셋 핀 없음 (-1로 설정, 소프트웨어 리셋 사용)
#define OLED_ADDRESS  0x3C        // OLED I2C 주소 (일반적으로 0x3C, 일부 모듈은 0x3D)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // OLED 객체 생성 및 I2C 인터페이스 연결

// ********* I2C 핀 설정 *********
// ESP32의 기본 I2C 핀(SDA=21, SCL=22) 대신 커스텀 핀 사용
#define SDA_PIN 8                 // I2C SDA 핀 (데이터 라인) - GPIO 8번 핀 사용
#define SCL_PIN 9                 // I2C SCL 핀 (클록 라인) - GPIO 9번 핀 사용

// ********* LED 스트립 설정 *********
#define LED_PIN_1 4               // 첫 번째 NeoPixel 스트립 데이터 핀 (온도 표시용) - GPIO 4번
#define LED_PIN_2 17              // 두 번째 NeoPixel 스트립 데이터 핀 (습도 표시용) - GPIO 17번
#define LED_COUNT 5               // 각 스트립당 LED 개수 (더 많은 LED 사용시 전류 소모량 고려 필요)
#define BRIGHTNESS 50             // LED 밝기 (0-255, 50은 약 20% 밝기로 전력 절약)

// NeoPixel 스트립 객체 생성 - NEO_GRB: 색상 순서, NEO_KHZ800: 통신 속도
Adafruit_NeoPixel strip1(LED_COUNT, LED_PIN_1, NEO_GRB + NEO_KHZ800); // 온도 표시용 스트립
Adafruit_NeoPixel strip2(LED_COUNT, LED_PIN_2, NEO_GRB + NEO_KHZ800); // 습도 표시용 스트립

// ********* 센서 데이터 변수 *********
// 센서에서 읽은 값을 저장할 전역 변수들
float temperature = 0.0;          // 현재 온도 값 저장 (섭씨, BME280에서 직접 섭씨로 출력)
float humidity = 0.0;             // 현재 습도 값 저장 (상대습도 %)
float pressure = 0.0;             // 현재 기압 값 저장 (hPa 단위로 변환 후 저장)

// ********* 타이머 변수 *********
// delay() 사용 대신 millis()를 이용한 비블로킹 타이머 구현
unsigned long previousMillis = 0; // 마지막 센서 읽기 시간 기록 (millis() 값)
const long interval = 5000;       // 센서 읽기 간격 (5000ms = 5초, 너무 자주 읽으면 센서 수명 단축)

// ********* HTML 웹페이지 (메모리에 저장) *********
// Raw String Literal 사용으로 HTML 코드를 문자열로 저장
const char* htmlPage = R"(
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Simple Weather Station</title>
    <meta charset="UTF-8">        <!-- UTF-8 인코딩으로 한글 지원 -->
    <meta name="viewport" content="width=device-width, initial-scale=1.0">  <!-- 모바일 반응형 웹 설정 -->
    <style>
        /* CSS 스타일 - 모던하고 깔끔한 웹 인터페이스 디자인 */
        body { 
            font-family: Arial, sans-serif;     /* 웹 안전 폰트 사용 */
            text-align: center;                 /* 모든 내용 중앙 정렬 */
            background-color: #f0f0f0;          /* 연한 회색 배경으로 눈의 피로 감소 */
            margin: 0;
            padding: 20px;
        }
        .container {
            max-width: 600px;                   /* 최대 너비 제한으로 가독성 향상 */
            margin: 0 auto;                     /* 컨테이너 중앙 정렬 */
            background: white;                  /* 흰색 배경으로 내용 강조 */
            padding: 20px;
            border-radius: 10px;                /* 모서리 둥글게 처리 */
            box-shadow: 0 4px 6px rgba(0,0,0,0.1); /* 그림자 효과로 입체감 */
        }
        h1 { 
            color: #333;
            margin-bottom: 30px;
        }
        .sensor-card {
            background: #4CAF50;                /* 기본 녹색 배경 */
            color: white;
            padding: 15px;
            margin: 10px;
            border-radius: 8px;
            display: inline-block;              /* 카드들을 가로로 배치 */
            min-width: 150px;
        }
        .sensor-value {
            font-size: 24px;                   /* 센서값을 크게 표시 */
            font-weight: bold;
        }
        .sensor-label {
            font-size: 14px;
            margin-top: 5px;
        }
        .temp { background: #FF6B35; }         /* 온도 카드 주황색 */
        .humi { background: #3498DB; }         /* 습도 카드 파란색 */
        .pres { background: #9B59B6; }         /* 기압 카드 보라색 */
        .update-time {
            margin-top: 20px;
            color: #666;
        }
    </style>
    <script>
        /* JavaScript - AJAX를 이용한 실시간 데이터 업데이트 */
        function updateData() {                // 센서 데이터를 서버에서 가져와 화면 업데이트
            var xhr = new XMLHttpRequest();    // AJAX 요청 객체 생성
            xhr.open('GET', '/data', true);    // GET 방식으로 '/data' 경로에 비동기 요청
            xhr.onreadystatechange = function() {
                if (xhr.readyState == 4 && xhr.status == 200) { // 요청 완료 및 성공 시
                    var data = xhr.responseText.split(',');      // CSV 형태의 응답을 배열로 분할
                    // DOM 요소에 센서 값 업데이트
                    document.getElementById('temp').innerHTML = data[0] + '°C';  // 온도 표시
                    document.getElementById('humi').innerHTML = data[1] + '%';   // 습도 표시
                    document.getElementById('pres').innerHTML = data[2] + ' hPa'; // 기압 표시
                    // 업데이트 시간을 현재 로컬 시간으로 표시
                    document.getElementById('time').innerHTML = '마지막 업데이트: ' + new Date().toLocaleTimeString();
                }
            };
            xhr.send();                        // 요청 전송
        }
        
        window.onload = function() {           // 페이지 로드 완료 시 실행
            updateData();                      // 페이지 열리자마자 첫 데이터 로드
            setInterval(updateData, 5000);     // 5초마다 자동 업데이트 (센서 읽기 주기와 동일)
        };
    </script>
</head>
<body>
    <div class="container">
        <h1>🌤️ ESP32 Weather Station</h1>     <!-- 제목과 날씨 이모지로 직관적 표현 -->
        
        <!-- 각 센서별로 색상이 다른 카드 형태로 데이터 표시 -->
        <div class="sensor-card temp">         <!-- 온도 표시 카드 (주황색) -->
            <div class="sensor-value" id="temp">--°C</div>     <!-- 온도 값 표시 영역 -->
            <div class="sensor-label">온도</div>
        </div>
        
        <div class="sensor-card humi">         <!-- 습도 표시 카드 (파란색) -->
            <div class="sensor-value" id="humi">--%</div>      <!-- 습도 값 표시 영역 -->
            <div class="sensor-label">습도</div>
        </div>
        
        <div class="sensor-card pres">         <!-- 기압 표시 카드 (보라색) -->
            <div class="sensor-value" id="pres">-- hPa</div>   <!-- 기압 값 표시 영역 -->
            <div class="sensor-label">기압</div>
        </div>
        
        <div class="update-time" id="time">데이터 로딩 중...</div> <!-- 마지막 업데이트 시간 표시 -->
    </div>
</body>
</html>
)";

void setup() {
    // ********* 시리얼 통신 초기화 *********
    // 디버깅 및 상태 확인을 위한 시리얼 통신 설정
    Serial.begin(115200);          // 높은 보드레이트로 빠른 디버깅 가능
    delay(1000);                   // 시리얼 포트 안정화 대기
    
    // ********* I2C 버스 초기화 *********
    // BME280 센서와 OLED 디스플레이가 공통으로 사용할 I2C 버스 설정
    Wire.begin(SDA_PIN, SCL_PIN);  // 커스텀 핀으로 I2C 초기화 (SDA=8, SCL=9)
    
    // ********* OLED 디스플레이 초기화 *********
    // 128x64 해상도의 SSD1306 OLED 초기화 및 에러 처리
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) { // OLED 초기화 시도
        setAllLEDs(255, 255, 0);   // OLED 오류시 노란색 LED로 시각적 알림
        delay(2000);               // 2초간 오류 상태 표시
    } else {
        // OLED 초기화 성공시 화면 클리어 후 대기
        display.clearDisplay();    // 초기화 완료 후 빈 화면으로 시작
        display.display();         // 클리어된 상태를 실제 화면에 적용
    }
    
    // ********* NeoPixel LED 스트립 초기화 *********
    // 두 개의 LED 스트립 초기화 및 밝기 설정
    strip1.begin();                // 온도용 스트립 초기화
    strip2.begin();                // 습도용 스트립 초기화
    strip1.setBrightness(BRIGHTNESS); // 밝기 설정 (전력 소모 조절)
    strip2.setBrightness(BRIGHTNESS);
    strip1.show();                 // 초기 상태 적용 (모든 LED OFF)
    strip2.show();
    
    // ********* BME280 센서 초기화 *********
    // 온도/습도/기압 센서 초기화 및 통신 확인
    if (!bme.begin(0x76)) {        // I2C 주소 0x76에서 BME280 찾기 (일부는 0x77)
        // 센서 초기화 실패시 빨간색 LED로 에러 표시 후 프로그램 중단
        setAllLEDs(255, 0, 0);     // 빨간색 = 심각한 하드웨어 오류
        while (1) delay(1000);     // 무한루프로 프로그램 중단 (센서 없이는 동작 불가)
    }
    
    // ********* WiFi 연결 초기화 *********
    // WiFi 네트워크 연결 시도 및 타임아웃 처리
    WiFi.begin(ssid, password);   // 설정된 WiFi 네트워크에 연결 시작
    setAllLEDs(255, 255, 0);       // 연결 시도 중임을 노란색 LED로 표시
    
    // WiFi 연결 대기 (최대 20초 타임아웃)
    int wifi_timeout = 0;          // 타임아웃 카운터
    while (WiFi.status() != WL_CONNECTED && wifi_timeout < 20) { // 연결 상태 확인
        delay(1000);               // 1초씩 대기
        wifi_timeout++;            // 타임아웃 카운터 증가
    }
    
    // ********* WiFi 연결 결과 처리 *********
    if (WiFi.status() == WL_CONNECTED) { // WiFi 연결 성공
        // 연결 성공 정보를 시리얼로 출력 (IP 주소 확인용)
        Serial.println("");
        Serial.println("WiFi 연결 성공!");
        Serial.print("IP 주소: ");
        Serial.println(WiFi.localIP()); // 할당받은 IP 주소 출력 (웹 접속용)
        
        setAllLEDs(0, 255, 0);     // 연결 성공을 초록색 LED로 표시
        delay(3000);               // 3초간 성공 상태 표시
        
        server.begin();            // 웹서버 시작 (포트 80에서 대기)
        
    } else {                       // WiFi 연결 실패
        setAllLEDs(255, 0, 0);     // 연결 실패를 빨간색 LED로 표시
        while (1) delay(1000);     // WiFi 없이는 웹서버 기능 불가하므로 프로그램 중단
    }
}

void loop() {
    // ********* 비블로킹 타이머로 센서 읽기 주기 관리 *********
    // delay() 사용 없이 millis()로 정확한 시간 간격 유지
    unsigned long currentMillis = millis(); // 현재 시간 획득 (부팅 후 밀리초)
    if (currentMillis - previousMillis >= interval) { // 설정된 간격(5초) 경과 확인
        previousMillis = currentMillis;     // 다음 주기 계산을 위해 시간 업데이트
        // 센서 데이터 처리 루틴 실행
        readSensors();             // BME280에서 온도/습도/기압 읽기
        updateLEDs();              // 센서 값에 따른 LED 색상/개수 업데이트
        updateOLED();              // OLED 화면에 최신 센서 값 표시
    }
    
    // ********* 웹서버 클라이언트 요청 처리 *********
    // 새로운 HTTP 요청이 있는지 확인하고 처리
    WiFiClient client = server.available(); // 대기 중인 클라이언트 연결 확인
    if (client) {                  // 클라이언트가 연결된 경우
        handleClient(client);      // HTTP 요청 파싱 및 응답 처리
    }
    
    // ********* WiFi 연결 상태 모니터링 *********
    // WiFi 연결이 끊어진 경우 자동 재연결 시도
    if (WiFi.status() != WL_CONNECTED) { // WiFi 연결 상태 확인
        setAllLEDs(255, 255, 0);   // 재연결 시도 중임을 노란색 LED로 표시
        WiFi.reconnect();          // WiFi 재연결 시도
        delay(5000);               // 5초 대기 후 다음 loop 실행
    }
}

void readSensors() {               // BME280 센서에서 3가지 환경 데이터 읽기
    // BME280은 온도, 습도, 기압을 동시에 측정 가능한 통합 센서
    temperature = bme.readTemperature(); // 온도 읽기 (섭씨로 직접 출력)
    humidity = bme.readHumidity();       // 상대습도 읽기 (% 단위)
    pressure = bme.readPressure() / 100.0F; // 기압 읽기 (Pa를 hPa로 변환)
}

void updateLEDs() {                // 센서 값에 따른 LED 시각화 처리
    // ********* 온도 LED 개수 계산 *********
    // 온도의 절댓값을 이용하여 LED 개수 결정 (영하/영상 구분 없이)
    float absTemp = fabs(temperature); // float 타입 절댓값 함수 사용
    
    int tempLEDs;                  // 켤 LED 개수 저장 변수
    // 온도 구간별 LED 개수 설정 (8도씩 구간 분할)
    if (absTemp <= 8) {            // 0-8도: 낮은 온도
        tempLEDs = 1;
    } else if (absTemp <= 16) {    // 9-16도: 보통 온도
        tempLEDs = 2;
    } else if (absTemp <= 24) {    // 17-24도: 적당한 온도
        tempLEDs = 3;
    } else if (absTemp <= 32) {    // 25-32도: 높은 온도
        tempLEDs = 4;
    } else {                       // 33도 이상: 매우 높은 온도
        tempLEDs = 5;
    }
    
    // ********* 습도 LED 개수 계산 *********
    int humiLEDs;                  // 켤 LED 개수 저장 변수
    // 습도 구간별 LED 개수 설정 (20%씩 구간 분할)
    if (humidity <= 20) {          // 0-20%: 매우 건조
        humiLEDs = 1;
    } else if (humidity <= 40) {   // 21-40%: 건조
        humiLEDs = 2;
    } else if (humidity <= 60) {   // 41-60%: 적당함
        humiLEDs = 3;
    } else if (humidity <= 80) {   // 61-80%: 습함
        humiLEDs = 4;
    } else {                       // 81-100%: 매우 습함
        humiLEDs = 5;
    }
    
    // ********* 온도 LED 스트립 업데이트 *********
    strip1.clear();                // 모든 LED를 먼저 끄기
    for (int i = 0; i < tempLEDs; i++) { // 계산된 LED 개수만큼 반복
        // 실제 온도값(절댓값 아님)에 따른 색상 결정
        if (temperature < 0) {     // 영하 온도
            strip1.setPixelColor(i, strip1.Color(255, 255, 255)); // 흰색 (얼음/서리)
        } else if (temperature < 20) { // 0-19도
            strip1.setPixelColor(i, strip1.Color(0, 100, 255)); // 파란색 (차가움)
        } else if (temperature < 30) { // 20-29도
            strip1.setPixelColor(i, strip1.Color(255, 165, 0)); // 주황색 (따뜻함)
        } else {                   // 30도 이상
            strip1.setPixelColor(i, strip1.Color(255, 0, 0)); // 빨간색 (더위)
        }
    }
    strip1.show();                 // LED 변경사항을 실제 하드웨어에 적용
    
    // ********* 습도 LED 스트립 업데이트 *********
    strip2.clear();                // 모든 LED를 먼저 끄기
    for (int i = 0; i < humiLEDs; i++) { // 계산된 LED 개수만큼 반복
        // 습도는 모든 구간에서 동일한 파란색 사용
        strip2.setPixelColor(i, strip2.Color(0, 150, 255)); // 파란색 (물/습기)
    }
    strip2.show();                 // LED 변경사항을 실제 하드웨어에 적용
}

void updateOLED() {                // OLED 화면에 센서 값 표시
    display.clearDisplay();        // 이전 내용 지우기
    display.setTextColor(SSD1306_WHITE); // 흰색 텍스트 설정
    
    // ********* 온도 표시 *********
    display.setTextSize(2);        // 큰 글자 크기 (2배)
    display.setCursor(0, 0);       // 화면 최상단 좌측에서 시작
    display.print(temperature, 1); // 소수점 1자리로 온도 출력
    display.println("C");          // 섭씨 단위 표시
    
    // ********* 습도 표시 *********
    display.setTextSize(2);        // 큰 글자 크기 유지
    display.print(humidity, 1);    // 소수점 1자리로 습도 출력
    display.println("%");          // 퍼센트 단위 표시
    
    // ********* 기압 표시 *********
    display.setTextSize(2);        // 큰 글자 크기 유지
    display.print(pressure, 0);    // 정수로 기압 출력 (화면 공간 절약)
    display.println("hPa");        // hPa 단위 표시
    
    display.display();             // 화면 버퍼의 내용을 실제 OLED에 출력
}

void setAllLEDs(int r, int g, int b) { // 모든 LED를 동일한 색상으로 설정하는 유틸리티 함수
    // 시스템 상태 표시용 (초기화, 오류, 연결 상태 등)
    for (int i = 0; i < LED_COUNT; i++) { // 각 스트립의 모든 LED에 대해
        strip1.setPixelColor(i, strip1.Color(r, g, b)); // 온도 스트립 LED 색상 설정
        strip2.setPixelColor(i, strip2.Color(r, g, b)); // 습도 스트립 LED 색상 설정
    }
    strip1.show();                 // 온도 스트립 변경사항 적용
    strip2.show();                 // 습도 스트립 변경사항 적용
}

void handleClient(WiFiClient client) { // 웹 클라이언트의 HTTP 요청 처리
    String request = "";           // HTTP 요청 헤더 저장 변수
    
    // ********* HTTP 요청 헤더 읽기 *********
    while (client.connected() && client.available()) { // 클라이언트가 연결되고 데이터가 있는 동안
        String line = client.readStringUntil('\n');    // 한 줄씩 읽기
        if (line.length() == 1 && line[0] == '\r') break; // 빈 줄 만나면 헤더 끝
        if (request.length() == 0) request = line;      // 첫 번째 줄을 요청으로 저장 (GET /path HTTP/1.1)
    }
    
    // ********* 요청 경로에 따른 응답 분기 *********
    if (request.indexOf("GET /data") >= 0) { // AJAX 요청 (/data 경로)
        // 센서 데이터를 CSV 형태로 응답
        String data = String(temperature, 1) + "," +    // 온도 (소수점 1자리)
                      String(humidity, 1) + "," +       // 습도 (소수점 1자리)
                      String(pressure, 1);              // 기압 (소수점 1자리)
        
        // HTTP 응답 헤더 전송
        client.println("HTTP/1.1 200 OK");              // 성공 응답
        client.println("Content-Type: text/plain");     // 일반 텍스트 응답
        client.println("Connection: close");            // 응답 후 연결 종료
        client.println();                               // 헤더와 본문 구분하는 빈 줄
        client.println(data);                           // 센서 데이터 전송
        
    } else {                       // 메인 페이지 요청 (기본 경로)
        // HTML 웹페이지 응답
        client.println("HTTP/1.1 200 OK");              // 성공 응답
        client.println("Content-Type: text/html; charset=UTF-8"); // HTML 응답, UTF-8 인코딩
        client.println("Connection: close");            // 응답 후 연결 종료
        client.println();                               // 헤더와 본문 구분하는 빈 줄
        client.println(htmlPage);                       // HTML 페이지 전송
    }
    
    client.stop();                 // 클라이언트 연결 종료
}
