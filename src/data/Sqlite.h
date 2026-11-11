#pragma once
#include <filesystem>
#include <string>
#include <winsqlite/winsqlite3.h>
namespace Storage {
// Close every connection on scope exit, including one whose open operation only partly succeeded.
struct Connection {
    sqlite3* CurrentHandle = nullptr;
    // Close an owned SQLite connection on scope exit when its handle is nonnull, including a
    // partially successful open. Statements must finish before their connection owner is
    // destroyed.
    ~Connection() {
        if (CurrentHandle)
            sqlite3_close(CurrentHandle);
    }
    // Disallow copy construction so a second object cannot duplicate exclusive state or resource
    // handles. Keep a single owner or use an explicitly supported move constructor instead.
    Connection(const Connection&) = delete;
    // Disallow copy assignment so two objects cannot accidentally share exclusive state or
    // resource ownership. Retain the existing owner or use an explicitly supported move operation
    // instead.
    Connection& operator=(const Connection&) = delete;
    // Use the compiler-generated default constructor and the member initializers declared in this
    // type. No external resources are acquired here; later operations establish the object's
    // usable state.
    Connection() = default;

    // Open the retained database path with SQLite's UTF-16 Windows API and set a short busy
    // timeout for concurrent writers. Return false for opening or timeout configuration errors,
    // retaining cleanup responsibility in this owner.
    bool Open(const std::filesystem::path& Path) {
        if (sqlite3_open16(Path.c_str(), &CurrentHandle) != SQLITE_OK)
            return false;
        return sqlite3_busy_timeout(CurrentHandle, 250) == SQLITE_OK;
    }
    // Execute fixed SQL without interpolating user-provided values and report whether SQLite
    // completed successfully. Use prepared Statement bindings for names and other variable data.
    bool Execute(const char* Sql) {
        return sqlite3_exec(CurrentHandle, Sql, nullptr, nullptr, nullptr) == SQLITE_OK;
    }
};
// Finalize prepared statements before their owning connection is closed.
struct Statement {
    sqlite3_stmt* CurrentHandle = nullptr;
    // Disallow copy construction so a second object cannot duplicate exclusive state or resource
    // handles. Keep a single owner or use an explicitly supported move constructor instead.
    Statement(const Statement&) = delete;
    // Disallow copy assignment so two objects cannot accidentally share exclusive state or
    // resource ownership. Retain the existing owner or use an explicitly supported move operation
    // instead.
    Statement& operator=(const Statement&) = delete;
    // Use the compiler-generated default constructor and the member initializers declared in this
    // type. No external resources are acquired here; later operations establish the object's
    // usable state.
    Statement() = default;
    // Finalize the owned prepared statement when its scope ends, releasing SQLite resources
    // before the surrounding connection closes. An empty statement needs no cleanup.
    ~Statement() {
        if (CurrentHandle)
            sqlite3_finalize(CurrentHandle);
    }

    // Compile fixed SQL against the supplied open connection and retain the resulting statement
    // handle. Return false on preparation failure while the statement owner still handles any
    // allocated cleanup.
    bool Prepare(Connection& CurrentConnection, const char* Sql) {
        return sqlite3_prepare_v2(CurrentConnection.CurrentHandle, Sql, -1, &CurrentHandle, nullptr) == SQLITE_OK;
    }
    // Bind a string to a one-based prepared-statement parameter using SQLITE_TRANSIENT. SQLite
    // copies the bytes, so the caller's string does not have to survive until the statement is
    // stepped.
    bool Text(int Index, const std::string& Value) {
        return sqlite3_bind_text(CurrentHandle, Index, Value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
    }
};
// Roll back uncommitted changes automatically when any statement or the commit itself fails.
struct Transaction {
    Connection& CurrentConnection;
    bool Active;
    // Begin an immediate write transaction on the borrowed connection and record whether
    // acquisition succeeded. The connection must outlive this guard; uncommitted active work is
    // rolled back automatically.
    explicit Transaction(Connection& Value)
        : CurrentConnection(Value), Active(Value.Execute("BEGIN IMMEDIATE")) {}
    // Disallow copy construction so a second object cannot duplicate exclusive state or resource
    // handles. Keep a single owner or use an explicitly supported move constructor instead.
    Transaction(const Transaction&) = delete;
    // Disallow copy assignment so two objects cannot accidentally share exclusive state or
    // resource ownership. Retain the existing owner or use an explicitly supported move operation
    // instead.
    Transaction& operator=(const Transaction&) = delete;
    // Roll back an active transaction when scope exits before a successful commit. This protects
    // multi-statement profile and score changes from partial publication after an error.
    ~Transaction() {
        if (Active)
            CurrentConnection.Execute("ROLLBACK");
    }
    // Commit an active transaction and clear rollback responsibility only when SQLite reports
    // success. Return false otherwise, leaving the guard active so destruction can roll back the
    // unfinished operation.
    bool Commit() {
        if (!Active || !CurrentConnection.Execute("COMMIT"))
            return false;
        Active = false;
        return true;
    }
};
}
