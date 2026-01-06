/**
 * @file result.hpp
 * @brief Result type for error handling
 * @version 4.2.0
 * @author Bennie Shearer
 * Copyright (c) 2025 Bennie Shearer - MIT License
 */
#ifndef DFSMSHSM_RESULT_HPP
#define DFSMSHSM_RESULT_HPP

#include <string>
#include <variant>
#include <optional>
#include <sstream>
#include <functional>

namespace dfsmshsm {

enum class ErrorCode {
    SUCCESS = 0,
    UNKNOWN_ERROR = 1, NOT_IMPLEMENTED = 2, INVALID_ARGUMENT = 3,
    OUT_OF_RANGE = 4, TIMEOUT = 5, CANCELLED = 6, INVALID_STATE = 7, NOT_SUPPORTED = 8,
    NOT_FOUND = 100, ALREADY_EXISTS = 101, RESOURCE_EXHAUSTED = 102,
    PERMISSION_DENIED = 103, RESOURCE_LOCKED = 104, RESOURCE_BUSY = 105,
    STORAGE_FULL = 200, STORAGE_OFFLINE = 201, VOLUME_NOT_FOUND = 202,
    TIER_NOT_AVAILABLE = 203, IO_ERROR = 204, CORRUPTION_DETECTED = 205,
    MIGRATION_FAILED = 300, RECALL_FAILED = 301, BACKUP_FAILED = 302,
    RESTORE_FAILED = 303, ENCRYPTION_FAILED = 304, DECRYPTION_FAILED = 305,
    COMPRESSION_FAILED = 306, DECOMPRESSION_FAILED = 307,
    CONFIG_INVALID = 400, CONFIG_MISSING = 401, CONFIG_PARSE_ERROR = 402,
    POLICY_VIOLATION = 403, QUOTA_EXCEEDED = 404,
    SYSTEM_ERROR = 500, NETWORK_ERROR = 501, SERVICE_UNAVAILABLE = 502, INTERNAL_ERROR = 503
};

inline std::string errorCodeToString(ErrorCode code) {
    switch (code) {
        case ErrorCode::SUCCESS: return "SUCCESS";
        case ErrorCode::UNKNOWN_ERROR: return "UNKNOWN_ERROR";
        case ErrorCode::NOT_IMPLEMENTED: return "NOT_IMPLEMENTED";
        case ErrorCode::INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case ErrorCode::OUT_OF_RANGE: return "OUT_OF_RANGE";
        case ErrorCode::TIMEOUT: return "TIMEOUT";
        case ErrorCode::CANCELLED: return "CANCELLED";
        case ErrorCode::INVALID_STATE: return "INVALID_STATE";
        case ErrorCode::NOT_SUPPORTED: return "NOT_SUPPORTED";
        case ErrorCode::NOT_FOUND: return "NOT_FOUND";
        case ErrorCode::ALREADY_EXISTS: return "ALREADY_EXISTS";
        case ErrorCode::RESOURCE_EXHAUSTED: return "RESOURCE_EXHAUSTED";
        case ErrorCode::PERMISSION_DENIED: return "PERMISSION_DENIED";
        case ErrorCode::RESOURCE_LOCKED: return "RESOURCE_LOCKED";
        case ErrorCode::RESOURCE_BUSY: return "RESOURCE_BUSY";
        case ErrorCode::STORAGE_FULL: return "STORAGE_FULL";
        case ErrorCode::STORAGE_OFFLINE: return "STORAGE_OFFLINE";
        case ErrorCode::VOLUME_NOT_FOUND: return "VOLUME_NOT_FOUND";
        case ErrorCode::TIER_NOT_AVAILABLE: return "TIER_NOT_AVAILABLE";
        case ErrorCode::IO_ERROR: return "IO_ERROR";
        case ErrorCode::CORRUPTION_DETECTED: return "CORRUPTION_DETECTED";
        case ErrorCode::MIGRATION_FAILED: return "MIGRATION_FAILED";
        case ErrorCode::RECALL_FAILED: return "RECALL_FAILED";
        case ErrorCode::BACKUP_FAILED: return "BACKUP_FAILED";
        case ErrorCode::RESTORE_FAILED: return "RESTORE_FAILED";
        case ErrorCode::ENCRYPTION_FAILED: return "ENCRYPTION_FAILED";
        case ErrorCode::DECRYPTION_FAILED: return "DECRYPTION_FAILED";
        case ErrorCode::COMPRESSION_FAILED: return "COMPRESSION_FAILED";
        case ErrorCode::DECOMPRESSION_FAILED: return "DECOMPRESSION_FAILED";
        case ErrorCode::CONFIG_INVALID: return "CONFIG_INVALID";
        case ErrorCode::CONFIG_MISSING: return "CONFIG_MISSING";
        case ErrorCode::CONFIG_PARSE_ERROR: return "CONFIG_PARSE_ERROR";
        case ErrorCode::POLICY_VIOLATION: return "POLICY_VIOLATION";
        case ErrorCode::QUOTA_EXCEEDED: return "QUOTA_EXCEEDED";
        case ErrorCode::SYSTEM_ERROR: return "SYSTEM_ERROR";
        case ErrorCode::NETWORK_ERROR: return "NETWORK_ERROR";
        case ErrorCode::SERVICE_UNAVAILABLE: return "SERVICE_UNAVAILABLE";
        case ErrorCode::INTERNAL_ERROR: return "INTERNAL_ERROR";
        default: return "UNKNOWN(" + std::to_string(static_cast<int>(code)) + ")";
    }
}

struct Error {
    ErrorCode code = ErrorCode::UNKNOWN_ERROR;
    std::string message;
    std::string context;
    std::string source_file;
    int source_line = 0;
    
