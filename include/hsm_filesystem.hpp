/**
 * @file hsm_filesystem.hpp
 * @brief Cross-platform filesystem utilities for DFSMShsm
 * @author IBM DFSMShsm Emulation Team
 * @version 4.2.0
 * @date 2025-01-06
 *
 * This header provides:
 * - Path manipulation utilities
 * - File operations with error handling
 * - Directory traversal
 * - Temporary file management
 * - File metadata utilities
 */

#ifndef HSM_FILESYSTEM_HPP
#define HSM_FILESYSTEM_HPP

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <fstream>
#include <sstream>
#include <chrono>
#include <functional>
#include <random>

namespace dfsmshsm {
namespace fs {

// Type aliases for convenience
namespace stdfs = std::filesystem;
using Path = stdfs::path;
using FileTime = stdfs::file_time_type;
using FileStatus = stdfs::file_status;
using DirectoryEntry = stdfs::directory_entry;
using DirectoryIterator = stdfs::directory_iterator;
using RecursiveIterator = stdfs::recursive_directory_iterator;

// ============================================================================
// Path Utilities
// ============================================================================

/**
 * @brief Get file extension (without dot)
 * @param path File path
 * @return Extension string (empty if none)
 */
[[nodiscard]] inline std::string getExtension(const Path& path) {
    std::string ext = path.extension().string();
    if (!ext.empty() && ext[0] == '.') {
        return ext.substr(1);
    }
    return ext;
}

/**
 * @brief Get filename without extension
 * @param path File path
 * @return Stem (filename without extension)
 */
[[nodiscard]] inline std::string getStem(const Path& path) {
    return path.stem().string();
}

/**
 * @brief Get filename with extension
 * @param path File path
 * @return Filename
 */
[[nodiscard]] inline std::string getFilename(const Path& path) {
    return path.filename().string();
}

/**
 * @brief Get parent directory path
 * @param path File path
 * @return Parent path
 */
[[nodiscard]] inline Path getParent(const Path& path) {
    return path.parent_path();
}

/**
 * @brief Join path components
 * @tparam Args Path component types
 * @param base Base path
 * @param components Path components to append
 * @return Combined path
 */
template<typename... Args>
[[nodiscard]] Path joinPath(const Path& base, Args&&... components) {
    Path result = base;
    (result /= ... /= Path(std::forward<Args>(components)));
    return result;
}

/**
 * @brief Normalize path (resolve . and ..)
 * @param path Path to normalize
 * @return Normalized path
 */
[[nodiscard]] inline Path normalize(const Path& path) {
    return path.lexically_normal();
}

/**
 * @brief Make path relative to base
 * @param path Path to make relative
 * @param base Base path
 * @return Relative path
 */
[[nodiscard]] inline Path makeRelative(const Path& path, const Path& base) {
    return path.lexically_relative(base);
}

/**
 * @brief Get absolute path
 * @param path Path to resolve
 * @return Absolute path
 */
[[nodiscard]] inline Path absolute(const Path& path) {
    std::error_code ec;
    auto result = stdfs::absolute(path, ec);
    return ec ? path : result;
}

/**
 * @brief Get canonical path (resolves symlinks)
 * @param path Path to resolve
 * @return Canonical path or empty on error
 */
[[nodiscard]] inline std::optional<Path> canonical(const Path& path) {
    std::error_code ec;
    auto result = stdfs::canonical(path, ec);
    return ec ? std::nullopt : std::optional<Path>(result);
}

/**
 * @brief Replace file extension
 * @param path Original path
 * @param newExt New extension (with or without dot)
 * @return Path with new extension
 */
[[nodiscard]] inline Path replaceExtension(const Path& path, std::string_view newExt) {
    Path result = path;
    std::string ext(newExt);
    if (!ext.empty() && ext[0] != '.') {
        ext = "." + ext;
    }
    result.replace_extension(ext);
    return result;
}

// ============================================================================
// File System Queries
// ============================================================================

/**
 * @brief Check if path exists
 * @param path Path to check
 * @return true if exists
 */
[[nodiscard]] inline bool exists(const Path& path) noexcept {
    std::error_code ec;
    return stdfs::exists(path, ec);
}

/**
 * @brief Check if path is a regular file
 * @param path Path to check
 * @return true if regular file
 */
[[nodiscard]] inline bool isFile(const Path& path) noexcept {
    std::error_code ec;
    return stdfs::is_regular_file(path, ec);
}

/**
 * @brief Check if path is a directory
 * @param path Path to check
 * @return true if directory
 */
[[nodiscard]] inline bool isDirectory(const Path& path) noexcept {
    std::error_code ec;
    return stdfs::is_directory(path, ec);
}

/**
 * @brief Check if path is a symbolic link
 * @param path Path to check
 * @return true if symbolic link
 */
[[nodiscard]] inline bool isSymlink(const Path& path) noexcept {
    std::error_code ec;
    return stdfs::is_symlink(path, ec);
}

/**
 * @brief Check if file is empty
 * @param path Path to check
 * @return true if empty
 */
[[nodiscard]] inline bool isEmpty(const Path& path) noexcept {
    std::error_code ec;
    return stdfs::is_empty(path, ec);
}

/**
 * @brief Get file size in bytes
 * @param path Path to file
 * @return File size or 0 on error
 */
[[nodiscard]] inline uint64_t fileSize(const Path& path) noexcept {
    std::error_code ec;
    auto size = stdfs::file_size(path, ec);
    return ec ? 0 : size;
}

/**
 * @brief Get available space on filesystem
 * @param path Path on target filesystem
 * @return Available bytes or 0 on error
 */
[[nodiscard]] inline uint64_t availableSpace(const Path& path) noexcept {
    std::error_code ec;
    auto info = stdfs::space(path, ec);
    return ec ? 0 : info.available;
}

/**
 * @brief Get total capacity of filesystem
 * @param path Path on target filesystem
 * @return Total bytes or 0 on error
 */
[[nodiscard]] inline uint64_t totalSpace(const Path& path) noexcept {
    std::error_code ec;
    auto info = stdfs::space(path, ec);
    return ec ? 0 : info.capacity;
}

/**
 * @brief Get last modification time
 * @param path Path to file
 * @return Modification time or nullopt on error
 */
[[nodiscard]] inline std::optional<FileTime> lastModified(const Path& path) noexcept {
    std::error_code ec;
    auto time = stdfs::last_write_time(path, ec);
    return ec ? std::nullopt : std::optional<FileTime>(time);
}

// ============================================================================
// File Operations
// ============================================================================

/**
 * @brief Result of file operation
 */
struct FileResult {
    bool success;
    std::string error;
    
