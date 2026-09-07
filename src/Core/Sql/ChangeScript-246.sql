-- Normally the source decides which packs a machine takes, so one person can
-- run the whole estate from one place. This is the way out of that for a single
-- machine: set it and the Packs field on this client wins instead.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('RelayPacksLocal', 'false');
