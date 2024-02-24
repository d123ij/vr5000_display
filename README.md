This project is to create a substitute display unit for Yaesu VR-5000 broad band receiver "famous" for failing LCDs. The design is made possible only because of prior work by kakiralime (this is youtube username). I would not enough patience to pull this through myself. 
There are three parts to the project:
 - Hardware (PCB)
 - FPGA firmware project
 - RP2040 firmware project

   1. PCB project. Pretty simple 2 layer board made with JLCPCB
   2. FPGA project is to emulate functionality of NJU6575 IC used in the original LCD and pass data along to the RP2040 that does all the graphics.
   3. RP2040 is responsible for displaying data. All the graphic utilities are based on kakiralime's work. I used them pretty much unchanged.   

   
