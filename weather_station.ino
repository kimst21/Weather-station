# Pico 2 W Weather Station - Below Zero Temperature LED Control Version
# Pico 2 W 기반의 날씨 측정소 - 영하 온도 감지 LED 제어 기능 포함

# ===== 필수 라이브러리 임포트 =====
import machine  # Pico의 하드웨어 제어를 위한 기본 라이브러리
import network  # WiFi 네트워크 연결을 위한 라이브러리
import socket   # 웹서버 구축을 위한 소켓 통신 라이브러리
import time     # 시간 관련 함수들 (지연, 타이밍 등)
import neopixel # WS2812B LED 스트립 제어를 위한 라이브러리
import json     # JSON 데이터 처리를 위한 라이브러리
from machine import Pin, I2C  # GPIO 핀과 I2C 통신을 위한 클래스들
import gc       # 가비지 컬렉션(메모리 정리)을 위한 라이브러리
import random   # 더미 데이터 생성 시 랜덤 값을 위한 라이브러리

# ===== 외부 센서 라이브러리 가용성 확인 =====
# BME280과 OLED 라이브러리가 설치되어 있는지 확인하고 가용성 플래그 설정
BME280_AVAILABLE = False  # BME280 센서 라이브러리 사용 가능 여부
OLED_AVAILABLE = False    # OLED 디스플레이 라이브러리 사용 가능 여부

# BME280 온습도 기압 센서 라이브러리 로드 시도
try:
    from bme280 import BME280  # BME280 센서 제어 클래스 임포트
    BME280_AVAILABLE = True    # 성공 시 사용 가능 플래그 설정
    print(" BME280 library loaded successfully")  # 성공 메시지 출력
except ImportError:
    # 라이브러리가 없는 경우 더미 데이터로 동작하도록 설정
    print(" BME280 library not found")

# SSD1306 OLED 디스플레이 라이브러리 로드 시도
try:
    from ssd1306 import SSD1306_I2C  # OLED 디스플레이 제어 클래스 임포트
    OLED_AVAILABLE = True            # 성공 시 사용 가능 플래그 설정
    print(" SSD1306 library loaded successfully")  # 성공 메시지 출력
except ImportError:
    # 라이브러리가 없는 경우 OLED 없이 동작하도록 설정
    print(" SSD1306 library not found")

# ===== 시스템 설정 및 하드웨어 핀 정의 =====
# Configuration
WIFI_SSID = "WeVO_2.4G"        # 연결할 WiFi 네트워크 이름 (SSID)
WIFI_PASSWORD = "Toolbox,WEVO8358+"  # WiFi 네트워크 비밀번호

# I2C 통신 핀 설정 (센서와 OLED 디스플레이 연결용)
SDA_PIN = 4  # I2C 데이터 라인 핀 번호 (GPIO 4)
SCL_PIN = 5  # I2C 클럭 라인 핀 번호 (GPIO 5)

# LED 스트립 제어 핀 설정
LED_PIN_1 = 0  # 온도 표시용 LED 스트립 핀 번호 (GPIO 0)
LED_PIN_2 = 8  # 습도 표시용 LED 스트립 핀 번호 (GPIO 8)

# LED 관련 설정값
LED_COUNT = 5        # 각 LED 스트립의 LED 개수 (5개씩)
LED_BRIGHTNESS = 0.2 # LED 밝기 설정 (0.0~1.0, 20% 밝기)

# ===== 전역 변수 선언 =====
# 센서에서 읽어온 환경 데이터를 저장할 변수들
temperature, humidity, pressure = 0.0, 0.0, 0.0  # 온도(°C), 습도(%), 기압(hPa)

# 센서 읽기 타이밍 제어 변수들
last_sensor_read = 0     # 마지막 센서 읽기 시간 저장
SENSOR_INTERVAL = 5000   # 센서 읽기 간격 (5초 = 5000ms)

# 하드웨어 객체들을 저장할 전역 변수들 (초기값 None)
i2c, bme_sensor, oled, strip1, strip2, wlan = None, None, None, None, None, None

