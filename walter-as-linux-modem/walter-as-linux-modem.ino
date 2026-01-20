/**
 * @file walter-as-linux-modem.ino
 * @author Daan Pape <daan@dptechnics.com>
 * @date 02 Jan 2026
 * @copyright DPTechnics bv
 * @brief Walter Modem library examples
 *
 * @section LICENSE
 *
 * Copyright (C) 2023, DPTechnics bv
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 * 
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 * 
 *   3. Neither the name of DPTechnics bv nor the names of its contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
 * 
 *   4. This software, with or without modification, must only be used with a
 *      Walter board from DPTechnics bv.
 * 
 *   5. Any software provided in binary form under this license must not be
 *      reverse engineered, decompiled, modified and/or disassembled.
 * 
 * THIS SOFTWARE IS PROVIDED BY DPTECHNICS BV “AS IS” AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL DPTECHNICS BV OR CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * @section DESCRIPTION
 * 
 * This file contains firmware for Walter that connects the GM02SP serial port 
 * to pins 8 (RX) and 9 (TX) of Walter. This allows Walter to be used as a modem
 * with a standard AT interface over PPP-over-serial connection.
 */

/**
 * @brief The RX pin on which modem data is received.
 */
#define WALTER_MODEM_PIN_RX 14

/**
 * @brief The TX to which modem data must be transmitted.
 */
#define WALTER_MODEM_PIN_TX 48

/**
 * @brief The RTS pin on the ESP32 side.
 */
#define WALTER_MODEM_PIN_RTS 21

/**
 * @brief The CTS pin on the ESP32 size.
 */
#define WALTER_MODEM_PIN_CTS 47

/**
 * @brief The active low modem reset pin.
 */
#define WALTER_MODEM_PIN_RESET 45

/**
 * @brief The pin to which the TX of the host must be connected.
 */
#define HOST_SERIAL_PIN_RX 8

/**
 * @brief The pin to which the RX of the host must be connected.
 */
#define HOST_SERIAL_PIN_TX 9

/**
 * @brief The serial interface to talk to the modem.
 */
#define ModemSerial Serial1

/**
 * @brief The serial interface to talk to the host.
 */
#define HostSerial Serial2

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  pinMode(WALTER_MODEM_PIN_RX, INPUT);
  pinMode(WALTER_MODEM_PIN_TX, OUTPUT);
  pinMode(WALTER_MODEM_PIN_CTS, INPUT);
  pinMode(WALTER_MODEM_PIN_RTS, OUTPUT);
  pinMode(WALTER_MODEM_PIN_RESET, OUTPUT);

  ModemSerial.begin(
    921600,
    SERIAL_8N1,
    WALTER_MODEM_PIN_RX,
    WALTER_MODEM_PIN_TX,
    false,
    WALTER_MODEM_PIN_RTS,
    WALTER_MODEM_PIN_CTS);

  HostSerial.begin(
    921600,
    SERIAL_8N1,
    HOST_SERIAL_PIN_RX,
    HOST_SERIAL_PIN_TX
  );

  gpio_hold_dis((gpio_num_t) WALTER_MODEM_PIN_RESET);
  digitalWrite(WALTER_MODEM_PIN_RESET, LOW);
  delay(1000);
  digitalWrite(WALTER_MODEM_PIN_RESET, HIGH);
  gpio_hold_en((gpio_num_t) WALTER_MODEM_PIN_RESET);

  Serial.println("Ready for PPP session");
}

void loop() {
  if(HostSerial.available()) {
    int x = HostSerial.read();
    ModemSerial.write(x);
  }

  if(ModemSerial.available()) {
    int x = ModemSerial.read();
    HostSerial.write(x);
  }
}