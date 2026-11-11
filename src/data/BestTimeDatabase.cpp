#include "BestTimeDatabase.h"
#include "Validation.h"
#include <winsqlite/winsqlite3.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <locale>
#include <sstream>
#include <utility>

namespace {
    const std::filesystem::path DatabasePath = "save_data/maze_game.db";

    // Close every connection on scope exit, including one whose open operation only partly succeeded.
    struct Connection {
        sqlite3* CurrentHandle = nullptr;
        // Close an owned SQLite connection on scope exit when its handle is nonnull, including a
        // partially successful open. Statements must finish before their connection owner is
        // destroyed.
        ~Connection() { if (CurrentHandle) sqlite3_close(CurrentHandle); }
        // Disallow copy construction so a second object cannot duplicate exclusive state or
        // resource handles. Keep a single owner or use an explicitly supported move constructor
        // instead.
        Connection(const Connection&) = delete;
        // Disallow copy assignment so two objects cannot accidentally share exclusive state or
        // resource ownership. Retain the existing owner or use an explicitly supported move
        // operation instead.
        Connection& operator=(const Connection&) = delete;
        // Use the compiler-generated default constructor and the member initializers declared in
        // this type. No external resources are acquired here; later operations establish the
        // object's usable state.
        Connection() = default;

        // Open the legacy implementation's fixed database path using SQLite's UTF-16 Windows API
        // and set a short busy timeout. Return false on setup failure while the owning Connection
        // retains cleanup responsibility.
        bool Open() {
            if (sqlite3_open16(DatabasePath.c_str(), &CurrentHandle) != SQLITE_OK) return false;
            return sqlite3_busy_timeout(CurrentHandle, 250) == SQLITE_OK;
        }
        // Execute fixed SQL without interpolating user-provided values and report whether SQLite
        // completed successfully. Use prepared Statement bindings for names and other variable
        // data.
        bool Execute(const char* Sql) {
            return sqlite3_exec(CurrentHandle, Sql, nullptr, nullptr, nullptr) == SQLITE_OK;
        }
    };

    // Finalize prepared statements before their owning connection is closed.
    struct Statement {
        sqlite3_stmt* CurrentHandle = nullptr;
        // Disallow copy construction so a second object cannot duplicate exclusive state or
        // resource handles. Keep a single owner or use an explicitly supported move constructor
        // instead.
        Statement(const Statement&) = delete;
        // Disallow copy assignment so two objects cannot accidentally share exclusive state or
        // resource ownership. Retain the existing owner or use an explicitly supported move
        // operation instead.
        Statement& operator=(const Statement&) = delete;
        // Use the compiler-generated default constructor and the member initializers declared in
        // this type. No external resources are acquired here; later operations establish the
        // object's usable state.
        Statement() = default;
        // Finalize the owned prepared statement when its scope ends, releasing SQLite resources
        // before the surrounding connection closes. An empty statement needs no cleanup.
        ~Statement() { if (CurrentHandle) sqlite3_finalize(CurrentHandle); }

        // Compile fixed SQL against the supplied open connection and retain the resulting
        // statement handle. Return false on preparation failure while the statement owner still
        // handles any allocated cleanup.
        bool Prepare(Connection& CurrentConnection, const char* Sql) {
            return sqlite3_prepare_v2(CurrentConnection.CurrentHandle, Sql, -1, &CurrentHandle, nullptr) == SQLITE_OK;
        }
        // Bind a string to a one-based prepared-statement parameter using SQLITE_TRANSIENT.
        // SQLite copies the bytes, so the caller's string does not have to survive until the
        // statement is stepped.
        bool Text(int Index, const std::string& Value) {
            return sqlite3_bind_text(CurrentHandle, Index, Value.c_str(), -1, SQLITE_TRANSIENT) == SQLITE_OK;
        }
    };