    [[nodiscard]] explicit operator bool() const noexcept { return success; }
    
    static FileResult ok() { return {true, ""}; }
    static FileResult fail(std::string_view msg) { return {false, std::string(msg)}; }
    static FileResult fromError(std::error_code ec) { 
        return {!ec, ec ? ec.message() : ""}; 
    }
};

/**
 * @brief Create directory (and parents if needed)
 * @param path Directory path
 * @return Operation result
 */
[[nodiscard]] inline FileResult createDirectory(const Path& path) {
    std::error_code ec;
    stdfs::create_directories(path, ec);
    return FileResult::fromError(ec);
}

/**
 * @brief Remove file or empty directory
 * @param path Path to remove
 * @return Operation result
 */
[[nodiscard]] inline FileResult remove(const Path& path) {
    std::error_code ec;
    stdfs::remove(path, ec);
    return FileResult::fromError(ec);
}

/**
 * @brief Remove file or directory recursively
 * @param path Path to remove
 * @return Number of items removed or -1 on error
 */
[[nodiscard]] inline int64_t removeAll(const Path& path) {
    std::error_code ec;
    auto count = stdfs::remove_all(path, ec);
    return ec ? -1 : static_cast<int64_t>(count);
}

/**
 * @brief Copy file
 * @param source Source path
 * @param dest Destination path
 * @param overwrite Overwrite if exists
 * @return Operation result
 */
[[nodiscard]] inline FileResult copyFile(const Path& source, const Path& dest, 
                                         bool overwrite = false) {
    std::error_code ec;
    auto options = overwrite ? stdfs::copy_options::overwrite_existing 
                             : stdfs::copy_options::none;
    stdfs::copy_file(source, dest, options, ec);
    return FileResult::fromError(ec);
}

/**
 * @brief Copy directory recursively
 * @param source Source directory
 * @param dest Destination directory
 * @param overwrite Overwrite existing files
 * @return Operation result
 */
[[nodiscard]] inline FileResult copyDirectory(const Path& source, const Path& dest,
                                              bool overwrite = false) {
    std::error_code ec;
    auto options = stdfs::copy_options::recursive;
    if (overwrite) {
        options |= stdfs::copy_options::overwrite_existing;
    }
    stdfs::copy(source, dest, options, ec);
    return FileResult::fromError(ec);
}

/**
 * @brief Move/rename file or directory
 * @param source Source path
 * @param dest Destination path
 * @return Operation result
 */
[[nodiscard]] inline FileResult move(const Path& source, const Path& dest) {
    std::error_code ec;
    stdfs::rename(source, dest, ec);
    return FileResult::fromError(ec);
}

/**
 * @brief Create symbolic link
 * @param target Target path
 * @param link Link path
 * @return Operation result
 */
[[nodiscard]] inline FileResult createSymlink(const Path& target, const Path& link) {
    std::error_code ec;
    stdfs::create_symlink(target, link, ec);
    return FileResult::fromError(ec);
}

// ============================================================================
// File Content Operations
// ============================================================================

/**
 * @brief Read entire file as string
 * @param path File path
 * @return File contents or nullopt on error
 */
[[nodiscard]] inline std::optional<std::string> readFile(const Path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) return std::nullopt;
    
