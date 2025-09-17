// ========== 섹션 1: 라이브러리 포함 및 기본 설정 ==========
// ********* 간단한 ESP32 Weather Station - 안정화 버전 *********
#include <WiFi.h>                  // ESP32 WiFi 연결과 웹서버 기능 제공
#include <Adafruit_BME280.h>        // BME280 온도/습도/기압 센서 제어
#include <Adafruit_Sensor.h>        // Adafruit 센서 통합 인터페이스
#include <Adafruit_NeoPixel.h>      // WS2812B RGB LED 스트립 제어
#include <Adafruit_GFX.h>           // Adafruit 그래픽 라이브러리 (OLED용)
#include <Adafruit_SSD1306.h>       // SSD1306 OLED 디스플레이 제어

// ********* WiFi 설정 (직접 입력) *********
const char* ssid = "YOUR_WIFI_SSID";      // 연결할 WiFi 네트워크 이름
const char* password = "YOUR_WIFI_PASSWORD"; // WiFi 네트워크 비밀번호

ESP32 Weather Station 프로젝트에 필요한 모든 외부 라이브러리를 포함하는 부분입니다.
각 라이브러리는 특정 하드웨어 컴포넌트를 제어하며, WiFi 통신부터 센서, LED, OLED 디스플레이까지 모든 기능을 지원합니다.
WiFi 연결 정보는 컴파일 시점에 코드에 하드코딩되므로 실제 사용 환경에 맞게 수정이 필요합니다.
이러한 라이브러리 추상화를 통해 복잡한 하드웨어 통신을 간단한 함수 호출로 처리할 수 있습니다.

// ========== 섹션 2: 하드웨어 객체 및 핀 설정 ==========
// ********* 웹서버 설정 *********
WiFiServer server(80);             // 포트 80에서 HTTP 웹서버 생성

// ********* BME280 센서 객체 *********
Adafruit_BME280 bme;              // I2C 통신으로 BME280 센서 제어

// ********* OLED 디스플레이 설정 *********
#define SCREEN_WIDTH 128          // OLED 디스플레이 가로 해상도
#define SCREEN_HEIGHT 64          // OLED 디스플레이 세로 해상도
#define OLED_RESET    -1          // 리셋 핀 없음 (-1로 설정)
#define OLED_ADDRESS  0x3C        // OLED I2C 주소 (일반적으로 0x3C 또는 0x3D)

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET); // OLED 객체 생성

// ********* I2C 핀 설정 *********
#define SDA_PIN 8                 // I2C SDA 핀 (데이터 라인)
#define SCL_PIN 9                 // I2C SCL 핀 (클록 라인)

// ********* LED 스트립 설정 *********
#define LED_PIN_1 4               // 첫 번째 LED 스트립 연결 핀 (온도 표시용)
#define LED_PIN_2 17              // 두 번째 LED 스트립 연결 핀 (습도 표시용)
#define LED_COUNT 5               // 각 스트립당 LED 개수
#define BRIGHTNESS 50             // LED 밝기 (0-255, 50은 약 20% 밝기)

Adafruit_NeoPixel strip1(LED_COUNT, LED_PIN_1, NEO_GRB + NEO_KHZ800); // 온도 표시 스트립
Adafruit_NeoPixel strip2(LED_COUNT, LED_PIN_2, NEO_GRB + NEO_KHZ800); // 습도 표시 스트립

시스템의 모든 하드웨어 컴포넌트와 GPIO 핀 연결을 정의하는 섹션입니다.
웹서버는 표준 HTTP 포트 80에서 서비스하고, OLED는 128x64 해상도로 I2C 주소 0x3C를 사용합니다.
사용자 정의 I2C 핀(SDA=8, SCL=9)을 설정하여 다른 기본 핀들과의 충돌을 방지합니다.
두 개의 NeoPixel LED 스트립은 각각 온도와 습도를 시각적으로 표현하기 위해 별도 GPIO 핀에 연결됩니다.