    // Roll back uncommitted changes automatically when any statement or the commit itself fails.
    struct Transaction {
        Connection& CurrentConnection;
        bool Active;
        // Begin an immediate write transaction on the borrowed connection and record whether
        // acquisition succeeded. The connection must outlive this guard; uncommitted active work
        // is rolled back automatically.
        explicit Transaction(Connection& Value) : CurrentConnection(Value), Active(Value.Execute("BEGIN IMMEDIATE")) {}
        // Roll back an active transaction when scope exits before a successful commit. This
        // protects multi-statement profile and score changes from partial publication after an
        // error.
        ~Transaction() { if (Active) CurrentConnection.Execute("ROLLBACK"); }
        // Commit an active transaction and clear rollback responsibility only when SQLite reports
        // success. Return false otherwise, leaving the guard active so destruction can roll back
        // the unfinished operation.
        bool Commit() {
            if (!Active || !CurrentConnection.Execute("COMMIT")) return false;
            Active = false;
            return true;
        }
    };

    // Translate the supported GameMode values into the stable strings stored in SQLite. Return
    // nullptr for an unsupported enum value so callers can reject it before binding or executing
    // a query.
    const char* ModeKey(GameMode Mode) {
        if (Mode == GameMode::Normal) return "normal";
        if (Mode == GameMode::ZeroGravity) return "zero_gravity";
        return nullptr;
    }

    // Read profiles in sort_order order into a temporary vector, validating names and the
    // player-count limit. Replace the caller's vector only after the complete query succeeds,
    // leaving it unchanged on any failure.
    bool ReadPlayers(Connection& CurrentConnection, std::vector<std::string>& CurrentPlayers) {
        Statement CurrentQuery;
        if (!CurrentQuery.Prepare(CurrentConnection, "SELECT name FROM players ORDER BY sort_order")) return false;
        std::vector<std::string> Result;
        int Status = SQLITE_ROW;
        while ((Status = sqlite3_step(CurrentQuery.CurrentHandle)) == SQLITE_ROW) {
            const auto Value = sqlite3_column_text(CurrentQuery.CurrentHandle, 0);
            if (!Value) return false;
            Result.emplace_back((const char*)Value);
            if (!IsValidPlayerName(Result.back()) || Result.size() > MaxPlayers) return false;
        }
        if (Status != SQLITE_DONE) return false;
        CurrentPlayers = std::move(Result);
        return true;
    }

    // Query whether the named migration marker exists and place that answer in exists. A missing
    // row is a successful query with exists false; preparation, binding, or stepping errors
    // return false.
    bool HasMarker(Connection& CurrentConnection, const char* Key, bool& Exists) {
        Statement Marker;
        if (!Marker.Prepare(CurrentConnection, "SELECT 1 FROM storage_metadata WHERE key=?1") || !Marker.Text(1,Key)) return false;
        const int Status = sqlite3_step(Marker.CurrentHandle);
        if (Status != SQLITE_ROW && Status != SQLITE_DONE) return false;
        Exists = Status == SQLITE_ROW;
        return true;
    }

    // Read valid, unique names from save_data/players.txt, falling back to its backup when
    // absent. Respect the player limit and preserve the source; this helper belongs to the older,
    // inactive storage implementation.
    bool ReadLegacyPlayers(std::vector<std::string>& CurrentPlayers) {
        std::filesystem::path Source = "save_data/players.txt";
        std::error_code Error;
        bool Exists = std::filesystem::exists(Source,Error);
        if (Error) return false;
        if (!Exists) {
            Source += ".bak";
            Exists = std::filesystem::exists(Source,Error);
            if (Error) return false;
        }
        CurrentPlayers.clear();
        if (!Exists) return true;
        std::ifstream CurrentInput(Source);
        if (!CurrentInput) return false;
        std::string CurrentName;
        while (std::getline(CurrentInput,CurrentName)) {
            if (!CurrentName.empty() && CurrentName.back() == '\r') CurrentName.pop_back();
            if (CurrentPlayers.size() < MaxPlayers && IsValidPlayerName(CurrentName) &&
                std::find(CurrentPlayers.begin(),CurrentPlayers.end(),CurrentName) == CurrentPlayers.end()) CurrentPlayers.push_back(CurrentName);
        }
        return !CurrentInput.bad();
    }

