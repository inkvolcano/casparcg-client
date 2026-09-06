-- Reconnect NDI tiles to their last sources on startup.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('NdiRestoreOutputs', 'true');
