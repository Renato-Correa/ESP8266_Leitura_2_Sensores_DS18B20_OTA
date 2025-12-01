/*************************************************************
 * Projeto.: Leitura de temeratura usando 2 sensores DS18B20 *
 *-----------------------------------------------------------*
 * SUMARIO DO PROJETO.:                                      *
 * LEITURA DE 2 SENSORES DE TEMPERATURAS INTERNO E EXTERNO   *
 * USANDO O SENSORES DS18B20 COM OTA                         *
 * USANDO MQTT 192.168.0.18:1883 sys/temp                    *
 * ----------------------------------------------------------*
 * CLIENTE.:   Prototipo                                     *
 * AUTOR  .:   Renato Correa                                 *
 * DATA   .:   20251128                                      *
 * Placa  .:   ARDUINO ESP8266                               *
 * Arquivo.:   NodeMCU_2_Temperatura_DS18B20_OTA.ino         *
 * Versoes.:                                                 *
 * VER  |DESCRICAO                            | DATA         *
 *  0.00|INICIAL                              | 20251128     *
 *  1.50|Ativacao MQTT                        | 20251201     *
 *                                                           *
 *                                                           *
 * Portas utilizadas (SAIDAS).:                              *
 * LED IN BUILD                                              *
 *                                                           *
 * Portas utilizadas (ENTRADAS).:2  SENSORES DS18B20         *
 * D4 - GPIO2                                                *
 *                                                           * 
 * OBS.:                                                     *
 *                                                           *
 *                                                           *
 *                                                           *
 *************************************************************/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>

ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;


// ---------------- CONFIGURAÇÕES ----------------
bool mqttOnline  = false;
const char* ssid = "RLC3G";
const char* password = "yasrlc12";

const char* mqtt_server = "192.168.0.18";
const int   mqtt_port   = 1883;

WiFiClient espClient;
PubSubClient mqtt(espClient);



// DS18B20
#define ONE_WIRE_BUS 2   // GPIO2 (D4)
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);

DeviceAddress sensorInterno;
DeviceAddress sensorExterno;

// NTP atualizado para data completa
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", -10800, 60000); // UTC-3

unsigned long lastSend = 0;
const unsigned long interval = 5000;  // 5 segundos


// ---------------- FUNÇÕES AUXILIARES ----------------

// Converte timestamp do NTP em "YYYY-MM-DD HH:MM:SS"
String getDateTime() {
    timeClient.update();

    unsigned long epoch = timeClient.getEpochTime();

    // Se o NTP ainda não sincronizou
    if (epoch < 100000) {
        return "NTP_SYNCING";
    }

    time_t rawTime = (time_t)epoch;
    struct tm *ptm = gmtime(&rawTime);

    if (ptm == nullptr) {
        return "INVALID_TIME";
    }

    char buffer[25];
    sprintf(buffer, "%04d-%02d-%02d %02d:%02d:%02d",
        ptm->tm_year + 1900,
        ptm->tm_mon + 1,
        ptm->tm_mday,
        timeClient.getHours(),
        timeClient.getMinutes(),
        timeClient.getSeconds()
    );

    return String(buffer);
}


// Temperaturas em JSON
String getTemperaturasJSON() {
    sensors.requestTemperatures();

    float tempInt = sensors.getTempC(sensorInterno);
    float tempExt = sensors.getTempC(sensorExterno);

    String json = "{";
    json += "\"temperatura_interna\": " + String(tempInt, 2) + ",";
    json += "\"temperatura_externa\": " + String(tempExt, 2) + ",";
     json += "\"datetime\": \"" + getDateTime() + "\",";
    json += "\"mqtt_online\": " + String(mqttOnline ? "true" : "false");
    json += "}";

    return json;
}


// ---------------- NOVA PÁGINA HTML BONITA ----------------

