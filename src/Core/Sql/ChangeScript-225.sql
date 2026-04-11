INSERT INTO Configuration (Name, Value) SELECT 'HideWhatsNew', 'false' WHERE NOT EXISTS (SELECT 1 FROM Configuration WHERE Name = 'HideWhatsNew');
