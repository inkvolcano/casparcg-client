-- Auto-save. A rundown is a file and nothing was ever written without being
-- asked for, so a crash took whatever had not been saved. This keeps a recovery
-- copy of anything with unsaved changes, offered back at the next launch and
-- thrown away on a clean quit. It never writes to the rundown's own file.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('AutoSaveEnabled', 'true');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('AutoSaveMinutes', '3');

-- Shell command items run a program on this machine, which is not something a
-- rundown arriving from somewhere else should be able to do unannounced. Off
-- until it is turned on, and an item in a rundown says so rather than failing
-- silently.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('AllowShellCommands', 'false');
