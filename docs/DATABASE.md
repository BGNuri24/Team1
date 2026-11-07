# Local best-time database

The Windows game stores personal bests in `save_data/maze_game.db`, using the SQLite library provided by Windows 10/11. No database server, account, extra download or connection string is required. The existing F5 build links `winsqlite3.lib` from the Windows SDK.

Each `best_times` row contains `player`, `maze_size`, `mode` (`normal` or `zero_gravity`) and `seconds`. The first three columns form the primary key. Saving a run inserts its first result or improves a slower result within a transaction. Equal or slower runs leave the record unchanged.

On first use, the game imports valid scores from `save_data/best_times.txt`, or its `.bak` file when the main text file is absent. For the default application database, missing local legacy files fall back to `src/assets/legacy/` in a checkout or `assets/legacy/` beside the executable. Explicit custom databases only import from their own directory. It imports only existing profiles, keeps the fastest duplicate for each size and mode, and skips malformed records. The import and its completion marker commit together. A failed import can be retried on the next launch. The original text files remain unchanged as a backup; subsequent scores are saved only in the database. Reading relocated files does not recreate the old tracked paths at the repository root.

Profiles are stored in the database's `players` table, ordered by `sort_order`. Existing `players.txt` profiles (or its `.bak` file) are imported once. Both profile deletion and deletion of all associated scores commit in one transaction; an error rolls back both. Adding a profile also checks the five-player limit inside a transaction. Existing score-only databases are upgraded without replacing their results. Legacy TXT files are left untouched as backups and are no longer written by the game. Sensitivity remains separate in `save_data/settings.txt`.

To transfer progress to another PC, close the game and copy `maze_game.db` into the other copy's `save_data` folder; it contains both profiles and scores. Copy `settings.txt` too if you want the same preferences. The database and its temporary journal files are ignored by Git; include the database explicitly when sharing personal progress in a ZIP.

To inspect results in any SQLite database viewer, open a copy of the database and run:

```sql
SELECT player, maze_size, mode, seconds
FROM best_times
ORDER BY maze_size, mode, seconds;
```

If a database is locked or cannot be written, the game does not report a successful personal best. The results screen shows a save-failure message. A damaged database is left untouched rather than silently replaced. Close any tool holding a write lock before trying another run; after repairing file access or restoring a database backup, restart the game.
