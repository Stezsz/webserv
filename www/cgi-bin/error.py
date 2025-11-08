#!/usr/bin/env python3

print("Content-Type: text/html\r")
print("\r")
print("<!DOCTYPE html><html><body>")
print("<h1>Error Test</h1>")

# This will cause an error
x = 1 / 0  # Division by zero
