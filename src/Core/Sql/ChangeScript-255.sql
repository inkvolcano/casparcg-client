-- Peak hold and the clip indicator on the Audio Levels meters.
--
-- The meters drew whatever number arrived last, so a single frame at -2 dB
-- between two frames at -20 was drawn once and gone, and a clip left no trace at
-- all - the one event a meter exists to report was the one it forgot fastest.
--
-- Both default on, because a meter that hides a clip is not a preference, it is
-- a fault. They are settings rather than hardcoded because the panel is small
-- and somebody metering eight channels in a narrow column may want the plain
-- bars back.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('MeterPeakHold', 'true');
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('MeterClipIndicator', 'true');
