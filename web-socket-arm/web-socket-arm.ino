// esp32_robotic_arm_websocket.ino
#include <WiFi.h>
#include <ArduinoWebsockets.h>
#include <ArduinoJson.h>
#include <ESP32Servo.h>

// WiFi credentials
const char* ssid = "drone-testbench-thesis";
const char* password = "tbthesis";

// WebSocket server details
const char* websocket_server = "ws://192.168.0.101:8765";  // <---------------  hostname -I

using namespace websockets;
WebsocketsClient client;

// Motor pin definitions
// Shoulder motor pins
const int SHOULDER_R_PWM = 14;  // Forward PWM pin
const int SHOULDER_L_PWM = 15;  // Reverse PWM pin

// Base motor pins
const int BASE_R_PWM = 16;      // Forward PWM pin
const int BASE_L_PWM = 17;      // Reverse PWM pin

// Elbow motor pins
const int ELBOW_R_PWM = 12;     // Forward PWM pin
const int ELBOW_L_PWM = 13;     // Reverse PWM pin

// Roller motor pins
const int ROLL_MOTOR1 = 22;
const int ROLL_MOTOR2 = 23;

// Gripper motor pins
const int GRIPPER_MOTOR1 = 19;
const int GRIPPER_MOTOR2 = 21;

// Servo pin
const int SERVO_PIN = 18; // <-------------include

// Create servo object
Servo wristServo;

// PWM settings
const int PWM_FREQ = 1000;
const int PWM_RESOLUTION = 10;  // 10-bit resolution (0-1023)
const int PWM_MAX_VALUE = 1023;

// PWM channels (ESP32 has 16 PWM channels)
const int SHOULDER_R_CHANNEL = 0;
const int SHOULDER_L_CHANNEL = 1;
const int BASE_R_CHANNEL = 2;
const int BASE_L_CHANNEL = 3;
const int ELBOW_R_CHANNEL = 4;
const int ELBOW_L_CHANNEL = 5;

// Timing variables
unsigned long lastReconnectAttempt = 0;
unsigned long lastHeartbeat = 0;
const unsigned long reconnectInterval = 5000;  // 5 seconds
const unsigned long heartbeatInterval = 30000; // 30 seconds

bool wifiConnected = false;
bool wsConnected = false;

// Motor control functions
void setupMotors() {
    // Setup PWM channels for shoulder motor - New ESP32 Arduino Core 3.x syntax
    if (!ledcAttach(SHOULDER_R_PWM, PWM_FREQ, PWM_RESOLUTION)) {
        Serial.println("Failed to attach SHOULDER_R_PWM to LEDC");
    }
    if (!ledcAttach(SHOULDER_L_PWM, PWM_FREQ, PWM_RESOLUTION)) {
        Serial.println("Failed to attach SHOULDER_L_PWM to LEDC");
    }
    
    // Setup PWM channels for base motor
    if (!ledcAttach(BASE_R_PWM, PWM_FREQ, PWM_RESOLUTION)) {
        Serial.println("Failed to attach BASE_R_PWM to LEDC");
    }
    if (!ledcAttach(BASE_L_PWM, PWM_FREQ, PWM_RESOLUTION)) {
        Serial.println("Failed to attach BASE_L_PWM to LEDC");
    }
    
    // Setup PWM channels for elbow motor
    if (!ledcAttach(ELBOW_R_PWM, PWM_FREQ, PWM_RESOLUTION)) {
        Serial.println("Failed to attach ELBOW_R_PWM to LEDC");
    }
    if (!ledcAttach(ELBOW_L_PWM, PWM_FREQ, PWM_RESOLUTION)) {
        Serial.println("Failed to attach ELBOW_L_PWM to LEDC");
    }
    
    // Setup GPIO pins for roller and gripper
    pinMode(ROLL_MOTOR1, OUTPUT);
    pinMode(ROLL_MOTOR2, OUTPUT);
    pinMode(GRIPPER_MOTOR1, OUTPUT);
    pinMode(GRIPPER_MOTOR2, OUTPUT);
    
    // Setup servo
    wristServo.attach(SERVO_PIN);
    wristServo.write(90);  // Initialize to center position
    Serial.println("Servo initialized to 90 degrees");
    
    // Initialize all motors to stopped state
    shoulder_off();
    base_off();
    elbow_off();
    roller_off();
    gripper_off();
}

