#!/usr/bin/env python3
import time

print("Content-Type: text/html\r")
print("\r")
print("<!DOCTYPE html><html><body>")
print("<h1>Infinite Loop Test</h1>")

# Infinite loop - should timeout
while True:
    time.sleep(1)
