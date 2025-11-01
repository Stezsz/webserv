# Webserv Evaluation - Test Commands by Category

## Setup
```bash
# Terminal 1: Start server
./webserv config/default.conf

# Terminal 2: Run tests from this file
```

---

## 1. CONFIGURATION TESTS

### Multiple servers with different ports
```bash
# Test port 8080 (Server 1 - Main)
curl -s http://127.0.0.1:8080/ | head -5

# Test port 8081 (Server 2)
curl -s http://127.0.0.1:8081/ | head -5

# Test port 8082 (Server 3 - API)
curl -s http://127.0.0.1:8082/ | head -5
```

### Multiple servers with different hostnames
```bash
# Test with Host header
curl -H "Host: localhost" http://127.0.0.1:8080/
curl -H "Host: webserv.local" http://127.0.0.1:8080/
curl -H "Host: test.local" http://127.0.0.1:8081/
curl -H "Host: api.local" http://127.0.0.1:8082/

# Test with curl --resolve
curl --resolve example.com:8080:127.0.0.1 http://example.com:8080/
```

### Default error pages
```bash
# Test 404 error page
curl -i http://127.0.0.1:8080/nonexistent.html

# Check if custom 404 page is returned
curl -s http://127.0.0.1:8080/doesnotexist | grep "404"
```

### Client body size limit
```bash
# Test within limit (should work)
curl -X POST -H "Content-Type: text/plain" \
  --data "small body" \
  http://127.0.0.1:8080/uploads

# Test exceeding limit (should return 413)
# Create large file first
dd if=/dev/zero of=/tmp/large_file.bin bs=1M count=11

# Try to upload (if limit is 10M, this should fail)
curl -X POST -F "file=@/tmp/large_file.bin" \
  -w "Status: %{http_code}\n" \
  http://127.0.0.1:8080/uploads

# Cleanup
rm /tmp/large_file.bin
```

### Routes to different directories
```bash
# Root location (/)
curl -s http://127.0.0.1:8080/ | head -5

# Uploads location (/uploads)
curl -s http://127.0.0.1:8080/uploads/ | head -5

# CGI location (/cgi-bin)
curl -s http://127.0.0.1:8080/cgi-bin/test.py | head -5
```

### Default index files
```bash
# Request directory - should return index.html
curl -s http://127.0.0.1:8080/ | grep "<title>"

# Request directory explicitly
curl -s http://127.0.0.1:8080/index.html | grep "<title>"
```

### Allowed methods per route
```bash
# GET on root (should work)
curl -X GET -w "Status: %{http_code}\n" -o /dev/null http://127.0.0.1:8080/

# POST on root (check if allowed)
curl -X POST -w "Status: %{http_code}\n" -o /dev/null http://127.0.0.1:8080/

# DELETE on uploads (should work)
echo "test" > /tmp/test_delete.txt
curl -X POST -F "file=@/tmp/test_delete.txt" http://127.0.0.1:8080/uploads
# Get filename from uploads directory
curl -X DELETE -w "Status: %{http_code}\n" http://127.0.0.1:8080/uploads/[filename]

# DELETE on root (should return 405 if not allowed)
curl -X DELETE -w "Status: %{http_code}\n" -o /dev/null http://127.0.0.1:8080/index.html

# Cleanup
rm /tmp/test_delete.txt
```

---

## 2. BASIC CHECKS

### GET request
```bash
# Simple GET
curl -s -o /dev/null -w "Status: %{http_code}\n" http://127.0.0.1:8080/

# GET with verbose output
curl -v http://127.0.0.1:8080/index.html
```

### POST request (Upload)
```bash
# Create test file
echo "This is a test upload file" > /tmp/test_upload.txt

# Upload file
curl -X POST -F "file=@/tmp/test_upload.txt" \
  -w "Status: %{http_code}\n" \
  http://127.0.0.1:8080/uploads

# Cleanup
rm /tmp/test_upload.txt
```

### GET request (Download)
```bash
# List uploaded files
curl -s http://127.0.0.1:8080/uploads/

# Download a specific file (replace [filename] with actual filename)
curl http://127.0.0.1:8080/uploads/[filename] -o /tmp/downloaded.txt

# Verify download
cat /tmp/downloaded.txt

# Cleanup
rm /tmp/downloaded.txt
```

### DELETE request
```bash
# Delete specific file (replace [filename] with actual filename)
curl -X DELETE -w "Status: %{http_code}\n" \
  http://127.0.0.1:8080/uploads/[filename]

# Verify file is deleted (should return 404)
curl -w "Status: %{http_code}\n" -o /dev/null \
  http://127.0.0.1:8080/uploads/[filename]
```

