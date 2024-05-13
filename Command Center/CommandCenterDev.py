import random
from dotenv import load_dotenv
import paho.mqtt.client as mqtt
import os
import schedule
import json
import time

load_dotenv()

polling_time = 30 # 30 Seconds


# MQTT broker config
MQTT_BROKER_URL = os.environ.get('MQTT_URL')
print("connecting to MQTT Broker", MQTT_BROKER_URL)
mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.connect(MQTT_BROKER_URL,1883)

topic = "@msg/cc"

def job():
    for sensor_number in range(1, 4):
        temp = random.uniform(25, 40)
        humid = random.uniform(40, 80)
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