# ===== 하드웨어 초기화 함수 =====
def init_hardware():

    global i2c, bme_sensor, oled, strip1, strip2  # 전역 변수들에 접근
    
    print("Initializing hardware...")  # 초기화 시작 메시지
    
    # ===== I2C 통신 초기화 =====
    # I2C는 센서와 OLED 디스플레이와 통신하기 위한 프로토콜
    try:
        # I2C 객체 생성: 0번 버스, SDA/SCL 핀 설정, 통신 속도 200kHz
        i2c = I2C(0, sda=Pin(SDA_PIN), scl=Pin(SCL_PIN), freq=200000)
        print("I2C initialization complete")  # 초기화 성공 메시지
        
        # 연결된 I2C 디바이스들의 주소를 스캔하여 출력
        print("I2C addresses:", [hex(addr) for addr in i2c.scan()])
    except Exception as e:
        print("I2C initialization failed:", e)  # 초기화 실패 시 오류 메시지
        return False  # 실패 시 함수 종료
    
    # ===== NeoPixel LED 스트립 초기화 =====
    # WS2812B LED 스트립 2개를 초기화 (온도용, 습도용)
    try:
        # 첫 번째 LED 스트립 (온도 표시용) 초기화
        strip1 = neopixel.NeoPixel(Pin(LED_PIN_1), LED_COUNT)
        # 두 번째 LED 스트립 (습도 표시용) 초기화
        strip2 = neopixel.NeoPixel(Pin(LED_PIN_2), LED_COUNT)
        
        # 모든 LED를 꺼서 초기 상태로 설정
        clear_all_leds()
        print("NeoPixel initialization complete")  # 초기화 성공 메시지
    except Exception as e:
        print("Neopixel initialization failed:", e)  # 초기화 실패 시 오류 메시지
        strip1, strip2 = None, None  # 실패 시 None으로 설정
    
    # ===== BME280 센서 초기화 =====
    # 온도, 습도, 기압을 측정하는 BME280 센서 초기화
    if BME280_AVAILABLE:  # BME280 라이브러리가 사용 가능한 경우에만
        try:
            # BME280 센서 객체 생성 (I2C 통신 사용)
            bme_sensor = BME280(i2c=i2c)
            print("BME280 initialization successful")  # 초기화 성공 메시지
        except Exception as e:
            print("BME280 initialization failed:", e)  # 초기화 실패 시 오류 메시지
            bme_sensor = None  # 실패 시 None으로 설정하여 더미 데이터 사용
    
    # ===== OLED 디스플레이 초기화 =====
    # 128x64 픽셀 OLED 디스플레이 초기화 및 시작 화면 표시
    if OLED_AVAILABLE:  # OLED 라이브러리가 사용 가능한 경우에만
        try:
            # OLED 디스플레이 객체 생성 (128x64 해상도, I2C 통신)
            oled = SSD1306_I2C(128, 64, i2c)
            oled.fill(0)  # 화면을 검은색으로 지우기
            
            # 시작 화면에 텍스트 표시
            oled.text("Weather Station", 0, 0)   # 제목 표시
            oled.text("Initializing...", 0, 20)  # 초기화 중 메시지 표시
            oled.show()  # 화면에 내용 출력
            
            print("OLED initialization successful")  # 초기화 성공 메시지
        except Exception as e:
            print("OLED initialization failed:", e)  # 초기화 실패 시 오류 메시지
            oled = None  # 실패 시 None으로 설정
    
    print("Hardware initialization complete")  # 모든 하드웨어 초기화 완료 메시지
    return True  # 성공적으로 초기화 완료