    // Bind player name, maze size, and mode to parameters 1, 2, and 3 of a prepared statement.
    // Return false for an unsupported mode or any binding error; the same ordering is used by
    // lookups and score writes.
    bool BindKey(Statement& CurrentStatement, const std::string& Player, int CurrentSize, GameMode Mode) {
        const char* Key = ModeKey(Mode);
        return Key && CurrentStatement.Text(1, Player) &&
            sqlite3_bind_int(CurrentStatement.CurrentHandle, 2, CurrentSize) == SQLITE_OK && CurrentStatement.Text(3, Key);
    }

    // Insert a score when its composite key is missing, then update an existing row only when the
    // new time is faster. Report SQL success separately from the changed flag; the caller
    // supplies the surrounding write transaction.
    bool WriteBest(Connection& CurrentConnection, const std::string& Player, int CurrentSize, float Seconds,
        GameMode Mode, bool& Changed) {
        Statement CurrentInsert;
        if (!CurrentInsert.Prepare(CurrentConnection, "INSERT OR IGNORE INTO best_times(player,maze_size,mode,seconds) VALUES(?1,?2,?3,?4)") ||
            !BindKey(CurrentInsert, Player, CurrentSize, Mode) ||
            sqlite3_bind_double(CurrentInsert.CurrentHandle, 4, Seconds) != SQLITE_OK ||
            sqlite3_step(CurrentInsert.CurrentHandle) != SQLITE_DONE) return false;
        Changed = sqlite3_changes(CurrentConnection.CurrentHandle) != 0;
        Statement Improve;
        if (!Improve.Prepare(CurrentConnection, "UPDATE best_times SET seconds=?4 WHERE player=?1 AND maze_size=?2 AND mode=?3 AND seconds>?4") ||
            !BindKey(Improve, Player, CurrentSize, Mode) ||
            sqlite3_bind_double(Improve.CurrentHandle, 4, Seconds) != SQLITE_OK ||
            sqlite3_step(Improve.CurrentHandle) != SQLITE_DONE) return false;
        Changed |= sqlite3_changes(CurrentConnection.CurrentHandle) != 0;
        return true;
    }

    // Import well-formed legacy score rows for existing profiles, retaining the fastest result
    // for each key. Keep the text sources untouched and let the caller commit the import and its
    // marker together.
    bool ImportLegacy(Connection& CurrentConnection, const std::vector<std::string>& CurrentPlayers) {
        std::filesystem::path Source = "save_data/best_times.txt";
        std::error_code Error;
        bool Exists = std::filesystem::exists(Source, Error);
        if (Error) return false;
        if (!Exists) {
            Source += ".bak";
            Exists = std::filesystem::exists(Source, Error);
            if (Error) return false;
        }
        if (!Exists) return true;
        std::ifstream CurrentInput(Source);
        if (!CurrentInput) return false;
        std::string Line;
        while (std::getline(CurrentInput, Line)) {
            std::istringstream Fields(Line);
            Fields.imbue(std::locale::classic());
            std::string Player, ModeName, Extra;
            int CurrentSize = 0;
            float Seconds = 0;
            GameMode Mode = GameMode::Normal;
            if (!(Fields >> Player >> CurrentSize >> Seconds)) continue;
            if (Fields >> ModeName) {
                if (ModeName == "zero_gravity") Mode = GameMode::ZeroGravity;
                else if (ModeName != "normal") continue;
                if (Fields >> Extra) continue;
            }
            if (!IsValidPlayerName(Player) || !IsValidSize(CurrentSize) || !std::isfinite(Seconds) || Seconds < 0 ||
                std::find(CurrentPlayers.begin(), CurrentPlayers.end(), Player) == CurrentPlayers.end()) continue;
            bool Changed = false;
            if (!WriteBest(CurrentConnection, Player, CurrentSize, Seconds, Mode, Changed)) return false;
        }
        return !CurrentInput.bad();
    }
}

