-- Named layouts. A layout is a couple of dozen configuration rows — the columns,
-- what sits in each, and every panel's size mode, height, span and anchor — so
-- rearranging for a different job and back again was a long manual job.
--
-- Scope keeps the two arrangements apart: 'panel' for the normal one, 'simple'
-- for the one Simple Mode uses. Data is a JSON object of the configuration keys
-- that scope owns, so applying a preset touches nothing else.
CREATE TABLE LayoutPreset (
    Id INTEGER PRIMARY KEY AUTOINCREMENT,
    Name TEXT NOT NULL,
    Scope TEXT NOT NULL,
    Data TEXT NOT NULL
);

-- One name per scope. The same name may exist once as a normal layout and once
-- as a Simple Mode layout, because they are never shown in the same list.
CREATE UNIQUE INDEX IX_LayoutPreset_Scope_Name ON LayoutPreset (Scope, Name COLLATE NOCASE);