String webPage() {
    sensors.requestTemperatures();

    float tempInt = sensors.getTempC(sensorInterno);
    float tempExt = sensors.getTempC(sensorExterno);

    String html = R"====(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>Monitor de Temperatura</title>
<style>
body {
    font-family: Arial;
    background: #f0f0f0;
    text-align: center;
    padding: 30px;
}
.card {
    background: white;
    padding: 20px;
    border-radius: 10px;
    width: 300px;
    margin: auto;
    box-shadow: 0 3px 8px rgba(0,0,0,0.2);
}
h1 { color: #333; }
.value {
    font-size: 2.2rem;
    font-weight: bold;
    color: #0077cc;
}
.label {
    font-size: 1rem;
    margin-top: 10px;
    color: #555;
}
.datahora {
    font-size: 1.2rem;     /* deixa a fonte menor */
    color: #e74c3c;        /* cor mais discreta */
    margin-top: 8px;
    display: block;        /* garante quebra de linha */
    font-weight: bold;     /* deixa em negrito */
    font-style: italic;    /* deixa em itálico */
}
</style>

<script>
function atualizar(){
    fetch("/json")
    .then(r => r.json())
    .then(d => {
        document.getElementById("tempInterna").innerHTML = d.temperatura_interna + " °C";
        document.getElementById("tempExterna").innerHTML = d.temperatura_externa + " °C";
        document.getElementById("datahora").innerHTML = d.datetime;

        // 👉 ATUALIZA O STATUS DO MQTT (ADICIONE AQUI)
        let status = document.getElementById("statusMQTT");

        if (d.mqtt_online) {
            status.innerHTML = "🟢 MQTT ONLINE";
            status.style.color = "green";
        } else {
            status.innerHTML = "🔴 MQTT OFFLINE";
            status.style.color = "red";
        }

    });
}
setInterval(atualizar, 5000);
</script>

</head>

<body onload="atualizar()">
<h1>🌡 Monitor de Temperatura</h1>
<h3>✨ By Renato [RLC] - Ver. 2025-12-01</h3>
<h4>🚨 Termometro usando tambem MQTT 🚨</h4>
<h5>[💥 192.168.0.18:1883 ❗ Topico: sys/temp ]</h5>

<div class="card">
    <div class="label">Temperatura Interna 🌡</div>
    <div id="tempInterna" class="value">--</div>

    <div class="label">Temperatura Externa 🌡</div>
    <div id="tempExterna" class="value">--</div>

    <div class="label">⏳ Data e Hora</div>
    <div id="datahora" class="datahora">--</div>
</div>

<div id="statusMQTT" 
     style="margin:20px; font-size:1.1rem; font-weight:bold;">
     Verificando MQTT...
</div>

<p>Atualiza automaticamente a cada 5 segundos.</p>

</body>
</html>
)====";

    return html;
}


// ---------------- CONFIGURAÇÕES DO WEBSERVER ----------------

void handleRoot() {
    server.send(200, "text/html", webPage());
}

void handleJSON() {
    server.send(200, "application/json", getTemperaturasJSON());
}


// ---------------- MQTT ----------------

unsigned long lastMQTTAttempt = 0;

void tryConnectMQTT() {
    if (mqtt.connected()) return;

    unsigned long now = millis();
    if (now - lastMQTTAttempt < 20000) return;  // tenta conectar a cada 20s

    lastMQTTAttempt = now;

    Serial.print("Tentando conectar ao MQTT... ");
    if (mqtt.connect("NodeMCU_Temperaturas")) {
        Serial.println("OK!");
        mqttOnline = true;   // broker online
    } else {
        Serial.println("Falhou (broker offline).");
        mqttOnline = false;  // broker offline
    }
}


// ---------------- SETUP ----------------




void setup() {
    Serial.begin(9600);
    delay(1000);
    Serial.println("\n\n** START - by RLC VER 00 - Medicao de temperatura com 2 sensores DS18B20 **");
    Serial.println(ssid);
    Serial.println(password);

    // WIFI
    WiFi.begin(ssid, password);
    Serial.println("\nCONECTANDO NO WIFI!");
    while (WiFi.status() != WL_CONNECTED) {
        delay(400);
        Serial.print(".");
    }
    Serial.println("\nWiFi conectado!");
    Serial.println(WiFi.localIP());

    // Web Server
    server.on("/", handleRoot);
    server.on("/json", handleJSON);

    // 📌 >>>>>>> AQUI ENTRA O OTA <<<<<<<<
    httpUpdater.setup(&server);   // habilita upload do firmware via /update
    // 🔐 (Opcional) — com usuário e senha:
    // httpUpdater.setup(&server, "admin", "1234");
    // 📌 >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

    server.begin();
    Serial.println("Servidor HTTP iniciado!");

    // Sensores
    sensors.begin();
    sensors.getAddress(sensorInterno, 0);
    sensors.getAddress(sensorExterno, 1);

    // NTP
    timeClient.begin();
    timeClient.update();

    // MQTT
    mqtt.setServer(mqtt_server, mqtt_port);
}


// ---------------- LOOP ----------------

void loop() {

    server.handleClient();
    timeClient.update();

    // tenta conectar sem travar
    tryConnectMQTT();

    if (mqtt.connected()) {
        mqtt.loop();
    }

    // Envio MQTT a cada 5 segundos
    unsigned long now = millis();
    if (now - lastSend >= interval) {
        lastSend = now;
        String payload = getTemperaturasJSON();
        Serial.println("LEITURA: " + payload);

        if (mqtt.connected()) {
            mqtt.publish("sys/temp", payload.c_str());
            Serial.println("MQTT enviado: " + payload);
        } else {
            Serial.println("MQTT OFFLINE – envio ignorado");
        }
    }
}

