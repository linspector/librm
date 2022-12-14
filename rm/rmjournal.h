/*
 * The rm project
 * Copyright (c) 2012-2017 Jan-Michael Brummer
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef __RM_JOURNAL_H__
#define __RM_JOURNAL_H__

#if !defined (__RM_H_INSIDE__) && !defined(RM_COMPILATION)
#error "Only <rm/rm.h> can be included directly."
#endif

#include <rm/rmcallentry.h>

G_BEGIN_DECLS

GList *rm_journal_add_call_entry(GList *journal, RmCallEntry *call);
gboolean rm_journal_save_as(GList *journal, gchar *file_name);
gboolean rm_journal_save(GList *journal);
GList *rm_journal_load(GList *journal);
gint rm_journal_sort_by_date(gconstpointer a, gconstpointer b);
GList *rm_journal_dup(GList *journal);
void rm_journal_free(GList *journal);

G_END_DECLS

#endif
