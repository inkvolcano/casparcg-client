-- Merge PanelExpanding toggle back into PanelSizeMode as 3-option system.
-- Where PanelExpanding was true, set the size mode to 'expanding'.
UPDATE Configuration SET Value = 'expanding'
    WHERE Name = 'PanelSizeMode_AudioLevels'
    AND EXISTS (SELECT 1 FROM Configuration WHERE Name = 'PanelExpanding_AudioLevels' AND Value = 'true');
UPDATE Configuration SET Value = 'expanding'
    WHERE Name = 'PanelSizeMode_Preview'
    AND EXISTS (SELECT 1 FROM Configuration WHERE Name = 'PanelExpanding_Preview' AND Value = 'true');
UPDATE Configuration SET Value = 'expanding'
    WHERE Name = 'PanelSizeMode_Library'
    AND EXISTS (SELECT 1 FROM Configuration WHERE Name = 'PanelExpanding_Library' AND Value = 'true');
UPDATE Configuration SET Value = 'expanding'
    WHERE Name = 'PanelSizeMode_Inspector'
    AND EXISTS (SELECT 1 FROM Configuration WHERE Name = 'PanelExpanding_Inspector' AND Value = 'true');
UPDATE Configuration SET Value = 'expanding'
    WHERE Name = 'PanelSizeMode_ServerStatus'
    AND EXISTS (SELECT 1 FROM Configuration WHERE Name = 'PanelExpanding_ServerStatus' AND Value = 'true');
UPDATE Configuration SET Value = 'expanding'
    WHERE Name = 'PanelSizeMode_Activity'
    AND EXISTS (SELECT 1 FROM Configuration WHERE Name = 'PanelExpanding_Activity' AND Value = 'true');
UPDATE Configuration SET Value = 'expanding'
    WHERE Name = 'PanelSizeMode_TriggerBanks'
    AND EXISTS (SELECT 1 FROM Configuration WHERE Name = 'PanelExpanding_TriggerBanks' AND Value = 'true');
UPDATE Configuration SET Value = 'expanding'
    WHERE Name = 'PanelSizeMode_Live'
    AND EXISTS (SELECT 1 FROM Configuration WHERE Name = 'PanelExpanding_Live' AND Value = 'true');
UPDATE Configuration SET Value = 'expanding'
    WHERE Name = 'PanelSizeMode_NDI'
    AND EXISTS (SELECT 1 FROM Configuration WHERE Name = 'PanelExpanding_NDI' AND Value = 'true');
UPDATE Configuration SET Value = 'expanding'
    WHERE Name = 'PanelSizeMode_Performance'
    AND EXISTS (SELECT 1 FROM Configuration WHERE Name = 'PanelExpanding_Performance' AND Value = 'true');
UPDATE Configuration SET Value = 'expanding'
    WHERE Name = 'PanelSizeMode_Clock'
    AND EXISTS (SELECT 1 FROM Configuration WHERE Name = 'PanelExpanding_Clock' AND Value = 'true');
UPDATE Configuration SET Value = 'expanding'
    WHERE Name = 'PanelSizeMode_StatusBar'
    AND EXISTS (SELECT 1 FROM Configuration WHERE Name = 'PanelExpanding_StatusBar' AND Value = 'true');
-- Remove all PanelExpanding rows.
DELETE FROM Configuration WHERE Name LIKE 'PanelExpanding_%';
