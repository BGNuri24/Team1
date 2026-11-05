#pragma once
#include <filesystem>
#include <string>
#include <winsqlite/winsqlite3.h>
namespace storage {
// Close every connection on scope exit, including one whose open operation only partly succeeded.
struct Connection {
    sqlite3* handle = nullptr;
    ~Connection() {
        if (handle)
            sqlite3_close(handle);
    }
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection() = default;

    // Use UTF-16 for Windows paths and wait briefly for another game's write transaction.
    bool open(const std::filesystem::path& path) {
        if (sqlite3_open16(path.c_str(), &handle) != SQLITE_OK)
            return false;
        return sqlite3_busy_timeout(handle, 250) == SQLITE_OK;
    }
    // Execute fixed SQL without interpolating any profile names or other user-provided values.
    bool execute(const char* sql) {
        return sqlite3_exec(handle, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
    }
};
// Finalize prepared statements before their owning connection is closed.
struct Statement {
    sqlite3_stmt* handle = nullptr;
    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;
    Statement() = default;
    ~Statement() {
        if (handle)
            sqlite3_finalize(handle);
    }

    // Compile a fixed query and retain its handle only for the surrounding operation.
    bool prepare(Connection& connection, const char* sql) {
        return sqlite3_prepare_v2(connection.handle, sql, -1, &handle, nullptr) == SQLITE_OK;
    }
    // Copy text into the statement so caller-owned strings need not outlive query execution.
    bool text(int index, const std::string& value) {
        return sqlite3_bind_text(handle, index, value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
    }
};
// Roll back uncommitted changes automatically when any statement or the commit itself fails.
struct Transaction {
    Connection& connection;
    bool active;
    explicit Transaction(Connection& value)
        : connection(value), active(value.execute("BEGIN IMMEDIATE")) {}
    Transaction(const Transaction&) = delete;
    Transaction& operator=(const Transaction&) = delete;
    ~Transaction() {
        if (active)
            connection.execute("ROLLBACK");
    }
    // Publish the complete write operation; retain rollback responsibility if publication fails.
    bool commit() {
        if (!active || !connection.execute("COMMIT"))
            return false;
        active = false;
        return true;
    }
};
}
