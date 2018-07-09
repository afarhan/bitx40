# INSTALLATION INSTRUCTIONS (for Windows 10)

## OVERVIEW

1. [Install](#installing-the-arduino-ide) the Arduino IDE on your PC

2. [Install](#installing-the-pinchangeinterrupt-library) the PinChangeInterrupt Library on your PC

3. [Download](#downloading-the-sketch) the Raduino sketch to your PC

4. [Open](#opening-the-sketch) the Raduino sketch in your IDE

5. [Compile](#compiling-the-sketch) the Raduino sketch

6. Power OFF the radio

7. Connect the USB cable

8. [Upload](#uploading-the-sketch) the firmware to the Raduino board

9. Disconnect the USB cable

10. Power ON the radio

## Installing the Arduino IDE

The Arduino IDE (Integrated Development Environment) is a piece of software that is needed to "compile" the software and then upload it to the Arduino microcontroller chip.
The Raduino software (or "sketch") is written in C (a programming language). The program statements in the sketch must be "translated" to digital instruction codes that the Arduino microcontroller can understand and execute. This translation process is called "compiling".

The Arduino IDE software can be downloaded for free from https://www.arduino.cc/en/Main/Software
Click on the link 'Windows Installer, for Windows XP and up':

![IDEinstall1](IDEinstall1.png)

Then click on 'just download':

![IDEinstall2](IDEinstall2.png)

Save the downloaded file and execute it.
Do you want to allow this app to make changes to your device? Click 'YES'.

Accept the License Agreement:

![IDEinstall3](IDEinstall3.png)

Leave all checkboxes on, just press 'Next':

![IDEinstall3a](IDEinstall3a.png)

Leave the Destination Folder as is, just press 'Install':

![IDEinstall4](IDEinstall4.png)

The installation will start, it will take a minute or so to extract and install all files:

![IDEinstall5](IDEinstall5.png)

When the installation is completed, press "Close":

![IDEinstall6](IDEinstall6.png)

A new Arduino icon has been created on your desktop. Double-click it to start the IDE:

![IDEinstall7](IDEinstall7.PNG)

The IDE software will start:

![IDEinstall8](IDEinstall8.PNG)

When it is ready, go to "Tools" => "Board:" => select Arduino Nano:

![IDEinstall9](IDEinstall9.png)

Then go to "Tools" => "Processor:" => select ATmega328P:

![IDEinstall10](IDEinstall10.png)

Then go to "Tools" => "Programmer:" => select AVRISP mkii:

![IDEinstall11](IDEinstall11.png)

## Installing the PinChangeInterrupt Library

This sketch version requires the library ["PinChangeInterrupt"](https://playground.arduino.cc/Main/PinChangeInterrupt) for interrupt handling.
This is a non-standard library (it is not installed by default). Execute the following steps to install it onto your PC.
You only need to do this once.

1. Go to 'Sketch' => 'Include Library' => 'Manage Libraries...':

![libray-install1](library-install1.PNG)

2. The Library Manager will be started. Wait until the list of installed libraries is updated:

![libray-install2](library-install2.PNG)

3. In the search box, enter "pinchangeinterrupt":

![libray-install3](library-install3.PNG)

4. Select the libary named PinChangeInterrupt by NicoHood, then press "install":

![libray-install4](library-install4.PNG)

5. Wait until the installation is completed, then press "close":

![libray-install5](library-install5.PNG)

## Downloading the sketch

On the github page, click the green button "Clone or download":

![download-sketch1](download_sketch1.png)

Then click on 'download ZIP':

![download-sketch2](download_sketch2.png)

The file will be downloaded to the download folder on your PC.
Go to your downloads folder, find the file named "bitx40-master", and double-click it:

![download-sketch3](download_sketch3.png)

Click on "Extract":

![download-sketch4](download_sketch4.png)

And then "Extract All":

![download-sketch5](download_sketch5.png)

Optionally, use the "browse" button to change the location where the files will be extracted to.
Then press "Extract":

![download-sketch6](download_sketch6.png)

A new folder named "bitx40-master" will be created on the location you selected in the previous step:

![download-sketch7](download_sketch7.png)

The newly created folder "bitx40-master" contains several files. One of them is named "raduino_v1.27.7.ino". This is the actual Raduino firmware.

![download-sketch8](open_sketch1.png)

## Opening the sketch

In the folder "bitx40-master", locate the file "raduino_v1.27.7.ino" and double-click it:

![open-sketch1](open_sketch1.png)

The Arduino IDE will be started:

![open-sketch1a](IDEinstall8.PNG)

The following message box may appear. Press OK:

![open-sketch2](open_sketch2.png)

The sketch will now be opened and the program code will be shown in your IDE:

![open-sketch3](open_sketch3.PNG)

## Compiling the sketch

In the IDE, press the "verify/compile" button:

![compile-sketch1](compile_sketch1.png)

Compilation will now start, it may take several minutes to complete:

![compile-sketch2](compile_sketch2.png)

When the compilation is completed following screen will be shown:

![compile-sketch3](compile_sketch3.png)

## Uploading the sketch

First power OFF the radio
Then connect the USB cable.
The Raduino display will light up because at this time it receives power from the PC via the USB cable - the radio itself will not work though.

In the IDE, press the "upload" button:

![upload-sketch1](upload-sketch1.png)

The sketch will first be compiled again, this may take a few minutes:

![upload-sketch1a](compile_sketch2.png)

Then the sketch will be uploaded to the Arduino, this will take another minute or so:

![upload-sketch2](upload-sketch2.png)

Uploading is completed:

![upload-sketch3](upload-sketch3.png)

The Raduino will boot up again, the version number of the new firmware will briefly be shown on the display:

![upload-sketch4](upload-sketch4.png)

Disconnect the USB cable.

Power ON the radio. The Raduino will boot up.

Enjoy the new firmware. Happy BitX-ing!

