/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: tborges- <tborges-@student.42lisboa.com    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/10/26 17:38:38 by tborges-          #+#    #+#             */
/*   Updated: 2025/11/01 15:23:28 by tborges-         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

/**
 * RequestHandler.cpp
 * Implementation of HTTP Request Handler
 */
#include "includes/http/RequestHandler.hpp"
#include "includes/cgi/CGIExecutor.hpp"
#include "includes/core/Settings.hpp"
#include "includes/core/Instance.hpp"
#include "includes/utils/Logger.hpp"
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <ctime>
#include <vector>

namespace HTTP {

// Constructor
RequestHandler::RequestHandler(const Server* server)
	: _server(server) {
}

RequestHandler::~RequestHandler() {}

// Handle request
Response RequestHandler::handle(const Request& request) {
	Logger::info << "Handling " << request.getMethod() << " " << request.getPath() << std::endl;

	// First, check if method is recognized (GET, POST, DELETE)
	std::string method = request.getMethod();
	if (method != "GET" && method != "POST" && method != "DELETE") {
		Logger::warning << "Unknown method: " << method << std::endl;
		return notImplemented(method);
	}

	// Find matching route
	const Route* route = _server->matchRoute(request.getPath());
	if (!route) {
		Logger::warning << "No route found for path: " << request.getPath() << std::endl;
		return notFound(request.getPath());
	}

	// Check if method is allowed
	if (!route->isMethodAllowed(request.getMethod())) {
		Logger::warning << "Method " << request.getMethod() << " not allowed for path: "
		                << request.getPath() << std::endl;
		return methodNotAllowed(request.getMethod());
	}

	// Check for redirect
	if (!route->getRedirect().empty()) {
		Logger::info << "Redirecting to: " << route->getRedirect() << std::endl;
		return Response::redirect(route->getRedirect(), 301);
	}

	// Handle based on method
	if (request.getMethod() == "GET") {
		return handleGet(request, route);
	} else if (request.getMethod() == "POST") {
		return handlePost(request, route);
	} else if (request.getMethod() == "DELETE") {
		return handleDelete(request, route);
	} else {
		return notImplemented(request.getMethod());
	}
}

// Handle GET request
Response RequestHandler::handleGet(const Request& request, const Route* route) {
	std::string filePath = resolveFilePath(request.getPath(), route);

	Logger::debug << "Resolved file path: " << filePath << std::endl;

	// Check if CGI is enabled and file extension matches
	if (route->isCgiEnabled()) {
		std::string ext = getFileExtension(filePath);
		std::string cgiExt = route->getCgiExtension();

		// Normalize extensions (add dot if missing)
		if (!ext.empty() && ext[0] != '.') {
			ext = "." + ext;
		}
		if (!cgiExt.empty() && cgiExt[0] != '.') {
			cgiExt = "." + cgiExt;
		}

		if (ext == cgiExt) {
			// This is a CGI script
			if (fileExists(filePath)) {
				return handleCGI(request, route, filePath);
			} else {
				return notFound(request.getPath());
			}
		}
	}

	// Check if file exists
	if (!fileExists(filePath)) {
		return notFound(request.getPath());
	}

	// Check if it's a directory
	if (isDirectory(filePath)) {
		// Try index files first
		const std::vector<std::string>& indexFiles = route->getIndexFiles();
		for (size_t i = 0; i < indexFiles.size(); ++i) {
			std::string indexPath = filePath;
			if (indexPath[indexPath.length() - 1] != '/') {
				indexPath += "/";
			}
			indexPath += indexFiles[i];

			if (fileExists(indexPath) && !isDirectory(indexPath)) {
				filePath = indexPath;
				break;
			}
		}

		// If still a directory, check if autoindex is enabled
		if (isDirectory(filePath)) {
			if (route->isDirectoryListingEnabled()) {
				// Generate directory listing
				Response response;
				response.setStatus(200);
				response.setContentType("text/html");
				response.setBody(generateDirectoryListing(filePath, request.getPath()));
				return response;
			} else {
				return forbidden("Directory listing is disabled");
			}
		}
	}

	// Read file
	std::string content = readFile(filePath);
	if (content.empty() && !fileExists(filePath)) {
		return internalServerError("Failed to read file");
	}

	// Build response
	Response response;
	response.setStatus(200);

	// Set content type based on file extension
	std::string extension = getFileExtension(filePath);
	Settings* settings = Instance::Get<Settings>();
	response.setContentType(settings->httpMimeType(extension));

	// Add cache headers
	struct stat fileStat;
	if (stat(filePath.c_str(), &fileStat) == 0) {
		// Set Last-Modified header
		response.setLastModified(fileStat.st_mtime);

		// Generate and set ETag (based on inode, mtime, and size)
		response.setETag(generateETag(filePath));

		// Check If-None-Match (ETag validation)
		if (request.hasHeader("if-none-match")) {
			std::string clientETag = request.getHeader("if-none-match");
			std::string serverETag = "\"" + generateETag(filePath) + "\"";
			if (clientETag == serverETag) {
				// File hasn't changed, return 304 Not Modified
				Response notModified;
				notModified.setStatus(304);
				notModified.setKeepAlive(false);
				return notModified;
			}
		}

		// Check If-Modified-Since
		if (request.hasHeader("if-modified-since")) {
			// For simplicity, we'll skip date parsing
			// In production, you'd parse the date and compare with fileStat.st_mtime
		}

		// Set Cache-Control header based on file type
		// HTML files with dynamic content should not be cached
		if (extension == "html" || extension == "htm") {
			// Disable cache for HTML files (they often contain dynamic JavaScript)
			response.setCacheControl("no-cache, no-store, must-revalidate");
			response.setHeader("Pragma", "no-cache");
			response.setHeader("Expires", "0");
		} else {
			// Cache static resources (CSS, JS, images, etc.) for 1 hour
			response.setCacheControl("public, max-age=3600");
		}
	}

	response.setBody(content);
	response.setKeepAlive(false); // For now, always close connection

	Logger::success << "Served file: " << filePath << " (" << content.length() << " bytes)" << std::endl;

	return response;
}

// Handle POST request
Response RequestHandler::handlePost(const Request& request, const Route* route) {
	Logger::info << "POST request - Content-Type: " << request.getContentType() << std::endl;

	// Check if this is a CGI request (before other handlers)
	if (route->isCgiEnabled()) {
		// Check if file extension matches CGI extension
		std::string path = resolveFilePath(request.getPath(), route);
		std::string ext = getFileExtension(path);
		std::string cgiExt = route->getCgiExtension();

		// Normalize extensions (add dot if missing)
		if (!ext.empty() && ext[0] != '.') {
			ext = "." + ext;
		}
		if (!cgiExt.empty() && cgiExt[0] != '.') {
			cgiExt = "." + cgiExt;
		}

		if (ext == cgiExt) {
			// This is a CGI script
			if (fileExists(path)) {
				return handleCGI(request, route, path);
			} else {
				return notFound(request.getPath());
			}
		}
	}

	// Check if this is a POST to a static file (should return 405)
	// Do this check BEFORE handling form data or other generic handlers
	std::string resolvedPath = resolveFilePath(request.getPath(), route);
	if (fileExists(resolvedPath) && !isDirectory(resolvedPath)) {
		// This is an existing file - check if it's a static file
		std::string ext = getFileExtension(resolvedPath);
		// Common static file extensions
		if (ext == "html" || ext == "htm" || ext == "css" || ext == "js" ||
		    ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "gif" ||
		    ext == "txt" || ext == "pdf" || ext == "ico") {
			// This is a static file with no POST handler (not CGI, not upload)
			return methodNotAllowed(request.getMethod());
		}
	}

	// Check if upload is enabled for multipart/form-data
	if (request.isMultipart()) {
		if (route->isUploadEnabled()) {
			return handleFileUpload(request, route);
		} else {
			return Response::errorResponse(403, "File upload is not allowed for this resource");
		}
	}

	// Handle application/x-www-form-urlencoded
	if (request.getContentType().find("application/x-www-form-urlencoded") != std::string::npos) {
		return handleFormData(request, route);
	}

	// Simple POST response (generic body received)
	Response response;
	response.setStatus(200);
	response.setContentType("text/html");

	std::ostringstream body;
	body << "<!DOCTYPE html>\n"
	     << "<html>\n"
	     << "<head><title>POST Received</title></head>\n"
	     << "<body>\n"
	     << "<h1>POST Request Received</h1>\n"
	     << "<p>Content-Type: " << request.getContentType() << "</p>\n"
	     << "<p>Body size: " << request.getBody().length() << " bytes</p>\n";

	if (request.isChunked()) {
		body << "<p>Transfer-Encoding: chunked</p>\n";
	}

	body << "</body>\n"
	     << "</html>\n";

	response.setBody(body.str());
	response.setKeepAlive(false);

	return response;
}

// Handle form data (application/x-www-form-urlencoded)
Response RequestHandler::handleFormData(const Request& request, const Route* /* route */) {
	std::map<std::string, std::string> formData = request.getFormData();

	Logger::info << "Form data received with " << formData.size() << " fields" << std::endl;

	Response response;
	response.setStatus(200);
	response.setContentType("text/html");

	std::ostringstream body;
	body << "<!DOCTYPE html>\n"
	     << "<html>\n"
	     << "<head><title>Form Received</title></head>\n"
	     << "<body>\n"
	     << "<h1>Form Data Received</h1>\n"
	     << "<table border='1'>\n"
	     << "<tr><th>Field</th><th>Value</th></tr>\n";

	for (std::map<std::string, std::string>::const_iterator it = formData.begin();
	     it != formData.end(); ++it) {
		body << "<tr><td>" << it->first << "</td><td>" << it->second << "</td></tr>\n";
		Logger::debug << "  " << it->first << " = " << it->second << std::endl;
	}

	body << "</table>\n"
	     << "</body>\n"
	     << "</html>\n";

	response.setBody(body.str());
	response.setKeepAlive(false);

	return response;
}

// Handle file upload (multipart/form-data)
Response RequestHandler::handleFileUpload(const Request& request, const Route* route) {
	std::string boundary = request.getMultipartBoundary();
	if (boundary.empty()) {
		return Response::errorResponse(400, "Missing boundary in multipart/form-data");
	}

	Logger::info << "File upload - boundary: " << boundary << std::endl;

	// Parse multipart data
	std::vector<UploadedFile> files = parseMultipartData(request.getBody(), boundary);

	if (files.empty()) {
		return Response::errorResponse(400, "No files found in upload");
	}

	// Get upload directory from route
	std::string uploadDir = route->getUploadPath();
	if (uploadDir.empty()) {
		uploadDir = "./uploads";
	}

	// Save files
	std::vector<std::string> savedPaths;
	for (size_t i = 0; i < files.size(); ++i) {
		std::string savedPath = saveUploadedFile(files[i].content, files[i].filename, uploadDir);
		if (!savedPath.empty()) {
			savedPaths.push_back(savedPath);
			Logger::success << "Saved uploaded file: " << savedPath << std::endl;
		}
	}

	// Build response
	Response response;
	response.setStatus(201); // Created
	response.setContentType("text/html; charset=utf-8");

	std::ostringstream body;
	body << "<!DOCTYPE html>\n"
	     << "<html lang=\"pt\">\n"
	     << "<head>\n"
	     << "<meta charset=\"UTF-8\">\n"
	     << "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
	     << "<title>Upload Bem-Sucedido</title>\n"
	     << "<meta http-equiv=\"refresh\" content=\"2;url=/delete.html\">\n"
	     << "<style>\n"
	     << "* { margin: 0; padding: 0; box-sizing: border-box; }\n"
	     << "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif; background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); min-height: 100vh; display: flex; align-items: center; justify-content: center; padding: 20px; }\n"
	     << ".container { background: white; border-radius: 12px; box-shadow: 0 20px 60px rgba(0,0,0,0.3); max-width: 500px; width: 100%; padding: 40px; animation: slideIn 0.3s ease-out; }\n"
	     << "@keyframes slideIn { from { opacity: 0; transform: translateY(-20px); } to { opacity: 1; transform: translateY(0); } }\n"
	     << ".icon { width: 80px; height: 80px; background: #4CAF50; border-radius: 50%; display: flex; align-items: center; justify-content: center; margin: 0 auto 20px; font-size: 40px; color: white; }\n"
	     << ".icon::before { content: '\\2713'; }\n"
	     << "h1 { color: #333; text-align: center; font-size: 24px; margin-bottom: 10px; }\n"
	     << ".subtitle { color: #666; text-align: center; margin-bottom: 30px; font-size: 14px; }\n"
	     << ".success-box { background: #f0f9ff; border-left: 4px solid #4CAF50; padding: 15px; border-radius: 4px; margin-bottom: 25px; }\n"
	     << ".success-box strong { color: #2e7d32; font-size: 16px; }\n"
	     << ".file-list { background: #f8f9fa; padding: 20px; border-radius: 8px; margin-bottom: 25px; }\n"
	     << ".file-list h3 { color: #555; font-size: 14px; margin-bottom: 12px; text-transform: uppercase; letter-spacing: 0.5px; }\n"
	     << ".file-list ul { list-style: none; }\n"
	     << ".file-list li { padding: 8px 12px; background: white; margin-bottom: 8px; border-radius: 4px; color: #333; font-size: 13px; word-break: break-all; border: 1px solid #e0e0e0; }\n"
	     << ".file-list li::before { content: '\\1F4C4'; margin-right: 8px; }\n"
	     << ".redirect-info { text-align: center; color: #666; font-size: 14px; margin-bottom: 15px; }\n"
	     << ".link { display: inline-block; color: #667eea; text-decoration: none; font-weight: 600; padding: 10px 20px; border: 2px solid #667eea; border-radius: 6px; transition: all 0.3s; }\n"
	     << ".link:hover { background: #667eea; color: white; }\n"
	     << ".progress { width: 100%; height: 4px; background: #e0e0e0; border-radius: 2px; overflow: hidden; margin-top: 20px; }\n"
	     << ".progress-bar { height: 100%; background: linear-gradient(90deg, #4CAF50, #8bc34a); animation: progress 2s linear; }\n"
	     << "@keyframes progress { from { width: 0%; } to { width: 100%; } }\n"
	     << "</style>\n"
	     << "</head>\n"
	     << "<body>\n"
	     << "<div class=\"container\">\n"
	     << "<div class=\"icon\"></div>\n"
	     << "<h1>Upload Bem-Sucedido!</h1>\n"
	     << "<p class=\"subtitle\">Seus arquivos foram enviados com sucesso</p>\n"
	     << "<div class=\"success-box\">\n"
	     << "<strong>" << savedPaths.size() << " arquivo(s) carregado(s)</strong>\n"
	     << "</div>\n"
	     << "<div class=\"file-list\">\n"
	     << "<h3>Arquivos Salvos</h3>\n"
	     << "<ul>\n";

	for (size_t i = 0; i < savedPaths.size(); ++i) {
		// Extract just the filename from the full path
		std::string filename = savedPaths[i];
		size_t lastSlash = filename.find_last_of('/');
		if (lastSlash != std::string::npos) {
			filename = filename.substr(lastSlash + 1);
		}
		body << "<li>" << filename << "</li>\n";
	}

	body << "</ul>\n"
	     << "</div>\n"
	     << "<p class=\"redirect-info\">Redirecionando em 2 segundos...</p>\n"
	     << "<div style=\"text-align: center;\">\n"
	     << "<a href=\"/delete.html\" class=\"link\">Ir Agora &rarr;</a>\n"
	     << "</div>\n"
	     << "<div class=\"progress\"><div class=\"progress-bar\"></div></div>\n"
	     << "</div>\n"
	     << "</body>\n"
	     << "</html>\n";

	response.setBody(body.str());
	// Disable cache for upload response to ensure fresh redirects
	response.setCacheControl("no-cache, no-store, must-revalidate");
	response.setHeader("Pragma", "no-cache");
	response.setHeader("Expires", "0");
	response.setKeepAlive(false);

	return response;
}

// Handle DELETE request
Response RequestHandler::handleDelete(const Request& request, const Route* route) {
	std::string filePath = resolveFilePath(request.getPath(), route);

	Logger::debug << "Attempting to delete: " << filePath << std::endl;

	// Check if file exists
	if (!fileExists(filePath)) {
		return notFound(request.getPath());
	}

	// Check if it's a directory
	if (isDirectory(filePath)) {
		return forbidden("Cannot delete directories");
	}

	// Check write permission
	if (!hasWritePermission(filePath)) {
		return Response::errorResponse(403, "Permission denied: cannot delete file");
	}

	// Try to delete file
	if (unlink(filePath.c_str()) == 0) {
		Logger::success << "Deleted file: " << filePath << std::endl;

		// Return 204 No Content (preferred for DELETE)
		Response response;
		response.setStatus(204);
		response.setKeepAlive(false);
		return response;
	} else {
		Logger::error << "Failed to delete file: " << filePath << std::endl;
		return Response::errorResponse(500, "Failed to delete file");
	}
}

// Resolve file path
std::string RequestHandler::resolveFilePath(const std::string& requestPath, const Route* route) {
	std::string root = route->getRoot();
	std::string routePath = route->getPath();

	Logger::debug << "resolveFilePath: requestPath='" << requestPath
	              << "', root='" << root
	              << "', routePath='" << routePath << "'" << std::endl;

	// Remove route prefix from request path
	std::string relativePath = requestPath;
	if (relativePath.compare(0, routePath.length(), routePath) == 0) {
		relativePath = relativePath.substr(routePath.length());
	}

	Logger::debug << "relativePath after prefix removal: '" << relativePath << "'" << std::endl;

	// Build full path
	std::string fullPath = root;
	if (!fullPath.empty() && fullPath[fullPath.length() - 1] != '/' &&
	    (relativePath.empty() || relativePath[0] != '/')) {
		fullPath += "/";
	}
	fullPath += relativePath;

	Logger::debug << "Final fullPath: '" << fullPath << "'" << std::endl;

	return fullPath;
}

// Get file extension
std::string RequestHandler::getFileExtension(const std::string& path) {
	size_t dotPos = path.find_last_of('.');
	if (dotPos != std::string::npos && dotPos < path.length() - 1) {
		return path.substr(dotPos + 1);
	}
	return "";
}

// Check if file exists
bool RequestHandler::fileExists(const std::string& path) {
	return access(path.c_str(), F_OK) == 0;
}

// Check if path is a directory
bool RequestHandler::isDirectory(const std::string& path) {
	struct stat statbuf;
	if (stat(path.c_str(), &statbuf) != 0) {
		return false;
	}
	return S_ISDIR(statbuf.st_mode);
}

// Read file content
std::string RequestHandler::readFile(const std::string& path) {
	std::ifstream file(path.c_str(), std::ios::binary);
	if (!file.is_open()) {
		return "";
	}

	std::ostringstream buffer;
	buffer << file.rdbuf();
	file.close();

	return buffer.str();
}

// Generate directory listing HTML
std::string RequestHandler::generateDirectoryListing(const std::string& dirPath, const std::string& requestPath) {
	std::ostringstream html;

	html << "<!DOCTYPE html>\n"
	     << "<html>\n"
	     << "<head><title>Index of " << requestPath << "</title></head>\n"
	     << "<body>\n"
	     << "<h1>Index of " << requestPath << "</h1>\n"
	     << "<hr>\n"
	     << "<ul>\n";

	// Add parent directory link if not root
	if (requestPath != "/") {
		html << "<li><a href=\"../\">../</a></li>\n";
	}

	// Open directory
	DIR* dir = opendir(dirPath.c_str());
	if (dir) {
		struct dirent* entry;
		while ((entry = readdir(dir)) != NULL) {
			std::string name = entry->d_name;

			// Skip . and ..
			if (name == "." || name == "..") {
				continue;
			}

			// Check if it's a directory
			std::string fullPath = dirPath;
			if (fullPath[fullPath.length() - 1] != '/') {
				fullPath += "/";
			}
			fullPath += name;

			if (isDirectory(fullPath)) {
				html << "<li><a href=\"" << name << "/\">" << name << "/</a></li>\n";
			} else {
				html << "<li><a href=\"" << name << "\">" << name << "</a></li>\n";
			}
		}
		closedir(dir);
	}

	html << "</ul>\n"
	     << "<hr>\n"
	     << "<p><em>webserv/1.0</em></p>\n"
	     << "</body>\n"
	     << "</html>\n";

	return html.str();
}

// Check write permission
bool RequestHandler::hasWritePermission(const std::string& path) {
	return access(path.c_str(), W_OK) == 0;
}

// Check read permission
bool RequestHandler::hasReadPermission(const std::string& path) {
	return access(path.c_str(), R_OK) == 0;
}

// Generate ETag based on file metadata
std::string RequestHandler::generateETag(const std::string& filePath) {
	struct stat fileStat;
	if (stat(filePath.c_str(), &fileStat) != 0) {
		return "";
	}

	// Simple ETag: inode-mtime-size
	std::ostringstream etag;
	etag << std::hex << fileStat.st_ino << "-" << fileStat.st_mtime << "-" << fileStat.st_size;
	return etag.str();
}

// Save uploaded file
std::string RequestHandler::saveUploadedFile(const std::string& content, const std::string& filename, const std::string& uploadDir) {
	// Create upload directory if it doesn't exist
	mkdir(uploadDir.c_str(), 0755);

	// Generate unique filename with timestamp
	time_t now = time(NULL);
	std::ostringstream uniqueName;
	uniqueName << now << "_" << filename;

	std::string fullPath = uploadDir;
	if (fullPath[fullPath.length() - 1] != '/') {
		fullPath += "/";
	}
	fullPath += uniqueName.str();

	// Write file
	std::ofstream file(fullPath.c_str(), std::ios::binary);
	if (!file.is_open()) {
		Logger::error << "Failed to create file: " << fullPath << std::endl;
		return "";
	}

	file.write(content.c_str(), content.length());
	file.close();

	return fullPath;
}

// Parse multipart/form-data
std::vector<RequestHandler::UploadedFile> RequestHandler::parseMultipartData(const std::string& body, const std::string& boundary) {
	std::vector<UploadedFile> files;

	std::string delimiter = "--" + boundary;
	std::string endDelimiter = "--" + boundary + "--";

	size_t pos = 0;
	while ((pos = body.find(delimiter, pos)) != std::string::npos) {
		// Skip the delimiter
		pos += delimiter.length();

		// Skip \r\n after delimiter
		if (pos < body.length() && body[pos] == '\r') pos++;
		if (pos < body.length() && body[pos] == '\n') pos++;

		// Find next delimiter
		size_t nextPos = body.find(delimiter, pos);
		if (nextPos == std::string::npos) {
			break;
		}

		// Extract part
		std::string part = body.substr(pos, nextPos - pos);

		// Split headers and content
		size_t headerEnd = part.find("\r\n\r\n");
		if (headerEnd == std::string::npos) {
			headerEnd = part.find("\n\n");
			if (headerEnd == std::string::npos) {
				continue;
			}
			headerEnd += 2;
		} else {
			headerEnd += 4;
		}

		std::string headers = part.substr(0, headerEnd);
		std::string content = part.substr(headerEnd);

		// Remove trailing \r\n from content
		if (content.length() >= 2 && content[content.length() - 2] == '\r') {
			content = content.substr(0, content.length() - 2);
		} else if (content.length() >= 1 && content[content.length() - 1] == '\n') {
			content = content.substr(0, content.length() - 1);
		}

		// Parse headers to extract filename and content-type
		std::string filename;
		std::string contentType = "application/octet-stream";

		std::istringstream headerStream(headers);
		std::string headerLine;
		while (std::getline(headerStream, headerLine)) {
			// Remove \r
			if (!headerLine.empty() && headerLine[headerLine.length() - 1] == '\r') {
				headerLine = headerLine.substr(0, headerLine.length() - 1);
			}

			// Look for Content-Disposition
			if (headerLine.find("Content-Disposition") != std::string::npos) {
				size_t fnamePos = headerLine.find("filename=\"");
				if (fnamePos != std::string::npos) {
					fnamePos += 10; // length of 'filename="'
					size_t fnameEnd = headerLine.find("\"", fnamePos);
					if (fnameEnd != std::string::npos) {
						filename = headerLine.substr(fnamePos, fnameEnd - fnamePos);
					}
				}
			}

			// Look for Content-Type
			if (headerLine.find("Content-Type:") != std::string::npos) {
				size_t ctPos = headerLine.find(":") + 1;
				contentType = headerLine.substr(ctPos);
				// Trim whitespace
				while (!contentType.empty() && std::isspace(contentType[0])) {
					contentType = contentType.substr(1);
				}
			}
		}

		// Only save if we have a filename
		if (!filename.empty()) {
			UploadedFile file;
			file.filename = filename;
			file.contentType = contentType;
			file.content = content;
			files.push_back(file);

			Logger::debug << "Parsed file: " << filename << " (" << content.length() << " bytes)" << std::endl;
		}

		pos = nextPos;
	}

	return files;
}

// Handle CGI request
Response RequestHandler::handleCGI(const Request& request, const Route* route, const std::string& scriptPath) {
	Logger::info << "Executing CGI script: " << scriptPath << std::endl;

	// Create CGI executor
	CGI::Executor executor;

	// Execute CGI and get response
	return executor.execute(request, _server, route, scriptPath);
}

// Error responses
Response RequestHandler::notFound(const std::string& path) {
	// Check if custom error page is configured
	std::string errorPage = _server->getErrorPage(404);
	if (!errorPage.empty()) {
		// Find matching route to resolve the error page path
		const Route* route = _server->matchRoute("/");
		if (route) {
			std::string errorFilePath = resolveFilePath(errorPage, route);
			if (fileExists(errorFilePath)) {
				std::string content = readFile(errorFilePath);
				if (!content.empty()) {
					Response response;
					response.setStatus(404);
					response.setContentType("text/html");
					response.setBody(content);
					response.setKeepAlive(false);
					Logger::info << "Serving custom 404 page: " << errorFilePath << std::endl;
					return response;
				}
			}
		}
	}
	// Fallback to generic error page
	return Response::errorResponse(404, "The requested URL " + path + " was not found on this server.");
}

Response RequestHandler::forbidden(const std::string& message) {
	// Check if custom error page is configured
	std::string errorPage = _server->getErrorPage(403);
	if (!errorPage.empty()) {
		const Route* route = _server->matchRoute("/");
		if (route) {
			std::string errorFilePath = resolveFilePath(errorPage, route);
			if (fileExists(errorFilePath)) {
				std::string content = readFile(errorFilePath);
				if (!content.empty()) {
					Response response;
					response.setStatus(403);
					response.setContentType("text/html");
					response.setBody(content);
					response.setKeepAlive(false);
					Logger::info << "Serving custom 403 page: " << errorFilePath << std::endl;
					return response;
				}
			}
		}
	}
	// Fallback to generic error page
	return Response::errorResponse(403, message);
}

Response RequestHandler::methodNotAllowed(const std::string& method) {
	// Check if custom error page is configured
	std::string errorPage = _server->getErrorPage(405);
	if (!errorPage.empty()) {
		const Route* route = _server->matchRoute("/");
		if (route) {
			std::string errorFilePath = resolveFilePath(errorPage, route);
			if (fileExists(errorFilePath)) {
				std::string content = readFile(errorFilePath);
				if (!content.empty()) {
					Response response;
					response.setStatus(405);
					response.setContentType("text/html");
					response.setBody(content);
					response.setKeepAlive(false);
					Logger::info << "Serving custom 405 page: " << errorFilePath << std::endl;
					return response;
				}
			}
		}
	}
	// Fallback to generic error page
	return Response::errorResponse(405, "Method " + method + " is not allowed for this resource.");
}

Response RequestHandler::notImplemented(const std::string& method) {
	// Check if custom error page is configured
	std::string errorPage = _server->getErrorPage(501);
	if (!errorPage.empty()) {
		const Route* route = _server->matchRoute("/");
		if (route) {
			std::string errorFilePath = resolveFilePath(errorPage, route);
			if (fileExists(errorFilePath)) {
				std::string content = readFile(errorFilePath);
				if (!content.empty()) {
					Response response;
					response.setStatus(501);
					response.setContentType("text/html");
					response.setBody(content);
					response.setKeepAlive(false);
					Logger::info << "Serving custom 501 page: " << errorFilePath << std::endl;
					return response;
				}
			}
		}
	}
	// Fallback to generic error page
	return Response::errorResponse(501, "Method " + method + " is not implemented.");
}

Response RequestHandler::internalServerError(const std::string& message) {
	// Check if custom error page is configured
	std::string errorPage = _server->getErrorPage(500);
	if (!errorPage.empty()) {
		const Route* route = _server->matchRoute("/");
		if (route) {
			std::string errorFilePath = resolveFilePath(errorPage, route);
			if (fileExists(errorFilePath)) {
				std::string content = readFile(errorFilePath);
				if (!content.empty()) {
					Response response;
					response.setStatus(500);
					response.setContentType("text/html");
					response.setBody(content);
					response.setKeepAlive(false);
					Logger::info << "Serving custom 500 page: " << errorFilePath << std::endl;
					return response;
				}
			}
		}
	}
	// Fallback to generic error page
	return Response::errorResponse(500, message);
}

} // namespace HTTP
