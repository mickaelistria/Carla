/*
  Copyright 2007-2011 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lv2/event.h"

#include "lilv_internal.h"

LilvPort*
lilv_port_new(LilvWorld*      world,
              const SordNode* node,
              uint32_t        index,
              const char*     symbol)
{
	LilvPort* port = (LilvPort*)malloc(sizeof(LilvPort));
	port->node    = sord_node_copy(node);
	port->index   = index;
	port->symbol  = lilv_node_new(world, LILV_VALUE_STRING, symbol);
	port->classes = lilv_nodes_new();
	return port;
}

void
lilv_port_free(const LilvPlugin* plugin, LilvPort* port)
{
	if (port) {
		sord_node_free(plugin->world->world, port->node);
		lilv_nodes_free(port->classes);
		lilv_node_free(port->symbol);
		free(port);
	}
}

LILV_API
bool
lilv_port_is_a(const LilvPlugin* plugin,
               const LilvPort*   port,
               const LilvNode*   port_class)
{
	LILV_FOREACH(nodes, i, port->classes)
		if (lilv_node_equals(lilv_nodes_get(port->classes, i), port_class))
			return true;

	return false;
}

LILV_API
bool
lilv_port_has_property(const LilvPlugin* p,
                       const LilvPort*   port,
                       const LilvNode*   property)
{
	assert(property);
	SordIter* results = lilv_world_query_internal(
		p->world,
		port->node,
		p->world->uris.lv2_portProperty,
		lilv_node_as_node(property));

	const bool ret = !sord_iter_end(results);
	sord_iter_free(results);
	return ret;
}

LILV_API
bool
lilv_port_supports_event(const LilvPlugin* p,
                         const LilvPort*   port,
                         const LilvNode*   event)
{
	assert(event);
	SordIter* results = lilv_world_query_internal(
		p->world,
		port->node,
		sord_new_uri(p->world->world, (const uint8_t*)LV2_EVENT__supportsEvent),
		lilv_node_as_node(event));

	const bool ret = !sord_iter_end(results);
	sord_iter_free(results);
	return ret;
}

static LilvNodes*
lilv_port_get_value_by_node(const LilvPlugin* p,
                            const LilvPort*   port,
                            const SordNode*   predicate)
{
	assert(sord_node_get_type(predicate) == SORD_URI);

	SordIter* results = lilv_world_query_internal(
		p->world,
		port->node,
		predicate,
		NULL);

	return lilv_nodes_from_stream_objects(p->world, results, SORD_OBJECT);
}

LILV_API
LilvNodes*
lilv_port_get_value(const LilvPlugin* p,
                    const LilvPort*   port,
                    const LilvNode*   predicate)
{
	if (!lilv_node_is_uri(predicate)) {
		LILV_ERRORF("Predicate `%s' is not a URI\n",
		            sord_node_get_string(predicate->node));
		return NULL;
	}

	return lilv_port_get_value_by_node(
		p, port,
		lilv_node_as_node(predicate));
}

LILV_API
uint32_t
lilv_port_get_index(const LilvPlugin* p,
                    const LilvPort*   port)
{
	return port->index;
}

LILV_API
const LilvNode*
lilv_port_get_symbol(const LilvPlugin* p,
                     const LilvPort*   port)
{
	return port->symbol;
}

LILV_API
LilvNode*
lilv_port_get_name(const LilvPlugin* p,
                   const LilvPort*   port)
{
	LilvNodes* results = lilv_port_get_value_by_node(
		p, port, p->world->uris.lv2_name);

	LilvNode* ret = NULL;
	if (results) {
		LilvNode* val = lilv_nodes_get_first(results);
		if (lilv_node_is_string(val))
			ret = lilv_node_duplicate(val);
		lilv_nodes_free(results);
	}

	if (!ret)
		LILV_WARNF("Plugin <%s> port has no (mandatory) doap:name\n",
		           lilv_node_as_string(lilv_plugin_get_uri(p)));

	return ret;
}

LILV_API
const LilvNodes*
lilv_port_get_classes(const LilvPlugin* p,
                      const LilvPort*   port)
{
	return port->classes;
}

LILV_API
void
lilv_port_get_range(const LilvPlugin* p,
                    const LilvPort*   port,
                    LilvNode**        def,
                    LilvNode**        min,
                    LilvNode**        max)
{
	if (def) {
		LilvNodes* defaults = lilv_port_get_value_by_node(
			p, port, p->world->uris.lv2_default);
		*def = defaults
			? lilv_node_duplicate(lilv_nodes_get_first(defaults))
			: NULL;
		lilv_nodes_free(defaults);
	}
	if (min) {
		LilvNodes* minimums = lilv_port_get_value_by_node(
			p, port, p->world->uris.lv2_minimum);
		*min = minimums
			? lilv_node_duplicate(lilv_nodes_get_first(minimums))
			: NULL;
		lilv_nodes_free(minimums);
	}
	if (max) {
		LilvNodes* maximums = lilv_port_get_value_by_node(
			p, port, p->world->uris.lv2_maximum);
		*max = maximums
			? lilv_node_duplicate(lilv_nodes_get_first(maximums))
			: NULL;
		lilv_nodes_free(maximums);
	}
}

LILV_API
LilvScalePoints*
lilv_port_get_scale_points(const LilvPlugin* p,
                           const LilvPort*   port)
{
	SordIter* points = lilv_world_query_internal(
		p->world,
		port->node,
		sord_new_uri(p->world->world, (const uint8_t*)LV2_CORE__scalePoint),
		NULL);

	LilvScalePoints* ret = NULL;
	if (!sord_iter_end(points))
		ret = lilv_scale_points_new();

	FOREACH_MATCH(points) {
		const SordNode* point = sord_iter_get_node(points, SORD_OBJECT);

		LilvNode* value = lilv_plugin_get_unique(
			p,
			point,
			p->world->uris.rdf_value);

		LilvNode* label = lilv_plugin_get_unique(
			p,
			point,
			p->world->uris.rdfs_label);

		if (value && label) {
			zix_tree_insert(
				(ZixTree*)ret, lilv_scale_point_new(value, label), NULL);
		}
	}
	sord_iter_free(points);

	assert(!ret || lilv_nodes_size(ret) > 0);
	return ret;
}

LILV_API
LilvNodes*
lilv_port_get_properties(const LilvPlugin* p,
                         const LilvPort*   port)
{
	LilvNode* pred = lilv_node_new_from_node(
		p->world, p->world->uris.lv2_portProperty);
	LilvNodes* ret = lilv_port_get_value(p, port, pred);
	lilv_node_free(pred);
	return ret;
}
