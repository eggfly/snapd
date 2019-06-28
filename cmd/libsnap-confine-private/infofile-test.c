/*
 * Copyright (C) 2019 Canonical Ltd
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "infofile.h"
#include "infofile.c"

#include <glib.h>
#include <unistd.h>

static void test_infofile_get_key(void) {
    int rc;
    sc_error *err;

    char text[] =
        "key=value\n"
        "other-key=other-value\n"
        "dup-key=value-one\n"
        "dup-key=value-two\n";
    FILE *stream = fmemopen(text, sizeof text - 1, "r");
    g_assert_nonnull(stream);

    char *value;

    /* Caller must provide the stream to scan. */
    rc = sc_infofile_get_key(NULL, "key", &value, &err);
    g_assert_cmpint(rc, ==, -1);
    g_assert_nonnull(err);
    g_assert_cmpstr(sc_error_domain(err), ==, SC_LIBSNAP_ERROR);
    g_assert_cmpint(sc_error_code(err), ==, SC_API_MISUSE);
    g_assert_cmpstr(sc_error_msg(err), ==, "stream cannot be NULL");
    sc_error_free(err);

    /* Caller must provide the key to look for. */
    rc = sc_infofile_get_key(stream, NULL, &value, &err);
    g_assert_cmpint(rc, ==, -1);
    g_assert_nonnull(err);
    g_assert_cmpstr(sc_error_domain(err), ==, SC_LIBSNAP_ERROR);
    g_assert_cmpint(sc_error_code(err), ==, SC_API_MISUSE);
    g_assert_cmpstr(sc_error_msg(err), ==, "key cannot be NULL");
    sc_error_free(err);

    /* Caller must provide storage for the value. */
    rc = sc_infofile_get_key(stream, "key", NULL, &err);
    g_assert_cmpint(rc, ==, -1);
    g_assert_nonnull(err);
    g_assert_cmpstr(sc_error_domain(err), ==, SC_LIBSNAP_ERROR);
    g_assert_cmpint(sc_error_code(err), ==, SC_API_MISUSE);
    g_assert_cmpstr(sc_error_msg(err), ==, "value cannot be NULL");
    sc_error_free(err);

    /* Keys that are not found get NULL values. */
    value = (void *)0xfefefefe;
    rewind(stream);
    rc = sc_infofile_get_key(stream, "missing-key", &value, &err);
    g_assert_cmpint(rc, ==, 0);
    g_assert_null(err);
    g_assert_null(value);

    /* Keys that are found get strdup-duplicated values. */
    value = NULL;
    rewind(stream);
    rc = sc_infofile_get_key(stream, "key", &value, &err);
    g_assert_cmpint(rc, ==, 0);
    g_assert_null(err);
    g_assert_nonnull(value);
    g_assert_cmpstr(value, ==, "value");
    free(value);

    /* When duplicate keys are present the first value is extracted. */
    char *dup_value;
    rewind(stream);
    rc = sc_infofile_get_key(stream, "dup-key", &dup_value, &err);
    g_assert_cmpint(rc, ==, 0);
    g_assert_null(err);
    g_assert_nonnull(dup_value);
    g_assert_cmpstr(dup_value, ==, "value-one");
    free(dup_value);

    fclose(stream);

    /* Key without a value. */
    char *malformed_value;
    char malformed1[] = "key\n";
    stream = fmemopen(malformed1, sizeof malformed1 - 1, "r");
    g_assert_nonnull(stream);
    rc = sc_infofile_get_key(stream, "key", &malformed_value, &err);
    g_assert_cmpint(rc, ==, -1);
    g_assert_nonnull(err);
    g_assert_cmpstr(sc_error_domain(err), ==, SC_LIBSNAP_ERROR);
    g_assert_cmpint(sc_error_code(err), ==, 0);
    g_assert_cmpstr(sc_error_msg(err), ==, "line 1 is not a key=value assignment");
    g_assert_null(malformed_value);
    sc_error_free(err);
    fclose(stream);

    /* Key-value pair with embedded NUL byte. */
    char malformed2[] = "key=value\0garbage\n";
    stream = fmemopen(malformed2, sizeof malformed2 - 1, "r");
    g_assert_nonnull(stream);
    rc = sc_infofile_get_key(stream, "key", &malformed_value, &err);
    g_assert_cmpint(rc, ==, -1);
    g_assert_nonnull(err);
    g_assert_cmpstr(sc_error_domain(err), ==, SC_LIBSNAP_ERROR);
    g_assert_cmpint(sc_error_code(err), ==, 0);
    g_assert_cmpstr(sc_error_msg(err), ==, "line 1 contains NUL byte");
    g_assert_null(malformed_value);
    sc_error_free(err);
    fclose(stream);

    /* Key with empty value (which is valid). */
    char malformed3[] = "key=";
    stream = fmemopen(malformed3, sizeof malformed3 - 1, "r");
    g_assert_nonnull(stream);
    rc = sc_infofile_get_key(stream, "key", &malformed_value, &err);
    g_assert_cmpint(rc, ==, 0);
    g_assert_null(err);
    g_assert_cmpstr(malformed_value, ==, "");
    sc_error_free(err);
    fclose(stream);
    free(malformed_value);

    /* Key with empty value with a trailing newline (which is also valid). */
    char malformed4[] = "key=\n";
    stream = fmemopen(malformed4, sizeof malformed4 - 1, "r");
    g_assert_nonnull(stream);
    rc = sc_infofile_get_key(stream, "key", &malformed_value, &err);
    g_assert_cmpint(rc, ==, 0);
    g_assert_null(err);
    g_assert_cmpstr(malformed_value, ==, "");
    sc_error_free(err);
    fclose(stream);
    free(malformed_value);

    /* The equals character alone (key is empty) */
    char malformed5[] = "=";
    stream = fmemopen(malformed5, sizeof malformed5 - 1, "r");
    g_assert_nonnull(stream);
    rc = sc_infofile_get_key(stream, "key", &malformed_value, &err);
    g_assert_cmpint(rc, ==, -1);
    g_assert_nonnull(err);
    g_assert_cmpstr(sc_error_domain(err), ==, SC_LIBSNAP_ERROR);
    g_assert_cmpint(sc_error_code(err), ==, 0);
    g_assert_cmpstr(sc_error_msg(err), ==, "line 1 contains empty key");
    g_assert_null(malformed_value);
    sc_error_free(err);
    fclose(stream);
}

static void __attribute__((constructor)) init(void) { g_test_add_func("/infofile/get_key", test_infofile_get_key); }
