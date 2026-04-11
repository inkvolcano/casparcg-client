INSERT INTO Configuration (Name, Value) SELECT 'ShowPVWButton', 'true' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'ShowPVWButton');
INSERT INTO Configuration (Name, Value) SELECT 'ShowServers', 'true' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'ShowServers');
INSERT INTO Configuration (Name, Value) SELECT 'ShowChannelLocks', 'true' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'ShowChannelLocks');
INSERT INTO Configuration (Name, Value) SELECT 'ShowChannelHeaders', 'true' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'ShowChannelHeaders');
INSERT INTO Configuration (Name, Value) SELECT 'ShowBankIcons', 'true' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'ShowBankIcons');
INSERT INTO Configuration (Name, Value) SELECT 'DisconnectMode', 'ask' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'DisconnectMode');
