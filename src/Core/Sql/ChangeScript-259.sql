-- The Preview panel goes back to legacy by default: the thumbnail the server
-- made, for stills and movies, and nothing opened. Rendering every selected
-- template in a web view and decoding every selected clip made the panel lag on
-- each click, and one clip with no audio track took a venue's client down
-- (build 222).
--
-- Reset once, so the new default also applies on a machine where the Settings
-- dialog had already written the old values. The newer preview and each of its
-- parts can be turned back on in Settings -> Preview.
UPDATE Configuration SET Value = 'true' WHERE Name = 'PreviewLegacyMode';
UPDATE Configuration SET Value = 'false' WHERE Name = 'PreviewTemplates';
UPDATE Configuration SET Value = 'false' WHERE Name = 'PreviewAudioMeters';
UPDATE Configuration SET Value = 'false' WHERE Name = 'PreviewAutoPlayVideo';
INSERT INTO Configuration (Name, Value) VALUES ('PreviewShowStills', 'false');
INSERT INTO Configuration (Name, Value) VALUES ('PreviewShowMovies', 'false');
