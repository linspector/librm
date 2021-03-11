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

#include <string.h>
#include <strings.h>

#include <glib.h>

#include <rm/rmcontact.h>
#include <rm/rmobjectemit.h>
#include <rm/rmrouter.h>

G_DEFINE_TYPE(RmContact, rm_contact, G_TYPE_OBJECT);

/**
 * SECTION:rmcontact
 * @title: RmContact
 * @short_description: Contact handling functions
 *
 * Contacts represents entries within an address book.
 */

/**
 * rm_contact_copy_numbers:
 * @src: a @RmPhoneNUmber
 * @data: NULL
 *
 * Copy phone numbers to copy
 *
 * Returns: new #RmPhoneNumber
 */
gpointer rm_contact_copy_numbers(gconstpointer src, gpointer data)
{
	RmPhoneNumber *copy = NULL;
	RmPhoneNumber *src_number = (RmPhoneNumber*)src;

	copy = g_slice_new0(RmPhoneNumber);

	copy->type = src_number->type;
	copy->number = g_strdup(src_number->number);

	return copy;
}

/**
 * rm_contact_copy_addresses:
 * @src: a @RmContactAddress
 * @data: NULL
 *
 * Copy addresses to copy
 *
 * Returns: new #RmContactAddress
 */
gpointer rm_contact_copy_addresses(gconstpointer src, gpointer data)
{
	RmContactAddress *copy = NULL;
	RmContactAddress *src_address = (RmContactAddress*)src;

	copy = g_slice_new0(RmContactAddress);

	copy->type = src_address->type;
	copy->street = g_strdup(src_address->street);
	copy->zip = g_strdup(src_address->zip);
	copy->city = g_strdup(src_address->city);
	copy->lookup = src_address->lookup;

	return copy;
}

/**
 * rm_contact_free_address:
 * @data: pointer to #RmContactAddress
 *
 * Frees #RmContactAddress.
 */
static void rm_contact_free_address(gpointer data)
{
	RmContactAddress *address = data;

	if (!address) {
		g_debug("%s(): No address at all, exit", __FUNCTION__);
		return;
	}

	g_free(address->street);
	address->street = NULL;
	g_free(address->zip);
	address->zip = NULL;
	g_free(address->city);
	address->city = NULL;

	g_slice_free(RmContactAddress, address);
}

/**
 * rm_contact_free_number:
 * @data: pointer to #RmPhoneNumber
 *
 * Frees #RmPhoneNumber.
 */
static void rm_contact_free_number(gpointer data)
{
	RmPhoneNumber *number = data;

	g_free(number->number);
	number->number = NULL;

	g_slice_free(RmPhoneNumber, number);
}

/**
 * rm_contact_copy:
 * @src: source #RmContact
 * @dst: destination #RmContact
 *
 * Copies one contact data to another
 */
void rm_contact_copy(RmContact *src, RmContact *dst)
{
	g_clear_pointer (&dst->name, g_free);
	g_clear_object (&dst->image);
	g_clear_pointer (&dst->company, g_free);

	if (dst->addresses) {
		g_list_free_full(g_steal_pointer (&dst->addresses), rm_contact_free_address);
	}
	if (dst->numbers) {
		g_list_free_full(g_steal_pointer (&dst->numbers), rm_contact_free_number);
	}

	g_clear_pointer (&dst->street, g_free);
	g_clear_pointer (&dst->zip, g_free);
	g_clear_pointer (&dst->city, g_free);

	dst->name = g_strdup(src->name);

	if (src->image) {
		dst->image = gdk_pixbuf_copy(src->image);
	} else {
		dst->image = NULL;
	}

	if (src->numbers) {
		dst->numbers = g_list_copy_deep(src->numbers, rm_contact_copy_numbers, NULL);
	} else {
		dst->numbers = NULL;
	}

	if (src->addresses) {
		dst->addresses = g_list_copy_deep(src->addresses, rm_contact_copy_addresses, NULL);
	} else {
		dst->addresses = NULL;
	}

	dst->number = g_strdup(src->number);
	dst->company = g_strdup(src->company);
	dst->street = g_strdup(src->street);
	dst->zip = g_strdup(src->zip);
	dst->city = g_strdup(src->city);

	dst->priv = src->priv;
}

