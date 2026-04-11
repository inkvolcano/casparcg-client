INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('DurationUnit', (SELECT Value FROM Configuration WHERE Name = 'DelayType'));
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('DurationFormat', 'HumanReadable');
