#include <ArduinoJson.h>
#include <ArduinoJson.hpp>

#include <Key.h>
#include <Keypad.h>

#include <Wire.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_Fingerprint.h>
#include <HTTPClient.h>
#include <ArduinoWebsockets.h>
#include <HardwareSerial.h>

// Khai báo chân kết nối
#define SDA_PIN 5
#define RST_PIN 15
#define AS_TXD 16
#define AS_RXD 17

#define R1 25
#define R2 26
#define R3 27
#define R4 14
#define C1 12
#define C2 13
#define C3 32
#define C4 33
#define LED 4

MFRC522 rfid(SDA_PIN, RST_PIN);
const char *ssid = "OPPO A74";
const char *password = "23082003n";

const char *server_host = "192.168.28.227";
const uint16_t server_port = 8080;//cổng websocket chờ kết nố

using namespace websockets;
WebsocketsClient client;

char keys[4][4] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte pin_rows[4] = {R1, R2, R3, R4};
byte pin_column[4] = {C1, C2, C3, C4};
Keypad keypad = Keypad(makeKeymap(keys), pin_rows, pin_column, 4, 4);

HardwareSerial mySerial(1); 
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD 16x2
Adafruit_Fingerprint finger(&mySerial);
uint8_t fingerTemplate[512]={0};// Mảng lưu dữ liệu mẫu vân tay

String uidString = "";
void init_wifi(){
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
}


void conect_websockets(){
  client.onMessage(onMessageCallback);
  bool connected = client.connect(server_host,server_port, "/");
  if (!connected)
  {
    Serial.println("WebSocket connection failed");
  }
  else Serial.println("WebSocket connectied");
}

void onMessageCallback(WebsocketsMessage mesage){//nhận lệnh từ server
  Serial.print("Nhận message: " + mesage.data());
  StaticJsonDocument<512> doc;
  DeserializationError error = deserializeJson(doc, mesage.data());
  if (error) {
    Serial.print("JSON Parsing Failed: ");
    Serial.println(error.c_str());
    return;
  }

  String header = doc["header"];
  JsonObject payload = doc["payload"];

  if(header=="Create new account"){//tạo tài khoản
    Serial.println("Yêu cầu tạo vân tay"); 
    String uid=payload["UID"];
    String password=payload["password"];
    tao_tk(uid, password);

  }
  else if(header=="Check_in response"){//thông tin phiên xác thực check in
    String uid = payload["UID"].as<String>();
    String password = payload["password"].as<String>();
    JsonArray finger_template = payload["finger"].as<JsonArray>();

    if (finger_template.size() != 512) {
      Serial.println("Kích thước mẫu vân tay không hợp lệ.");
      return;
    }

    uint16_t vantay_nhan[512];
    for (size_t i = 0; i < 512; i++) {
      vantay_nhan[i] = finger_template[i];
    }
    // Tải mẫu vân tay vào bộ nhớ của cảm biến
    uint16_t fingerID = 20;
    if (finger.loadModel(*vantay_nhan)) {
      Serial.println("Tải mẫu vân tay vào bộ nhớ thành công!");
      for (int i=0;i<512;i++){
        Serial.print(vantay_nhan[i]);
        Serial.print(" ");
      }
       // ID của mẫu vân tay,có thể thay đổi ID cho từng mẫu
      if (finger.storeModel(fingerID) == FINGERPRINT_OK) {
        Serial.println("Lưu mẫu vân tay thành công!");
      } else {
        Serial.println("Lưu mẫu vân tay thất bại.");
      }
    } 
    else Serial.println("Tải mẫu vân tay vào bộ nhớ thất bại.");

    xacthuc(fingerID, password, uid );
  }
}

void tao_tk(String uid, String password){
  //kiểm tra mật khẩu
  String input_password=nhap_password();
  if(input_password != password){
    Serial.println("Sai mật khẩu, hãy nhập lại");
  }
  else{
    int result=enrollFingerprint();//tạo vân tay
    if(result==-1){
      Serial.print("Tạo vân tay thất bại");
      StaticJsonDocument<200> jsonDoc;
      jsonDoc["header"]="response new account";
      jsonDoc["UID"]=uid;
      jsonDoc["status"]="failed";
      String msg;
      serializeJson(jsonDoc, msg);
      client.send(msg);
    }
    else{
      Serial.print("Tạo vân tay thành công");
      StaticJsonDocument<1024> jsonDoc;
      jsonDoc["header"]="response new account";
      jsonDoc["UID"]=uid;
      jsonDoc["status"]="successfully";
      JsonArray fingerArray = jsonDoc.createNestedArray("finger");
      for (int i = 0; i < 512; i++) {// Chuyển từng phần tử của mảng vào JSON array
        fingerArray.add(fingerTemplate[i]);
      }
      String msg;
      serializeJson(jsonDoc, msg);
      client.send(msg);
      memset(fingerTemplate, 0, sizeof(fingerTemplate));//xóa mảng lưu vân tay khi gửi thành công
    }
  }
}


