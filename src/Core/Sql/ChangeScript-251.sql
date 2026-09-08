-- Check-ins for the GitHub route.
--
-- A venue pulling from a relay checked in to that relay. A venue pulling from a
-- private GitHub repository checked in nowhere, on purpose: its token there is
-- read-only, and writing a check-in file into the templates repository would
-- mean every venue holding a token that can push templates to every other venue.
--
-- Those are two questions, not one. Where templates come from and where this
-- machine reports only looked joined because a relay answers both. Set an
-- address here and a GitHub venue reports to a relay while still pulling its
-- templates from GitHub, with no token upgrade anywhere.
--
-- Empty means the old behaviour: report to the relay if there is one, otherwise
-- report nowhere.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('RelayCheckInUrl', '');

-- Falls back to the relay token when left empty.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('RelayCheckInToken', '');
