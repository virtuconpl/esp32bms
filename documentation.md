request:
0x68·0x31·0xCE·0x68·0x02·0x02·0x5C·0x60·0x8F·0x16

response

0x68·0x31·0xCE·0x68·0x82·0x2D·0x51·0xA7·0x33·0x37·0x34·0x1E·0x33·0x33·0x33·0x33·0x67·0x33·0x34·0x32·0x4D·0x4D·0x4D·0x4D·0x4D·0x41·0xF6·0x41·0xF3·0x41·0xF1·0x41·0xEB·0x41·0xEF·0x41·0x05·0x41·0x05·0x41·0xFD·0x41·0xFA·0x41·0xFA·0x41·0xFC·0x41·0xF7·0x41·0xFE·0x6C·0x16


============================================

BMS UART PROTOCOL – FRAME 0x82 (STATUS)

--------------------------------------------------
GENERAL FRAME STRUCTURE
--------------------------------------------------

[0]   0x68                // Start
[1]   0x31
[2]   0xCE                // Address
[3]   0x68                // Start (repeat)

[4]   0x82                // Frame type (Status)
[5]   LEN                 // Payload length (e.g. 0x2D = 45 bytes)

[6..] PAYLOAD             // Encoded with +0x33 offset

[...] CHECKSUM
[last] 0x16              // End

--------------------------------------------------
ENCODING
--------------------------------------------------

All payload bytes are encoded:

REAL_VALUE = BYTE - 0x33

--------------------------------------------------
PAYLOAD STRUCTURE
--------------------------------------------------

[0–5]   GLOBAL PARAMETERS (PARTIALLY UNKNOWN)
        Likely:
        - Pack voltage (2 bytes)
        - Current (2 bytes, signed)
          • Positive → charging
          • Negative → discharging / load
        - Status / flags (2 bytes)

[6–9]   RESERVED / PADDING
        Always:
        0x33 0x33 0x33 0x33 → 0x00 0x00 0x00 0x00

[10]    SOC (State of Charge)
        SOC = BYTE - 0x33

        Examples:
        0x7A → 71%
        0x67 → 52%
        0x6A → 55% (przy lekkim obciążeniu silnika)

[11–15] TEMPERATURE SENSORS (CONFIRMED)
        5 sensors total

        TEMP[°C] = BYTE - 0x33

        Observed behavior:
        - Czujniki reagują na podłączenie ładowarki (prąd ładowania)
        - Czujniki reagują na delikatne obciążenie (silnik)
        - W niektórych warunkach 0–1 °C w spoczynku
        - Typowe temp pracy 20–30 °C

[16–18] STATUS / FLAGS (PARTIALLY UNKNOWN)
        Example decoded:
        0x00 0x01 0xFF

        Likely:
        - BMS state
        - charging/discharging flags
        - sensor presence

[19–44] CELL VOLTAGES (CRITICAL SECTION)
        13 cells (13S), 2 bytes per cell

        For each cell:
        MSB = byte1 - 0x33
        LSB = byte2 - 0x33

        value = (MSB << 8) | LSB
        voltage = value / 1000

        Resolution: 1 mV

--------------------------------------------------
CHECKSUM
--------------------------------------------------

Checksum is calculated as:

CS = sum(all bytes from first 0x68 to last payload byte) & 0xFF

--------------------------------------------------
REQUEST → RESPONSE MAPPING
--------------------------------------------------

Request:
0x68 31 CE 68 02 02 5C 60 8F 16

Response:
0x68 31 CE 68 82 ...

Meaning:
0x02 → status request
0x82 → status response

--------------------------------------------------
CURRENT REVERSE ENGINEERING STATUS
--------------------------------------------------

✔ Offset encoding (+0x33)
✔ SOC position
✔ Cell voltages (13S, 1 mV resolution)
✔ Temperature sensors (5x, °C)
✔ Current / load observable (signed)
✔ Request/response mapping

--------------------------------------------------
NOTES
--------------------------------------------------

- Frame is sufficient to emulate real BMS behavior
- Only dynamic fields needed for emulator:
  - SOC
  - Cell voltages
  - Temperatures
  - Current (optional, for load/charge simulation)

- Remaining fields can be static (for now)
- Temperature sensors respond to:
  • ładowanie (prąd +1–2 A)
  • delikatne obciążenie silnikiem (prąd ~-1–2 A)
- Cell voltage sequence remains stable, zmiany napięć pod obciążeniem w mV

- Observed currents:
  • 0x19 → ~1.9 A ładowania
  • 0x17 → ~1.7 A ładowania
  • ujemne wartości przy lekkim rozładowaniu przez silnik