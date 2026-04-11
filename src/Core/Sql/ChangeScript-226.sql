INSERT INTO Configuration (Name, Value) SELECT 'LayoutColumnOrder', 'panel1,mainwindow,panel2' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'LayoutColumnOrder');
INSERT INTO Configuration (Name, Value) SELECT 'LayoutPanel1', 'AudioLevels,Preview,Library,Duration,StatusBar' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'LayoutPanel1');
INSERT INTO Configuration (Name, Value) SELECT 'LayoutPanel2', 'Clock,ServerStatus,Activity,TriggerBanks,Live,Inspector' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'LayoutPanel2');
INSERT INTO Configuration (Name, Value) SELECT 'LayoutPanel3', '' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'LayoutPanel3');
INSERT INTO Configuration (Name, Value) SELECT 'LayoutPanel4', '' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'LayoutPanel4');
