# 🕹️ Birthday Arcade & The Secret Maze

A retro-style gaming console built on the **Arduino Uno** platform, displaying on a **128x64 I2C OLED screen**. It features three classic arcade games, high-score persistence via EEPROM, and a hidden Easter egg game—a procedurally generated maze that unlocks a scrollable, heartfelt message.

---

## 🎮 Features

### 1. Main Menu & Classics
*   **🐍 Snake**: Command the snake, eat food, and grow. The game speed dynamically increases as your score grows.
*   **🏓 Pong**: Classic single-player Pong against the wall. Deflect the ball with your paddle; angles vary based on where the ball hits the paddle.
*   **🐦 Flappy**: Flappy Bird clone where you navigate a bird through procedurally positioned pipe gaps.

### 2. 💾 High Score Persistence (EEPROM)
*   High scores for Snake, Pong, and Flappy Bird are saved directly to the Arduino's non-volatile EEPROM memory.
*   Your records are maintained even after powering off the board.

### 3. 🔑 The Secret Maze (Easter Egg)
*   **Trigger**: Hold **`UP` + `DOWN` + `A`** simultaneously on the main menu.
*   **Generation**: The game generates a unique, solvable maze of size 16x10 using a randomized **Depth-First Search (DFS)** algorithm with backtracking.
*   **Objective**: Guide the player character to the heart-shaped goal.
*   **The Reward**: Reaching the heart opens a smooth, momentum-based scrolling letter/birthday greeting, complete with scrolling physics (velocity, friction, and text clipping optimization).

---

## 🛠️ Hardware Setup

### Components
1.  **Arduino Uno**
2.  **SSD1306 / SH1106 OLED Display** (128x64 resolution, I2C interface)
3.  **6x Tactile Pushbuttons** (with internal pullups enabled)
4.  **Breadboard and Jumper Wires**

### Wiring Diagram & Pin Mapping

Below is the wiring configuration mapping the physical buttons and the OLED display to the Arduino Uno:

| Component | Arduino Uno Pin | Pin Type / Config | Description |
| :--- | :--- | :--- | :--- |
| **BTN_RIGHT** | `D2` | Input (Pull-up) | Navigate Right |
| **BTN_DOWN** | `D3` | Input (Pull-up) | Navigate Down / Scroll Down |
| **BTN_UP** | `D4` | Input (Pull-up) | Navigate Up / Scroll Up |
| **BTN_LEFT** | `D5` | Input (Pull-up) | Navigate Left |
| **BTN_A** | `D6` | Input (Pull-up) | Action / Jump / Confirm |
| **BTN_QUIT** | `D7` | Input (Pull-up) | Quit current game to main menu |
| **OLED SDA** | `A4` | I2C Data | Serial Data Line |
| **OLED SCL** | `A5` | I2C Clock | Serial Clock Line |
| **OLED VCC** | `5V` | Power | 5V Power Supply |
| **OLED GND** | `GND` | Ground | System Ground |

---

## 💻 Software & Libraries

The project is written in C++/Arduino and uses the following libraries:
*   `<Wire.h>`: Core library for I2C communication.
*   `<EEPROM.h>`: Core library for persistent storage.
*   `U8g2lib` (`<U8g2lib.h>`): High-performance monochrome graphics library supporting the SH1106/SSD1306 controllers.

---

## 🚀 How to Run

### Option A: Simulation (Wokwi)
This project is fully configured for simulation on [Wokwi](https://wokwi.com/). 
1. Open the project directory.
2. Load the `diagram.json` file in a Wokwi-compatible editor or upload the workspace files to a Wokwi Arduino project.
3. Click **Start Simulation**.

### Option B: Physical Arduino Uno
1.  **Install Arduino IDE**: Make sure you have the [Arduino IDE](https://www.arduino.cc/en/software) installed.
2.  **Install Libraries**:
    *   Open Arduino IDE.
    *   Go to **Sketch** -> **Include Library** -> **Manage Libraries...**
    *   Search for **U8g2** and install the library by *Oliver*.
3.  **Connect Hardware**: Wire the buttons and OLED display as shown in the [Wiring Diagram](#wiring-diagram--pin-mapping) section.
4.  **Upload the Code**:
    *   Open `a_MAZE_ing_stuff.ino` in the Arduino IDE.
    *   Select your board (**Tools** -> **Board** -> **Arduino Uno**) and the correct serial port.
    *   Click **Upload** (right arrow icon).

---

## 🕹️ Controls Guide

| Game Mode | Action | Button |
| :--- | :--- | :--- |
| **Main Menu** | Move Cursor Up | `UP` |
| | Move Cursor Down | `DOWN` |
| | Confirm Selection | `A` |
| | **Unlock Secret Maze** | Hold `UP` + `DOWN` + `A` |
| **Snake** | Change Direction | `UP` / `DOWN` / `LEFT` / `RIGHT` |
| | Return to Menu | `QUIT` |
| **Pong** | Move Paddle Up | `UP` |
| | Move Paddle Down | `DOWN` |
| | Return to Menu | `QUIT` |
| **Flappy** | Flap / Jump | `A` |
| | Return to Menu | `QUIT` |
| **Maze** | Move Player | `UP` / `DOWN` / `LEFT` / `RIGHT` |
| | Return to Menu | `QUIT` |
| **Secret Letter** | Scroll Text | `UP` (upwards) / `DOWN` (downwards) |
| | Exit to Menu | `QUIT` |
| **Game Over** | Toggle Retry/Menu | `UP` / `DOWN` |
| | Confirm Option | `A` |

---

## ⚙️ Technical Highlights

*   **Custom Button Debouncing**: Implements a simple transition-state reader (`readInput()` and `flushInput()`) keeping track of `current`, `previous`, and edge-triggered `pressed` button statuses.
*   **Procedural Generation**: Utilizes a stack-based randomized backtracking depth-first search for generating a full-screen layout maze dynamically on an 8-bit microprocessor, optimized using bitwise operations (`visited` status stored in a compact bit-array).
*   **Smooth Scroll Momentum**: The secret text viewer features floating-point velocity calculation with friction emulation, giving a smooth physical feel to the scrolling, alongside strict line boundary caching and offscreen rendering skips to maintain stable frame rates on low-resource hardware.
*   **PROGMEM Usage**: Game menu options and the detailed letters are stored in Flash Program Memory (`PROGMEM`) using string tables, saving valuable Arduino Dynamic Memory (SRAM) for the stack-based maze generator.
