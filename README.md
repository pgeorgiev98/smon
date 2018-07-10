# smon

A small terminal-based system monitor for Linux that I write in my free time

![Screenshot](screenshot.png?raw=true "smon screenshot")

## Features
The program can monitor the following stats:
- CPU usage, frequency and temperature
- Disk usage (Just bytes per second)
- Network usage (Just bytes per second)
- Battery charge, current and voltage

And log them to a csv-formatted file

CPU usage is measured via `/proc/stat`, while everything else uses `/sys/`

## Building
```
git clone https://github.com/pgeorgiev98/smon
cd smon
mkdir build
cd build
cmake ..
make
```

## Installing
After building `smon` you can install it by running
```
sudo make install
```
