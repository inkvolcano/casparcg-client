-- Add HttpLog panel default size mode.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PanelSizeMode_HttpLog', 'fixed');

-- Auto-add HttpLog to the layout panel that contains Inspector (before Inspector).
-- Check each LayoutPanel column for Inspector and insert HttpLog before it.
UPDATE Configuration SET Value = REPLACE(Value, 'Inspector', 'HttpLog,Inspector')
    WHERE Name IN ('LayoutPanel1', 'LayoutPanel2', 'LayoutPanel3', 'LayoutPanel4')
    AND Value LIKE '%Inspector%'
    AND Value NOT LIKE '%HttpLog%';