    Error() = default;
    Error(ErrorCode c, std::string msg = "") : code(c), message(std::move(msg)) {}
    Error(ErrorCode c, std::string msg, std::string ctx) 
        : code(c), message(std::move(msg)), context(std::move(ctx)) {}
    
    [[nodiscard]] std::string toString() const {
        std::ostringstream oss;
        oss << "[" << errorCodeToString(code) << "]";
        if (!message.empty()) oss << " " << message;
        if (!context.empty()) oss << " (context: " << context << ")";
        if (!source_file.empty()) oss << " at " << source_file << ":" << source_line;
        return oss.str();
    }
    
    [[nodiscard]] bool is(ErrorCode c) const { return code == c; }
    [[nodiscard]] bool isResourceError() const { 
        return static_cast<int>(code) >= 100 && static_cast<int>(code) < 200; 
    }
    [[nodiscard]] bool isStorageError() const { 
        return static_cast<int>(code) >= 200 && static_cast<int>(code) < 300; 
    }
    [[nodiscard]] bool isOperationError() const { 
        return static_cast<int>(code) >= 300 && static_cast<int>(code) < 400; 
    }
    [[nodiscard]] bool isConfigError() const { 
        return static_cast<int>(code) >= 400 && static_cast<int>(code) < 500; 
    }
    [[nodiscard]] bool isSystemError() const { return static_cast<int>(code) >= 500; }
};

template<typename T>
class Result {
public:
    Result(const T& value) : data_(value) {}
    Result(T&& value) : data_(std::move(value)) {}
    Result(const Error& err) : data_(err) {}
    Result(Error&& err) : data_(std::move(err)) {}
    Result(ErrorCode code, const std::string& msg = "") : data_(Error{code, msg}) {}
    
    [[nodiscard]] bool isOk() const { return std::holds_alternative<T>(data_); }
    [[nodiscard]] bool isError() const { return std::holds_alternative<Error>(data_); }
    [[nodiscard]] bool has_value() const { return isOk(); }
    [[nodiscard]] explicit operator bool() const { return isOk(); }
    
    [[nodiscard]] const T& value() const& { return std::get<T>(data_); }
    [[nodiscard]] T& value() & { return std::get<T>(data_); }
    [[nodiscard]] T valueOr(const T& defaultValue) const {
        return isOk() ? std::get<T>(data_) : defaultValue;
    }
    
    [[nodiscard]] const Error& error() const { return std::get<Error>(data_); }
    [[nodiscard]] ErrorCode errorCode() const { 
        return isError() ? std::get<Error>(data_).code : ErrorCode::SUCCESS; 
    }
    
    [[nodiscard]] const T* operator->() const { return &std::get<T>(data_); }
    [[nodiscard]] T* operator->() { return &std::get<T>(data_); }
    [[nodiscard]] const T& operator*() const& { return std::get<T>(data_); }
    [[nodiscard]] T& operator*() & { return std::get<T>(data_); }
    
    template<typename F>
    [[nodiscard]] auto map(F&& f) const -> Result<decltype(f(std::declval<T>()))> {
        using U = decltype(f(std::declval<T>()));
        if (isOk()) return Result<U>(f(std::get<T>(data_)));
        return Result<U>(std::get<Error>(data_));
    }
    
    template<typename F> Result& onSuccess(F&& f) {
        if (isOk()) f(std::get<T>(data_));
        return *this;
    }
    
    template<typename F> Result& onError(F&& f) {
        if (isError()) f(std::get<Error>(data_));
        return *this;
    }
    
    [[nodiscard]] Result withContext(const std::string& ctx) const {
        if (isError()) {
            Error err = std::get<Error>(data_);
            err.context = ctx;
            return Result(err);
        }
        return *this;
    }

private:
    std::variant<T, Error> data_;
};

template<>
class Result<void> {
public:
    Result() : error_(std::nullopt) {}
    Result(const Error& err) : error_(err) {}
    Result(Error&& err) : error_(std::move(err)) {}
    Result(ErrorCode code, const std::string& msg = "") : error_(Error{code, msg}) {}
    
    [[nodiscard]] bool isOk() const { return !error_.has_value(); }
    [[nodiscard]] bool isError() const { return error_.has_value(); }
    [[nodiscard]] bool has_value() const { return isOk(); }
    [[nodiscard]] explicit operator bool() const { return isOk(); }
    
    [[nodiscard]] const Error& error() const { return *error_; }
    [[nodiscard]] ErrorCode errorCode() const { 
        return isError() ? error_->code : ErrorCode::SUCCESS; 
    }
    
    template<typename F> Result& onSuccess(F&& f) {
        if (isOk()) f();
        return *this;
    }
    
    template<typename F> Result& onError(F&& f) {
        if (isError()) f(*error_);
        return *this;
    }
    
    [[nodiscard]] Result withContext(const std::string& ctx) const {
        if (isError()) {
            Error err = *error_;
            err.context = ctx;
            return Result(err);
        }
        return *this;
    }

private:
    std::optional<Error> error_;
};

// Helper functions
template<typename T>
Result<T> Ok(T&& value) { return Result<T>(std::forward<T>(value)); }

inline Result<void> Ok() { return Result<void>(); }

template<typename T>
Result<T> Err(ErrorCode code, const std::string& msg = "") {
    return Result<T>(code, msg);
}

template<typename T>
Result<T> Err(const Error& err) { return Result<T>(err); }

// Specialization for Result<void>
inline Result<void> Err(ErrorCode code, const std::string& msg = "") {
    return Result<void>(code, msg);
}

} // namespace dfsmshsm

#endif // DFSMSHSM_RESULT_HPP
