#!/usr/bin/env python3
import os

print("Content-Type: text/html\r")
print("\r")
print("<!DOCTYPE html>")
print("<html><head><title>CGI GET Test</title></head><body>")
print("<h1>✅ CGI GET Method Test</h1>")
print("<p><strong>Request Method:</strong> " + os.environ.get('REQUEST_METHOD', 'N/A') + "</p>")
print("<p><strong>Query String:</strong> " + os.environ.get('QUERY_STRING', 'N/A') + "</p>")
print("<p><strong>Script Name:</strong> " + os.environ.get('SCRIPT_NAME', 'N/A') + "</p>")
print("</body></html>")
