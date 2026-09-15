-- Indexes for the lookups the client makes most. Only LayoutPreset had one:
-- every setting read, every thumbnail lookup and every library query by device
-- scanned its whole table. IF NOT EXISTS because a database may meet this twice.
CREATE INDEX IF NOT EXISTS IX_Configuration_Name ON Configuration (Name);
CREATE INDEX IF NOT EXISTS IX_Library_DeviceId_Name ON Library (DeviceId, Name);
CREATE INDEX IF NOT EXISTS IX_Library_Name ON Library (Name);
