-- The Live panel's network cache (build 320): 300 ms instead of 1000 puts the
-- picture about 0.55 s behind the server's output instead of 1.2 s, at a full
-- frame rate (150 ms dropped frames). Measured against a 2.5 server. Only
-- where it was never changed.
UPDATE Configuration SET Value = '300' WHERE Name = 'NetworkCache' AND Value = '1000';