### UNKNOWN method
```bash
# Test with UNKNOWN method (should return 501 and not crash)
curl -X UNKNOWN -w "Status: %{http_code}\n" -o /dev/null \
  http://127.0.0.1:8080/

# Verify server is still running
ps aux | grep webserv | grep -v grep

# Test other unknown methods
curl -X PATCH -w "Status: %{http_code}\n" -o /dev/null http://127.0.0.1:8080/
curl -X PUT -w "Status: %{http_code}\n" -o /dev/null http://127.0.0.1:8080/
curl -X OPTIONS -w "Status: %{http_code}\n" -o /dev/null http://127.0.0.1:8080/
```

### Appropriate status codes
```bash
# 200 OK
curl -w "Status: %{http_code}\n" -o /dev/null http://127.0.0.1:8080/

# 404 Not Found
curl -w "Status: %{http_code}\n" -o /dev/null http://127.0.0.1:8080/nonexistent

# 405 Method Not Allowed (if DELETE not allowed on /)
curl -X DELETE -w "Status: %{http_code}\n" -o /dev/null http://127.0.0.1:8080/

# 501 Not Implemented
curl -X UNKNOWN -w "Status: %{http_code}\n" -o /dev/null http://127.0.0.1:8080/
```

---

## 3. CGI TESTS

### CGI with GET method
```bash
# Simple GET to CGI
curl http://127.0.0.1:8080/cgi-bin/test.py

# GET with query string
curl "http://127.0.0.1:8080/cgi-bin/test_get.py?name=webserv&test=123"
```

### CGI with POST method
```bash
# POST data to CGI
curl -X POST \
  -d "username=testuser&password=secret123" \
  http://127.0.0.1:8080/cgi-bin/test_post.py
```

### CGI runs in correct directory
```bash
# Test that CGI can access files with relative paths
curl http://127.0.0.1:8080/cgi-bin/directory_test.py
```

### CGI error handling (division by zero)
```bash
# Run CGI script with error
curl http://127.0.0.1:8080/cgi-bin/error.py

# Verify server is still running
ps aux | grep webserv | grep -v grep
```

### CGI timeout (infinite loop)
```bash
# Run CGI script with infinite loop (should timeout)
curl --max-time 10 http://127.0.0.1:8080/cgi-bin/infinite_loop.py

# Verify server recovered and is still running
ps aux | grep webserv | grep -v grep

# Verify server still responds
curl -w "Status: %{http_code}\n" -o /dev/null http://127.0.0.1:8080/
```

### CGI environment variables
```bash
# Test that environment variables are set correctly
curl http://127.0.0.1:8080/cgi-bin/test.py | grep -E "(REQUEST_METHOD|SERVER_NAME|SERVER_PORT)"
```

---

## 4. BROWSER TESTS

### Open browser and check headers
```
1. Open Firefox/Chrome
2. Press F12 to open Developer Tools
3. Go to Network tab
4. Navigate to: http://127.0.0.1:8080/
5. Click on the request
6. Check Request Headers:
   - Host: 127.0.0.1:8080
   - User-Agent: Mozilla/...
   - Accept: text/html...
7. Check Response Headers:
   - HTTP/1.1 200 OK
   - Content-Type: text/html
   - Content-Length: ...
```

### Static website serving
```
Test these URLs in browser:
- http://127.0.0.1:8080/
- http://127.0.0.1:8080/index.html
- http://127.0.0.1:8080/upload.html
- http://127.0.0.1:8080/delete.html
- http://127.0.0.1:8080/cgi.html

All should load correctly with status 200.
```

### Wrong URL
```
Navigate to: http://127.0.0.1:8080/nonexistent.html
Should show 404 error page.
```

### Directory listing
```
Navigate to: http://127.0.0.1:8080/uploads/
Should either:
- Show directory listing (if autoindex on) - Status 200
- Show 403 Forbidden (if autoindex off)
```

### Redirected URL
```
Navigate to: http://127.0.0.1:8080/uploads (without trailing slash)
Should redirect to: http://127.0.0.1:8080/uploads/
Check in Network tab:
- First request: 301/302 with Location header
- Second request: 200/403 (final destination)
```

---

## 5. PORT TESTS

