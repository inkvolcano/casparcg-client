-- Where builds of the client itself are published.
--
-- Templates got a distribution route years before the thing that plays them did.
-- A venue runs whatever was copied onto it, the title bar says "build 206" to
-- nobody in particular, and finding out what an estate is running has meant
-- visiting every machine.
--
-- Help -> Check for Updates asks a repository's releases whether anything newer
-- exists, and can download and verify it. Both empty by default, and nothing is
-- checked, downloaded or installed without somebody pressing something: on a
-- machine that may be on air, the client is not something to have quietly
-- replace itself between shows.
--
-- The token is only needed for a private repository. A public one - the ordinary
-- case for builds - needs nothing.
INSERT INTO Configuration (Name, Value) VALUES ('UpdateSource', '');
INSERT INTO Configuration (Name, Value) VALUES ('UpdateToken', '');
