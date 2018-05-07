# smon

A small terminal-based system monitor for Linux that I write in my free time

## Features
The program can monitor the following stats:
- CPU usage
- CPU frequency
- CPU temperature
- Disk usage (Just bytes per second)
- Network usage (Just bytes per second)

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