    std::ostringstream oss;
    oss << file.rdbuf();
    return oss.str();
}

/**
 * @brief Read file as vector of bytes
 * @param path File path
 * @return File contents or nullopt on error
 */
[[nodiscard]] inline std::optional<std::vector<uint8_t>> readBinaryFile(const Path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) return std::nullopt;
    
    auto size = file.tellg();
    file.seekg(0);
    
    std::vector<uint8_t> data(static_cast<std::size_t>(size));
    file.read(reinterpret_cast<char*>(data.data()), size);
    
    return data;
}

/**
 * @brief Read file lines
 * @param path File path
 * @return Vector of lines or nullopt on error
 */
[[nodiscard]] inline std::optional<std::vector<std::string>> readLines(const Path& path) {
    std::ifstream file(path);
    if (!file) return std::nullopt;
    
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        lines.push_back(std::move(line));
    }
    return lines;
}

/**
 * @brief Write string to file
 * @param path File path
 * @param content File content
 * @param append Append instead of overwrite
 * @return Operation result
 */
[[nodiscard]] inline FileResult writeFile(const Path& path, std::string_view content,
                                          bool append = false) {
    auto mode = std::ios::binary;
    if (append) mode |= std::ios::app;
    
    std::ofstream file(path, mode);
    if (!file) return FileResult::fail("Failed to open file for writing");
    
    file.write(content.data(), static_cast<std::streamsize>(content.size()));
    return file ? FileResult::ok() : FileResult::fail("Write failed");
}

/**
 * @brief Write binary data to file
 * @param path File path
 * @param data Binary data
 * @return Operation result
 */
[[nodiscard]] inline FileResult writeBinaryFile(const Path& path, 
                                                const std::vector<uint8_t>& data) {
    std::ofstream file(path, std::ios::binary);
    if (!file) return FileResult::fail("Failed to open file for writing");
    
    file.write(reinterpret_cast<const char*>(data.data()), 
               static_cast<std::streamsize>(data.size()));
    return file ? FileResult::ok() : FileResult::fail("Write failed");
}

/**
 * @brief Write lines to file
 * @param path File path
 * @param lines Lines to write
 * @return Operation result
 */
[[nodiscard]] inline FileResult writeLines(const Path& path, 
                                           const std::vector<std::string>& lines) {
    std::ofstream file(path);
    if (!file) return FileResult::fail("Failed to open file for writing");
    
    for (const auto& line : lines) {
        file << line << '\n';
    }
    return file ? FileResult::ok() : FileResult::fail("Write failed");
}

