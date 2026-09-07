-- The NDI panel is keyed "NDI" everywhere: PanelAnchor_NDI, PanelSizeMode_NDI,
-- NDIPanelHeight and the rest. Two places spelled it "Ndi" instead, both for the
-- header colour overrides, and they agreed with each other so the feature worked
-- while quietly using a key nothing else would look under.
--
-- The code now says NDI. These carry any colour already chosen across to it. The
-- NDI-spelled rows cannot hold anything real, because only the Ndi-spelled ones
-- were ever written, so clearing them first loses nothing.
DELETE FROM Configuration WHERE Name IN ('HeaderLine_NDI', 'HeaderBlock_NDI', 'HeaderText_NDI');
UPDATE Configuration SET Name = 'HeaderLine_NDI'  WHERE Name = 'HeaderLine_Ndi';
UPDATE Configuration SET Name = 'HeaderBlock_NDI' WHERE Name = 'HeaderBlock_Ndi';
UPDATE Configuration SET Name = 'HeaderText_NDI'  WHERE Name = 'HeaderText_Ndi';

-- Left behind by a build that keyed the panel "Ndi". Nothing reads or writes it
-- now, and NDIPanelHeight beside it is the one in use.
DELETE FROM Configuration WHERE Name = 'NdiPanelHeight';
