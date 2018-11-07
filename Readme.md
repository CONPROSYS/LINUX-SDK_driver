# Driver Modules for CPS-MCS341 [![Build Status](https://travis-ci.org/CONPROSYS/LINUX-SDK_driver.svg?branch=master)](https://travis-ci.org/CONPROSYS/LINUX-SDK_driver) ![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)

## Features
* Version : 1.3.3.1 (2018/11/07)
* GNU Public License Ver 2.0

## Support CONTEC Devices
### Controllers
* [CPS-MCS341-DS1-111](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/m2m-controller/cps-mcs341-ds1-111/price/)
* [CPS-MCS341-DS1-131](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/m2m-controller/cps-mcs341-ds1-131/price/)
* [CPS-MCS341G-DS1-130](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/m2m-controller/cps-mcs341g-ds1-130/price/)
* [CPS-MCS341Q-DS1-131](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/m2m-controller/cps-mcs341q-ds1-131/price/)

### Modules
* Analog Input/Output
    * [CPS-AI-1608LI](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-ai-1608li/price/)
    * [CPS-AO-1608LI](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-ao-1604li/price/)
    * [CPS-AI-1608ALI](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-ai-1608ali/price/)
    * [CPS-AO-1608VLI](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-ao-1604vli/price/)
* Sensor Input
    * [CPS-SSI-4P](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-ssi-4p/price/)
* Digital Input/Output
    * [CPS-DIO-0808L](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-dio-0808l/price/)
    * [CPS-DIO-0808BL](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-dio-0808bl/price/)
    * [CPS-DIO-0808RL](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-dio-0808rl/price/)
    * CPS-DIO-0808RBL
    * [CPS-RRY-4PCC](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-rry-4pcc/price/)
    * [CPS-DI-16L](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-di-16l/price/)
    * CPS-DI-16BL
    * [CPS-DI-16RL](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-di-16rl/price/)
    * CPS-DI-16RBL
    * [CPS-DO-16L](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-do-16l/price/)
    * CPS-DO-16BL
    * [CPS-DO-16RL](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-do-16rl/price/)
    * CPS-DO-16RBL
* Serial Communication
    * [CPS-COM-1PC](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-com-1pc/price/)
    * [CPS-COM-2PC](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-com-2pc/price/)
    * [CPS-COM-1PD](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-com-1pd/price/)
    * [CPS-COM-2PD](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-com-2pd/price/)
* LoRa Communication
    * [CPS-COM-1QL](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-com-1ql/price/)
* Counter
    * [CPS-CNT-3202I](https://www.contec.com/en/products-services/daq-control/iiot-conprosys/cps-io-module/cps-cnt-3202i/price/)
* Leakage Current
    * CPS-MM-LC

## Folder / Sources
* cps-driver
    CPS-MCS341 Driver Modules [![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://github.com/CONPROSYS/LINUX-SDK_driver/blob/master/cps-drivers/gpl-2.0.txt)
    * aio (Analog Input/Output)
        * Version 1.1.0.1
    * dio (Digital Input/Output)
        * Version 1.0.4
    * com (Serial Communication)
        * Version 1.0.5
    * cnt (Counter)
        * Version 0.9.6
    * ssi (Sensor Input)
        * Version 1.0.11
    * system (Base)
        * Version 1.1.1
    * spi/dio (Digital Input/Output)

* cps-iolib 
    CPS-MCS341 I/O Driver Modules [![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](https://github.com/CONPROSYS/LINUX-SDK_driver/blob/master/cps-iolib/gpl-2.0.txt)


## Change log
* Ver 1.3.3.1
    * cpsaio Ver.1.1.0 -> Ver.1.1.0.1
        * Fixed Calibration Read , Write, and Clear.(CPS-AO-1604LI, CPS-AO-1604VLI)
* Ver 1.3.3.0
    * 8250_cpscom Ver.1.0.4 -> Ver.1.0.5
        * Add Leakage Current Modules (CPS-MM-LC)