// Initialize the legacy database implementation's schema and import markers inside one
// transaction. Publish the caller's profiles only after a successful commit; the active game
// build now uses GameDatabase instead.
bool BestTimeDatabase::Initialize(std::vector<std::string>& CurrentPlayers) {
    CurrentReady = false;
    Failed = true;
    std::error_code Error;
    std::filesystem::create_directories(DatabasePath.parent_path(), Error);
    if (Error) return false;
    Connection CurrentConnection;
    if (!CurrentConnection.Open()) return false;
    Transaction CurrentTransaction(CurrentConnection);
    if (!CurrentTransaction.Active || !CurrentConnection.Execute(
        "CREATE TABLE IF NOT EXISTS best_times("
        "player TEXT NOT NULL CHECK(length(player) BETWEEN 1 AND 12),"
        "maze_size INTEGER NOT NULL CHECK(maze_size BETWEEN 10 AND 50),"
        "mode TEXT NOT NULL CHECK(mode IN ('normal','zero_gravity')),"
        "seconds REAL NOT NULL CHECK(seconds>=0),"
        "PRIMARY KEY(player,maze_size,mode));"
        "CREATE TABLE IF NOT EXISTS storage_metadata(key TEXT PRIMARY KEY,value TEXT NOT NULL);"
        "CREATE TABLE IF NOT EXISTS players("
        "name TEXT PRIMARY KEY NOT NULL CHECK(length(name) BETWEEN 1 AND 12 AND name NOT GLOB '*[^A-Za-z0-9_]*'),"
        "sort_order INTEGER NOT NULL UNIQUE CHECK(sort_order>=0));")) return false;
    bool ProfilesImported = false;
    if (!HasMarker(CurrentConnection,"legacy_players_imported",ProfilesImported)) return false;
    if (!ProfilesImported) {
        std::vector<std::string> LegacyPlayers;
        if (!ReadLegacyPlayers(LegacyPlayers)) return false;
        for (int Index = 0; Index < (int)LegacyPlayers.size(); ++Index) {
            Statement CurrentInsert;
            if (!CurrentInsert.Prepare(CurrentConnection,"INSERT INTO players(name,sort_order) VALUES(?1,?2)") ||
                !CurrentInsert.Text(1,LegacyPlayers[Index]) || sqlite3_bind_int(CurrentInsert.CurrentHandle,2,Index) != SQLITE_OK ||
                sqlite3_step(CurrentInsert.CurrentHandle) != SQLITE_DONE) return false;
        }
        if (!CurrentConnection.Execute("INSERT INTO storage_metadata(key,value) VALUES('legacy_players_imported','1')")) return false;
    }
    std::vector<std::string> StoredPlayers;
    if (!ReadPlayers(CurrentConnection,StoredPlayers)) return false;
    bool Imported = false;
    if (!HasMarker(CurrentConnection,"legacy_best_times_imported",Imported)) return false;
    if (!Imported && (!ImportLegacy(CurrentConnection, StoredPlayers) || !CurrentConnection.Execute(
        "INSERT INTO storage_metadata(key,value) VALUES('legacy_best_times_imported','1')"))) return false;
    if (!CurrentTransaction.Commit()) return false;
    CurrentPlayers = std::move(StoredPlayers);
    CurrentReady = true;
    Failed = false;
    return true;
}

// Add a valid unique profile under a write transaction, checking current capacity and cleaning
// orphaned scores for that name. Refresh the caller's list only on commit; this legacy
// implementation is not part of the active game target.
bool BestTimeDatabase::AddPlayer(const std::string& Player, std::vector<std::string>& CurrentPlayers) {
    Failed = false;
    if (!IsValidPlayerName(Player)) return false;
    Failed = true;
    if (!CurrentReady) return false;
    Connection CurrentConnection;
    if (!CurrentConnection.Open()) return false;
    Transaction CurrentTransaction(CurrentConnection);
    std::vector<std::string> Current;
    if (!CurrentTransaction.Active || !ReadPlayers(CurrentConnection,Current)) return false;
    if (Current.size() >= MaxPlayers || std::find(Current.begin(),Current.end(),Player) != Current.end()) {
        Failed = false;
        return false;
    }
    Statement CurrentCleanup, CurrentInsert;
    if (!CurrentCleanup.Prepare(CurrentConnection,"DELETE FROM best_times WHERE player=?1") || !CurrentCleanup.Text(1,Player) ||
        sqlite3_step(CurrentCleanup.CurrentHandle) != SQLITE_DONE ||
        !CurrentInsert.Prepare(CurrentConnection,"INSERT INTO players(name,sort_order) SELECT ?1,COALESCE(MAX(sort_order),-1)+1 FROM players") ||
        !CurrentInsert.Text(1,Player) || sqlite3_step(CurrentInsert.CurrentHandle) != SQLITE_DONE ||
        !ReadPlayers(CurrentConnection,Current) || !CurrentTransaction.Commit()) return false;
    CurrentPlayers = std::move(Current);
    Failed = false;
    return true;
}

