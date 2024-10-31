#!/usr/bin/env python3
# -*-coding:utf-8-*-

# Copyright 2021 Espressif Systems (Shanghai) PTE LTD
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

# WARNING: we don't check for Python build-time dependencies until
# check_environment() function below. If possible, avoid importing
# any external libraries here - put in external script, or import in
# their specific function instead.

import sys
import csv
import json
import argparse
import pandas as pd
import numpy as np
import matplotlib.pyplot as pp

import serial
from os import path
from io import StringIO

from PyQt5.Qt import *
from pyqtgraph import PlotWidget
from PyQt5 import QtCore
import pyqtgraph as pq

import threading
import time
import mdet

# Reduce displayed waveforms to avoid display freezes
CSI_VAID_SUBCARRIER_INTERVAL = 1

# Remove invalid subcarriers
# secondary channel : below, HT, 40 MHz, non STBC, v, HT-LFT: 0~63, -64~-1, 384
csi_vaid_subcarrier_color = []
color_step = 255 // (28 // CSI_VAID_SUBCARRIER_INTERVAL + 1)

LEGACY_LLTF_MASK = np.ones(64, dtype=bool)
LEGACY_LLTF_MASK[:1] = False
LEGACY_LLTF_MASK[27:27+10] = False
LEGACY_LLTF_MASK[63:] = False


LLTF_MASK = np.ones(64, dtype=bool)
LLTF_MASK[:6] = False
LLTF_MASK[32:33] = False
LLTF_MASK[59:] = False

# LLTF: 52
csi_vaid_subcarrier_color += [(i * color_step, 0, 0) for i in range(1,  26 // CSI_VAID_SUBCARRIER_INTERVAL + 2)]
csi_vaid_subcarrier_color += [(0, i * color_step, 0) for i in range(1,  26 // CSI_VAID_SUBCARRIER_INTERVAL + 2)]
CSI_DATA_LLFT_COLUMNS = 26

CSI_DATA_INDEX = 50  # buffer size
CSI_DATA_COLUMNS = CSI_DATA_LLFT_COLUMNS
DATA_COLUMNS_NAMES = ["type", "id", "mac", "rssi", "rate", "noise_floor", "channel", "local_timestamp", "sig_len", "rx_state", "len", "first_word", "data"]
csi_data_array = np.zeros(
    [CSI_DATA_INDEX, CSI_DATA_COLUMNS], dtype=np.float32)

class csi_data_graphical_window(QWidget):
    def __init__(self):
        super().__init__()

        self.resize(1280, 720)
        self.plotWidget_ted = PlotWidget(self)
        self.plotWidget_ted.setGeometry(QtCore.QRect(0, 0, 1280, 720))

        self.plotWidget_ted.setYRange(-20, 100)
        self.plotWidget_ted.addLegend()

        self.curve_list = []

        # print(f"csi_vaid_subcarrier_color, len: {len(csi_vaid_subcarrier_color)}, {csi_vaid_subcarrier_color}")

        for i in range(CSI_DATA_COLUMNS):
            curve = self.plotWidget_ted.plot(
                csi_data_array[:, i], name=str(i), pen=csi_vaid_subcarrier_color[i])
            self.curve_list.append(curve)

        self.timer = pq.QtCore.QTimer()
        self.timer.timeout.connect(self.update_data)
        self.timer.start(100)

    def update_data(self):
        
        #pp.show()
        for i in range(CSI_DATA_COLUMNS):
            self.curve_list[i].setData(csi_data_array[:, i])


def csi_data_read_parse(port: str, csv_writer, log_file_fd):
    ser = serial.Serial(port=port, baudrate=115200,
                        bytesize=8, parity='N', stopbits=1)
    if ser.isOpen():
        print("open success")
    else:
        print("open failed")
        return
    
    detector = mdet.CSIMagnitudeDetector()

    while True:
        strings = str(ser.readline())
        if not strings:
            break
        strings = strings.lstrip('b\'').rstrip('\\r\\n\'')
        index = strings.find('CSI_DATA')

        if index == -1:
            # Save serial output other than CSI data
            #log_file_fd.write(strings + '\n')
            #log_file_fd.flush()
            continue

        csv_reader = csv.reader(StringIO(strings))
        csi_data = next(csv_reader)

        if len(csi_data) != len(DATA_COLUMNS_NAMES):
            print("element number is not equal")
            #log_file_fd.write("element number is not equal\n")
            #log_file_fd.write(strings + '\n')
            #log_file_fd.flush()
            continue
        
        try:
            csi_raw_data = json.loads(csi_data[-1])
        except json.JSONDecodeError:
            print("data is incomplete")
            #log_file_fd.write("data is incomplete\n")
            #log_file_fd.write(strings + '\n')
            #log_file_fd.flush()
            continue

        # Reference on the length of CSI data and usable subcarriers
        # https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/wifi.html#wi-fi-channel-state-information
        if len(csi_raw_data) != 128 and len(csi_raw_data) != 256 and len(csi_raw_data) != 384:
            print(f"element number is not equal: {len(csi_raw_data)}")
            #log_file_fd.write(f"element number is not equal: {len(csi_raw_data)}\n")
            #log_file_fd.write(strings + '\n')
            #log_file_fd.flush()
            continue

        #csv_writer.writerow(csi_data)

        he = int(csi_data[-2])
        x = np.array(csi_raw_data, dtype=np.float32)
        z = x.reshape(-1, 2).view(np.complex64)

        if he:
            x = np.squeeze(z[LLTF_MASK])
        else:
            x = np.squeeze(z[LEGACY_LLTF_MASK])

        x = x[:26]
        y = np.abs(x)

        #pp.plot(y[:26])

        is_present, confidence = detector.detect_presence(y)
        if is_present:
            print(f"Human presence detected! (Confidence: {confidence:.2f}, thresh: {detector.threshold:.2f})")
        else:
            print(f"No presence detected. (Confidence: {confidence:.2f}, thresh: {detector.threshold:.2f})")

        # Rotate data to the left
        csi_data_array[:-1] = csi_data_array[1:]
        csi_data_array[-1] = y

    ser.close()
    return


class SubThread (QThread):
    def __init__(self, serial_port, save_file_name, log_file_name):
        super().__init__()
        self.serial_port = serial_port

        #save_file_fd = open(save_file_name, 'w')
        #self.log_file_fd = open(log_file_name, 'w')
        self.csv_writer = None #csv.writer(save_file_fd)
        #self.csv_writer.writerow(DATA_COLUMNS_NAMES)

    def run(self):
        csi_data_read_parse(self.serial_port, None, None)

    def __del__(self):
        self.wait()
        #self.log_file_fd.close()


if __name__ == '__main__':
    if sys.version_info < (3, 6):
        print(" Python version should >= 3.6")
        exit()

    parser = argparse.ArgumentParser(
        description="Read CSI data from serial port and display it graphically")
    parser.add_argument('-p', '--port', dest='port', action='store', required=True,
                        help="Serial port number of csv_recv device")
    parser.add_argument('-s', '--store', dest='store_file', action='store', default='./csi_data.csv',
                        help="Save the data printed by the serial port to a file")
    parser.add_argument('-l', '--log', dest="log_file", action="store", default="./csi_data_log.txt",
                        help="Save other serial data the bad CSI data to a log file")

    args = parser.parse_args()
    serial_port = args.port
    file_name = args.store_file
    log_file_name = args.log_file

    app = QApplication(sys.argv)

    subthread = SubThread(serial_port, file_name, log_file_name)
    subthread.start()

    window = csi_data_graphical_window()
    window.show()

    sys.exit(app.exec())
