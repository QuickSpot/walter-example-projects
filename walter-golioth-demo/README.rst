..
   Copyright (c) 2024 DPTechnics
   SPDX-License-Identifier: Apache-2.0

Walter Golioth Demo
###################

Overview
********

This repo contains a demo application for using Walter with the Golioth IoT cloud.
It is based on the `Golioth Reference Design Template`_ and also contains the 
needed board definitions to use Walter with the Zephyr RTOS.

The application reads temperature and humidity measurements from a DHT20 sensor
and sends the measurements as a CBOR object to the Golioth LightDB Stream service.

Local set up
************

.. pull-quote::
   [!IMPORTANT]

   Do not clone this repo using git. Zephyr's ``west`` meta tool should be used to
   set up your local workspace.

Install the Python virtual environment (recommended)
====================================================

.. code-block:: shell

   cd ~
   mkdir walter-golioth-demo
   python -m venv walter-golioth-demo/.venv
   source walter-golioth-demo/.venv/bin/activate
   pip install wheel west

Use ``west`` to initialize and install
======================================

.. code-block:: shell

   cd ~/walter-golioth-demo
   west init -m https://github.com/QuickSpot/walter-golioth-demo.git .
   west update
   west zephyr-export
   pip install -r deps/zephyr/scripts/requirements.txt

Building the application
************************

Enter your Golioth device credentials in the proj.conf file and build the application:

.. code-block:: text

   $ (.venv) west build -p -b walter/esp32s3/procpu app
   $ (.venv) west flash
   $ (.venv) west espressif monitor

.. _Golioth Reference Design Template: https://github.com/golioth/reference-design-template
