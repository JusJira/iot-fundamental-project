"""
Consumer File
Listen to the subscribed topic, store data in the database, 
and feed streaming data to the real-time prediction algorithm.
"""

# Importing relevant modules
import os
from dotenv import load_dotenv
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import ASYNCHRONOUS
import paho.mqtt.client as mqtt
import json
import requests

# Load environment variables from ".env"
load_dotenv()

# InfluxDB config
BUCKET = os.environ.get('INFLUXDB_BUCKET')
print("connecting to",os.environ.get('INFLUXDB_URL'))
client = InfluxDBClient(
    url=str(os.environ.get('INFLUXDB_URL')),
    token=str(os.environ.get('INFLUXDB_TOKEN')),
    org=os.environ.get('INFLUXDB_ORG')
)
write_api = client.write_api()
 
# MQTT broker config
MQTT_BROKER_URL = os.environ.get('MQTT_URL')
print("connecting to MQTT Broker", MQTT_BROKER_URL)
mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.connect(MQTT_BROKER_URL,1883)

# REST API endpoint for predicting output
predict_url = os.environ.get('PREDICT_URL')
 
def on_connect(client, userdata, flags, rc, properties):
    """ The callback for when the client connects to the broker."""
    print("Connected with result code "+str(rc))
 
# Subscribe to a topic
mqttc.subscribe([("@msg/data/1", 0),("@msg/data/2", 0),("@msg/data/3", 0)])
 
def on_message(client, userdata, msg):
    """ The callback for when a PUBLISH message is received from the server."""
    print(msg.topic+" "+str(msg.payload))
    sensor_point = msg.topic.split("/")[-1]

    # Write data in InfluxDB
    payload = json.loads(msg.payload)
    write_to_influxdb(payload,sensor_point)

    # POST data to predict the output label
    json_data = json.dumps(payload)
    print(json_data)
    post_to_predict(json_data, sensor_point)

# Function to post to real-time prediction endpoint
def post_to_predict(data,sensor_point):
    response = requests.post(predict_url, data=data, headers={'Sensor-Number': sensor_point})
    if response.status_code == 200:
        print("POST request successful")
    else:
        print("POST request failed!", response.status_code)

# Function to write data to InfluxDB
def write_to_influxdb(data, sensor_point):
    # format data
    point = Point("sensor_"+sensor_point)\
        .field("Temperature", data["temperature"])\
        .field("Humidity", data["humidity"])\
        .field("Pressure", data["pressure"])\

    write_api.write(BUCKET, os.environ.get('INFLUXDB_ORG'), point)

## MQTT logic - Register callbacks and start MQTT client
mqttc.on_connect = on_connect
mqttc.on_message = on_message
mqttc.loop_forever()
