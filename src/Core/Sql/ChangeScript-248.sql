-- The Preview panel used to show a database thumbnail for stills and play the
-- local file for movies, and nothing at all for anything else. It now reads the
-- real file off disk, renders templates, and meters the audio it is playing.
-- None of that needs a CasparCG server to be running.

-- Puts the panel back exactly as it was, for anyone who wants it that way.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PreviewLegacyMode', 'false');

-- Whether selecting a movie starts it playing. Off, because selecting an item in
-- a rundown during a show should not start making noise on its own.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PreviewAutoPlayVideo', 'false');

-- Meters drawn over the picture. The levels are decoded from the file rather
-- than tapped from the player, so they follow scrubbing and hold while paused.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PreviewAudioMeters', 'true');

-- Rendering HTML templates in the panel. Needs a build with Qt WebEngine; when
-- the build has none, the panel says so and this setting does nothing.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('PreviewTemplates', 'true');
