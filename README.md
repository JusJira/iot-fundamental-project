# IoT Fundamentals Final Project

## Introduction

This project is part of the 2102541 IoT Fundamentals course

### Contents

- [Members](#members)
- [System Architecture](#system-architecture)
  - [System Diagram](#system-diagram)
  - [Data Flow](#data-flow)
- [System Components](#system-components)
  - [ESP32](#esp32)
  - [Raspberry Pi](#raspberry-pi)
  - [Cloud Server](#cloud-server-iotcloudserve)
- [Source Code](#source-code)
  - [ESP32 Code](#esp32-code)
  - [Consumer](#consumer)
  - [Python Prediction Model](#python-prediction-model)
  - [Command Center](#command-center)

## Members

- Justin Jiraratsakul 6430039021
- Chayuth Thanakitkoses 6430068221
- Nantiya Tanasupawat 6430207121
- Sarith Rapeearpakul 6430408021

## System Architecture

### System Diagram

![Diagram](/Diagram.svg)

### Data Flow

![Data Flow](/Dataflow-eraser.svg)

### System Components

#### ESP32

- Cucumber RS
- Acts as a Sensor
- Pushes data to an MQTT server ([Raspberry Pi](#raspberry-pi))

#### Raspberry Pi

- EDGE Server
- Docker Container
  - MQTT Broker ([EMQX](https://www.emqx.io/))
  - MQTT Consumer (Reads data from MQTT and sends it to InfluxDB on [Cloud Server](#cloud-server-iotcloudserve))
  - Command Center
    - Polls the data from InfluxDB and sends a trigger to the Sensors via MQTT

#### Cloud Server (IoTCloudServe)

- Kubernetes Cluster
  - [InfluxDB](https://hub.docker.com/_/influxdb) (Time-Series Database)
  - [Grafana](https://grafana.com/docs/grafana/latest/setup-grafana/installation/docker/) (Dashboard)
  - [Python Prediction Model](#python-prediction-model) (Using [Flask](https://flask.palletsprojects.com/en/3.0.x/) as API Endpoint)

## Source Code

### ESP32 Code

- Reads data from the sensors and sends it to an MQTT Server (Raspberry Pi)
- [main.ino](ESP32/main.ino) sends the data directly without relying on the queue
- [queue.ino](ESP32/queue.ino) adds the data to the queue and sends it when time is up (Using XTask)
- Multiple sensor support
  - Assign each sensor different topics (@msg/data/1,@msg/data/2,@msg/data/3 and @msg/data/4)

### Consumer

- Reads data from MQTT and uploads it to InfluxDB, also triggering the prediction model
- Deployed on the Raspberry Pi as an EDGE Server via Docker
- [Consumer.py](Consumer/Consumer.py)
- Edit the .env with your variables before using

### Python Prediction Model

- Receive requests from the [Consumer](#consumer) and then fetch data from InfluxDB to make a prediction, then save the prediction on InfluxDB
- [RealTimePrediction.py](Prediction/RealTimePrediction.py) this code is used as an API to receive requests from the Consumer
- [IoT_fun_Final.ipynb](PredictionTraining/IoT_fun_Final.ipynb) this code was used to train the model

### Command Center

- Polls the data from InfluxDB and sends it to the sensors via MQTT

## Deployment

### Dashboard

The dashboard is deployed on the cloud server you can visit the page via this [link](https://iot-group5-service3.iotcloudserve.net/public-dashboards/ac62c72b7e56497080056afb10efc0eb)

#### Preview

![Dashboard Preview 1](/IMG_6144.PNG)
![Dashboard Preview 2](/IMG_6145.PNG)
