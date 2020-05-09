# FAT32-reader-in-C
A program written in C that reads FAT32 disk images and prints out the file structure.
<br>
`a4image_m` is a sample disk image.

# How to run:
1. Issue `make`
2. 
* Issue `./fat32 [imagename] info` to run the program in "info" mode. 
* Issue `./fat32 [imagename] list` to run the program in "list" mode. 
* Issue `./fat32 imagename get [absolute path]` to run the program in "get" mode.

# Note:
My solution:
* supports **long name** in both "**list**" and "**get**" mode.
* only prints .txt files in "get" mode. 
