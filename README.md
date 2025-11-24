# Webserv – 42 School Project

The **webserv** project is a comprehensive implementation of an HTTP/1.1 server written in C++ 98. It is designed to handle HTTP requests, serve static and dynamic content, and manage multiple client connections simultaneously. The challenge involves building a robust server from scratch, avoiding blocking operations and ensuring compatibility with standard web browsers.

## Main Objectives

* Implement a HTTP/1.1 compliant server using system calls.
* Utilize non-blocking I/O and I/O multiplexing (e.g., `poll()` or `select()`).
* Parse configuration files to set up server ports, routes, and error pages.
* Handle standard HTTP methods: **GET**, **POST**, and **DELETE**.
* Implement CGI (Common Gateway Interface) to execute backend scripts.
* Ensure no memory leaks and robust error handling.

## Key Features

* **Configuration Parsing:** Reads `.conf` files to configure server blocks, listening ports, hostnames, root directories, client body size limits, and default error pages.
* **I/O Multiplexing:** Uses a single execution thread to handle multiple open file descriptors (sockets) simultaneously, ensuring the server remains responsive.
* **HTTP Request Handling:** Parses headers and bodies, handles chunked transfer encoding, and supports file uploads via `POST`.
* **Method Support:**
    * `GET`: Retrieves files and directory listings.
    * `POST`: Handles data submission and file creation.
    * `DELETE`: Removes files from the server.
* **CGI Executor:** Executes external scripts (like Python or PHP) based on file extensions, passing environment variables and handling input/output redirection.
* **Error Handling:** Returns appropriate HTTP status codes (e.g., 404, 403, 500) and serves custom HTML error pages.

## Conclusion

The **webserv** project is a significant milestone in understanding network programming and the architecture of the World Wide Web. It provides hands-on experience with the TCP/IP stack, socket manipulation, and the strict requirements of the HTTP protocol. By building this server, one gains a deep understanding of non-blocking architecture and the request/response cycle that powers the internet.

---

✅ **Final Grade: 100/100**
**Project made at** [42 Lisboa](https://www.42lisboa.com/)
👤 **Author:** Stephan Rodrigues Lassaponari ([@Stezsz](https://github.com/Stezsz))
