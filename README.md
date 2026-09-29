# DevLab_I2SPDM

Arduino library for I2S and PDM digital audio interfaces.

> Work in progress: this repository currently contains only the library skeleton.

---

# Installation

Copy this folder into your Arduino `libraries/` directory, or install it from the Library Manager once published.

---

# Quick Start

```cpp
#include <DevLab_I2SPDM.h>

DevLab_I2SPDM pdm;

void setup() {
  Serial.begin(115200);

  if (!pdm.begin()) {
    Serial.println("I2S/PDM initialization failed.");
    while (1);
  }
}

void loop() {
}
```

---

# Folder Structure

```text
DevLab_I2SPDM/
├── examples/
│   └── BasicRead/
├── src/
│   ├── DevLab_I2SPDM.h
│   └── DevLab_I2SPDM.cpp
├── library.properties
├── README.md
└── LICENSE
```

---

# Author

Jonathan Mejorado Lopez

UNIT Electronics - DevLab Ecosystem

---

# License

MIT License
