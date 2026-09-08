-- The size and the date the server has been sending all along.
--
-- CLS answers with name, type, size, timestamp, length and rate on every line.
-- The client parsed the last two into a timecode and threw the size and the
-- timestamp away, so the Library could sort by name and by nothing else - while
-- the two fields that answer "where is the clip that just landed" arrived on
-- every refresh and were dropped on the floor.
--
-- Both default to nothing rather than to zero. A file of unknown size is not an
-- empty file, and the sort has to be able to tell the difference: unknown rows
-- sort last, or a library of stills with no size would fill the top of a
-- size-sorted list and look like a fault.
--
-- Existing rows have neither until the next refresh, which is a connect away.
ALTER TABLE Library ADD COLUMN Size INTEGER DEFAULT -1;
ALTER TABLE Library ADD COLUMN Timestamp TEXT;
