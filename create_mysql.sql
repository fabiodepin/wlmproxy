-- vim:noet:sw=8
CREATE TABLE IF NOT EXISTS acls (
	id INT UNSIGNED NOT NULL AUTO_INCREMENT,
	localim VARCHAR(128) NOT NULL,
	remoteim VARCHAR(128) NOT NULL,
	action TINYINT UNSIGNED NOT NULL,
	PRIMARY KEY (id),
	UNIQUE KEY uk_acls (localim, remoteim)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS badwords (
	id INT UNSIGNED NOT NULL AUTO_INCREMENT,
	badword VARCHAR(128) NOT NULL,
	isregex TINYINT UNSIGNED NOT NULL DEFAULT 0,
	isenabled TINYINT UNSIGNED NOT NULL,
	PRIMARY KEY (id),
	UNIQUE KEY uk_badwords (badword)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS settings (
	id SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT,
	name VARCHAR(64) NOT NULL,
	value VARCHAR(255) NULL,
	PRIMARY KEY (id),
	UNIQUE KEY uk_settings (name)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT IGNORE INTO settings (name, value) VALUES ('allow_self_reg', '1');
INSERT IGNORE INTO settings (name, value) VALUES ('min_protocol_version', '8');
INSERT IGNORE INTO settings (name, value) VALUES ('max_protocol_version', '21');
INSERT IGNORE INTO settings (name, value) VALUES ('filtered_msg', 'Ouch...');
INSERT IGNORE INTO settings (name, value) VALUES ('default_warning', 'Big Brother is watching you');

CREATE TABLE IF NOT EXISTS usergroups (
	id INT UNSIGNED NOT NULL AUTO_INCREMENT,
	groupname VARCHAR(64) NOT NULL,
	isactive TINYINT UNSIGNED NOT NULL,
	isbuiltin TINYINT UNSIGNED NOT NULL,
	description VARCHAR(512) NULL,
	PRIMARY KEY (id),
	UNIQUE KEY uk_usergroups (groupname)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT IGNORE INTO usergroups (groupname, isactive, isbuiltin) VALUES ('guest', 1, 1);

CREATE TABLE IF NOT EXISTS rules (
	id SMALLINT UNSIGNED NOT NULL AUTO_INCREMENT,
	rulename VARCHAR(64) NOT NULL,
	rulevalue VARCHAR(255) NULL,
	description VARCHAR(512) NULL,
	PRIMARY KEY (id),
	UNIQUE KEY uk_rules (rulename)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

INSERT IGNORE INTO rules (id, rulename, description) VALUES (1, 'Conversation history', 'Save instant message conversations');
INSERT IGNORE INTO rules (id, rulename, description) VALUES (2, 'Disclaimer', 'Notify the user that the messages are being monitored');
INSERT IGNORE INTO rules (id, rulename, description) VALUES (3, 'Block file transfers', 'Automatically reject file transfers');
INSERT IGNORE INTO rules (id, rulename) VALUES (4, 'Block unofficial messages');
INSERT IGNORE INTO rules (id, rulename) VALUES (5, 'Block webcam');
INSERT IGNORE INTO rules (id, rulename) VALUES (6, 'Block Remote Assistance');
INSERT IGNORE INTO rules (id, rulename) VALUES (7, 'Block application sharing');
INSERT IGNORE INTO rules (id, rulename) VALUES (8, 'Block custom emoticons');
INSERT IGNORE INTO rules (id, rulename) VALUES (9, 'Block handwriting');
INSERT IGNORE INTO rules (id, rulename) VALUES (10, 'Block nudges');
INSERT IGNORE INTO rules (id, rulename) VALUES (11, 'Block winks');
INSERT IGNORE INTO rules (id, rulename) VALUES (12, 'Block voice clips');
INSERT IGNORE INTO rules (id, rulename) VALUES (13, 'Block encrypted messages');
INSERT IGNORE INTO rules (id, rulename, description) VALUES (14, 'Badword filtering', 'Filter messages based on defined badwords');
INSERT IGNORE INTO rules (id, rulename) VALUES (15, 'Block MSN Games');
INSERT IGNORE INTO rules (id, rulename) VALUES (16, 'Block photo sharing');

CREATE TABLE IF NOT EXISTS grouprules (
	id INT UNSIGNED NOT NULL AUTO_INCREMENT,
	rule_id SMALLINT UNSIGNED NOT NULL,
	group_id INT UNSIGNED NOT NULL,
	PRIMARY KEY (id),
	UNIQUE KEY uk_grouprules (rule_id, group_id),
	FOREIGN KEY (rule_id) REFERENCES rules(id),
	FOREIGN KEY (group_id) REFERENCES usergroups(id)
	  ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS users (
	id INT UNSIGNED NOT NULL AUTO_INCREMENT,
	group_id INT UNSIGNED NOT NULL DEFAULT 1,
	username VARCHAR(128) NOT NULL,
	displayname VARCHAR(130) NOT NULL DEFAULT '',
	psm VARCHAR(130) NOT NULL DEFAULT '',
	status ENUM('NLN', 'BSY', 'IDL', 'AWY', 'BRB', 'PHN', 'LUN', 'HDN', 'FLN') NOT NULL DEFAULT 'FLN',
	lastlogin DATETIME NULL,
	isenabled TINYINT UNSIGNED NOT NULL DEFAULT 1,
	PRIMARY KEY (id),
	UNIQUE KEY uk_users (username),
	FOREIGN KEY (group_id) REFERENCES usergroups(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS buddies (
	id INT UNSIGNED NOT NULL AUTO_INCREMENT,
	user_id INT UNSIGNED NOT NULL,
	username VARCHAR(128) NOT NULL,
	displayname VARCHAR(130) NOT NULL DEFAULT '',
	psm VARCHAR(130) NOT NULL DEFAULT '',
	status ENUM('NLN', 'BSY', 'IDL', 'AWY', 'BRB', 'PHN', 'LUN', 'HDN', 'FLN') NOT NULL DEFAULT 'FLN',
	isblocked TINYINT UNSIGNED NOT NULL DEFAULT 0,
	PRIMARY KEY (id),
	UNIQUE KEY uk_buddies (user_id, username),
	FOREIGN KEY (user_id) REFERENCES users(id)
	  ON DELETE CASCADE
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS conversations (
	id INT UNSIGNED NOT NULL AUTO_INCREMENT,
	user_id INT UNSIGNED NOT NULL,
	timestamp DATETIME NOT NULL,
	status TINYINT UNSIGNED NOT NULL DEFAULT 1,
	PRIMARY KEY (id),
	KEY ix_user_id (user_id),
	FOREIGN KEY (user_id) REFERENCES users(id)
) ENGINE=InnoDB DEFAULT CHARSET=utf8;

CREATE TABLE IF NOT EXISTS messages (
	id BIGINT UNSIGNED NOT NULL AUTO_INCREMENT,
	timestamp DATETIME NOT NULL,
	conversation_id INT UNSIGNED NOT NULL,
	clientip INT UNSIGNED NOT NULL,
	inbound TINYINT UNSIGNED NOT NULL,
	type TINYINT UNSIGNED NOT NULL,
	localim VARCHAR(128) NOT NULL,
	remoteim VARCHAR(128) NOT NULL,
	filtered TINYINT UNSIGNED NOT NULL,
	content VARCHAR(2000) NOT NULL,
	PRIMARY KEY (id),
	FULLTEXT KEY ft_messages (content)
) ENGINE=MyISAM DEFAULT CHARSET=utf8;

DROP FUNCTION IF EXISTS fn_check_version;
DELIMITER //

CREATE FUNCTION fn_check_version(ver_param INT) RETURNS INT
BEGIN
DECLARE min_version, max_version INT;
SELECT value INTO min_version FROM settings WHERE name='min_protocol_version';
SELECT value INTO max_version FROM settings WHERE name='max_protocol_version';

IF ver_param < min_version OR ver_param > max_version THEN
	RETURN 1;
END IF;

RETURN 0;
END //

DELIMITER ;

DROP PROCEDURE IF EXISTS sp_add_user;
DELIMITER //

CREATE PROCEDURE sp_add_user(IN p_username VARCHAR(128))
BEGIN
DECLARE tmp INT DEFAULT 0;

SELECT value INTO tmp FROM settings WHERE name='allow_self_reg';
IF tmp = 1 THEN
	INSERT IGNORE INTO users(username) VALUES (p_username);
END IF;

END //

DELIMITER ;