// Gripper control functions
void gripper_forward() {
    digitalWrite(GRIPPER_MOTOR1, LOW);
    digitalWrite(GRIPPER_MOTOR2, HIGH);
    Serial.println("***Gripper Forward (Open)");
}

void gripper_backward() {
    digitalWrite(GRIPPER_MOTOR1, HIGH);
    digitalWrite(GRIPPER_MOTOR2, LOW);
    Serial.println("***Gripper Backward (Close)");
}

void gripper_off() {
    digitalWrite(GRIPPER_MOTOR1, LOW);
    digitalWrite(GRIPPER_MOTOR2, LOW);
    Serial.println("***Gripper Off");
}

// Roller control functions
void roller_forward() {
    digitalWrite(ROLL_MOTOR1, LOW);
    digitalWrite(ROLL_MOTOR2, HIGH);
    Serial.println("***Roller Forward");
}

void roller_backward() {
    digitalWrite(ROLL_MOTOR1, HIGH);
    digitalWrite(ROLL_MOTOR2, LOW);
    Serial.println("***Roller Backward");
}

void roller_off() {
    digitalWrite(ROLL_MOTOR1, LOW);
    digitalWrite(ROLL_MOTOR2, LOW);
    Serial.println("***Roller Off");
}

// Shoulder control functions
void shoulder_forward(int pwm_value = PWM_MAX_VALUE) {
    ledcWrite(SHOULDER_R_PWM, pwm_value);
    ledcWrite(SHOULDER_L_PWM, 0);
    Serial.printf("***Shoulder Forward (PWM: %d)\n", pwm_value);
}

void shoulder_backward(int pwm_value = PWM_MAX_VALUE) {
    ledcWrite(SHOULDER_R_PWM, 0);
    ledcWrite(SHOULDER_L_PWM, pwm_value);
    Serial.printf("***Shoulder Backward (PWM: %d)\n", pwm_value);
}

void shoulder_off() {
    ledcWrite(SHOULDER_R_PWM, 0);
    ledcWrite(SHOULDER_L_PWM, 0);
    Serial.println("***Shoulder Stopped");
}

// Elbow control functions
void elbow_forward(int pwm_value = PWM_MAX_VALUE) {
    ledcWrite(ELBOW_R_PWM, pwm_value);
    ledcWrite(ELBOW_L_PWM, 0);
    Serial.printf("***Elbow Forward (PWM: %d)\n", pwm_value);
}

void elbow_backward(int pwm_value = PWM_MAX_VALUE) {
    ledcWrite(ELBOW_R_PWM, 0);
    ledcWrite(ELBOW_L_PWM, pwm_value);
    Serial.printf("***Elbow Backward (PWM: %d)\n", pwm_value);
}

void elbow_off() {
    ledcWrite(ELBOW_R_PWM, 0);
    ledcWrite(ELBOW_L_PWM, 0);
    Serial.println("***Elbow Stopped");
}

// Base control functions
void base_forward(int pwm_value = PWM_MAX_VALUE) {
    ledcWrite(BASE_R_PWM, pwm_value);
    ledcWrite(BASE_L_PWM, 0);
    Serial.printf("***Base Forward (PWM: %d)\n", pwm_value);
}

void base_backward(int pwm_value = PWM_MAX_VALUE) {
    ledcWrite(BASE_R_PWM, 0);
    ledcWrite(BASE_L_PWM, pwm_value);
    Serial.printf("***Base Backward (PWM: %d)\n", pwm_value);
}

void base_off() {
    ledcWrite(BASE_R_PWM, 0);
    ledcWrite(BASE_L_PWM, 0);
    Serial.println("***Base Stopped");
}

// Servo control functions
void setServoAngle(int angle) {
    // Constrain angle to valid servo range (0-180)
    angle = constrain(angle, 0, 180);
    wristServo.write(angle);
    Serial.printf("***Servo set to %d degrees\n", angle);
}

void servoSweepTest() {
    Serial.println("Starting servo sweep test...");
    for (int angle = 0; angle <= 180; angle += 10) {
        wristServo.write(angle);
        Serial.printf("Servo: %d°\n", angle);
        delay(100);
    }
    for (int angle = 180; angle >= 0; angle -= 10) {
        wristServo.write(angle);
        Serial.printf("Servo: %d°\n", angle);
        delay(100);
    }
    wristServo.write(90);  // Return to center
    Serial.println("Servo sweep test completed");
}

