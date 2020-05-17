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

# Modes:
* "info": Print information about the drive. 
* "list": Output all files and directories on the drive recursively. 
* "get": Be able to fetch, and return a file from the drive. The command, assuming your program is named `fat32` would be `./fat32 imagename get path/to/file.txt`. The file will be written out as a file to the same directory as the program with the same name as the file in the FAT32 image.