// Query the legacy database for the supplied player, size, and mode, returning -1 for a missing
// result or unavailable storage. Consult storageFailed to distinguish failure; active gameplay
// uses GameDatabase's explicit result type.
float BestTimeDatabase::BestTime(const std::string& Player, int CurrentSize, GameMode Mode) const {
    Failed = true;
    if (!CurrentReady) return -1;
    Connection CurrentConnection;
    Statement CurrentQuery;
    if (!CurrentConnection.Open() || !CurrentQuery.Prepare(CurrentConnection,
        "SELECT seconds FROM best_times WHERE player=?1 AND maze_size=?2 AND mode=?3") ||
        !BindKey(CurrentQuery, Player, CurrentSize, Mode)) return -1;
    const int Result = sqlite3_step(CurrentQuery.CurrentHandle);
    if (Result != SQLITE_ROW && Result != SQLITE_DONE) return -1;
    Failed = false;
    return Result == SQLITE_ROW ? (float)sqlite3_column_double(CurrentQuery.CurrentHandle, 0) : -1;
}

// Insert a first score or improve a slower result while holding an immediate transaction. This
// legacy API returns false for both unchanged scores and failures, with storageFailed
// distinguishing the latter.
bool BestTimeDatabase::Record(const std::string& Player, int CurrentSize, float Seconds, GameMode Mode) {
    Failed = false;
    if (!IsValidPlayerName(Player) || !IsValidSize(CurrentSize) || !std::isfinite(Seconds) || Seconds < 0 || !ModeKey(Mode)) return false;
    Failed = true;
    if (!CurrentReady) return false;
    Connection CurrentConnection;
    if (!CurrentConnection.Open()) return false;
    Transaction CurrentTransaction(CurrentConnection);
    std::vector<std::string> CurrentPlayers;
    if (!CurrentTransaction.Active || !ReadPlayers(CurrentConnection,CurrentPlayers)) return false;
    if (std::find(CurrentPlayers.begin(),CurrentPlayers.end(),Player) == CurrentPlayers.end()) {
        Failed = false;
        return false;
    }
    bool Changed = false;
    if (!CurrentTransaction.Active || !WriteBest(CurrentConnection, Player, CurrentSize, Seconds, Mode, Changed) ||
        !CurrentTransaction.Commit()) return false;
    Failed = false;
    return Changed;
}

// Delete a profile and every associated score atomically, publishing the refreshed profile list
// only on commit. Failure rolls the transaction back and leaves the caller's list unchanged.
bool BestTimeDatabase::RemovePlayer(const std::string& Player, std::vector<std::string>& CurrentPlayers) {
    Failed = true;
    if (!CurrentReady) return false;
    Connection CurrentConnection;
    if (!CurrentConnection.Open()) return false;
    Transaction CurrentTransaction(CurrentConnection);
    Statement Profile, Scores;
    std::vector<std::string> Remaining;
    if (!CurrentTransaction.Active || !Profile.Prepare(CurrentConnection,"DELETE FROM players WHERE name=?1") ||
        !Profile.Text(1,Player) || sqlite3_step(Profile.CurrentHandle) != SQLITE_DONE) return false;
    if (!sqlite3_changes(CurrentConnection.CurrentHandle)) { Failed = false; return false; }
    if (!Scores.Prepare(CurrentConnection,"DELETE FROM best_times WHERE player=?1") || !Scores.Text(1,Player) ||
        sqlite3_step(Scores.CurrentHandle) != SQLITE_DONE || !ReadPlayers(CurrentConnection,Remaining) ||
        !CurrentTransaction.Commit()) return false;
    CurrentPlayers = std::move(Remaining);
    Failed = false;
    return true;
}