// Process arm control data from WebSocket
void processArmControl(JsonArray armData) {
    if (armData.size() < 6) {
        Serial.println("❌ Invalid arm control data - insufficient parameters");
        return;
    }
    
    // Extract control data
    int gripper_state = armData[0].as<int>();
    int roller_state = armData[1].as<int>();
    int servo_angle = armData[2].as<int>();
    
    JsonArray elbow_data = armData[3];
    JsonArray shoulder_data = armData[4];
    JsonArray base_data = armData[5];
    
    // Control gripper (0 = open/forward, 1 = close/backward)
    if (gripper_state == 1) {
        gripper_backward();  // Close gripper
    } else {
        gripper_forward();   // Open gripper
    }
    
    // Control roller (0 = off, 1 = on/forward)
    if (roller_state == 1) {
        roller_forward();
    } else {
        roller_off();
    }
    
    // Control servo
    setServoAngle(servo_angle);
    
    // Control elbow motor
    if (elbow_data.size() >= 2) {
        int elbow_dir = elbow_data[0].as<int>();
        int elbow_pwm = elbow_data[1].as<int>();
        
        if (elbow_pwm == 0) {
            elbow_off();
        } else if (elbow_dir == 1) {
            elbow_forward(elbow_pwm);
        } else {
            elbow_backward(elbow_pwm);
        }
    }
    
    // Control shoulder motor
    if (shoulder_data.size() >= 2) {
        int shoulder_dir = shoulder_data[0].as<int>();
        int shoulder_pwm = shoulder_data[1].as<int>();
        
        if (shoulder_pwm == 0) {
            shoulder_off();
        } else if (shoulder_dir == 1) {
            shoulder_forward(shoulder_pwm);
        } else {
            shoulder_backward(shoulder_pwm);
        }
    }
    
    // Control base motor
    if (base_data.size() >= 2) {
        int base_dir = base_data[0].as<int>();
        int base_pwm = base_data[1].as<int>();
        
        if (base_pwm == 0) {
            base_off();
        } else if (base_dir == 1) {
            base_forward(base_pwm);
        } else {
            base_backward(base_pwm);
        }
    }
    
    Serial.printf("🤖 Servo angle: %d°\n", servo_angle);
}

// Process serial commands (for manual testing)
void processSerialCommands() {
    if (Serial.available()) {
        String command = Serial.readStringUntil('\n');
        command.trim();
        
        if (command.length() == 1) {
            char cmd = command.charAt(0);
            
            switch (cmd) {
                case 'a': shoulder_forward(); break;
                case 'b': shoulder_backward(); break;
                case 's': shoulder_off(); elbow_off(); gripper_off(); roller_off(); base_off(); break;
                
                case 'c': elbow_forward(); break;
                case 'd': elbow_backward(); break;
                
                case 'f': base_forward(); break;
                case 'g': base_backward(); break;
                
                case 'i': roller_forward(); break;
                case 'j': roller_backward(); break;
                
                case 'l': gripper_forward(); break;
                case 'm': gripper_backward(); break;
                
                case 'o': setServoAngle(0); break;    // Servo to 0 degrees
                case 'p': setServoAngle(90); break;   // Servo to 90 degrees
                case 'q': setServoAngle(180); break;  // Servo to 180 degrees
                case 'r': servoSweepTest(); break;    // Servo sweep test
                
                default:
                    Serial.println("❌ Invalid command!");
                    Serial.println("Commands: a/b/s (shoulder), c/d/e (elbow), f/g/h (base), i/j/k (roller), l/m/n (gripper)");
                    Serial.println("         o/p/q (servo 0°/90°/180°), r (servo sweep test)");
                    break;
            }
        }
    }
}

void onMessageCallback(WebsocketsMessage message) {
    Serial.println("=== Received Message ===");
    Serial.print("Type: ");
    Serial.println(message.isText() ? "Text" : "Binary");
    Serial.print("Data: ");
    Serial.println(message.data());
    
    // Try to parse as JSON
    DynamicJsonDocument doc(1024);
    DeserializationError error = deserializeJson(doc, message.data());
    
    if (!error) {
        Serial.println("=== Parsed JSON ===");
        
        // Check if it's arm control data (array format)
        if (doc.is<JsonArray>()) {
            JsonArray arr = doc.as<JsonArray>();
            if (arr.size() >= 6) {
                Serial.println("🤖 ARM CONTROL DATA RECEIVED:");
                processArmControl(arr);
            }
        } else {
            // Print all JSON fields
            for (JsonPair kv : doc.as<JsonObject>()) {
                Serial.printf("  %s: ", kv.key().c_str());
                Serial.println(kv.value().as<String>());
            }
        }
    } 
    else {
        Serial.println("=== Plain Text Message ===");
        Serial.println(message.data());
    }
    
    Serial.println("========================");
}

