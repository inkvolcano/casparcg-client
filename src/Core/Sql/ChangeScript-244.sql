-- Where the GitHub API lives. Empty means github.com. A GitHub Enterprise
-- Server has its own, usually https://github.example.com/api/v3, and without
-- this a client on one could not reach its own repositories at all.
INSERT OR IGNORE INTO Configuration (Name, Value) VALUES('RelayGitHubApi', '');
