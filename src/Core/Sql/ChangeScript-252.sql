-- OGraf support, as one switch.
--
-- OGraf is the EBU's open specification for HTML broadcast graphics. The client
-- can find graphics in a template folder, render them in the Preview panel and
-- build the Inspector's typed fields from a graphic's manifest.
--
-- Off by default, and off means off: with this unset the Library does no extra
-- walking of the template folder, and neither the Preview panel nor the Inspector
-- looks for a manifest. A client that does not use OGraf pays nothing for it.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('OgrafEnabled', 'false');
