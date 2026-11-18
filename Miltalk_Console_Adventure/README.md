# Miltalk Console — Adventure edition

Five activities on the Rev C ESP32-S3 N16R8 camera kit: Mind Reader, Pet Trainer, Detective, Racer, and Camera Lab. The earlier console and camera sketches are preserved separately.

## Open and upload

Unzip the complete folder. Open `Miltalk_Console_Adventure.ino`; keep ALL `.h` files beside it. Assets are compiled into flash: there is no SD card, online image service, or additional image library required on the device. Do not paste just the `.ino` into another folder without its headers.

- Board: ESP32S3 Dev Module; locally compiled with Espressif core 3.3.7.
- Flash: 16MB, QIO 80MHz; partition Huge APP; PSRAM **OPI PSRAM**.
- Native USB connector: Hardware CDC and JTAG, USB CDC On Boot Enabled. For the separate USB-UART connector use CDC On Boot Disabled and choose that adapter's serial port.
- Libraries: Adafruit GFX, Adafruit ST7735 and ST7789, Adafruit BusIO. Camera, Preferences, SPI, Wi-Fi and ESP-NOW come from the Espressif core.
- TFT: ST7789V, 240x320 portrait, SPI mode 3, rotation 2 (180 degrees), hardware SPI 16MHz. MOSI 38, SCLK 39, CS 40, DC 42, display RESET 41. Existing power wiring stays VCC 5V / LED 3.3V / common GND. If needed lower TFT_SPI_HZ to 8000000; do not change the pin mapping or SPI mode.
- Buttons: A 21, B 47, TEST 14, application RESET 1, active LOW. The application RESET button is distinct from the ESP32 EN/reset button.

## Starting page and menu

On each boot choose Dragon, Princess or Batman with A/B; TEST confirms. The last choice is remembered and preselected next time. RESET from the five-activity menu reopens character selection. The selected picture is used by Pet Trainer, along with a separate happy/neutral/sad reaction badge. The supplied artwork itself is static, not newly generated animation frames.

Menu: A/B move; TEST opens an activity. Bright rounded cards, the supplied Miltalk logo, and a full-screen PSRAM canvas replace incremental TFT painting. Only completed changed frames are transferred.

## Activities

- Mind Reader: TEST starts. A rock, B paper, TEST scissors; application RESET saves/exits. Hold A+B on its introduction for 1.5s to forget learned patterns.
- Pet Trainer: TEST starts. A rewards, B rejects, TEST skips; RESET saves/exits. Hold A+B on its introduction to forget lessons. The four action-value bars and reaction badge show the effect of feedback. The selected character does not change the four underlying learning situations/actions.
- Detective: A yes, B no. While teaching, A/B scroll and TEST picks. RESET exits. Hold A+B on its introduction to reset its decision tree.
- Racer: A/B move through Train 20 runs, Watch AI drive, You drive, Forget training; TEST selects. Driving uses A left/B right. RESET returns to the racer hub, then RESET again exits. Your car photo and circular obstacle picture replace rectangle sprites; the four nearest road rows are enlarged while the original eight-row physics and Q-learning state space are retained. Different random training histories give different scores; a 300-point run after a fixed number of training runs is not guaranteed.
- Camera Lab: train two visually different objects/categories using A and B, then TEST to classify. After a prediction A approves, B corrects to the other class, TEST skips feedback. Camera button taps act on release so the four-button send gesture cannot accidentally capture/reset data.

### Camera Lab controls

- A tap: capture a Set A example (or approve a pending prediction).
- B tap: capture a Set B example (or correct a pending prediction).
- TEST tap: classify (or skip pending feedback).
- RESET tap: back to the menu, retaining examples in RAM.
- Hold RESET alone for 1.5s: clear camera training.
- Hold all four together for 0.8s: send the trained model to the existing TESTER using the previous packet format and acknowledgement/retry behavior. No receiver firmware or new servo-prediction message format is included. Both sets must have examples; a compatible TESTER must be listening on the same Wi-Fi channel.

Camera preview is grayscale and holds for 3 seconds. There are at most 20 examples per set. The 16x16 mean-centred feature extraction, fixed exposure/gain settings and nearest-centroid classifier are retained from the earlier camera activity. The percentage is a relative distance/separation score, not a calibrated probability. This simple classifier can learn backgrounds/lighting instead of material identity: use varied examples, consistent framing, and independent test objects.

Camera training is RAM-only: it survives switching activities but not a power cycle. Camera initialization is lazy; it shuts down on leaving Camera Lab. A camera error does not prevent playing the other games. RESET returns to the menu and TEST retries initialization. Radio starts only when sending a model and shuts down on leaving Camera Lab.

Game learning is saved using the original Preferences namespaces. Mind Reader/Pet Trainer save every five interactions and on exit, Detective saves new branches, Racer saves training. Latest unsaved interactions may be lost on sudden power loss. Normal uploads retain Preferences unless flash is erased.

## Images and distribution

`game_assets.h` contains proportionally downsampled RGB565 versions of the five user-supplied pictures. Backgrounds, the princess stock watermark/checkerboard and all other artwork are retained; no background or watermark was removed. Source photo rectangles may remain visible on the road. `logo_asset.h` contains the previously supplied Miltalk logo.

These files do not establish any commercial image licence. Obtain appropriate rights for every picture, particularly the stock princess image and Batman character, before selling or distributing this artwork in a product. Replace the assets with licensed/owned artwork for production. `convert_assets.py` is an optional desktop conversion utility requiring Pillow, not an Arduino dependency; it prints the header as JSON for the development workflow.

## Verification and limits

- Compiled for ESP32-S3, 16MB flash, OPI PSRAM, Huge APP, native USB CDC: 1,136,319 bytes of flash and 64,360 bytes static RAM. A separate 153,600-byte screen canvas and camera frame buffer use PSRAM at runtime.
- Actual drawing functions rendered on the host with the installed Adafruit GFX library for selection/menu/buddy/racer/classifier layout inspection. The review sheet is a software render, not a photograph of a TFT.
- Host logic checks passed with address/undefined-behavior sanitizers: all 48 racer state indices, collision and lane bounds, 20 training runs, centroid distance cases, and camera tap/send/clear/exit gesture routing. Hardware camera/radio calls are stubbed in these checks.
- Physical TFT operation, camera ribbon/pin compatibility, capture quality, timing, buttons, actual flash persistence and ESP-NOW receiver interoperability still require testing on the user's kit. Do not treat a compile or host test as hardware qualification.
