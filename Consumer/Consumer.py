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
MQTT_PUBLISH_TOPIC = "@msg/data"
print("connecting to MQTT Broker", MQTT_BROKER_URL)
mqttc = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
mqttc.connect(MQTT_BROKER_URL,1883)

# REST API endpoint for predicting output
predict_url = os.environ.get('PREDICT_URL')
 
def on_connect(client, userdata, flags, rc, properties):
    """ The callback for when the client connects to the broker."""
    print("Connected with result code "+str(rc))
 
# Subscribe to a topic
# mqttc.subscribe(MQTT_PUBLISH_TOPIC)
mqttc.subscribe([("@msg/data", 0), ("@msg/data1", 0)])
 
def on_message(client, userdata, msg):
    """ The callback for when a PUBLISH message is received from the server."""
    print(msg.topic+" "+str(msg.payload))

    # Write data in InfluxDB
    payload = json.loads(msg.payload)
    # write_to_influxdb(payload)

    # POST data to predict the output label
    json_data = json.dumps(payload)
    # post_to_predict(json_data)

# Function to post to real-time prediction endpoint
# def post_to_predict(data):
#     response = requests.post(predict_url, data=data)
#     if response.status_code == 200:
#         print("POST request successful")
#     else:
#         print("POST request failed!", response.status_code)

# Function to write data to InfluxDB
# def write_to_influxdb(data):
#     # format data
#     point = Point("sensor_data")\
#         .field("S1_Temp", data["S1_Temp"])\
#         .field("S2_Temp", data["S2_Temp"])\
#         .field("S3_Temp", data["S3_Temp"])\
#         .field("S4_Temp", data["S4_Temp"])\
#         .field("S1_Light", data["S1_Light"])\
#         .field("S2_Light", data["S2_Light"])\
#         .field("S3_Light", data["S3_Light"])\
#         .field("S4_Light", data["S4_Light"])\
#         .field("S1_Sound", data["S1_Sound"])\
#         .field("S2_Sound", data["S2_Sound"])\
#         .field("S3_Sound", data["S3_Sound"])\
#         .field("S4_Sound", data["S4_Sound"])\
#         .field("S5_CO2", data["S5_CO2"])\
#         .field("S5_CO2_Slope", data["S5_CO2_Slope"])\
#         .field("S6_PIR", data["S6_PIR"])\
#         .field("S7_PIR", data["S7_PIR"])\
#         .field("Room_Occupancy_Count", data["Room_Occupancy_Count"])

#     write_api.write(BUCKET, os.environ.get('INFLUXDB_ORG'), point)

## MQTT logic - Register callbacks and start MQTT client
mqttc.on_connect = on_connect
mqttc.on_message = on_message
mqttc.loop_forever()
