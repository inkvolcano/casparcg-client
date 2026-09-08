-- A rundown row pointing at media that is not there looked exactly like one that
-- was fine. The Inspector hinted at it by blanking the target field, but only if
-- you opened the item. The rundown now marks those rows when it is loaded.
--
-- On by default: a warning that only appears when something is wrong costs
-- nothing when nothing is.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('WarnMissingMedia', 'true');
