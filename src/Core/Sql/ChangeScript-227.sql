UPDATE Configuration SET Value = replace(Value, 'StatusPanel', 'ServerStatus,Activity,TriggerBanks') WHERE Name LIKE 'LayoutPanel%' AND Value LIKE '%StatusPanel%';
INSERT INTO Configuration (Name, Value) SELECT 'ActivityGrowMode', 'grow' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'ActivityGrowMode');
