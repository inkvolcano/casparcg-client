-- Migrate old header/tab color keys to new unified Header{Line,Block,Text} keys.
INSERT OR IGNORE INTO Configuration (Name, Value)
  SELECT 'HeaderLineMaster', Value FROM Configuration WHERE Name = 'WidgetHeaderColor';
INSERT OR IGNORE INTO Configuration (Name, Value)
  SELECT 'HeaderBlockMaster', Value FROM Configuration WHERE Name = 'TabColorMaster';
INSERT OR IGNORE INTO Configuration (Name, Value)
  SELECT 'HeaderBlockRundown', Value FROM Configuration WHERE Name = 'TabColorRundown';

-- Migrate per-widget line overrides (WidgetHeader_<Panel> -> HeaderLine_<Panel>).
INSERT OR IGNORE INTO Configuration (Name, Value)
  SELECT 'HeaderLine_' || SUBSTR(Name, 14), Value FROM Configuration
  WHERE Name LIKE 'WidgetHeader_%' AND Name != 'WidgetHeaderColor';

-- Default text color (white).
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES ('HeaderTextMaster', '#FFFFFFFF');

-- Clean up old keys.
DELETE FROM Configuration WHERE Name IN ('WidgetHeaderColor', 'TabColorEnabled', 'TabColorMaster', 'TabColorRundown');
DELETE FROM Configuration WHERE Name LIKE 'WidgetHeader_%';