# ===== WiFi 연결 함수 =====
def connect_wifi():
    """
    WiFi 네트워크에 연결하는 함수
    최대 15초 동안 연결을 시도하며, 성공/실패 여부를 반환
    """
    global wlan  # 전역 WiFi 객체에 접근
    
    print("Attempting WiFi connection...")  # WiFi 연결 시도 메시지
    
    # WiFi 스테이션 모드로 설정 및 활성화
    wlan = network.WLAN(network.STA_IF)  # 스테이션 인터페이스 객체 생성
    wlan.active(True)  # WiFi 인터페이스 활성화
    
    # 지정된 SSID와 비밀번호로 연결 시도
    wlan.connect(WIFI_SSID, WIFI_PASSWORD)
    
    # 연결 완료까지 최대 15초 대기
    for _ in range(15):  # 15번 반복 (1초씩 대기)
        if wlan.isconnected():  # 연결 상태 확인
            # 연결 성공 시 IP 주소와 함께 성공 메시지 출력
            print("WiFi connection successful! IP:", wlan.ifconfig()[0])
            return True  # 연결 성공 반환
        time.sleep(1)  # 1초 대기
        print(".", end="")  # 연결 시도 중임을 나타내는 점 출력
    
    print("WiFi connection failed!")  # 연결 실패 메시지
    return False  # 연결 실패 반환

# ===== 센서 데이터 읽기 함수 =====
def read_sensors():
    """
    BME280 센서에서 온도, 습도, 기압 데이터를 읽어오는 함수
    센서가 없거나 읽기에 실패하면 더미 데이터를 생성
    """
    global temperature, humidity, pressure  # 전역 센서 데이터 변수들에 접근
    
    # BME280 센서가 사용 가능하고 초기화되어 있는 경우
    if BME280_AVAILABLE and bme_sensor:
        try:
            # ===== BME280에서 실제 센서 값 읽기 =====
            # 센서 라이브러리마다 다른 방식으로 데이터를 제공하므로 두 가지 방법 시도
            
            # 방법 1: values 속성을 통한 문자열 형태 데이터 읽기
            if hasattr(bme_sensor, 'values'):
                str_values = bme_sensor.values  # 문자열 형태의 센서 값들
                
                # 값들이 리스트나 튜플 형태이고 3개 이상의 요소를 가지는 경우
                if isinstance(str_values, (list, tuple)) and len(str_values) >= 3:
                    # 각 문자열에서 숫자와 소수점, 음수 기호만 추출
                    temp_str = ''.join(filter(lambda x: x.isdigit() or x == '.' or x == '-', str_values[0]))
                    press_str = ''.join(filter(lambda x: x.isdigit() or x == '.' or x == '-', str_values[1]))
                    hum_str = ''.join(filter(lambda x: x.isdigit() or x == '.' or x == '-', str_values[2]))
                    
                    # 추출된 문자열을 float로 변환 (실패 시 0.0)
                    temperature = float(temp_str) if temp_str else 0.0
                    pressure = float(press_str) if press_str else 0.0
                    humidity = float(hum_str) if hum_str else 0.0
                    return True  # 성공적으로 데이터 읽기 완료
            else:
                # 방법 2: read_compensated_data를 직접 사용하는 방법
                t, p, h = bme_sensor.read_compensated_data()  # 보정된 센서 데이터 읽기
                temperature = t        # 온도 (°C)
                pressure = p / 100     # 기압 (Pa를 hPa로 변환)
                humidity = h           # 습도 (%)
                return True  # 성공적으로 데이터 읽기 완료
                
        except Exception as e:
            print("BME280 read error:", e)  # 센서 읽기 오류 메시지 출력
    
    # ===== 더미 데이터 생성 =====
    # 센서가 없거나 읽기에 실패한 경우 테스트용 더미 데이터 생성
    base_time = time.ticks_ms() / 10000  # 시간 기반 변화량 계산
    
    # 온도: -5°C ~ 35°C 범위의 시뮬레이션 데이터 (영하 테스트 포함)
    temperature = 15.0 + 20 * (0.5 + 0.3 * (base_time % 60) / 60) + random.uniform(-1, 1) - 10
    
    # 습도: 40% ~ 85% 범위의 시뮬레이션 데이터
    humidity = 55.0 + 15 * (0.5 + 0.3 * ((base_time + 25) % 80) / 80) + random.uniform(-2, 2)
    
    # 기압: 1003 ~ 1033 hPa 범위의 시뮬레이션 데이터
    pressure = 1013.0 + 10 * (0.5 + 0.2 * ((base_time + 40) % 100) / 100) + random.uniform(-2, 2)
    
    return True  # 더미 데이터 생성 완료

