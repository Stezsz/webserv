#!/usr/bin/env python3

import os
import sys
import datetime

print("Content-Type: text/html\r")
print("\r")
print("<!DOCTYPE html>")
print("<html>")
print("<head>")
print("<title>CGI Test - webserv</title>")
print("<style>")
print("body { font-family: Arial; max-width: 700px; margin: 50px auto; padding: 20px; background: #f5f5f5; }")
print(".container { background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }")
print("h1 { color: #333; border-bottom: 3px solid #4CAF50; padding-bottom: 10px; }")
print(".info { background: #e7f3fe; border-left: 4px solid #2196F3; padding: 15px; margin: 20px 0; }")
print("table { width: 100%; border-collapse: collapse; margin: 20px 0; }")
print("td { padding: 10px; border-bottom: 1px solid #ddd; }")
print("td:first-child { font-weight: bold; color: #555; width: 200px; }")
print("</style>")
print("</head>")
print("<body>")
print("<div class='container'>")
print("<h1>✅ CGI Script Executed Successfully</h1>")

print("<div class='info'>")
print("<p><strong>Python CGI is working!</strong></p>")
print("<p>This page was generated dynamically by a Python script.</p>")
print("</div>")

print("<h2>Environment Variables</h2>")
print("<table>")
print("<tr><td>Request Method:</td><td>" + os.environ.get('REQUEST_METHOD', 'N/A') + "</td></tr>")
print("<tr><td>Query String:</td><td>" + os.environ.get('QUERY_STRING', 'N/A') + "</td></tr>")
print("<tr><td>Server Name:</td><td>" + os.environ.get('SERVER_NAME', 'N/A') + "</td></tr>")
print("<tr><td>Server Port:</td><td>" + os.environ.get('SERVER_PORT', 'N/A') + "</td></tr>")
print("<tr><td>Script Name:</td><td>" + os.environ.get('SCRIPT_NAME', 'N/A') + "</td></tr>")
print("<tr><td>Current Time:</td><td>" + str(datetime.datetime.now()) + "</td></tr>")
print("</table>")

if os.environ.get('REQUEST_METHOD') == 'POST':
    content_length = int(os.environ.get('CONTENT_LENGTH', 0))
    if content_length > 0:
        post_data = sys.stdin.read(content_length)
        print("<h2>POST Data</h2>")
        print("<div class='info'>")
        print("<pre>" + post_data + "</pre>")
        print("</div>")

print("<div style='margin-top: 30px; text-align: center;'>")
print("<a href='/'>← Back to Home</a>")
print("</div>")

print("</div>")
print("</body>")
print("</html>")