// ============================================================================
// Directory Traversal
// ============================================================================

/**
 * @brief List directory contents
 * @param path Directory path
 * @param recursive Include subdirectories
 * @return Vector of paths or empty on error
 */
[[nodiscard]] inline std::vector<Path> listDirectory(const Path& path, 
                                                     bool recursive = false) {
    std::vector<Path> result;
    std::error_code ec;
    
    if (recursive) {
        for (const auto& entry : RecursiveIterator(path, ec)) {
            result.push_back(entry.path());
        }
    } else {
        for (const auto& entry : DirectoryIterator(path, ec)) {
            result.push_back(entry.path());
        }
    }
    
    return result;
}

/**
 * @brief List files in directory (not subdirectories)
 * @param path Directory path
 * @param recursive Include subdirectories
 * @return Vector of file paths
 */
[[nodiscard]] inline std::vector<Path> listFiles(const Path& path, 
                                                 bool recursive = false) {
    std::vector<Path> result;
    std::error_code ec;
    
    auto process = [&](const DirectoryEntry& entry) {
        if (entry.is_regular_file(ec)) {
            result.push_back(entry.path());
        }
    };
    
    if (recursive) {
        for (const auto& entry : RecursiveIterator(path, ec)) {
            process(entry);
        }
    } else {
        for (const auto& entry : DirectoryIterator(path, ec)) {
            process(entry);
        }
    }
    
    return result;
}

/**
 * @brief List subdirectories
 * @param path Directory path
 * @param recursive Include nested subdirectories
 * @return Vector of directory paths
 */
[[nodiscard]] inline std::vector<Path> listDirectories(const Path& path,
                                                       bool recursive = false) {
    std::vector<Path> result;
    std::error_code ec;
    
    auto process = [&](const DirectoryEntry& entry) {
        if (entry.is_directory(ec)) {
            result.push_back(entry.path());
        }
    };
    
    if (recursive) {
        for (const auto& entry : RecursiveIterator(path, ec)) {
            process(entry);
        }
    } else {
        for (const auto& entry : DirectoryIterator(path, ec)) {
            process(entry);
        }
    }
    
    return result;
}

/**
 * @brief Find files matching pattern
 * @param path Search directory
 * @param extension File extension to match (without dot)
 * @param recursive Include subdirectories
 * @return Matching file paths
 */
[[nodiscard]] inline std::vector<Path> findByExtension(const Path& path,
                                                       std::string_view extension,
                                                       bool recursive = false) {
    auto files = listFiles(path, recursive);
    std::vector<Path> result;
    
    for (const auto& file : files) {
        if (getExtension(file) == extension) {
            result.push_back(file);
        }
    }
    return result;
}

/**
 * @brief Walk directory tree with callback
 * @param path Root directory
 * @param callback Function called for each entry
 */
inline void walkDirectory(const Path& path, 
                         std::function<void(const Path&, bool isDir)> callback) {
    std::error_code ec;
    for (const auto& entry : RecursiveIterator(path, ec)) {
        callback(entry.path(), entry.is_directory(ec));
    }
}

// ============================================================================
// Temporary Files
// ============================================================================

/**
 * @brief Get system temporary directory
 * @return Temp directory path
 */
[[nodiscard]] inline Path getTempDirectory() {
    std::error_code ec;
    return stdfs::temp_directory_path(ec);
}

/**
 * @brief Generate unique temporary filename
 * @param prefix Filename prefix
 * @param extension File extension
 * @return Unique temporary file path
 */
[[nodiscard]] inline Path generateTempPath(std::string_view prefix = "tmp",
                                           std::string_view extension = "") {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<uint64_t> dist;
    
    std::ostringstream oss;
    oss << prefix << "_" << std::hex << dist(gen);
    if (!extension.empty()) {
        if (extension[0] != '.') oss << '.';
        oss << extension;
    }
    
    return getTempDirectory() / oss.str();
}

/**
 * @brief RAII temporary file (deleted on destruction)
 */