### Multiple ports with different websites
```
Browser test:
1. http://127.0.0.1:8080/ - Should show Server 1 (Main)
2. http://127.0.0.1:8081/ - Should show Server 2 (different content)
3. http://127.0.0.1:8082/ - Should show Server 3 (API, different content)

Each should display different website content.
```

### Same port multiple times (should fail)
```bash
# Create config with duplicate ports
cat > /tmp/duplicate_port.conf << 'EOF'
server {
    listen 8080;
    host 127.0.0.1;
    server_name server1;
    location / { root ./www; }
}
server {
    listen 8080;
    host 127.0.0.1;
    server_name server2;
    location / { root ./www2; }
}
EOF

# Try to start server with duplicate config (should fail or warn)
./webserv /tmp/duplicate_port.conf

# Expected: Error message or warning about duplicate port
# Cleanup
rm /tmp/duplicate_port.conf
```

### Multiple servers with common ports
```bash
# Terminal 1: Start first server on port 8080
./webserv config/default.conf

# Terminal 2: Try to start second server also using port 8080
./webserv config/secondary.conf

# Expected: Second server should fail to bind to port 8080
# Only one server can listen on a specific port at a time
```

---

## 6. SIEGE & STRESS TESTS

### Install siege
```bash
# Ubuntu/Debian
sudo apt-get install siege

# macOS
brew install siege
```

### Basic availability test (>99.5%)
```bash
# Benchmark mode on empty page for 30 seconds
siege -b -t 30S http://127.0.0.1:8080/

# Check results:
# - Availability should be > 99.5%
# - No failed transactions
```

### Concurrent users
```bash
# 20 concurrent users, 100 requests each
siege -c 20 -r 100 http://127.0.0.1:8080/

# Check results:
# - All transactions successful
# - Reasonable response time
```

### Monitor memory (no leaks)
```bash
# Terminal 1: Monitor server memory in real-time
watch -n 1 'ps aux | grep webserv | grep -v grep | awk "{print \$6}"'

# Terminal 2: Run prolonged siege test
siege -c 10 -t 5M http://127.0.0.1:8080/

# Expected: Memory usage should be stable, not growing indefinitely
```

### No hanging connections
```bash
# Check connections while under load
# Terminal 1: Run siege
siege -c 50 -t 60S http://127.0.0.1:8080/

# Terminal 2: Monitor connections
watch -n 1 'netstat -an | grep 8080 | grep ESTABLISHED | wc -l'

# After siege ends, connections should drop to 0 or very few
# No connections should stay in CLOSE_WAIT or TIME_WAIT indefinitely
```

### Indefinite operation test
```bash
# Run siege for extended period (15 minutes+)
siege -c 10 -t 15M http://127.0.0.1:8080/

# Server should:
# - Not crash
# - Not need restart
# - Maintain stable memory
# - Maintain high availability (>99.5%)
```

---

## 7. MEMORY LEAK TESTS (VALGRIND)

### Run with valgrind
```bash
# Terminal 1: Start server with valgrind
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --log-file=valgrind.log \
         ./webserv config/default.conf

# Terminal 2: Run various tests
curl http://127.0.0.1:8080/
curl -X POST -F "file=@test.txt" http://127.0.0.1:8080/uploads
curl -X DELETE http://127.0.0.1:8080/uploads/[filename]
curl http://127.0.0.1:8080/cgi-bin/test.py

# Terminal 1: Stop server (Ctrl+C)
# Check valgrind report
cat valgrind.log | grep "definitely lost"

# Expected: 0 bytes definitely lost
```

---

## EXPECTED RESULTS SUMMARY

### Status Codes
- 200 OK - Successful GET
- 201 Created - File upload
- 204 No Content - Successful DELETE
- 301/302 - Redirects
- 304 Not Modified - Cache hit
- 403 Forbidden - No directory listing or permission denied
- 404 Not Found - Resource not found
- 405 Method Not Allowed - Method not allowed for route
- 413 Payload Too Large - Body exceeds limit
- 500 Internal Server Error - Server error
- 501 Not Implemented - Unknown HTTP method

### Server Stability
- ✅ Never crashes
- ✅ No memory leaks
- ✅ No hanging connections
- ✅ Handles errors gracefully
- ✅ Works indefinitely without restart
- ✅ Availability > 99.5% under siege

### CGI Requirements
- ✅ GET and POST methods work
- ✅ Runs in correct directory
- ✅ Handles errors without crashing server
- ✅ Timeout on infinite loops
- ✅ Environment variables set correctly

---

**Note:** Replace [filename] with actual filenames from your uploads directory.
