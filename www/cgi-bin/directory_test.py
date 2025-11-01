#!/usr/bin/env python3
import os

# Test relative path file access
current_dir = os.getcwd()

print("Content-Type: text/html\r")
print("\r")
print("<!DOCTYPE html><html><body>")
print("<h1>Directory Test</h1>")
print("<p><strong>Current Working Directory:</strong> " + current_dir + "</p>")
print("<p><strong>Script Directory:</strong> " + os.path.dirname(os.path.abspath(__file__)) + "</p>")

# Try to read a file with relative path
try:
    with open("test.py", "r") as f:
        print("<p>✅ Successfully accessed test.py with relative path</p>")
except Exception as e:
    print("<p>❌ Error accessing test.py: " + str(e) + "</p>")

print("</body></html>")
