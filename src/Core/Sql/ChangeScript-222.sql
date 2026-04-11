UPDATE Configuration SET Value = 'false' WHERE Name = 'ShowPreviewPanel';
UPDATE Configuration SET Value = 'false' WHERE Name = 'ShowLivePanel';
INSERT INTO Configuration (Name, Value) SELECT 'ShowServerStatusPanel', 'true' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'ShowServerStatusPanel');
INSERT INTO Configuration (Name, Value) SELECT 'ShowActivityPanel', 'true' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'ShowActivityPanel');
INSERT INTO Configuration (Name, Value) SELECT 'ShowTriggerBanksPanel', 'true' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'ShowTriggerBanksPanel');