# ===== LED 상태 업데이트 함수 =====
def update_leds():
 
    # LED 스트립이 초기화되지 않은 경우 함수 종료
    if not strip1 or not strip2:
        return
    
    # ===== 모든 LED 초기화 =====
    clear_all_leds()  # 이전 상태를 지우고 모든 LED 끄기
    
    # ===== 온도 기반 LED 개수 결정 =====
    # 온도의 절댓값을 사용하여 LED 개수 결정 (영하온도도 강도로 표시)
    abs_temp = abs(temperature)  # 온도의 절댓값 계산
    
    # 온도 범위별 LED 개수 설정 (5단계)
    if abs_temp <= 8:
        temp_leds = 1    # 매우 낮은 온도: LED 1개
    elif abs_temp <= 16:
        temp_leds = 2    # 낮은 온도: LED 2개
    elif abs_temp <= 24:
        temp_leds = 3    # 보통 온도: LED 3개
    elif abs_temp <= 32:
        temp_leds = 4    # 높은 온도: LED 4개
    else:
        temp_leds = 5    # 매우 높은 온도: LED 5개 (모두 점등)
    
    # ===== 온도 기반 LED 색상 결정 =====
    # 실제 온도 값(음수 포함)을 사용하여 색상 결정
    if temperature < 0:
        color_temp = (255, 255, 255)  # 영하 온도: 흰색 (특별 표시)
    elif temperature < 20:
        color_temp = (0, 100, 255)    # 낮은 온도 (0~20°C): 파란색
    elif temperature < 30:
        color_temp = (255, 165, 0)    # 보통 온도 (20~30°C): 주황색  
    else:
        color_temp = (255, 0, 0)      # 높은 온도 (30°C 이상): 빨간색

    # ===== 온도 LED 설정 적용 =====
    # strip1에 결정된 개수만큼 온도 색상 LED 점등
    for i in range(temp_leds):
        strip1[i] = color_temp  # 각 LED에 온도 색상 설정
    
    # ===== 습도 기반 LED 개수 결정 =====
    # 습도 수준에 따라 LED 개수 결정 (5단계)
    if humidity <= 20:
        humi_leds = 1    # 매우 건조 (0~20%): LED 1개
    elif humidity <= 40:
        humi_leds = 2    # 건조 (21~40%): LED 2개
    elif humidity <= 60:
        humi_leds = 3    # 보통 (41~60%): LED 3개
    elif humidity <= 80:
        humi_leds = 4    # 습함 (61~80%): LED 4개
    else:
        humi_leds = 5    # 매우 습함 (81~100%): LED 5개 (모두 점등)
    
    # ===== 습도 LED 색상 설정 =====
    color_humi = (0, 150, 255)  # 습도는 항상 파란색으로 표시
    
    # ===== 습도 LED 설정 적용 =====
    # strip2에 결정된 개수만큼 습도 색상 LED 점등
    for i in range(humi_leds):
        strip2[i] = color_humi  # 각 LED에 습도 색상 설정
    
    # ===== LED 스트립에 변경사항 적용 =====
    # 메모리에 설정된 LED 색상을 실제 하드웨어에 전송
    strip1.write()  # 온도 LED 스트립 업데이트
    strip2.write()  # 습도 LED 스트립 업데이트

