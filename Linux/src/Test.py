#!/usr/bin/env python3 
#
# SimpleCharDriverTest.py
# 
# 
#
import os
from os import path

if os.geteuid() != 0:
  print("Run as root!")
  exit(1) 

   


#
# Basic test- write then read a device. 
#
#
#ffff9e00c0fb4180 1657276607 S Ci:3:008:0 s c1 01 0000 0000 0004 4 <
#ffff9e00c0fb4180 1657277850 C Ci:3:008:0 0 4 = 0100ff8e
#
#ffff9e00c0fb4180 1658688197 S Co:3:008:0 s 41 07 0000 0008 0000 0       Checks i2c a
#ffff9e00c0fb4180 1659084781 C Co:3:008:0 0 0
#ffff9e00c0fb4180 1659084918 S Ci:3:008:0 s c1 03 0000 0000 0001 1 <
#ffff9e00c0fb4180 1659086047 C Ci:3:008:0 0 1 = 01
#ffff9e00c0fb4180 1659086236 S Co:3:008:0 s 41 07 0000 0009 0000 0
#ffff9e00c0fb4180 1659087182 C Co:3:008:0 0 0
#ffff9e00c0fb4180 1659087214 S Ci:3:008:0 s c1 03 0000 0000 0001 1 <
#ffff9e00c0fb4180 1659089366 C Ci:3:008:0 0 1 = 02

passed = True


def urb_tests(device):
    for i,urbandcheck in enumerate(
                      [("S Ci:3:008:0 s c1 01 0000 0000 0004 4",lambda x: x == "0x0100ff8e"),
    #                    ("S Ci:3:008:0 s 41 07 0000 0008 0000 0",None),
    #                    ("S Ci:3:008:0 s c1 03 0000 0000 0001 1",lambda x: x == "0x02"),
    #                    ("S Ci:3:008:0 s 41 07 0000 0009 0000 0",None),
    #                    ("S Ci:3:008:0 s c1 03 0000 0000 0001 1",lambda x: x == "0x02")
                      ]):

        urb = urbandcheck[0]
        check = urbandcheck[1]

        fd1 = os.open(device,os.O_RDWR)
        assert(os.write(fd1,urb.encode('utf-8')) == len(urb)) 
        try:
            fd2 = os.open(device,os.O_RDWR)
            assert(False) 
        except Exception:
            pass 

        urbresult = os.read(fd1,20).decode('utf-8') 
        urbresult = urbresult.strip('\n') 
  
        if check:  
            if not check(urbresult):
                passed = False
                print("Test "+str(i)+" " + urbresult) 

        os.close(fd1)
   

def check_no_space(device):
    fd1 = os.open(device,os.O_RDWR)
    try:
        fd2 = os.open(device,os.O_RDWR)
        assert(False),"Should not have let me open a second fd" 
    except OSError as e:
        pass 


def run():
    print ("Press ENTER when the USB device is plugged in.")
    _ = input()
    device = "/dev/simpleusb"
    assert os.path.exists(device)
    assert os.path.exists("/sys/class/simpleusb/") 

    urb_tests(device)
    check_no_space(device) 

    print ("Remove the device and press ENTER again") 
    _ = input()

    assert not os.path.exists(device)
    assert not os.path.exists("/sys/class/simpleusb/") 
   
    if passed:
        print ("PASS!")
    else:
        print("FAIL")

if __name__ == "__main__":
   run()
