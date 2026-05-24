# 3D Scanner UI & Firmware

A complete firmware solution and graphical user interface for an automated, photogrammetry-based 3D scanner. This system utilizes an Arduino Mega to synchronize four independent stepper motor axes with automated camera shutter triggering, controlled entirely via a modern, touch-enabled TFT interface.

Developed as part of the **U34 Research Project**.

## Features

*   **Modern Touch Interface:** A custom-designed "OLED Cyber" themed UI with responsive buttons, nested menus, and real-time state feedback.
*   **4-Axis Motion Control:** Independent, non-blocking control of the rotary table, dual elevator lead screws, and camera carriage using the `AccelStepper` library.
*   **Automated Photogrammetry:** Synchronized continuous table rotation with timed camera shutter releases via an onboard relay.
*   **Absolute Positioning & Homing:** Built-in homing sequences using mechanical limit switches for both the elevator (Z-axis) and camera reference points.
*   **Live Diagnostics:** Real-time tracking of scan duration, active shutter counts, and pause/resume functionality with instant motor braking.

---

## Hardware Requirements

*   **Microcontroller:** Arduino Mega 2560
*   **Display:** 3.5" TFT LCD Touch Shield (ILI9486 Driver)
*   **Motors:** 4x NEMA Stepper Motors (Rotary Table, 2x Elevator, Camera Carriage)
*   **Motor Drivers:** Compatible stepper drivers (e.g., A4988, DRV8825, or external TB6600)
*   **Sensors:** 8x Mechanical Limit Switches (Normally Open / Pull-up configuration)
*   **Actuator:** 1x 5V Relay Module (for DSLR shutter release)

---

## Pinout Configuration

### Stepper Motors
| Axis | Step Pin | Direction Pin | Notes |
| :--- | :--- | :--- | :--- |
| **Rotary Table** | 22 | 23 | Continuous smooth movement |
| **Elevator 1** | 24 | 25 | T8 Pitch 4mm, 1/16 Microstepping |
| **Elevator 2** | 26 | 27 | T8 Pitch 4mm, 1/16 Microstepping |
| **Camera Carriage**| 28 | 29 | High-speed traverse |

### Limit Switches & Relay
| Component | Pin | Description |
| :--- | :--- | :--- |
| **Elevator Down Limits** | 30, 31 | Bottom homing switches for dual elevators |
| **Elevator Up Limits** | 32, 33 | Top safety stops |
| **Camera End Limits** | 34, 35 | Carriage maximum travel boundaries |
| **Camera Ref Limits** | 36, 37 | Carriage zero-point homing switches |
| **Camera Shutter Relay** | 38 | Triggers external camera shutter |

### TFT Touch Screen
*   **Control Pins:** `XP = 8`, `XM = A2`, `YP = A3`, `YM = 9`
*   **Calibration Bounds:** `LEFT = 900`, `RIGHT = 130`, `TOP = 950`, `BOTTOM = 120`

---

## Software Dependencies

Ensure the following libraries are installed via the Arduino Library Manager before compiling:

*   [`MCUFRIEND_kbv`](https://github.com/prenticedavid/MCUFRIEND_kbv) - For TFT display rendering.
*   [`Adafruit_GFX`](https://github.com/adafruit/Adafruit-GFX-Library) - Core graphics library.
*   [`TouchScreen`](https://github.com/adafruit/Adafruit_TouchScreen) - Touch panel handling.
*   [`AccelStepper`](https://github.com/waspinator/AccelStepper) - For advanced, non-blocking motor acceleration and multi-axis control.

---

## UI Navigation & Usage

1.  **Home Screen:** The entry point. Select either to start a new scan or enter the setup menu.
2.  **Setup Menu:** 
    *   **Speed:** Adjust the rotary table's RPM (steps per second) and run a live 5-second test.
    *   **Elevator:** Set absolute height positioning (in mm). The system will automatically calculate the required steps (800 steps/mm) and actuate both Z-axis motors symmetrically.
    *   **Shutter:** Define the interval (in milliseconds × 100) between automatic camera triggers.
3.  **Pre-Scan:** Execute the camera homing sequence to move the carriage to its absolute zero reference point before capturing data.
4.  **Scanning:** Displays an active progress interface. Allows the user to **Pause**, **Resume**, or **Abort** the scan. If limit switches are struck during this phase, the system executes an instant emergency brake and backs off.
5.  **Results:** Provides a post-scan summary, calculating the exact time elapsed (excluding paused durations) and the total number of photographs successfully triggered.

---

## Project Credits & Acknowledgments

**Coded by:** Eng. Ahmed M. Ubayed  
**Electrical Integration:** Ahmed Atef  
**Mechanical Design:** Abdell-Rahman Mohamed  
**Project Supervisors:** Eng. Ahmed Mosaad & Eng. Marwa Bassim  

*Developed with love, passion, and excessive amounts of coffee for the U34 Research Project.*