class TempFile {
public:
    /**
     * @brief Create temporary file
     * @param prefix Filename prefix
     * @param extension File extension
     */
    explicit TempFile(std::string_view prefix = "tmp", 
                      std::string_view extension = "")
        : path_(generateTempPath(prefix, extension)) {
        // Create empty file
        std::ofstream(path_).close();
    }
    
    /**
     * @brief Destructor - removes file
     */
    ~TempFile() {
        std::error_code ec;
        stdfs::remove(path_, ec);
    }
    
    // Non-copyable
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    
    // Movable
    TempFile(TempFile&& other) noexcept : path_(std::move(other.path_)) {
        other.path_.clear();
    }
    
    TempFile& operator=(TempFile&& other) noexcept {
        if (this != &other) {
            std::error_code ec;
            stdfs::remove(path_, ec);
            path_ = std::move(other.path_);
            other.path_.clear();
        }
        return *this;
    }
    
    /**
     * @brief Get file path
     */
    [[nodiscard]] const Path& path() const noexcept { return path_; }
    
    /**
     * @brief Release ownership (file won't be deleted)
     */
    Path release() {
        Path result = std::move(path_);
        path_.clear();
        return result;
    }

private:
    Path path_;
};

/**
 * @brief RAII temporary directory (deleted on destruction)
 */
class TempDirectory {
public:
    /**
     * @brief Create temporary directory
     * @param prefix Directory name prefix
     */
    explicit TempDirectory(std::string_view prefix = "tmpdir")
        : path_(generateTempPath(prefix)) {
        std::error_code ec;
        stdfs::create_directories(path_, ec);
    }
    
    /**
     * @brief Destructor - removes directory recursively
     */
    ~TempDirectory() {
        std::error_code ec;
        stdfs::remove_all(path_, ec);
    }
    
    // Non-copyable
    TempDirectory(const TempDirectory&) = delete;
    TempDirectory& operator=(const TempDirectory&) = delete;
    
    // Movable
    TempDirectory(TempDirectory&& other) noexcept : path_(std::move(other.path_)) {
        other.path_.clear();
    }
    
    TempDirectory& operator=(TempDirectory&& other) noexcept {
        if (this != &other) {
            std::error_code ec;
            stdfs::remove_all(path_, ec);
            path_ = std::move(other.path_);
            other.path_.clear();
        }
        return *this;
    }
    
    /**
     * @brief Get directory path
     */
    [[nodiscard]] const Path& path() const noexcept { return path_; }
    
    /**
     * @brief Release ownership (directory won't be deleted)
     */
    Path release() {
        Path result = std::move(path_);
        path_.clear();
        return result;
    }

private:
    Path path_;
};

// ============================================================================
// Current Directory
// ============================================================================

/**
 * @brief Get current working directory
 * @return Current directory path
 */
[[nodiscard]] inline Path currentDirectory() {
    std::error_code ec;
    return stdfs::current_path(ec);
}

/**
 * @brief Set current working directory
 * @param path New working directory
 * @return Operation result
 */
[[nodiscard]] inline FileResult setCurrentDirectory(const Path& path) {
    std::error_code ec;
    stdfs::current_path(path, ec);
    return FileResult::fromError(ec);
}

/**
 * @brief RAII directory change (restores on destruction)
 */
class ScopedDirectory {
public:
    /**
     * @brief Change to directory
     * @param path Target directory
     */
    explicit ScopedDirectory(const Path& path)
        : original_(currentDirectory()) {
        setCurrentDirectory(path);
    }
    
    /**
     * @brief Destructor - restores original directory
     */
    ~ScopedDirectory() {
        setCurrentDirectory(original_);
    }
    
    // Non-copyable, non-movable
    ScopedDirectory(const ScopedDirectory&) = delete;
    ScopedDirectory& operator=(const ScopedDirectory&) = delete;
    ScopedDirectory(ScopedDirectory&&) = delete;
    ScopedDirectory& operator=(ScopedDirectory&&) = delete;

private:
    Path original_;
};

} // namespace fs
} // namespace dfsmshsm

#endif // HSM_FILESYSTEM_HPP
