/*
 * GeeXboX Valhalla: tiny media scanner API.
 * Copyright (C) 2009 Benjamin Zores <ben@geexbox.org>
 *
 * This file is part of libvalhalla.
 *
 * libvalhalla is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * libvalhalla is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with libvalhalla; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "grabber_common.h"
#include "grabber_tvrage.h"
#include "metadata.h"
#include "xml_utils.h"
#include "url_utils.h"
#include "grabber_utils.h"
#include "utils.h"
#include "logs.h"

#define GRABBER_CAP_FLAGS \
  GRABBER_CAP_VIDEO

/*
 * The documentation is available on:
 *  http://services.tvrage.com/index.php?page=public
 */

#define TVRAGE_HOSTNAME           "services.tvrage.com"

#define TVRAGE_QUERY_SEARCH       "http://%s/feeds/search.php?show=%s"
#define TVRAGE_QUERY_INFO         "http://%s/feeds/full_show_info.php?sid=%s"

typedef struct grabber_tvrage_s {
  url_t *handler;
  const metadata_plist_t *pl;
} grabber_tvrage_t;

static const metadata_plist_t tvrage_pl[] = {
  { NULL,                             VALHALLA_METADATA_PL_NORMAL   }
};


static int
grabber_tvrage_get (grabber_tvrage_t *tvrage, file_data_t *fdata,
                    const char *keywords, char *escaped_keywords)
{
  char url[MAX_URL_SIZE];
  url_data_t udata;
  int i;

  xmlDocPtr doc;
  xmlChar *tmp = NULL;
  xmlNode *n, *node;

  if (!keywords || !escaped_keywords)
    return -1;

  /* proceed with TVRage search request */
  snprintf (url, sizeof (url), TVRAGE_QUERY_SEARCH,
            TVRAGE_HOSTNAME, escaped_keywords);

  vh_log (VALHALLA_MSG_VERBOSE, "Search Request: %s", url);

  udata = vh_url_get_data (tvrage->handler, url);
  if (udata.status != 0)
    return -1;

  vh_log (VALHALLA_MSG_VERBOSE, "Search Reply: %s", udata.buffer);

  /* parse the XML answer */
  doc = vh_xml_get_doc_from_memory (udata.buffer);
  free (udata.buffer);

  if (!doc)
    return -1;

  /* check for a known DB entry */
  n = vh_xml_get_node_tree (xmlDocGetRootElement (doc), "show");
  if (!n)
  {
    vh_log (VALHALLA_MSG_VERBOSE,
            "Unable to find the item \"%s\"", escaped_keywords);
    goto error;
  }

  /* get TVRage show id */
  tmp = vh_xml_get_prop_value_from_tree (n, "showid");
  if (!tmp)
    goto error;

  xmlFreeDoc (doc);
  doc = NULL;

  /* proceed with TVRage search request */
  snprintf (url, sizeof (url),
            TVRAGE_QUERY_INFO, TVRAGE_HOSTNAME, tmp);
  xmlFree (tmp);

  vh_log (VALHALLA_MSG_VERBOSE, "Info Request: %s", url);

  udata = vh_url_get_data (tvrage->handler, url);
  if (udata.status != 0)
    goto error;

  vh_log (VALHALLA_MSG_VERBOSE, "Info Reply: %s", udata.buffer);

  /* parse the XML answer */
  doc = vh_xml_get_doc_from_memory (udata.buffer);
  free (udata.buffer);
  if (!doc)
    goto error;

  n = xmlDocGetRootElement (doc);

  /* fetch tv show french title (to be extended to language param) */
  tmp = vh_xml_get_prop_value_from_tree_by_attr (n, "aka", "country", "FR");
  if (tmp)
  {
    vh_metadata_add_auto (&fdata->meta_grabber,
                          VALHALLA_METADATA_TITLE_ALTERNATIVE,
                          (char *) tmp, VALHALLA_LANG_FR, tvrage->pl);
    xmlFree (tmp);
  }

  /* fetch tv show country */
  vh_grabber_parse_str (fdata, n, "origin_country", VALHALLA_METADATA_COUNTRY,
                        VALHALLA_LANG_EN, tvrage->pl);

  /* fetch tv show studio */
  vh_grabber_parse_str (fdata, n, "network", VALHALLA_METADATA_STUDIO,
                        VALHALLA_LANG_UNDEF, tvrage->pl);

  /* fetch tv show runtime (in minutes) */
  vh_grabber_parse_str (fdata, n, "runtime", VALHALLA_METADATA_RUNTIME,
                        VALHALLA_LANG_UNDEF, tvrage->pl);

  /* fetch movie categories */
  node = vh_xml_get_node_tree (n, "genre");
  for (i = 0; i < 5; i++)
  {
    if (!node)
      break;

    tmp = vh_xml_get_prop_value_from_tree (node, "genre");
    if (tmp)
    {
      vh_metadata_add_auto (&fdata->meta_grabber,
                            VALHALLA_METADATA_CATEGORY,
                            (char *) tmp, VALHALLA_LANG_EN, tvrage->pl);
      xmlFree (tmp);
    }
    node = node->next;
  }

  xmlFreeDoc (doc);
  return 0;

 error:
  if (doc)
    xmlFreeDoc (doc);

  return -1;
}

/****************************************************************************/
/* Private Grabber API                                                      */
/****************************************************************************/

static void *
grabber_tvrage_priv (void)
{
  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  return calloc (1, sizeof (grabber_tvrage_t));
}

static int
grabber_tvrage_init (void *priv, const grabber_param_t *param)
{
  grabber_tvrage_t *tvrage = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!tvrage)
    return -1;

  tvrage->handler = vh_url_new (param->url_ctl);
  tvrage->pl      = param->pl;
  return tvrage->handler ? 0 : -1;
}

static void
grabber_tvrage_uninit (void *priv)
{
  grabber_tvrage_t *tvrage = priv;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  if (!tvrage)
    return;

  vh_url_free (tvrage->handler);
  free (tvrage);
}

static int
grabber_tvrage_grab (void *priv, file_data_t *data)
{
  grabber_tvrage_t *tvrage = priv;
  const metadata_t *tag = NULL;
  char *keywords;
  int err;

  vh_log (VALHALLA_MSG_VERBOSE, __FUNCTION__);

  err = vh_metadata_get (data->meta_parser, "title", 0, &tag);
  if (err)
    return -1;

  /* Format the keywords */
  keywords = vh_url_escape_string (tvrage->handler, tag->value);
  if (!keywords)
    return -2;

  err = grabber_tvrage_get (tvrage, data, tag->value, keywords);
  free (keywords);

  return err;
}

/****************************************************************************/
/* Public Grabber API                                                       */
/****************************************************************************/

/* vh_grabber_tvrage_register () */
GRABBER_REGISTER (tvrage,
                  GRABBER_CAP_FLAGS,
                  tvrage_pl,
                  0,
                  grabber_tvrage_priv,
                  grabber_tvrage_init,
                  grabber_tvrage_uninit,
                  grabber_tvrage_grab,
                  NULL)
