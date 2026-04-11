INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('ShowSTEPButton', 'true');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('HotkeyToggleAutostep', 'Ctrl+Shift+N');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('HotkeyToggleAutostepAlt', '');
DELETE FROM Configuration WHERE Name = 'PreviewOnAutoStep';
DELETE FROM Configuration WHERE Name = 'ClearDelayedCommandsOnAutoStep';
