-- Panel sizing mode defaults.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelSizeMode_AudioLevels', 'fixed');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelSizeMode_Preview', 'resizable');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelSizeMode_Library', 'fixed');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelSizeMode_Inspector', 'fixed');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelSizeMode_ServerStatus', 'fixed');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelSizeMode_Activity', 'fixed');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelSizeMode_TriggerBanks', 'fixed');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelSizeMode_Live', 'resizable');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelSizeMode_NDI', 'resizable');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelSizeMode_Performance', 'fixed');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelSizeMode_Clock', 'fixed');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelSizeMode_StatusBar', 'fixed');
-- Panel expanding toggle defaults.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelExpanding_AudioLevels', 'false');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelExpanding_Preview', 'false');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelExpanding_Library', 'true');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelExpanding_Inspector', 'true');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelExpanding_ServerStatus', 'false');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelExpanding_Activity', 'true');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelExpanding_TriggerBanks', 'false');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelExpanding_Live', 'false');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelExpanding_NDI', 'false');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelExpanding_Performance', 'false');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelExpanding_Clock', 'false');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelExpanding_StatusBar', 'false');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('BottomWidgetAnchor', 'down');
-- Migrate old "expanding" mode values to the new split system.
UPDATE Configuration SET Value = 'fixed' WHERE Name LIKE 'PanelSizeMode_%' AND Value = 'expanding';
