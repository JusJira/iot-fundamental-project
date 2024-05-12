from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import ASYNCHRONOUS
from dotenv import load_dotenv
import paho.mqtt.client as mqtt
import os
import schedule
import json
import time

load_dotenv()

polling_time = 30 # 30 Seconds

# InfluxDB Config
BUCKET = os.environ.get('INFLUXDB_BUCKET')
print("connecting to",os.environ.get('INFLUXDB_URL'))
client = InfluxDBClient(
    url=str(os.environ.get('INFLUXDB_URL')),
    token=str(os.environ.get('INFLUXDB_TOKEN')),
    org=os.environ.get('INFLUXDB_ORG')
)
query_api = client.query_api()

# MQTT broker config
MQTT_BROKER_URL = os.environ.get('MQTT_URL')
print("connecting to MQTT Broker", MQTT_BROKER_URL)
mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.connect(MQTT_BROKER_URL,1883)

topic = "@msg/cc"
message = "Your message"

def job():
    for sensor_number in range(1, 4):
        fluxQueryTemperature = 'from(bucket: "sensor_data")|> range(start: -2d)|> filter(fn: (r) => r["_measurement"] == "predict_sensor_{}")|> filter(fn: (r) => r["_field"] == "Predicted Temperature")|> last()'.format(sensor_number)
        fluxQueryHumidity = 'from(bucket: "sensor_data")|> range(start: -2d)|> filter(fn: (r) => r["_measurement"] == "predict_sensor_{}")|> filter(fn: (r) => r["_field"] == "Predicted Humidity")|> last()'.format(sensor_number)
        resultTemp = query_api.query(query=fluxQueryTemperature)
        resultHumid = query_api.query(query=fluxQueryHumidity)
        for table in resultTemp:
            for row in table.records:
                temp = row["_value"]
        for table in resultHumid:
            for row in table.records:
                humid = row["_value"]
        messageJson = {
            "sensor": sensor_number,
            "predictedTemp": round(temp,3),
            "predictedHumidity": round(humid,3)
        }
        mqttc.publish(topic, json.dumps(messageJson))
    
schedule.every(polling_time).seconds.do(job)
# schedule.every(1).seconds.do(job)

while True:
    schedule.run_pending()
    time.sleep(1)
