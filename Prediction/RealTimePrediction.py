"""
Real-Time prediction
Retrieve streaming data from the consumer and predict the number 
of people inside the room. Utilize Flask, a simple REST API server, 
as the endpoint for data feeding.
"""

# Importing relevant modules
import joblib
import pandas as pd
from flask import Flask, request, jsonify
from dotenv import load_dotenv
import os
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import ASYNCHRONOUS
import json

# Load the trained model
knn_model_temp = joblib.load('stacking_temp_model.pkl')
knn_model_humidity = joblib.load('stacking_humid_model.pkl')

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
query_api = client.query_api()

# Create simple REST API server
app = Flask(__name__)

# Default route: check if model is available.
@app.route('/')
def check_model():
    if knn_model_temp and knn_model_humidity:
        return "Model is ready for prediction"
    return "Server is running but something wrongs with the model"

# Predict route: predict the output from streaming data
@app.route('/predict', methods=['POST'])
def predict():
    try:
        header_value = request.headers.get('Sensor-Number')
        print('Header:',header_value)
        # Get JSON text from the request
        json_text = request.data
        # Convert JSON text to JSON object
        json_data = json.loads(json_text)
        # Add to dataFrame
        query = pd.DataFrame([json_data])
        # print(query)
        # Get last 2 data from InfluxDB and 1 Humidity
        fluxQueryTemp = 'from(bucket: "sensor_data")|> range(start: -1h)|> filter(fn: (r) => r["_measurement"] == "sensor_{}")|> filter(fn: (r) => r["_field"] == "Temperature")|> tail(n:3)'.format(header_value)
        fluxQueryHumidity = 'from(bucket: "sensor_data")|> range(start: -1h)|> filter(fn: (r) => r["_measurement"] == "sensor_{}")|> filter(fn: (r) => r["_field"] == "Humidity")|> last()'.format(header_value)
        resultTemp = query_api.query(query=fluxQueryTemp)
        resultHumid = query_api.query(query=fluxQueryHumidity)
        sensorDataTemp = {}
        for table in resultTemp:
            i = 3
            for row in table.records:
                sensorDataTemp['temp_lag_'+str(i)] = row["_value"]
                i -= 1
                
        for table in resultHumid:
            for row in table.records:
                sensorDataTemp['humidity_lag_1'] = (row["_value"])
                
        # print(sensorDataTemp)
        
        # Get last 2 Humidity from InfluxDB and 1 Temperature
        fluxQueryTemp_1 = 'from(bucket: "sensor_data")|> range(start: -1h)|> filter(fn: (r) => r["_measurement"] == "sensor_{}")|> filter(fn: (r) => r["_field"] == "Temperature")|> last()'.format(header_value)
        fluxQueryHumidity_3 = 'from(bucket: "sensor_data")|> range(start: -1h)|> filter(fn: (r) => r["_measurement"] == "sensor_{}")|> filter(fn: (r) => r["_field"] == "Humidity")|> tail(n:3)'.format(header_value)
        resultTemp_1 = query_api.query(query=fluxQueryTemp_1)
        resultHumid_3 = query_api.query(query=fluxQueryHumidity_3)
        
        sensorDataHumidity = {}
        for table in resultHumid_3:
            i = 3
            for row in table.records:
                sensorDataHumidity['humidity_lag_'+str(i)] = row["_value"]
                i -= 1
                
        for table in resultTemp_1:
            for row in table.records:
                sensorDataHumidity['temp_lag_1'] = (row["_value"])
        
        # print(sensorDataHumidity)
        # Extract features and label from data
        # Temperature
        dfTemp = pd.DataFrame([sensorDataTemp])
        dfTemp = dfTemp[['temp_lag_1','temp_lag_2','temp_lag_3', 'humidity_lag_1']]
        # Humidity
        dfHumidity = pd.DataFrame([sensorDataHumidity])
        dfHumidity = dfHumidity[['temp_lag_1', 'humidity_lag_1','humidity_lag_2','humidity_lag_3']]
        
        feature_sample_temp = dfTemp
        feature_sample_humidity = dfHumidity
        target_temp = query['temperature'][0]
        target_humidity = query['humidity'][0]
        # Predict the number of people inside the room
        # predict_sample = knn_model.predict(feature_sample)
        predict_temperature = knn_model_temp.predict(feature_sample_temp)
        predict_humidity = knn_model_humidity.predict(feature_sample_humidity)
        print("Actual Temperature", int(target_temp),
              "\tPredicted Temperature:", int(predict_temperature[0]),
              "Actual Humidity", int(target_humidity),
              "\tPredicted Humidity:", int(predict_humidity[0]))
        # Assign the true label and predicted label into Point
        point = Point("predict_sensor_{}".format(header_value))\
            .field("Predicted Temperature", predict_temperature[0])\
            .field("Predicted Humidity", predict_humidity[0])
        
        # Write that Point into database
        write_api.write(BUCKET, os.environ.get('INFLUXDB_ORG'), point)
        return jsonify({"Actual Temperature": round(float(target_temp),2), 
                        "Predicted Temperature": round(float(predict_temperature[0]),2),
                        "Actual Humidity": round(float(target_humidity),2),
                        "Predicted Humidity": round(float(predict_humidity[0]),2)}), 200
    
    except:
        # Something error with data or model
        return "Recheck the data", 400
    
if __name__ == '__main__':
    app.run(port=5555, debug=True)