/**
 * @file rest_api.hpp
 * @brief REST API interface
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_REST_API_HPP
#define DFSMSHSM_REST_API_HPP

// Windows: Must define WIN32_LEAN_AND_MEAN and include winsock2.h FIRST
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif
typedef SOCKET socket_t;
#define INVALID_SOCK INVALID_SOCKET
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCK (-1)
#endif

#include "hsm_types.hpp"
#include "metrics_exporter.hpp"
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <regex>

namespace dfsmshsm {

enum class HttpMethod { GET, POST, PUT, DELETE_METHOD, OPTIONS };
enum class HttpStatus { OK = 200, CREATED = 201, NO_CONTENT = 204, BAD_REQUEST = 400, NOT_FOUND = 404, INTERNAL_ERROR = 500, SERVICE_UNAVAILABLE = 503 };

struct HttpRequest {
    HttpMethod method = HttpMethod::GET;
    std::string path, query, body, client_ip;
    std::map<std::string, std::string> headers, params;
};

struct HttpResponse {
    HttpStatus status = HttpStatus::OK;
    std::map<std::string, std::string> headers;
    std::string body, content_type = "application/json";
    void setJSON(const std::string& j) { content_type = "application/json"; body = j; }
    void setText(const std::string& t) { content_type = "text/plain"; body = t; }
};

using RouteHandler = std::function<HttpResponse(const HttpRequest&)>;
struct Route { HttpMethod method; std::string pattern; std::regex regex; RouteHandler handler; std::string description; };

class RESTServer {
public:
    explicit RESTServer(uint16_t port = 8080) : port_(port) {}
    ~RESTServer() { stop(); }

    void get(const std::string& path, RouteHandler h, const std::string& d = "") { addRoute(HttpMethod::GET, path, h, d); }
    void post(const std::string& path, RouteHandler h, const std::string& d = "") { addRoute(HttpMethod::POST, path, h, d); }
    void del(const std::string& path, RouteHandler h, const std::string& d = "") { addRoute(HttpMethod::DELETE_METHOD, path, h, d); }

    bool start() {
        if (running_) return true;
#ifdef _WIN32
        WSADATA wsaData; if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) return false;
#endif
        server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket_ == INVALID_SOCK) return false;
        int opt = 1;
#ifdef _WIN32
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (char*)&opt, sizeof(opt));
#else
        setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
        sockaddr_in addr{}; addr.sin_family = AF_INET; addr.sin_addr.s_addr = INADDR_ANY; addr.sin_port = htons(port_);
        if (bind(server_socket_, (sockaddr*)&addr, sizeof(addr)) < 0) { closeSocket(server_socket_); return false; }
        if (listen(server_socket_, 10) < 0) { closeSocket(server_socket_); return false; }
        running_ = true;
        server_thread_ = std::thread(&RESTServer::acceptLoop, this);
        return true;
    }

    void stop() {
        if (!running_) return;
        running_ = false;
        closeSocket(server_socket_);
        if (server_thread_.joinable()) server_thread_.join();
#ifdef _WIN32
        WSACleanup();
#endif
    }

    bool isRunning() const { return running_; }
    uint16_t getPort() const { return port_; }
    std::vector<Route> getRoutes() const { std::lock_guard<std::mutex> lock(routes_mutex_); return routes_; }

private:
    void addRoute(HttpMethod m, const std::string& p, RouteHandler h, const std::string& d) {
        std::lock_guard<std::mutex> lock(routes_mutex_);
        Route r; r.method = m; r.pattern = p; r.handler = h; r.description = d;
        r.regex = std::regex("^" + std::regex_replace(p, std::regex(":[a-zA-Z_]+"), "([^/]+)") + "$");
        routes_.push_back(r);
    }

    void acceptLoop() {
        while (running_) {
            sockaddr_in client_addr{}; socklen_t client_len = sizeof(client_addr);
            socket_t client_socket = accept(server_socket_, (sockaddr*)&client_addr, &client_len);
            if (client_socket == INVALID_SOCK) { if (running_) continue; break; }
            std::thread([this, client_socket, client_addr]() { handleClient(client_socket, client_addr); }).detach();
        }
    }

    void handleClient(socket_t client_socket, sockaddr_in client_addr) {
        char buffer[8192] = {0};
        int bytes = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes > 0) {
            HttpRequest req = parseRequest(buffer, bytes);
            req.client_ip = inet_ntoa(client_addr.sin_addr);
            HttpResponse resp = handleRequest(req);
            std::string respStr = buildResponse(resp);
            send(client_socket, respStr.c_str(), static_cast<int>(respStr.size()), 0);
        }
        closeSocket(client_socket);
    }

    HttpRequest parseRequest(const char* buf, int len) {
        HttpRequest req;
        std::istringstream s(std::string(buf, len));
        std::string method, path, ver; s >> method >> path >> ver;
        if (method == "GET") req.method = HttpMethod::GET;
        else if (method == "POST") req.method = HttpMethod::POST;
        else if (method == "DELETE") req.method = HttpMethod::DELETE_METHOD;
        else if (method == "OPTIONS") req.method = HttpMethod::OPTIONS;
        size_t qp = path.find('?');
        if (qp != std::string::npos) { req.query = path.substr(qp + 1); req.path = path.substr(0, qp); }
        else req.path = path;
        size_t bs = std::string(buf, len).find("\r\n\r\n");
        if (bs != std::string::npos) req.body = std::string(buf + bs + 4, len - bs - 4);
        return req;
    }

    HttpResponse handleRequest(const HttpRequest& req) {
        std::lock_guard<std::mutex> lock(routes_mutex_);
        if (req.method == HttpMethod::OPTIONS) {
            HttpResponse r; r.status = HttpStatus::NO_CONTENT;
            r.headers["Access-Control-Allow-Origin"] = "*";
            r.headers["Access-Control-Allow-Methods"] = "GET, POST, DELETE, OPTIONS";
            r.headers["Access-Control-Allow-Headers"] = "Content-Type";
            return r;
        }
        for (const auto& route : routes_) {
            if (route.method == req.method) {
                std::smatch match;
                if (std::regex_match(req.path, match, route.regex)) {
                    try {
                        HttpResponse r = route.handler(req);
                        r.headers["Access-Control-Allow-Origin"] = "*";
                        return r;
                    } catch (const std::exception& e) {
                        HttpResponse r; r.status = HttpStatus::INTERNAL_ERROR;
                        r.setJSON("{\"error\": \"" + std::string(e.what()) + "\"}");
                        return r;
                    }
                }
            }
        }
        HttpResponse r; r.status = HttpStatus::NOT_FOUND; r.setJSON("{\"error\": \"Not found\"}"); return r;
    }

    std::string buildResponse(const HttpResponse& r) {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << static_cast<int>(r.status) << " " << statusText(r.status) << "\r\n";
        oss << "Content-Type: " << r.content_type << "\r\n";
        oss << "Content-Length: " << r.body.size() << "\r\n";
        oss << "Connection: close\r\nServer: DFSMShsm/4.0.2\r\n";
        for (const auto& [n, v] : r.headers) oss << n << ": " << v << "\r\n";
        oss << "\r\n" << r.body;
        return oss.str();
    }

    std::string statusText(HttpStatus s) {
        switch (s) {
            case HttpStatus::OK: return "OK";
            case HttpStatus::CREATED: return "Created";
            case HttpStatus::NO_CONTENT: return "No Content";
            case HttpStatus::BAD_REQUEST: return "Bad Request";
            case HttpStatus::NOT_FOUND: return "Not Found";
            case HttpStatus::INTERNAL_ERROR: return "Internal Server Error";
            case HttpStatus::SERVICE_UNAVAILABLE: return "Service Unavailable";
            default: return "Unknown";
        }
    }

    void closeSocket(socket_t s) {
#ifdef _WIN32
        if (s != INVALID_SOCKET) closesocket(s);
#else
        if (s >= 0) close(s);
#endif
    }

    socket_t server_socket_ = INVALID_SOCK;
    uint16_t port_;
    std::atomic<bool> running_{false};
    std::thread server_thread_;
    mutable std::mutex routes_mutex_;
    std::vector<Route> routes_;
};

class HSMRestAPI {
public:
    using DatasetHandler = std::function<std::vector<DatasetInfo>()>;
    using CreateHandler = std::function<OperationResult(const std::string&, uint64_t)>;
    using DeleteHandler = std::function<OperationResult(const std::string&)>;
    using MigrateHandler = std::function<OperationResult(const std::string&, StorageTier)>;
    using RecallHandler = std::function<OperationResult(const std::string&)>;
    using BackupHandler = std::function<OperationResult(const std::string&)>;
    using StatusHandler = std::function<std::string()>;

    explicit HSMRestAPI(uint16_t port = 8080) : server_(port) {}

    void setHandlers(DatasetHandler l, CreateHandler c, DeleteHandler d, MigrateHandler m, RecallHandler r, BackupHandler b, StatusHandler s) {
        listDatasets_ = l; createDataset_ = c; deleteDataset_ = d; migrateDataset_ = m; recallDataset_ = r; backupDataset_ = b; getStatus_ = s;
    }

    bool start() { registerRoutes(); return server_.start(); }
    void stop() { server_.stop(); }
    uint16_t getPort() const { return server_.getPort(); }

private:
    void registerRoutes() {
        server_.get("/api/v1", [this](const HttpRequest&) { return apiDoc(); }, "API docs");
        server_.get("/api/v1/health", [](const HttpRequest&) { HttpResponse r; r.setJSON("{\"status\":\"healthy\",\"version\":\"4.0.2\"}"); return r; }, "Health");
        server_.get("/metrics", [](const HttpRequest&) { HttpResponse r; r.setText(HSMMetrics::instance().exportPrometheus()); r.content_type = "text/plain"; return r; }, "Prometheus");
        server_.get("/api/v1/metrics", [](const HttpRequest&) { HttpResponse r; r.setJSON(HSMMetrics::instance().exportJSON()); return r; }, "JSON metrics");
        server_.get("/api/v1/datasets", [this](const HttpRequest&) {
            HttpResponse r;
            if (!listDatasets_) { r.status = HttpStatus::SERVICE_UNAVAILABLE; r.setJSON("{\"error\":\"Not configured\"}"); return r; }
            auto ds = listDatasets_();
            std::ostringstream oss; oss << "{\"datasets\":[";
            for (size_t i = 0; i < ds.size(); ++i) {
                if (i > 0) oss << ",";
                oss << "{\"name\":\"" << ds[i].name << "\",\"size\":" << ds[i].size << ",\"tier\":\"" << tierToString(ds[i].tier) << "\"}";
            }
            oss << "],\"count\":" << ds.size() << "}";
            r.setJSON(oss.str()); return r;
        }, "List datasets");
        server_.post("/api/v1/datasets", [this](const HttpRequest& req) {
            HttpResponse r;
            if (!createDataset_) { r.status = HttpStatus::SERVICE_UNAVAILABLE; r.setJSON("{\"error\":\"Not configured\"}"); return r; }
            std::string name = extractField(req.body, "name");
            std::string sizeStr = extractField(req.body, "size");
            uint64_t size = sizeStr.empty() ? 0 : std::stoull(sizeStr);
            if (name.empty()) { r.status = HttpStatus::BAD_REQUEST; r.setJSON("{\"error\":\"Name required\"}"); return r; }
            auto result = createDataset_(name, size);
            r.status = (result.status == OperationStatus::COMPLETED) ? HttpStatus::CREATED : HttpStatus::BAD_REQUEST;
            r.setJSON(resultToJSON(result)); return r;
        }, "Create dataset");
        server_.get("/api/v1/status", [this](const HttpRequest&) {
            HttpResponse r; if (getStatus_) r.setText(getStatus_()); else r.setJSON("{\"status\":\"running\"}"); return r;
        }, "Status");
    }

    HttpResponse apiDoc() {
        HttpResponse r;
        std::ostringstream oss;
        oss << "{\"name\":\"DFSMShsm REST API\",\"version\":\"4.0.2\",\"endpoints\":[";
        auto routes = server_.getRoutes();
        for (size_t i = 0; i < routes.size(); ++i) {
            if (i > 0) oss << ",";
            std::string m = (routes[i].method == HttpMethod::GET) ? "GET" : (routes[i].method == HttpMethod::POST) ? "POST" : "DELETE";
            oss << "{\"method\":\"" << m << "\",\"path\":\"" << routes[i].pattern << "\",\"description\":\"" << routes[i].description << "\"}";
        }
        oss << "]}";
        r.setJSON(oss.str()); return r;
    }

    std::string resultToJSON(const OperationResult& r) {
        std::ostringstream oss;
        bool ok = (r.status == OperationStatus::COMPLETED);
        oss << "{\"success\":" << (ok ? "true" : "false") << ",\"operation_id\":\"" << r.operation_id << "\",\"status\":\"" << statusToString(r.status) << "\",\"message\":\"" << r.message << "\"}";
        return oss.str();
    }

    std::string extractField(const std::string& json, const std::string& field) {
        std::string search = "\"" + field + "\":";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '"')) pos++;
        size_t end = pos;
        bool inQuotes = (pos > 0 && json[pos - 1] == '"');
        if (inQuotes) end = json.find('"', pos);
        else while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != ' ') end++;
        return json.substr(pos, end - pos);
    }

    RESTServer server_;
    DatasetHandler listDatasets_;
    CreateHandler createDataset_;
    DeleteHandler deleteDataset_;
    MigrateHandler migrateDataset_;
    RecallHandler recallDataset_;
    BackupHandler backupDataset_;
    StatusHandler getStatus_;
};

} // namespace dfsmshsm
#endif