# ===== OLED 디스플레이 업데이트 함수 =====
def update_oled():

    # OLED가 초기화되지 않은 경우 함수 종료
    if not oled:
        return
    
    try:
        # ===== 화면 지우기 및 기본 정보 표시 =====
        oled.fill(0)  # 화면을 검은색으로 지우기
        
        # 첫 번째 줄: 온도 정보 표시 (소수점 1자리까지)
        oled.text("T:{:.1f}C".format(temperature), 0, 0)
        
        # 두 번째 줄: 습도 정보 표시 (소수점 1자리까지)
        oled.text("H:{:.1f}%".format(humidity), 0, 20)
        
        # 세 번째 줄: 기압 정보 표시 (정수로 표시)
        oled.text("P:{:.0f}hPa".format(pressure), 0, 40)
        
        # ===== 센서 상태 표시 =====
        # 화면 오른쪽에 현재 사용 중인 센서 종류 표시
        if bme_sensor:
            oled.text("BME280", 70, 0)  # 실제 BME280 센서 사용 중
        else:
            oled.text("DUMMY", 70, 0)   # 더미 데이터 사용 중
        
        # ===== 영하 온도 특별 경고 표시 =====
        # 온도가 0도 미만일 경우 경고 메시지 표시
        if temperature < 0:
            oled.text("BELOW ZERO", 0, 50)  # 네 번째 줄에 영하 경고 표시
            
        # ===== 화면에 모든 내용 출력 =====
        oled.show()  # 메모리의 내용을 실제 OLED 화면에 출력
        
    except Exception as e:
        print("OLED update error:", e)  # OLED 업데이트 오류 메시지

# ===== LED 유틸리티 함수들 =====
def set_all_leds(r, g, b):
    """
    모든 LED를 지정된 RGB 색상으로 설정하는 함수
    Args:
        r (int): 빨간색 값 (0-255)
        g (int): 초록색 값 (0-255) 
        b (int): 파란색 값 (0-255)
    """
    # 첫 번째 LED 스트립의 모든 LED를 지정 색상으로 설정
    if strip1:
        for i in range(LED_COUNT):
            strip1[i] = (r, g, b)  # 각 LED에 RGB 색상 설정
        strip1.write()  # 하드웨어에 변경사항 적용
        
    # 두 번째 LED 스트립의 모든 LED를 지정 색상으로 설정
    if strip2:
        for i in range(LED_COUNT):
            strip2[i] = (r, g, b)  # 각 LED에 RGB 색상 설정
        strip2.write()  # 하드웨어에 변경사항 적용

def clear_all_leds():
    """
    모든 LED를 끄는 함수 (검은색으로 설정)
    LED 상태를 초기화하거나 이전 표시를 지울 때 사용
    """
    set_all_leds(0, 0, 0)  # 모든 LED를 (0,0,0) = 검은색으로 설정

