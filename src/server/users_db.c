/*
Copyright (c) 2024, Lance Borden
All rights reserved.

This software is licensed under the BSD 3-Clause License.
You may obtain a copy of the license at:
https://opensource.org/licenses/BSD-3-Clause

Redistribution and use in source and binary forms, with or without
modification, are permitted under the conditions stated in the BSD 3-Clause
License.

THIS SOFTWARE IS PROVIDED "AS IS" WITHOUT ANY WARRANTIES,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
*/

#include "users_db.h"
#include <pthread.h>
#include <sqlite3.h>
#include <stdio.h>

static int create_user_table(sqlite3 *db) {
	const char *sql = "CREATE TABLE IF NOT EXISTS users ("
					  "id INTEGER PRIMARY KEY AUTOINCREMENT,"
					  "email TEXT NOT NULL UNIQUE,"
					  "username TEXT NOT NULL UNIQUE,"
					  "password TEXT NOT NULL);";
	char *err_msg = 0;

	int rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
	if (rc != SQLITE_OK) {
		printf("Error in creating user table: %s\n", err_msg);
		sqlite3_free(err_msg);
		return rc;
	}

	return SQLITE_OK;
}

// the user_t coming in should be empty to recieve data
static int create_user(sqlite3 *db, user_t *usr, const char *email,
					   const char *usrname, const char *psswd_hash) {
	sqlite3_stmt *stmt;
	const char *sql =
		"INSERT INTO users (email, username, password) VALUES (?, ?, ?);";

	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		printf("Error preparing add user statement: %s\n", sqlite3_errmsg(db));
		return rc;
	}

	sqlite3_bind_text(stmt, 1, email, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, usrname, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 3, psswd_hash, -1, SQLITE_STATIC);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		printf("Error execution of add user statement failed: %s\n",
			   sqlite3_errmsg(db));
		sqlite3_finalize(stmt);
		return rc;
	}

	sqlite3_finalize(stmt);
	return SQLITE_OK;
}

// the user_t should be empty to recieve data
static int read_user(sqlite3 *db, user_t *usr, const int id) {
	sqlite3_stmt *stmt;
	const char *sql =
		"SELECT id, email, username, password FROM users WHERE id = ?;";

	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		printf("Error preparing read user statement: %s\n", sqlite3_errmsg(db));
		return rc;
	}

	// sets id for query based on function parameter
	sqlite3_bind_int(stmt, 1, id);

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		usr->id = sqlite3_column_int(stmt, 0);
		usr->email = sqlite3_column_text(stmt, 1);
		usr->username = sqlite3_column_text(stmt, 2);
		usr->psswd_hash = sqlite3_column_text(stmt, 3);
	} else if (rc == SQLITE_DONE) {
		printf("No user found with id: %d\n", id);
	} else {
		printf("Error execution of read user statement failed: %s\n",
			   sqlite3_errmsg(db));
	}

	sqlite3_finalize(stmt);
	return rc;
}

// the usr coming in should have the data to be updated
static int update_user(sqlite3 *db, user_t *usr) {
	sqlite3_stmt *stmt;
	const char *sql =
		"UPDATE users SET email = ?, username = ?, password = ? WHERE id = ?;";

	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		printf("Error preparing update user statement: %s\n",
			   sqlite3_errmsg(db));
		return rc;
	}

	sqlite3_bind_text(stmt, 1, (const char *)usr->email, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 2, (const char *)usr->username, -1, SQLITE_STATIC);
	sqlite3_bind_text(stmt, 3, (const char *)usr->psswd_hash, -1,
					  SQLITE_STATIC);
	sqlite3_bind_int(stmt, 4, usr->id);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		printf("Error execution of update user statement failed: %s\n",
			   sqlite3_errmsg(db));
	}

	sqlite3_finalize(stmt);
	return rc;
}

static int delete_user(sqlite3 *db, const int id) {
	sqlite3_stmt *stmt;
	const char *sql = "DELETE FROM users WHERE id = ?;";

	int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		printf("Error preparing delete user statement: %s\n",
			   sqlite3_errmsg(db));
		return rc;
	}

	sqlite3_bind_int(stmt, 1, id);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		printf("Error execution of delete user statement failed: %s\n",
			   sqlite3_errmsg(db));
	}

	sqlite3_finalize(stmt);
	return rc;
}

static void *database_thread_main(void *arg) {
	sqlite3 *users_db;
	int exit = 0;
	exit = sqlite3_open("users.db", &users_db);

	if (exit) {
		printf("Error opening DB: %s\n", sqlite3_errmsg(users_db));
		return NULL;
	} else {
		printf("DB opened successfully.\n");
	}
	sqlite3_close(users_db);
	return (0);
}

void open_database_thread() {
	pthread_t id;
	pthread_create(&id, NULL, database_thread_main, NULL);
}
