/* MAX_NUMBER is the next number to be used, always one more than the highest message number. */
set bulk_insert INSERT INTO FACILITIES (LAST_CHANGE, FACILITY, FAC_CODE, MAX_NUMBER) VALUES (?, ?, ?, ?);
--
('2008-08-08 13:01:57', 'JRD', 0, 617)
('2005-09-02 00:55:59', 'QLI', 1, 513)
('1996-11-07 13:38:37', 'GDEF', 2, 345)
('2007-04-07 13:11:00', 'GFIX', 3, 116)
('1996-11-07 13:39:40', 'GPRE', 4, 1)
--
--('1996-11-07 13:39:40', 'GLTJ', 5, 1)
--('1996-11-07 13:39:40', 'GRST', 6, 1)
--
('2005-11-05 13:09:00', 'DSQL', 7, 32)
('2008-06-03 09:22:00', 'DYN', 8, 255)
--
--('1996-11-07 13:39:40', 'FRED', 9, 1)
--
('1996-11-07 13:39:40', 'INSTALL', 10, 1)
('1996-11-07 13:38:41', 'TEST', 11, 4)
('2008-04-10 23:40:00', 'GBAK', 12, 306)
('2008-03-17 12:05:00', 'SQLERR', 13, 952)
('1996-11-07 13:38:42', 'SQLWARN', 14, 613)
('2006-09-10 03:04:31', 'JRD_BUGCHK', 15, 307)
--
--('1996-11-07 13:38:43', 'GJRN', 16, 241)
--
('2008-03-12 08:02:00', 'ISQL', 17, 161)
('2007-12-27 13:11:00', 'GSEC', 18, 94)
('2002-03-05 02:30:12', 'LICENSE', 19, 60)
('2002-03-05 02:31:54', 'DOS', 20, 74)
('2007-04-07 13:11:00', 'GSTAT', 21, 37)
('2007-05-02 13:15:27', 'FBSVCMGR', 22, 47)
('2007-12-21 19:03:07', 'UTL', 23, 2)
stop

COMMIT WORK;

