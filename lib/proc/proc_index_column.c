/* -*- c-basic-offset: 2 -*- */
/*
  Copyright(C) 2019 Kouhei Sutou <kou@clear-code.com>

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License version 2.1 as published by the Free Software Foundation.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "../grn_proc.h"

#include "../grn_ctx.h"
#include "../grn_db.h"
#include "../grn_str.h"

#include <groonga/plugin.h>

static const char *remains_column_name = "remains";
static const char *missings_column_name = "missings";

static void
index_column_diff_output_postings(grn_ctx *ctx,
                                  grn_column_flags index_column_flags,
                                  grn_obj *postings,
                                  const char *name)
{
  size_t i;
  size_t n_elements = 1;
  if (index_column_flags & GRN_OBJ_WITH_SECTION) {
    n_elements++;
  }
  if (index_column_flags & GRN_OBJ_WITH_POSITION) {
    n_elements++;
  }
  size_t n = GRN_UINT32_VECTOR_SIZE(postings);
  grn_ctx_output_array_open(ctx, name, n / n_elements);
  for (i = 0; i < n; i += n_elements) {
    grn_ctx_output_map_open(ctx, "posting", n_elements);
    {
      size_t j = i;
      grn_ctx_output_cstr(ctx, "record_id");
      grn_id record_id = GRN_UINT32_VALUE_AT(postings, j);
      grn_ctx_output_uint32(ctx, record_id);
      if (index_column_flags & GRN_OBJ_WITH_SECTION) {
        j++;
        grn_ctx_output_cstr(ctx, "section_id");
        grn_id section_id = GRN_UINT32_VALUE_AT(postings, j);
        grn_ctx_output_uint32(ctx, section_id);
      }
      if (index_column_flags & GRN_OBJ_WITH_POSITION) {
        j++;
        grn_ctx_output_cstr(ctx, "position");
        uint32_t position = GRN_UINT32_VALUE_AT(postings, j);
        grn_ctx_output_uint32(ctx, position);
      }
    }
    grn_ctx_output_map_close(ctx);
  }
  grn_ctx_output_array_close(ctx);
}

static void
index_column_diff_output(grn_ctx *ctx,
                         grn_obj *diff,
                         grn_obj *lexicon,
                         grn_column_flags index_column_flags)
{
  grn_obj *remains_column =
    grn_obj_column(ctx,
                   diff,
                   remains_column_name,
                   strlen(remains_column_name));
  grn_obj *missings_column =
    grn_obj_column(ctx,
                   diff,
                   missings_column_name,
                   strlen(missings_column_name));
  grn_obj key;
  GRN_OBJ_INIT(&key, GRN_BULK, GRN_OBJ_DO_SHALLOW_COPY, lexicon->header.domain);
  grn_obj remains;
  GRN_UINT32_INIT(&remains, GRN_OBJ_VECTOR);
  grn_obj missings;
  GRN_UINT32_INIT(&missings, GRN_OBJ_VECTOR);
  grn_ctx_output_array_open(ctx, "diffs", grn_table_size(ctx, diff));
  {
    GRN_TABLE_EACH_BEGIN(ctx, diff, cursor, id) {
      grn_ctx_output_map_open(ctx, "diff", 3);
      {
        grn_ctx_output_cstr(ctx, "token");
        grn_ctx_output_map_open(ctx, "token", 2);
        {
          grn_ctx_output_cstr(ctx, "id");
          void *token_id_buffer;
          grn_table_cursor_get_key(ctx, cursor, &token_id_buffer);
          grn_id token_id = *((grn_id *)token_id_buffer);
          grn_ctx_output_uint32(ctx, token_id);

          grn_ctx_output_cstr(ctx, "value");
          char key_buffer[GRN_TABLE_MAX_KEY_SIZE];
          int key_size = grn_table_get_key(ctx,
                                           lexicon,
                                           token_id,
                                           key_buffer,
                                           sizeof(key_buffer));
          GRN_TEXT_SET(ctx, &key, key_buffer, key_size);
          grn_ctx_output_obj(ctx, &key, NULL);
        }
        grn_ctx_output_map_close(ctx);

        grn_ctx_output_cstr(ctx, "remains");
        GRN_BULK_REWIND(&remains);
        grn_obj_get_value(ctx, remains_column, id, &remains);
        index_column_diff_output_postings(ctx,
                                          index_column_flags,
                                          &remains,
                                          "remains");

        grn_ctx_output_cstr(ctx, "missings");
        GRN_BULK_REWIND(&missings);
        grn_obj_get_value(ctx, missings_column, id, &missings);
        index_column_diff_output_postings(ctx,
                                          index_column_flags,
                                          &missings,
                                          "missings");
      }
      grn_ctx_output_map_close(ctx);
    } GRN_TABLE_EACH_END(ctx, cursor);
  }
  grn_ctx_output_array_close(ctx);
  GRN_OBJ_FIN(ctx, &missings);
  GRN_OBJ_FIN(ctx, &remains);
  GRN_OBJ_FIN(ctx, &key);
}

static grn_obj *
command_index_column_diff(grn_ctx *ctx,
                          int n_args,
                          grn_obj **args,
                          grn_user_data *user_data)
{
  grn_raw_string table_name;
  grn_raw_string column_name;
  grn_obj *table = NULL;
  grn_obj *column = NULL;
  grn_obj *diff = NULL;

  table_name.value =
    grn_plugin_proc_get_var_string(ctx, user_data,
                                   "table", -1,
                                   &(table_name.length));
  column_name.value =
    grn_plugin_proc_get_var_string(ctx, user_data,
                                   "name", -1,
                                   &(column_name.length));

  table = grn_ctx_get(ctx, table_name.value, table_name.length);
  if (!table) {
    GRN_PLUGIN_ERROR(ctx,
                     GRN_INVALID_ARGUMENT,
                     "[index-column][diff] table doesn't exist: <%.*s>",
                     (int)(table_name.length),
                     table_name.value);
    goto exit;
  }
  if (!grn_obj_is_lexicon(ctx, table)) {
    GRN_PLUGIN_ERROR(ctx,
                     GRN_INVALID_ARGUMENT,
                     "[index-column][diff] table must be lexicon: <%.*s>: %s",
                     (int)(table_name.length),
                     table_name.value,
                     grn_obj_type_to_string(table->header.type));
    goto exit;
  }

  column = grn_obj_column(ctx, table, column_name.value, column_name.length);
  if (!column) {
    GRN_PLUGIN_ERROR(ctx,
                     GRN_INVALID_ARGUMENT,
                     "[index-column][diff] column doesn't exist: <%.*s>: <%.*s>",
                     (int)(table_name.length),
                     table_name.value,
                     (int)(column_name.length),
                     column_name.value);
    goto exit;
  }
  if (!grn_obj_is_index_column(ctx, column)) {
    GRN_PLUGIN_ERROR(ctx,
                     GRN_INVALID_ARGUMENT,
                     "[index-column][diff] column must be index column: "
                     "<%.*s>: <%.*s>: %s",
                     (int)(table_name.length),
                     table_name.value,
                     (int)(column_name.length),
                     column_name.value,
                     grn_obj_type_to_string(column->header.type));
    goto exit;
  }

  grn_index_column_diff(ctx, column, &diff);
  if (ctx->rc != GRN_SUCCESS) {
    GRN_PLUGIN_ERROR(ctx,
                     ctx->rc,
                     "[index-column][diff] failed to diff: "
                     "<%.*s>: <%.*s>: %s",
                     (int)(table_name.length),
                     table_name.value,
                     (int)(column_name.length),
                     column_name.value,
                     ctx->errbuf);
    goto exit;
  }

  index_column_diff_output(ctx,
                           diff,
                           table,
                           grn_column_get_flags(ctx, column));

exit :
  if (grn_obj_is_accessor(ctx, column)) {
    grn_obj_close(ctx, column);
  }

  if (diff) {
    grn_obj_close(ctx, diff);
  }

  return NULL;
}

void
grn_proc_init_index_column_diff(grn_ctx *ctx)
{
  grn_expr_var vars[2];
  unsigned int n_vars = 0;

  grn_plugin_expr_var_init(ctx, &(vars[n_vars++]), "table", -1);
  grn_plugin_expr_var_init(ctx, &(vars[n_vars++]), "name", -1);
  grn_plugin_command_create(ctx,
                            "index_column_diff", -1,
                            command_index_column_diff,
                            n_vars,
                            vars);
}