void onEventsCallback(WebsocketsEvent event, String data) {
    switch(event) {
        case WebsocketsEvent::ConnectionOpened:
            Serial.println("✅ WebSocket Connected!");
            wsConnected = true;
            client.send("ESP32 Robotic Arm Controller - Ready for commands!");
            break;
            
        case WebsocketsEvent::ConnectionClosed:
            Serial.println("❌ WebSocket Disconnected!");
            wsConnected = false;
            break;
            
        case WebsocketsEvent::GotPing:
            Serial.println("📶 Got Ping, sending Pong");
            client.pong();
            break;
            
        case WebsocketsEvent::GotPong:
            Serial.println("📶 Got Pong");
            break;
    }
}

void connectToWiFi() {
    Serial.println("🔄 Connecting to WiFi...");
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println();
        Serial.println("✅ WiFi Connected!");
        Serial.print("📡 IP Address: ");
        Serial.println(WiFi.localIP());
        Serial.print("📶 Signal Strength: ");
        Serial.print(WiFi.RSSI());
        Serial.println(" dBm");
    } else {
        wifiConnected = false;
        Serial.println();
        Serial.println("❌ WiFi Connection Failed!");
    }
}

void connectToWebSocket() {
    if (!wifiConnected) return;
    
    Serial.println("🔄 Connecting to WebSocket server...");
    
    client.onMessage(onMessageCallback);
    client.onEvent(onEventsCallback);
    
    bool connected = client.connect(websocket_server);
    
    if (connected) {
        Serial.println("✅ WebSocket connection established");
        wsConnected = true;
    } else {
        Serial.println("❌ WebSocket connection failed");
        wsConnected = false;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("================================");
    Serial.println("🤖 ESP32 Robotic Arm Controller");
    Serial.println("================================");
    
    // Initialize motor control
    setupMotors();
    Serial.println("✅ Motors initialized");
    
    // Connect to WiFi
    connectToWiFi();
    
    // Connect to WebSocket if WiFi is connected
    if (wifiConnected) {
        connectToWebSocket();
    }
    
    lastReconnectAttempt = millis();
    lastHeartbeat = millis();
    
    Serial.println("🎮 Manual control commands:");
    Serial.println("  Shoulder: a(fwd), b(back), s(stop)");
    Serial.println("  Elbow: c(fwd), d(back), s(stop)");
    Serial.println("  Base: f(fwd), g(back), s(stop)");
    Serial.println("  Roller: i(fwd), j(back), s(stop)");
    Serial.println("  Gripper: l(open), m(close), s(stop)");
    Serial.println("  Servo: o(0°), p(90°), q(180°), r(sweep test)");
}

void loop() {
    unsigned long currentTime = millis();
    
    // Check WiFi connection
    if (WiFi.status() != WL_CONNECTED) {
        if (wifiConnected) {
            Serial.println("❌ WiFi connection lost!");
            wifiConnected = false;
            wsConnected = false;
        }
        
        // Try to reconnect WiFi every 5 seconds
        if (currentTime - lastReconnectAttempt > reconnectInterval) {
            connectToWiFi();
            lastReconnectAttempt = currentTime;
        }
    } 
    else {
        wifiConnected = true;
        
        // Check WebSocket connection
        if (!wsConnected && (currentTime - lastReconnectAttempt > reconnectInterval)) {
            connectToWebSocket();
            lastReconnectAttempt = currentTime;
        }
    }
    
    // Poll WebSocket if connected
    if (wsConnected && client.available()) {
        client.poll();
    }
    
    // Process serial commands for manual testing
    processSerialCommands();
    
    // Send heartbeat every 30 seconds
    if (wsConnected && (currentTime - lastHeartbeat > heartbeatInterval)) {
        client.send("ESP32 Robotic Arm - Heartbeat " + String(millis()));
        lastHeartbeat = currentTime;
    }
    
    // Small delay to prevent watchdog issues
    delay(10);
}