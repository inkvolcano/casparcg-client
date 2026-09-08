-- Receiving template packs pushed from a dev machine. Off, and with no token,
-- until an operator turns it on: an empty token never matches.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('TemplatePushEnabled', 'false');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('TemplatePushToken', '');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('TemplatePushPath', '');
