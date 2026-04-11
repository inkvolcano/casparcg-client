-- Add ServerPath column to Device table for server process management.
ALTER TABLE Device ADD COLUMN ServerPath TEXT DEFAULT '';