int enrollFingerprint() {//tạo vân tay
  int p = -1;
  
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("dat ngon tay len");
  lcd.setCursor(0, 0);
  lcd.print("cam bien lan 1");

  Serial.println("Đặt ngón tay lên cảm biến...");
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_OK) {
      Serial.println("Lấy ảnh lần 1 thành công.");
    } else {
      Serial.println("Lỗi không xác định.");
    }
  }

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Không thể chuyển đổi ảnh sang mẫu.");
    return -1;
  }
  Serial.println("Nhấc ngón tay ra và đặt lại lần nữa...");
  delay(2000);
  // Lấy mẫu lần 2
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("dat ngon tay len");
  lcd.setCursor(0, 0);
  lcd.print("cam bien lan 2");
  p = -1;
  while (p != FINGERPRINT_OK) {
    p = finger.getImage();
    if (p == FINGERPRINT_OK) {
      Serial.println("Lấy ảnh lần 2 thành công.");
    }else {
      Serial.println("Lỗi không xác định.");
    }
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Không thể chuyển đổi ảnh sang mẫu.");
    return -1;
  }

  p = finger.createModel();// Ghép mẫu
  if (p != FINGERPRINT_OK) {
    Serial.println("Ghép mẫu thất bại.");
    return -1;
  }
  Serial.println("Ghép mẫu thành công.");

  p = finger.getModel();  
  if (p == FINGERPRINT_OK) {
    for (int i = 0; i <512; i++) {
      while (!mySerial.available()); // Chờ dữ liệu UART
      fingerTemplate[i] = mySerial.read();
    }
    
    Serial.println("Tạo mẫu vân tay thành công.");
    return 1; // trả về 1
  } 
  else {
    Serial.println("Tạo mẫu vân tay thất bại.");
    memset(fingerTemplate, 0, sizeof(fingerTemplate));
    return -1; // 
  }
}


bool isUnlocked = false;
void xacthuc(int finger_id, String password, String uid){
  unsigned long startTime = 0;  // To store the time when function starts
  const unsigned long TIMEOUT = 30000; 

  startTime = millis();

  while (!isUnlocked) {

    if (millis() - startTime >= TIMEOUT) {
        Serial.println("Hết thời gian xác thực (30 giây)!");
        Serial.println("Hệ thống đã bị khóa!");
        xoa_vantay(finger_id);
    }
    if (kiemtra_vantay(finger_id)) {
        Serial.println("Mở khóa bằng vân tay thành công!");
        isUnlocked = true;
    }  
    /*
    else if (kiemtra_matkhau(password)) {
      Serial.println("Mở khóa bằng mật khẩu thành công!");
      isUnlocked = true;
    } */
    else {
      Serial.println("Xác thực thất bại. Thử lại...");
    }
  }
 
  if (isUnlocked) {
    Serial.println("HỆ THỐNG ĐÃ MỞ KHÓA!");
    digitalWrite(LED, HIGH);
    StaticJsonDocument<300> jsonDoc;
      jsonDoc["header"]="Check_in successfully";
      jsonDoc["UID"]=uid;
      String msg;
      serializeJson(jsonDoc, msg);
      client.send(msg);
    delay(10000); //  mở khóa trong 10s

    isUnlocked = false;
    Serial.println("Khóa lại hệ thống...");
    digitalWrite(LED, LOW);
    xoa_vantay(finger_id);
  }
}

bool kiemtra_matkhau(String password){
  String input_pass = nhap_password();
  if(password==input_pass) return true;
  else return false;
}
bool kiemtra_vantay(int finger_id){
  int result = getFingerprintID();
  if(result==finger_id) return true; 
  else return false;
}

int getFingerprintID() {
  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) return -1; // Không lấy được hình ảnh
  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1; // Không chuyển đổi được hình ảnh
  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -1; // Không tìm thấy ID
  return finger.fingerID;
}

void xoa_vantay(int fingerID){
  if (finger.deleteModel(fingerID) == FINGERPRINT_OK) {
    Serial.println("Xóa mẫu vân tay thành công!");
  } else {
    Serial.println("Xóa mẫu vân tay thất bại.");
  }
}

String nhap_password(){
  Serial.println("Nhập mật khẩu!");
  String inputBuffer = "";
  char key;
  while (true) {
    key = keypad.getKey();
    if (key) {
      if (key == '#') {
        return inputBuffer;
      } else {
        inputBuffer += key;
      }
    }
  }
  return "";
}


void setup() {
  Serial.begin(115200);
  init_wifi();
  conect_websockets();

  mySerial.begin(57600, SERIAL_8N1, AS_TXD, AS_RXD);     //vân tay 
  delay(100);
  finger.begin(57600);

  if (finger.verifyPassword()) {
    Serial.println("Kết nối cảm biến vân tay thành công!");
  } else {
    Serial.println("Không thể kết nối cảm biến vân tay");
    while (1);
  }  
  SPI.begin();             // Khởi động giao tiếp SPI
  rfid.PCD_Init();         // Khởi tạo module RFID
  lcd.init();              // Khởi tạo LCD
  lcd.backlight();         // Bật đèn nền LCD

  Serial.println("Đang chờ thẻ RFID...");
  lcd.setCursor(0, 0);
  lcd.print("Quet the RFID...");
}


void loop() {
  client.poll();
  // Kiểm tra xem có thẻ RFID nào không
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  // Kiểm tra xem thẻ có thể đọc được không
  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  uidString = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uidString += "0"; // Thêm '0' nếu byte < 0x10
    uidString += String(rfid.uid.uidByte[i], HEX);    
  }
  uidString.toUpperCase(); 

  Serial.print("UID của thẻ: ");
  Serial.println(uidString);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("UID: ");
  lcd.setCursor(0, 1);
  lcd.print(uidString);
  // Hủy chọn thẻ để sẵn sàng đọc thẻ tiếp theo
  rfid.PICC_HaltA();
}
