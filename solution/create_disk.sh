#!/bin/bash

#Need to make a variable for name of disk.img, size, and count?

#No idea if this is right

diskName = $1
diskSize = $2
diskCount = $3

#default is disk.img 1M 1
dd if=/dev/zero of=$diskName bs=$diskSize count=$diskCount