/**
 * rm_contact_dup:
 * @src: source #RmContact
 *
 * Duplicates a #RmContact
 *
 * Returns: new #RmContact
 */
RmContact *rm_contact_dup(RmContact *src)
{
	RmContact *dst = g_slice_new0(RmContact);

	rm_contact_copy(src, dst);

	return dst;
}

/**
 * rm_contact_name_compare:
 * @a: pointer to first #RmContact
 * @b: pointer to second #RmContact
 *
 * Compares two contacts based on contacts name.
 *
 * Returns: return values of #strcasecmp
 */
gint rm_contact_name_compare(gconstpointer a, gconstpointer b)
{
	RmContact *contact_a = (RmContact*)a;
	RmContact *contact_b = (RmContact*)b;

	return strcasecmp(contact_a->name, contact_b->name);
}

/**
 * rm_contact_find_by_number:
 * @number: phone number
 *
 * Try to find a contact by it's number
 *
 * Returns: a #RmContact if number has been found, or %NULL% if not.
 */
RmContact *rm_contact_find_by_number(gchar *number)
{
	RmContact *contact = g_slice_new0(RmContact);
	GList *numbers;
	GList *addresses;
	gint type = -1;

	/** Ask for contact information */
	contact->number = number;
	rm_object_emit_contact_process(contact);

	/* Depending on the number set the active address */
	for (numbers = contact->numbers; numbers != NULL; numbers = numbers->next) {
		RmPhoneNumber *phone_number = numbers->data;

		if (!strcmp(number, phone_number->number)) {
			type = phone_number->type;
			break;
		}
	}

	g_debug("%s(): phone number type %d", __FUNCTION__, type);
	if (type == -1) {
		return contact;
	}

	switch (type) {
	case RM_PHONE_NUMBER_TYPE_WORK:
	case RM_PHONE_NUMBER_TYPE_FAX_WORK:
	case RM_PHONE_NUMBER_TYPE_PAGER:
		type = 1;
		break;
	case RM_PHONE_NUMBER_TYPE_HOME:
	case RM_PHONE_NUMBER_TYPE_FAX_HOME:
	case RM_PHONE_NUMBER_TYPE_MOBILE:
	default:
		type = 0;
		break;
	}

	for (addresses = contact->addresses; addresses != NULL; addresses = addresses->next) {
		RmContactAddress *contact_address = addresses->data;

		if (contact_address->type == type) {
			contact->street = g_strdup(contact_address->street);
			contact->city = g_strdup(contact_address->city);
			contact->zip = g_strdup(contact_address->zip);
			break;
		}
	}

	return contact;
}

/**
 * rm_contact_free:
 * @contact: a #RmContact
 *
 * Frees a #RmContact.
 */
void rm_contact_free(RmContact *contact)
{
	if (!contact)
		return;

	g_clear_pointer(&contact->name, g_free);
	g_clear_pointer(&contact->company, g_free);
	g_clear_pointer(&contact->number, g_free);
	g_clear_pointer(&contact->street, g_free);
	g_clear_pointer(&contact->zip, g_free);
	g_clear_pointer(&contact->city, g_free);
	if (contact->image) {
		g_object_unref(contact->image);
	}

	if (contact->addresses) {
		g_list_free_full(contact->addresses, rm_contact_free_address);
	}
	if (contact->numbers) {
		g_list_free_full(contact->numbers, rm_contact_free_number);
	}
}

/**
 * rm_contact_set_image_from_file:
 * @contact: a #RmContact
 * @file: image file name
 *
 * Replaces current contact image with the image file.
 */
void rm_contact_set_image_from_file(RmContact *contact, gchar *file)
{
	if (!contact) {
		return;
	}

	if (contact->image) {
		g_object_unref(contact->image);
		contact->image = NULL;
	}

	if (file != NULL) {
		contact->image = gdk_pixbuf_new_from_file(file, NULL);
	}
}

static void rm_contact_class_init(RmContactClass *klass)
{
}

static void rm_contact_init(RmContact *widget)
{
}

GObject *rm_contact_new(void)
{
        return g_object_new(RM_TYPE_CONTACT, NULL);
}

