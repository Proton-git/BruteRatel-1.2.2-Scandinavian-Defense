#!/bin/bash
if [ $# -lt 3 ]
    then echo "Need shellcode binary file to compile: eg: binCompiler.sh 86/64 <shellcode.bin> <output.exe>"
else
    if [ ! -f $2 ]
        then echo "!! Path does not exist: $2"
else
        cp $2 shellcode.bin -rf
        xxd -i shellcode.bin > shellcode.h
        if [ $1 == "86" ]
        then echo "* Compiling x86" 
        i686-w64-mingw32-gcc runshellcode.c -o S3 -s
else
        echo "* Compiling x64"
        x86_64-w64-mingw32-gcc runshellcode.c -o S3 -s
        fi	
        echo "+ Success"
        rm -rf shellcode.bin shellcode.h 
    fi 
fi