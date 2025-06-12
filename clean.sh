#!/bin/bash
echo "Cleaning MyOS build artifacts..."
rm -rf build/ iso/
find . -name "*.o" -delete
find . -name "*.d" -delete  
find . -name "*.bin" -delete
find . -name "*.elf" -delete
find . -name "*.iso" -delete
find . -name "*.log" -delete
find . -name "*.tmp" -delete
echo "Clean complete!"
