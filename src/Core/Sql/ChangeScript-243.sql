-- Pulling template packs from a relay, so a dev machine on another network can
-- reach this client without this client opening anything inbound. Off until an
-- operator turns it on, and an empty token never matches.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('RelayEnabled', 'false');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('RelayUrl', '');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('RelayToken', '');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('RelayPollMinutes', '15');
-- Empty means every pack the relay carries. A list means only those.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('RelayPacks', '');