// ========== 섹션 3: 전역 변수 선언 ==========
// ********* 센서 데이터 변수 *********
float temperature = 0.0;          // 현재 온도 값 저장 (섭씨)
float humidity = 0.0;             // 현재 습도 값 저장 (%)
float pressure = 0.0;             // 현재 기압 값 저장 (hPa)

// ********* 타이머 변수 *********
unsigned long previousMillis = 0; // 마지막 센서 읽기 시간 기록
const long interval = 5000;       // 센서 읽기 간격 (5000ms = 5초)

센서에서 읽어온 데이터를 저장할 전역 변수와 타이밍 제어를 위한 변수들을 선언합니다.
float 타입으로 소수점 정밀도를 유지하며, 온도는 섭씨, 습도는 퍼센트, 기압은 hPa 단위로 저장됩니다.
비차단(non-blocking) 방식의 타이머 구현을 위해 millis() 함수와 함께 사용할 변수들입니다.
이 방식을 통해 delay() 없이도 정확한 시간 간격으로 센서를 읽어 웹서버 응답성을 보장합니다.

// ========== 섹션 4: HTML 웹페이지 정의 ==========
// ********* HTML 웹페이지 (메모리에 저장) *********
const char* htmlPage = R"(        // Raw String Literal로 HTML 코드 저장
<!DOCTYPE html>
<html>
<head>
    <title>ESP32 Simple Weather Station</title>
    <meta charset="UTF-8">         <!-- UTF-8 인코딩으로 한글 지원 -->
    <meta name="viewport" content="width=device-width, initial-scale=1.0"> <!-- 반응형 웹 설정 -->
    <style>
        body { 
            font-family: Arial, sans-serif;     /* 웹 안전 폰트 사용 */
            text-align: center;                 /* 모든 내용 중앙 정렬 */
            background-color: #f0f0f0;          /* 연한 회색 배경 */
            margin: 0;                          /* 기본 여백 제거 */
            padding: 20px;                      /* 전체 20px 패딩 */
        }
        .container {
            max-width: 600px;                   /* 최대 너비 600px로 제한 */
            margin: 0 auto;                     /* 컨테이너 중앙 정렬 */
            background: white;                  /* 흰색 배경 */
            padding: 20px;                      /* 내부 여백 20px */
            border-radius: 10px;                /* 모서리 둥글게 10px */
            box-shadow: 0 4px 6px rgba(0,0,0,0.1); /* 그림자 효과 */
        }
        h1 { 
            color: #333;                        /* 어두운 회색 제목 */
            margin-bottom: 30px;                /* 제목 아래 30px 여백 */
        }
        .sensor-card {
            background: #4CAF50;                /* 기본 녹색 배경 */
            color: white;                       /* 흰색 텍스트 */
            padding: 15px;                      /* 카드 내부 여백 */
            margin: 10px;                       /* 카드 외부 여백 */
            border-radius: 8px;                 /* 카드 모서리 둥글게 */
            display: inline-block;              /* 가로 배치 */
            min-width: 150px;                   /* 최소 너비 150px */
        }
        .sensor-value {
            font-size: 24px;                   /* 센서값 큰 글씨 */
            font-weight: bold;                  /* 센서값 굵은 글씨 */
        }
        .sensor-label {
            font-size: 14px;                   /* 라벨 작은 글씨 */
            margin-top: 5px;                    /* 라벨 위쪽 여백 */
        }
        .temp { background: #FF6B35; }         /* 온도 카드 주황색 */
        .humi { background: #3498DB; }         /* 습도 카드 파란색 */
        .pres { background: #9B59B6; }         /* 기압 카드 보라색 */
        .update-time {
            margin-top: 20px;                  /* 업데이트 시간 위쪽 여백 */
            color: #666;                       /* 회색 텍스트 */
        }
    </style>
    <script>
        function updateData() {                // 센서 데이터 업데이트 함수
            var xhr = new XMLHttpRequest();    // AJAX 요청 객체 생성
            xhr.open('GET', '/data', true);    // GET 방식으로 /data 요청
            xhr.onreadystatechange = function() {
                if (xhr.readyState == 4 && xhr.status == 200) { // 요청 완료 및 성공 시
                    var data = xhr.responseText.split(',');      // CSV 데이터를 배열로 분할
                    document.getElementById('temp').innerHTML = data[0] + '°C';  // 온도 업데이트
                    document.getElementById('humi').innerHTML = data[1] + '%';   // 습도 업데이트
                    document.getElementById('pres').innerHTML = data[2] + ' hPa'; // 기압 업데이트
                    document.getElementById('time').innerHTML = '마지막 업데이트: ' + new Date().toLocaleTimeString(); // 시간 업데이트
                }
            };
            xhr.send();                        // 요청 전송
        }
        
        window.onload = function() {           // 페이지 로드 완료 시 실행
            updateData();                      // 즉시 첫 데이터 로드
            setInterval(updateData, 5000);     // 5초마다 자동 업데이트
        };
    </script>
</head>
<body>
    <div class="container">                    <!-- 메인 컨테이너 -->
        <h1>🌤️ ESP32 Weather Station</h1>     <!-- 제목과 날씨 이모지 -->
        
        <div class="sensor-card temp">         <!-- 온도 표시 카드 -->
            <div class="sensor-value" id="temp">--°C</div>     <!-- 온도 값 표시 영역 -->
            <div class="sensor-label">온도</div>                <!-- 온도 라벨 -->
        </div>
        
        <div class="sensor-card humi">         <!-- 습도 표시 카드 -->
            <div class="sensor-value" id="humi">--%</div>      <!-- 습도 값 표시 영역 -->
            <div class="sensor-label">습도</div>                <!-- 습도 라벨 -->
        </div>
        
        <div class="sensor-card pres">         <!-- 기압 표시 카드 -->
            <div class="sensor-value" id="pres">-- hPa</div>   <!-- 기압 값 표시 영역 -->
            <div class="sensor-label">기압</div>                <!-- 기압 라벨 -->
        </div>
        
        <div class="update-time" id="time">데이터 로딩 중...</div> <!-- 업데이트 시간 표시 -->
    </div>
</body>
</html>
)";

웹 인터페이스를 위한 완전한 HTML 페이지를 C++ Raw String Literal로 메모리에 저장하는 섹션입니다.
반응형 CSS 디자인을 구현하여 온도, 습도, 기압을 각각 다른 색상의 카드로 시각적으로 구분합니다.
JavaScript AJAX를 사용하여 5초마다 '/data' 엔드포인트에서 실시간 센서 데이터를 가져와 화면을 업데이트합니다.
SD 카드 의존성을 제거하고 프로그램 메모리에 웹페이지를 저장하여 시스템 구조를 단순화했습니다.

// ========== 섹션 5: setup() 함수 - 초기화 시작 ==========
void setup() {
    // ********* 시리얼 통신 시작 *********
    Serial.begin(115200);          // 시리얼 통신 115200bps로 시작
    delay(1000);                   // 1초 대기로 시리얼 포트 안정화
    
    // ********* I2C 초기화 (커스텀 핀 사용) *********
    Wire.begin(SDA_PIN, SCL_PIN);  // SDA=8, SCL=9 핀으로 I2C 시작
    Serial.println("I2C 초기화 완료 (SDA=8, SCL=9)");
    
    // ********* OLED 디스플레이 초기화 *********
    if(!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) { // OLED 초기화 시도
        Serial.println("OLED 초기화 실패!");
        setAllLEDs(255, 255, 0);   // OLED 오류시 노란색 LED 깜박임
        delay(2000);
    } else {
        Serial.println("OLED 초기화 완료");
        display.clearDisplay();    // OLED 화면 지우기
        display.setTextSize(1);    // 텍스트 크기 1로 설정
        display.setTextColor(SSD1306_WHITE); // 흰색 텍스트
        display.setCursor(0,0);    // 커서를 좌상단으로 이동
        display.println("ESP32 Weather");    // 시작 메시지 표시
        display.println("Station Starting...");
        display.display();         // OLED에 내용 출력
        delay(2000);               // 2초간 시작 메시지 표시
    }
    
    // ********* LED 스트립 초기화 *********
    strip1.begin();                // 첫 번째 LED 스트립 초기화
    strip2.begin();                // 두 번째 LED 스트립 초기화
    strip1.setBrightness(BRIGHTNESS); // 첫 번째 스트립 밝기 설정
    strip2.setBrightness(BRIGHTNESS); // 두 번째 스트립 밝기 설정
    strip1.show();                 // 첫 번째 스트립 상태 적용 (모든 LED OFF)
    strip2.show();                 // 두 번째 스트립 상태 적용 (모든 LED OFF)

ESP32 부팅 시 한 번만 실행되는 시스템 초기화 과정의 첫 번째 단계입니다.
고속 시리얼 통신(115200bps)을 설정하고 사용자 정의 I2C 핀으로 통신을 초기화합니다.
OLED 디스플레이 초기화를 시도하여 성공하면 시작 메시지를, 실패하면 LED 경고를 표시합니다.
NeoPixel LED 스트립들을 초기화하고 적절한 밝기로 설정한 후 모든 LED를 꺼진 상태로 시작합니다.

// ========== 섹션 6: setup() 함수 - 센서 초기화 ==========
    Serial.println("BME280 센서 초기화 중...");
    if (!bme.begin(0x76)) {        // I2C 주소 0x76에서 BME280 센서 찾기
        Serial.println("BME280 센서를 찾을 수 없습니다!");
        setAllLEDs(255, 0, 0);     // 센서 오류시 모든 LED 빨간색 표시
        display.clearDisplay();    // OLED 오류 메시지 표시
        display.setCursor(0,0);
        display.println("BME280 Error!");
        display.println("Check I2C");
        display.println("Connection");
        display.display();
        while (1) delay(1000);     // 무한루프로 프로그램 중단
    }
    Serial.println("BME280 센서 초기화 완료");

BME280 온도/습도/기압 센서의 I2C 통신 초기화를 담당하는 중요한 섹션입니다.
I2C 주소 0x76에서 센서와의 통신을 시도하고 연결 상태를 확인합니다.
센서 연결에 실패하면 빨간색 LED와 OLED에 명확한 오류 메시지를 표시하여 문제 진단을 돕습니다.
센서가 핵심 컴포넌트이므로 초기화 실패 시 무한루프로 진입하여 프로그램 진행을 중단시킵니다.

// ========== 섹션 7: setup() 함수 - WiFi 연결 및 웹서버 시작 ==========
    Serial.println("WiFi 연결 중...");
    WiFi.begin(ssid, password);   // 설정된 WiFi에 연결 시도
    
    setAllLEDs(255, 255, 0);       // WiFi 연결 중 노란색 LED 표시
    
    int wifi_timeout = 0;          // WiFi 연결 타임아웃 카운터
    while (WiFi.status() != WL_CONNECTED && wifi_timeout < 20) { // 최대 20초 대기
        delay(1000);               // 1초씩 대기
        Serial.print(".");         // 연결 진행상황 표시
        wifi_timeout++;            // 타임아웃 카운터 증가
    }
    
    if (WiFi.status() == WL_CONNECTED) { // WiFi 연결 성공시
        Serial.println("");
        Serial.println("WiFi 연결 성공!");
        Serial.print("IP 주소: ");
        Serial.println(WiFi.localIP()); // 할당받은 IP 주소 출력
        
        setAllLEDs(0, 255, 0);     // 연결 성공시 초록색 LED 표시
        
        // OLED에 WiFi 연결 정보 표시
        display.clearDisplay();    // OLED 화면 지우기
        display.setCursor(0,0);    // 커서를 좌상단으로 이동
        display.println("WiFi Connected!");
        display.println("");
        display.print("IP: ");
        display.println(WiFi.localIP()); // IP 주소를 OLED에 표시
        display.println("");
        display.println("Web Server Ready");
        display.display();         // OLED에 내용 출력
        delay(3000);               // 3초간 연결 정보 표시
        
        server.begin();            // 웹서버 시작
        Serial.println("웹서버 시작됨");
        
    } else {                       // WiFi 연결 실패시
        Serial.println("");
        Serial.println("WiFi 연결 실패!");
        setAllLEDs(255, 0, 0);     // 연결 실패시 빨간색 LED 표시
        
        // OLED에 연결 실패 메시지 표시
        display.clearDisplay();
        display.setCursor(0,0);
        display.println("WiFi Failed!");
        display.println("Check SSID");
        display.println("and Password");
        display.display();
        
        while (1) delay(1000);     // 무한루프로 프로그램 중단
    }
}

WiFi 네트워크 연결과 웹서버 시작을 처리하는 setup() 함수의 마지막 부분입니다.
연결 시도 중에는 노란색 LED로 진행 상태를 표시하고 최대 20초의 타임아웃을 설정합니다.
연결 성공 시 초록색 LED와 OLED에 IP 주소를 표시하여 사용자가 브라우저로 접속할 수 있도록 안내합니다.
연결 실패 시에는 빨간색 LED와 명확한 오류 메시지로 문제를 알리고 프로그램을 중단합니다.

// ========== 섹션 8: loop() 함수 - 메인 실행 루프 ==========
void loop() {
    unsigned long currentMillis = millis(); // 현재 시간 획득 (밀리초)
    if (currentMillis - previousMillis >= interval) { // 설정된 간격(5초) 경과시
        previousMillis = currentMillis;     // 이전 시간 업데이트
        readSensors();             // 센서 데이터 읽기
        updateLEDs();              // LED 상태 업데이트
        updateOLED();              // OLED 디스플레이 업데이트
    }
    
    WiFiClient client = server.available(); // 새로운 클라이언트 연결 확인
    if (client) {                  // 클라이언트가 연결된 경우
        handleClient(client);      // 클라이언트 요청 처리
    }
    
    if (WiFi.status() != WL_CONNECTED) { // WiFi 연결이 끊어진 경우
        Serial.println("WiFi 연결이 끊어졌습니다. 재연결 시도 중...");
        setAllLEDs(255, 255, 0);   // 재연결 중 노란색 LED 표시
        WiFi.reconnect();          // WiFi 재연결 시도
        delay(5000);               // 5초 대기 후 재시도
    }
}

ESP32가 동작하는 동안 계속 반복 실행되는 메인 처리 루프입니다.
millis() 기반 비차단 타이머로 5초마다 센서 읽기, LED 및 OLED 업데이트를 수행합니다.
웹서버 클라이언트 요청을 지속적으로 모니터링하여 접속이 있으면 즉시 처리합니다.
WiFi 연결 상태를 감시하여 연결이 끊어지면 자동 재연결을 시도하고 시각적 피드백을 제공합니다.

// ========== 섹션 9: 센서 데이터 읽기 함수 ==========
void readSensors() {               // 센서 데이터 읽기 함수
    temperature = bme.readTemperature(); // BME280에서 온도 읽기 (섭씨)
    humidity = bme.readHumidity();       // BME280에서 습도 읽기 (%)
    pressure = bme.readPressure() / 100.0F; // BME280에서 기압 읽기, hPa 단위로 변환
    
    Serial.printf("온도: %.1f°C, 습도: %.1f%%, 기압: %.1f hPa\n", 
                  temperature, humidity, pressure); // 센서 값 시리얼 출력
}

BME280 센서에서 온도, 습도, 기압 데이터를 동시에 읽어오는 전용 함수입니다.
온도는 섭씨로, 습도는 상대습도 퍼센트로, 기압은 파스칼에서 hPa 단위로 변환하여 저장합니다.
읽어온 센서 값들을 시리얼 모니터에 실시간으로 출력하여 모니터링과 디버깅을 지원합니다.
이 함수는 메인 루프에서 5초마다 호출되어 전역 변수에 최신 센서 데이터를 업데이트합니다.

// ========== 섹션 10: LED 업데이트 함수 ==========
void updateLEDs() {                // LED 상태 업데이트 함수
    int tempLEDs;                  // 온도에 따른 LED 개수 저장 변수
    
    // 온도의 절댓값을 사용하여 정확한 구간별 LED 개수 결정
    float absTemp = abs(temperature); // 온도의 절댓값 계산
    
    // 구간별 LED 개수 결정 (1~8도=1개, 9~16도=2개, 17~24도=3개, 25~32도=4개, 33~40도=5개)
    if (absTemp >= 1 && absTemp <= 8) {
        tempLEDs = 1;
    } else if (absTemp >= 9 && absTemp <= 16) {
        tempLEDs = 2;
    } else if (absTemp >= 17 && absTemp <= 24) {
        tempLEDs = 3;
    } else if (absTemp >= 25 && absTemp <= 32) {
        tempLEDs = 4;
    } else if (absTemp >= 33 && absTemp <= 40) {
        tempLEDs = 5;
    } else if (absTemp > 40) {
        tempLEDs = 5; // 40도 초과시에도 최대 5개
    } else {
        tempLEDs = 1; // 1도 미만시에도 최소 1개
    }
    
    int humiLEDs = constrain(map(humidity, 0, 100, 1, LED_COUNT), 1, LED_COUNT);   // 습도(0-100%)를 LED 개수(1-5)로 매핑
    
    strip1.clear();                // 첫 번째 스트립 모든 LED OFF
    for (int i = 0; i < tempLEDs; i++) { // 온도에 따른 LED 개수만큼 반복
        if (temperature < 0) {     // 영하 온도인 경우
            strip1.setPixelColor(i, strip1.Color(255, 255, 255)); // 흰색으로 표시
        } else if (temperature < 20) { // 영상 20도 미만이면
            strip1.setPixelColor(i, strip1.Color(0, 100, 255)); // 파란색 (차가움)
        } else if (temperature < 30) { // 영상 20-30도 사이면
            strip1.setPixelColor(i, strip1.Color(255, 165, 0)); // 주황색 (적당함)
        } else {                   // 영상 30도 이상이면
            strip1.setPixelColor(i, strip1.Color(255, 0, 0));   // 빨간색 (더움)
        }
    }
    strip1.show();                 // 첫 번째 스트립 변경사항 적용
    
    strip2.clear();                // 두 번째 스트립 모든 LED OFF
    for (int i = 0; i < humiLEDs; i++) { // 습도에 따른 LED 개수만큼 반복
        strip2.setPixelColor(i, strip2.Color(0, 150, 255)); // 파란색으로 습도 표시
    }
    strip2.show();                 // 두 번째 스트립 변경사항 적용
}

온도와 습도 값에 따라 NeoPixel LED 스트립의 개수와 색상을 동적으로 제어하는 함수입니다.
온도의 절댓값을 사용하여 영하와 영상 모두에서 동일한 로직으로 LED 개수를 결정합니다.
영하 온도는 흰색, 영상 온도는 온도 구간별로 파란색(차가움), 주황색(적당), 빨간색(더움)으로 직관적 표시합니다.
습도는 0-100% 범위를 1-5개 LED로 선형 매핑하여 파란색으로 일관되게 표시합니다.

// ========== 섹션 11: OLED 업데이트 함수 ==========
void updateOLED() {                // OLED 디스플레이 업데이트 함수
    display.clearDisplay();        // OLED 화면 전체 지우기
    
    // 온도 표시 (큰 글자)
    display.setTextSize(2);        // 큰 글자 크기로 설정
    display.setCursor(0, 0);       // 커서를 최상단 좌측으로 이동
    display.print(temperature, 1); // 소수점 1자리로 온도 표시
    display.println("C");          // 섭씨 단위 표시
    
    // 습도 표시 (큰 글자)
    display.setTextSize(2);        // 큰 글자 크기 유지
    display.print(humidity, 1);    // 소수점 1자리로 습도 표시
    display.println("%");          // 퍼센트 단위 표시
    
    // 기압 표시 (큰 글자)
    display.setTextSize(2);        // 큰 글자 크기 유지
    display.print(pressure, 0);    // 정수로 기압 표시 (공간 절약)
    display.println("hPa");        // hPa 단위 표시
    
    display.display();             // OLED에 모든 내용 출력
}

OLED 디스플레이에 센서 데이터를 큰 글자로 간결하게 표시하는 함수입니다.
제목이나 라벨 없이 센서 값과 단위만 표시하여 128x64 화면 공간을 효율적으로 활용합니다.
온도와 습도는 소수점 1자리로 정밀하게, 기압은 정수로 표시하여 가독성과 공간 절약을 균형있게 처리합니다.
display.display() 함수 호출로 메모리 버퍼의 모든 내용을 실제 OLED 하드웨어에 출력합니다.

// ========== 섹션 12: 유틸리티 함수들 ==========
void setAllLEDs(int r, int g, int b) { // 모든 LED를 같은 색으로 설정하는 함수
    for (int i = 0; i < LED_COUNT; i++) { // 모든 LED에 대해 반복
        strip1.setPixelColor(i, strip1.Color(r, g, b)); // 첫 번째 스트립 LED 색상 설정
        strip2.setPixelColor(i, strip2.Color(r, g, b)); // 두 번째 스트립 LED 색상 설정
    }
    strip1.show();                 // 첫 번째 스트립 변경사항 적용
    strip2.show();                 // 두 번째 스트립 변경사항 적용
}

void handleClient(WiFiClient client) { // 웹 클라이언트 요청 처리 함수
    String request = "";           // HTTP 요청 저장 변수
    
    // HTTP 요청 읽기
    while (client.connected() && client.available()) { // 클라이언트가 연결되고 데이터가 있는 동안
        String line = client.readStringUntil('\n');    // 한 줄씩 읽기
        if (line.length() == 1 && line[0] == '\r') break; // 빈 줄이면 헤더 끝
        if (request.length() == 0) request = line;      // 첫 줄을 요청으로 저장
    }
    
    // 요청 URL 확인
    if (request.indexOf("GET /data") >= 0) { // "/data" 경로 요청시 (AJAX 요청)
        String data = String(temperature, 1) + "," +    // 온도를 소수점 1자리로
                      String(humidity, 1) + "," +       // 습도를 소수점 1자리로
                      String(pressure, 1);              // 기압을 소수점 1자리로, CSV 형식
        
        client.println("HTTP/1.1 200 OK");              // HTTP 성공 응답
        client.println("Content-Type: text/plain");     // 일반 텍스트 형식
        client.println("Connection: close");            // 응답 후 연결 종료
        client.println();                               // 헤더와 본문 구분 빈 줄
        client.println(data);                           // 센서 데이터 전송
        
    } else {                       // 메인 페이지 요청시
        client.println("HTTP/1.1 200 OK");              // HTTP 성공 응답
        client.println("Content-Type: text/html; charset=UTF-8"); // HTML 형식, UTF-8 인코딩
        client.println("Connection: close");            // 응답 후 연결 종료
        client.println();                               // 헤더와 본문 구분 빈 줄
        client.println(htmlPage);                       // HTML 페이지 전송
    }
    
    client.stop();                 // 클라이언트 연결 종료
    Serial.println("클라이언트 연결 종료"); // 연결 종료 로그 출력
}

시스템의 보조 기능을 담당하는 핵심 유틸리티 함수들입니다.
setAllLEDs() 함수는 시스템 상태 표시를 위해 모든 LED를 동일한 색상으로 설정하는 편리한 도구입니다.
handleClient() 함수는 웹서버의 핵심으로 HTTP 요청을 파싱하고 URL에 따라 적절한 응답을 전송합니다.
AJAX 요청('/data')에는 CSV 형식의 실시간 센서 데이터를, 일반 요청에는 HTML 페이지를 응답하여 완전한 웹 서비스를 제공합니다.