# ===== 웹 클라이언트 요청 처리 함수 =====
def handle_client(client):
    """
    웹 브라우저나 API 클라이언트의 HTTP 요청을 처리하는 함수
    - '/data' 경로: JSON 형태로 센서 데이터 반환 (API용)
    - 기타 경로: HTML 웹페이지 반환 (브라우저용)
    """
    try:
        # ===== HTTP 요청 읽기 =====
        # 클라이언트로부터 HTTP 요청 데이터를 받아서 문자열로 디코딩
        request = client.recv(1024).decode('utf-8')
        
        # ===== 요청 경로에 따른 분기 처리 =====
        if '/data' in request:
            # ===== JSON API 응답 처리 =====
            # API 요청 시 센서 데이터를 JSON 형태로 반환
            
            # 센서 데이터를 딕셔너리로 구성
            data = {
                'temperature': round(temperature, 1),  # 온도 (소수점 1자리)
                'humidity': round(humidity, 1),        # 습도 (소수점 1자리)
                'pressure': round(pressure, 1),        # 기압 (소수점 1자리)
                # 센서 상태 정보 (실제 센서 사용 여부)
                'status': "BME280 Active" if (BME280_AVAILABLE and bme_sensor) else "Dummy Data"
            }
            
            # HTTP 응답 헤더와 JSON 데이터를 결합하여 응답 생성
            response = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: close\r\n\r\n" + json.dumps(data)
            
            # 클라이언트에게 JSON 응답 전송
            client.send(response.encode('utf-8'))
            
        else:
            # ===== HTML 웹페이지 응답 처리 =====
            # 브라우저 요청 시 사용자 친화적인 웹페이지 반환
            
            # HTML 템플릿 문자열 (CSS 스타일 포함)
            html = """HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nConnection: close\r\n\r\n
<!DOCTYPE html><html><head><title>Pico Weather</title>
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<meta charset="UTF-8">
<style>
/* 전체 페이지 스타일 설정 */
body{{font-family:Arial,sans-serif;text-align:center;margin:10px;padding:10px;background:#f5f5f5;}}
.container{{max-width:300px;margin:0 auto;}}  /* 중앙 정렬된 컨테이너 */
h1{{font-size:20px;margin-bottom:15px;color:#333;}}  /* 제목 스타일 */

/* 데이터 표시 박스 스타일 */
.data-box{{font-size:18px;margin:8px 0;padding:12px;border-radius:8px;color:white;width:100%;box-sizing:border-box;}}
.temp{{background:#FF6B35;}}   /* 온도 박스: 주황색 배경 */
.humi{{background:#3498DB;}}   /* 습도 박스: 파란색 배경 */
.pres{{background:#9B59B6;}}   /* 기압 박스: 보라색 배경 */

/* 상태 정보와 영하 온도 경고 스타일 */
.status{{margin-top:15px;font-size:14px;color:#666;}}
.below-zero{{color:red; font-weight:bold;}}  /* 영하 온도 경고: 빨간색 굵은 글씨 */
</style></head>
<body>
<div class="container">
<h1>Pico Weather Station</h1>

<!-- 센서 데이터 표시 박스들 -->
<!-- 온도 표시: 영하일 경우 경고 메시지 추가 -->
<div class="data-box temp">Temp: {:.1f}°C {}</div>
<!-- 습도 표시 -->
<div class="data-box humi">Humi: {:.1f}%</div>  
<!-- 기압 표시 -->
<div class="data-box pres">Press: {:.0f}hPa</div>

<!-- 센서 상태 정보 표시 -->
<div class="status">{}</div>
</div>

<!-- 자동 새로고침 스크립트 (5초마다) -->
<script>setTimeout(()=>location.reload(),5000);</script>
</body></html>""".format(
    temperature,  # 온도 값
    # 영하 온도일 경우에만 경고 메시지 추가
    "<span class='below-zero'>(Below Zero)</span>" if temperature < 0 else "",
    humidity,     # 습도 값
    pressure,     # 기압 값
    # 센서 상태 메시지
    "BME280 Active" if (BME280_AVAILABLE and bme_sensor) else "Dummy Data"
)
            
            # 클라이언트에게 HTML 응답 전송
            client.send(html.encode('utf-8'))
                
    except Exception as e:
        print("Client handling error:", e)  # 클라이언트 처리 중 오류 발생 시 메시지 출력
    finally:
        # ===== 연결 종료 =====
        # 오류가 발생하더라도 반드시 클라이언트 연결을 종료
        try:
            client.close()  # 클라이언트 소켓 연결 종료
        except:
            pass  # 연결 종료 중 오류가 발생해도 무시 (이미 종료된 경우 등)

