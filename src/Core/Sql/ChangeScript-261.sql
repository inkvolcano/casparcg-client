-- Settings nothing reads any more (build 313): the show-panel switches the
-- Layout editor replaced, the two autostep options global STEP replaced, the
-- TriCaster fields, and two leftovers. Tables of the same names are kept.
DELETE FROM Configuration WHERE Name = 'ShowAudioLevelsPanel';
DELETE FROM Configuration WHERE Name = 'ShowPreviewPanel';
DELETE FROM Configuration WHERE Name = 'ShowLivePanel';
DELETE FROM Configuration WHERE Name = 'ShowDurationPanel';
DELETE FROM Configuration WHERE Name = 'ShowServerStatusPanel';
DELETE FROM Configuration WHERE Name = 'ShowActivityPanel';
DELETE FROM Configuration WHERE Name = 'ShowTriggerBanksPanel';
DELETE FROM Configuration WHERE Name = 'ShowStatusPanel';
DELETE FROM Configuration WHERE Name = 'PreviewOnAutoStep';
DELETE FROM Configuration WHERE Name = 'ClearDelayedCommandsOnAutoStep';
DELETE FROM Configuration WHERE Name = 'TriCasterPort';
DELETE FROM Configuration WHERE Name = 'TriCasterProduct';
DELETE FROM Configuration WHERE Name = 'OpenRecent';
DELETE FROM Configuration WHERE Name = 'BottomWidgetAnchor';
