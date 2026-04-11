INSERT INTO Configuration (Name, Value) SELECT 'ClockTimezone1', 'Local'
    WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'ClockTimezone1');
INSERT INTO Configuration (Name, Value) SELECT 'ClockTimezone2', 'UTC'
    WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'ClockTimezone2');
