# webserv - Testing Guide

This directory contains test commands for evaluating the webserv project according to the 42 evaluation sheet.

## Quick Start

1. **Compile the server:**
```bash
make
```

2. **Start the server (Terminal 1):**
```bash
./webserv config/default.conf
```

3. **Run tests (Terminal 2):**
```bash
# Use the commands from eval_tests.txt
# Or test manually with curl, browser, or siege
```

## Test Files

- **eval_tests.txt** - Contains all curl commands for basic checks and CGI tests

## Mandatory Tests (from subject)

### 1. Basic Checks
Test GET, POST, DELETE, and UNKNOWN methods. Upload and download files.

### 2. CGI Tests
Test CGI execution with GET and POST methods, verify correct directory execution, test error handling and timeouts.

### 3. Browser Tests
Open browser, check headers, test static website serving, try wrong URLs, directory listing, and redirects.

### 4. Port Tests
Test multiple ports with different websites, test duplicate port errors, test multiple server instances.

### 5. Siege & Stress Tests

**Install siege:**
```bash
# Ubuntu/Debian
sudo apt-get install siege

# macOS
brew install siege
```

**Run stress tests:**
```bash
# Basic availability test (should be > 99.5%)
siege -b -t 30S http://127.0.0.1:8080/

# Concurrent users test
siege -c 20 -r 100 http://127.0.0.1:8080/

# Monitor memory (no leaks, no indefinite growth)
watch -n 1 'ps aux | grep webserv | grep -v grep'
```

**Expected results:**
- Availability: > 99.5%
- No memory leaks (stable memory usage)
- No hanging connections
- Server should run indefinitely without restart

## Memory Leak Testing

**With Valgrind:**
```bash
# Terminal 1: Start server with valgrind
valgrind --leak-check=full \
         --show-leak-kinds=all \
         --track-origins=yes \
         --log-file=valgrind.log \
         ./webserv config/default.conf

# Terminal 2: Run tests
# ... perform various tests ...

# Terminal 1: Stop server (Ctrl+C)
# Check valgrind.log for leaks
cat valgrind.log | grep "definitely lost"
```

**Expected:** 0 bytes definitely lost

## Configuration Tests

Test the following in configuration file:
- Multiple servers with different ports ✅
- Multiple servers with different hostnames ✅
- Default error pages ✅
- Client body size limit ✅
- Routes to different directories ✅
- Default index files ✅
- Allowed methods per route ✅

## Status Codes to Verify

- 200 OK - Successful GET
- 201 Created - Successful POST/upload
- 204 No Content - Successful DELETE
- 301/302 Redirect - Redirections
- 304 Not Modified - Cache validation
- 403 Forbidden - Directory listing off or no permission
- 404 Not Found - Wrong URL
- 405 Method Not Allowed - Method not allowed for route
- 413 Payload Too Large - Body exceeds limit
- 500 Internal Server Error - Server error
- 501 Not Implemented - Unknown method

## Notes

- Server should NEVER crash
- All operations should return appropriate status codes
- Error handling must work properly for all edge cases
- select() (or equivalent) must check read and write AT THE SAME TIME
- Only one read/write per client per select()
- Never check errno after read/recv/write/send
- Never read/write file descriptors without going through select()

---

**For detailed curl commands, see:** `eval_tests.txt`
