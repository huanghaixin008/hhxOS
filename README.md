# hhxOS
A self-made operation system referencing Orange's OS.
<br>
The OS is similar to the Orange's OS, with trivial changes.
<br>
It is developed and debugged using 'bochs' virtual machine.

# How to use
A few 'make' commands is provided, listed in the MakeFile.
<br>
Normally run 'make everything' and it will clean and re-compile, and then copy binaries to images (including hard drive image and floppy disk drive). 
<br>
Then run 'bochs -r bochsrc' to run a virtual machine.
<br>
The OS supports booting by both hard drive and floppy drive, default is booting by hard drive.
<br>
To change booting device, open file 'bochsrc' and change booting device configuration.

# File structure
The directory 'command' contains programs that are going to be installed (or has been installed) in the hard drive.
<br>
The MakeFile there is for compiling them.