# ===== 메인 함수 (프로그램 진입점) =====
def main():
    """
    프로그램의 메인 실행 함수
    - 하드웨어 초기화
    - WiFi 연결
    - 웹서버 설정
    - 메인 루프 실행 (센서 읽기, LED 업데이트, 웹 요청 처리)
    """
    global last_sensor_read  # 마지막 센서 읽기 시간 추적용 전역 변수
    
    print("Pico 2 W Weather Station starting")  # 프로그램 시작 메시지
    
    # ===== 단계 1: 하드웨어 초기화 =====
    # I2C, LED, 센서, OLED 등 모든 하드웨어 컴포넌트 초기화
    init_hardware()
    
    # ===== 단계 2: WiFi 네트워크 연결 =====
    # WiFi 연결 실패 시 시스템 재시작으로 재시도
    if not connect_wifi():
        print("WiFi connection failed! Retrying in 10 seconds...")
        time.sleep(10)      # 10초 대기
        machine.reset()     # 시스템 재시작으로 처음부터 다시 시도
    
    # ===== 단계 3: 웹서버 설정 =====
    # HTTP 웹서버를 설정하여 브라우저와 API 요청을 처리할 수 있도록 준비
    try:
        # TCP 소켓 생성 및 설정
        server_socket = socket.socket()  # TCP 소켓 생성
        server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)  # 주소 재사용 설정
        server_socket.bind(('', 80))     # 모든 인터페이스의 80번 포트에 바인딩
        server_socket.listen(1)          # 최대 1개의 대기 연결 허용
        server_socket.settimeout(0.5)    # 0.5초 타임아웃 설정 (논블로킹 처리용)
        
        # 웹서버 시작 완료 메시지 (접속 가능한 IP 주소 포함)
        print("Web server started - http://{}".format(wlan.ifconfig()[0]))
    except Exception as e:
        print("Web server initialization failed:", e)  # 웹서버 초기화 실패 시 오류 메시지
        return  # 웹서버 실패 시 프로그램 종료
    
    # ===== 단계 4: 메인 루프 시작 =====
    # 센서 읽기 타이밍 초기화
    last_sensor_read = time.ticks_ms()  # 현재 시간을 마지막 센서 읽기 시간으로 설정
    
    # 무한 루프: 센서 데이터 수집과 웹 요청 처리를 지속적으로 수행
    while True:
        try:
            # ===== 센서 데이터 주기적 업데이트 =====
            # 지정된 간격(5초)마다 센서 데이터를 읽고 디스플레이 업데이트
            if time.ticks_diff(time.ticks_ms(), last_sensor_read) >= SENSOR_INTERVAL:
                last_sensor_read = time.ticks_ms()  # 현재 시간으로 업데이트
                
                # 센서 데이터 읽기 및 출력 장치 업데이트 순서
                read_sensors()    # BME280에서 온도/습도/기압 데이터 읽기
                update_leds()     # LED 스트립 상태 업데이트 (색상, 개수)
                update_oled()     # OLED 디스플레이 내용 업데이트
                
                # 콘솔에 현재 센서 데이터 출력 (디버깅 및 모니터링용)
                print("Sensor data: T={:.1f}C, H={:.1f}%, P={:.0f}hPa".format(
                    temperature, humidity, pressure))
                
                # 메모리 정리 (마이크로컨트롤러의 제한된 메모리 관리)
                gc.collect()
            
            # ===== 웹 클라이언트 요청 처리 =====
            # 웹 브라우저나 API 클라이언트의 연결 요청을 처리
            try:
                # 새로운 클라이언트 연결 수락 (논블로킹, 타임아웃 0.5초)
                client, addr = server_socket.accept()
                print("Client connected:", addr)  # 클라이언트 연결 정보 출력
                
                # 클라이언트 요청 처리 (HTML 또는 JSON 응답)
                handle_client(client)
                
            except OSError:
                # 타임아웃 발생은 정상적인 상황 (연결 요청이 없는 경우)
                pass  # 타임아웃은 무시하고 계속 진행
            except Exception as e:
                print("Client accept error:", e)  # 기타 연결 오류 메시지 출력
            
            # ===== WiFi 연결 상태 모니터링 =====
            # WiFi 연결이 끊어진 경우 자동 재연결 시도
            if not wlan.isconnected():
                print("WiFi connection lost. Attempting reconnection...")
                if not connect_wifi():  # 재연결 시도
                    print("Reconnection failed")  # 재연결 실패 메시지
                    time.sleep(5)  # 5초 대기 후 다시 시도
            
            # ===== 루프 주기 조절 =====
            # CPU 사용률을 낮추고 다른 작업에 시간을 할당하기 위한 짧은 대기
            time.sleep(0.1)  # 100ms 대기
            
        except Exception as e:
            # 메인 루프에서 예상치 못한 오류 발생 시 처리
            print("Main loop error:", e)  # 오류 메시지 출력
            time.sleep(1)  # 1초 대기 후 루프 계속 (시스템 안정성 확보)


if __name__ == "__main__":
    main()  # 메인 함수 실행으로 프로그램 시작
