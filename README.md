# BER Estimator

This project demonstrates an implementation of a digital communication channel using the [PlutoSDR](https://wiki.analog.com/university/tools/pluto) and the [IT++ (ITPP) library](http://itpp.sourceforge.net/). The system transmits digitally modulated signals over a channel, receives them through the PlutoSDR, and estimates the Bit Error Rate (BER) in real-world conditions.

---

## 📦 Requirements

- **Hardware**

  - [PlutoSDR](https://www.analog.com/en/design-center/evaluation-hardware-and-software/evaluation-boards-kits/adalm-pluto.html)
  - Antennas for TX/RX

- **Software / Libraries**

  - [ITPP](http://itpp.sourceforge.net/) (signal processing & comms)
  - [libiio](https://github.com/analogdevicesinc/libiio) (PlutoSDR interface)

---

## 📄 License

This project is licensed under the **GNU General Public License v3.0 (GPLv3)**.
You are free to use, modify, and distribute this software under the terms of the GPL.
See the [LICENSE](LICENSE) file for details.
