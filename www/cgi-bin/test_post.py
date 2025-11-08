#!/usr/bin/env python3
import os
import sys

print("Content-Type: text/html\r")
print("\r")
print("<!DOCTYPE html>")
print("<html><head><title>CGI POST Test</title></head><body>")
print("<h1>✅ CGI POST Method Test</h1>")
print("<p><strong>Request Method:</strong> " + os.environ.get('REQUEST_METHOD', 'N/A') + "</p>")
print("<p><strong>Content Length:</strong> " + os.environ.get('CONTENT_LENGTH', '0') + "</p>")

content_length = int(os.environ.get('CONTENT_LENGTH', 0))
if content_length > 0:
    post_data = sys.stdin.read(content_length)
    print("<h2>POST Data Received:</h2>")
    print("<pre>" + post_data + "</pre>")
else:
    print("<p>No POST data received</p>")

print("</body></html>")
