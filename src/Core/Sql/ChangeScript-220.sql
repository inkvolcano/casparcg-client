INSERT INTO Configuration (Name, Value) SELECT 'PreviewModifier', 'Shift' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'PreviewModifier');
INSERT INTO Configuration (Name, Value) SELECT 'HotkeyTogglePreview', 'Ctrl+P' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'HotkeyTogglePreview');
INSERT INTO Configuration (Name, Value) SELECT 'HotkeyTogglePreviewAlt', '' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'HotkeyTogglePreviewAlt');
