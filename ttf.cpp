#include "ttf.h"
#include <string.h>
#include <stdlib.h>

/* Big to little endian conversion functions. */

int64_t be_to_le(int64_t x) {
    return ((x & 0xFF) << 56) |
        ((x & 0xFF00) << 40) | 
        ((x & 0xFF0000) << 24) | 
        ((x & 0xFF000000) << 8) | 
        ((x & 0xFF00000000) >> 8) | 
        ((x & 0xFF0000000000) >> 24) | 
        ((x & 0xFF000000000000) >> 40) | 
        ((x & 0xFF00000000000000) >> 54);
}

uint32_t be_to_le(uint32_t x) {
    return ((x & 0xFF) << 24) |
        ((x & 0xFF00) << 8) | 
        ((x & 0xFF0000) >> 8) | 
        ((x & 0xFF000000) >> 24);
}

int32_t be_to_le(int32_t x) {
    return ((x & 0xFF) << 24) |
        ((x & 0xFF00) << 8) | 
        ((x & 0xFF0000) >> 8) | 
        ((x & 0xFF000000) >> 24);
}

uint16_t be_to_le(uint16_t x) {
    return ((x & 0xFF) << 8) |
        ((x & 0xFF00) >> 8); 
}

int16_t be_to_le(int16_t x) {
    return ((x & 0xFF) << 8) |
        ((x & 0xFF00) >> 8); 
}

/* Functions to advance byte stream and return specific value. */

int64_t pop_int64(uint8_t **ptr) {
    int64_t value = be_to_le(**(int64_t **)ptr);
    *ptr += sizeof(int64_t);
    return value;
}

uint32_t pop_uint32(uint8_t **ptr) {
    uint32_t value = be_to_le(**(uint32_t **)ptr);
    *ptr += sizeof(uint32_t);
    return value;
}

int32_t pop_int32(uint8_t **ptr) {
    int32_t value = be_to_le(**(int32_t **)ptr);
    *ptr += sizeof(int32_t);
    return value;
}

uint16_t pop_uint16(uint8_t **ptr) {
    uint16_t value = be_to_le(**(uint16_t **)ptr);
    *ptr += sizeof(uint16_t);
    return value;
}

int16_t pop_int16(uint8_t **ptr) {
    int16_t value = be_to_le(**(int16_t **)ptr);
    *ptr += sizeof(int16_t);
    return value;
}

uint8_t pop_uint8(uint8_t **ptr) {
    uint8_t value = **(uint8_t **)ptr;
    *ptr += sizeof(uint8_t);
    return value;
}

int8_t pop_int8(uint8_t **ptr) {
    int8_t value = **(int8_t **)ptr;
    *ptr += sizeof(int8_t);
    return value;
}


Tag pop_tag(uint8_t **ptr) {
    Tag result;
    memcpy(&result, *ptr, sizeof(Tag));
    *ptr += sizeof(Tag);
    return result;
}

/* Table directory */

TableRecord pop_table_record(uint8_t **bytes) {
    TableRecord result = {};

    result.tag = pop_tag(bytes);
    result.checksum = pop_uint32(bytes);
    result.offset = pop_uint32(bytes);
    result.length = pop_uint32(bytes);

    return result;
}

TableDirectory ttf::get_table_directory(uint8_t *bytes) {
    TableDirectory result = {};

    result.sfnt_version = pop_uint32(&bytes);
    result.num_tables = pop_uint16(&bytes);
    result.search_range = pop_uint16(&bytes);
    result.entry_selector = pop_uint16(&bytes);
    result.range_shift = pop_uint16(&bytes);
    result.table_records = (TableRecord *)malloc(sizeof(TableRecord) * result.num_tables);
    for(uint16_t i = 0; i < result.num_tables; ++i) {
        result.table_records[i] = pop_table_record(&bytes);
    }

    return result;
}

void ttf::release(TableDirectory *table_directory) {
    free(table_directory->table_records);
}

/* OS2Table */

OS2Table ttf::get_os2_table(uint8_t *bytes) {
    OS2Table result = {};

    result.version = pop_uint16(&bytes);
    result.x_avg_char_width = pop_int16(&bytes);
    result.us_weight_class = pop_uint16(&bytes);
    result.us_width_class = pop_uint16(&bytes);
    result.fs_type = pop_uint16(&bytes);
    result.y_subscript_x_size = pop_int16(&bytes);
    result.y_subscript_y_size = pop_int16(&bytes);
    result.y_subscript_x_offset = pop_int16(&bytes);
    result.y_subscript_y_offset = pop_int16(&bytes);
    result.y_superscript_x_size = pop_int16(&bytes);
    result.y_superscript_y_size = pop_int16(&bytes);
    result.y_superscript_x_offset = pop_int16(&bytes);
    result.y_superscript_y_offset = pop_int16(&bytes);
    result.y_strikeout_size = pop_int16(&bytes);
    result.y_strikeout_position = pop_int16(&bytes);
    result.s_family_class = pop_int16(&bytes);
    for(int i = 0; i < 10; ++i) {
        result.panose[i] = pop_uint8(&bytes);
    }
    result.ul_unicode_range_1 = pop_uint32(&bytes);
    result.ul_unicode_range_2 = pop_uint32(&bytes);
    result.ul_unicode_range_3 = pop_uint32(&bytes);
    result.ul_unicode_range_4 = pop_uint32(&bytes);
    result.ach_vend_id = pop_tag(&bytes);
    result.fs_selection = pop_uint16(&bytes);
    result.us_first_char_index = pop_uint16(&bytes);
    result.us_last_char_index = pop_uint16(&bytes);
    result.s_typo_ascender = pop_int16(&bytes);
    result.s_typo_descender = pop_int16(&bytes);
    result.s_typo_line_gap = pop_int16(&bytes);
    result.us_win_ascent = pop_uint16(&bytes);
    result.us_win_descent = pop_uint16(&bytes);
    result.ul_code_page_range_1 = pop_uint32(&bytes);
    result.ul_code_page_range_2 = pop_uint32(&bytes);
    result.sx_height = pop_int16(&bytes);
    result.s_cap_height = pop_int16(&bytes);
    result.us_default_char = pop_uint16(&bytes);
    result.us_break_char = pop_uint16(&bytes);
    result.us_max_context = pop_uint16(&bytes);

    if(result.version == 5) {
        result.us_lower_optical_point_size = pop_uint16(&bytes);
        result.us_upper_optical_point_size = pop_uint16(&bytes);
    }

    return result;
}

/* cmap table */

Format4SubTable get_format4_subtable(uint8_t *bytes) {
    Format4SubTable result = {};

    result.length = pop_uint16(&bytes);
    result.language = pop_uint16(&bytes);
    result.seg_count = pop_uint16(&bytes) / 2;
    result.search_range = pop_uint16(&bytes);
    result.entry_selector = pop_uint16(&bytes);
    result.range_shift = pop_uint16(&bytes);
    result.end_code = (uint16_t *)malloc(sizeof(uint16_t) * result.seg_count);
    for(uint16_t i = 0; i < result.seg_count; ++i) {
        result.end_code[i] = pop_uint16(&bytes);
    }
    result.reserve_pad = pop_uint16(&bytes);
    result.start_code = (uint16_t *)malloc(sizeof(uint16_t) * result.seg_count);
    for(uint16_t i = 0; i < result.seg_count; ++i) {
        result.start_code[i] = pop_uint16(&bytes);
    }
    result.id_delta = (int16_t *)malloc(sizeof(int16_t) * result.seg_count);
    for(uint16_t i = 0; i < result.seg_count; ++i) {
        result.id_delta[i] = pop_int16(&bytes);
    }

    uint16_t table_size_up_to_glyph_id_array = sizeof(uint16_t) * (7 + result.seg_count * 4);
    uint16_t glyph_id_array_size = result.length - table_size_up_to_glyph_id_array;
    uint16_t glyph_id_array_length = glyph_id_array_size / sizeof(uint16_t);

    // We need to allocate id_range_offset and glyph_id_array as contiguous memory chunk because
    // id_range_offsets expect that.
    result.id_range_offset = (uint16_t *)malloc(sizeof(uint16_t) * (result.seg_count + glyph_id_array_length));
    for(uint16_t i = 0; i < result.seg_count; ++i) {
        result.id_range_offset[i] = pop_uint16(&bytes);
    }
    result.glyph_id_array = result.id_range_offset + result.seg_count;
    for(uint16_t i = 0; i < glyph_id_array_length; ++i) {
        result.glyph_id_array[i] = pop_uint16(&bytes);
    }

    return result;
}

void release(Format4SubTable *subtable) {
    free(subtable->end_code);
    free(subtable->start_code);
    free(subtable->id_delta);
    free(subtable->id_range_offset);
}

EncodingRecord pop_encoding_record(uint8_t **bytes, uint8_t *cmap_table_start) {
    EncodingRecord result = {};

    result.platform_id = pop_uint16(bytes);
    result.encoding_id = pop_uint16(bytes);
    
    uint32_t subtable_offset = pop_uint32(bytes);
    uint8_t *subtable_ptr = cmap_table_start + subtable_offset; 

    result.subtable_format = pop_uint16(&subtable_ptr);
    if(result.subtable_format == 4) {
        result.subtable = get_format4_subtable(subtable_ptr);
    }

    return result;
}

void release(EncodingRecord *encoding_record) {
    if(encoding_record->subtable_format == 4) {
        release(&encoding_record->subtable);
    }
}

CmapTable ttf::get_cmap_table(uint8_t *bytes) {
    uint8_t *cmap_table_start = bytes;

    CmapTable result = {};

    result.version = pop_uint16(&bytes);
    result.num_tables = pop_uint16(&bytes);
    result.encoding_records = (EncodingRecord *)malloc(sizeof(EncodingRecord) * result.num_tables);
    for(uint16_t i = 0; i < result.num_tables; ++i) {
        result.encoding_records[i] = pop_encoding_record(&bytes, cmap_table_start);
    }

    return result;
}

void ttf::release(CmapTable *cmap_table) {
    for(uint16_t i = 0; i < cmap_table->num_tables; ++i) {
        release(&cmap_table->encoding_records[i]);
    }
    free(cmap_table->encoding_records);
}

/* head table */

HeadTable ttf::get_head_table(uint8_t *bytes) {
    HeadTable result = {};

    result.major_version = pop_uint16(&bytes);
    result.minor_version = pop_uint16(&bytes);
    result.revision = pop_int32(&bytes);
    result.checksum_adjustment = pop_uint32(&bytes);
    result.magic_number = pop_uint32(&bytes);
    result.flags = pop_uint16(&bytes);
    result.units_per_em = pop_uint16(&bytes);
    result.created = pop_int64(&bytes);
    result.modified = pop_int64(&bytes);
    result.x_min = pop_int16(&bytes);
    result.y_min = pop_int16(&bytes);
    result.x_max = pop_int16(&bytes);
    result.y_max = pop_int16(&bytes);
    result.mac_style = pop_uint16(&bytes);
    result.lowest_rec_ppem = pop_uint16(&bytes);
    result.font_direction_hint = pop_int16(&bytes);
    result.index_to_loc_format = pop_int16(&bytes);
    result.glyph_data_format = pop_int16(&bytes);

    return result;
}

/* hhea table */

HheaTable ttf::get_hhea_table(uint8_t *bytes) {
    HheaTable result = {};

    result.major_version = pop_uint16(&bytes);
    result.minor_version = pop_uint16(&bytes);
    result.ascender = pop_int16(&bytes);
    result.descender = pop_int16(&bytes);
    result.line_gap = pop_int16(&bytes);
    result.advance_width_max = pop_uint16(&bytes);
    result.min_left_side_bearing = pop_int16(&bytes);
    result.min_right_side_bearing = pop_int16(&bytes);
    result.x_max_extent = pop_int16(&bytes);
    result.caret_slop_rise = pop_int16(&bytes);
    result.caret_slop_run = pop_int16(&bytes);
    result.caret_offset = pop_int16(&bytes);
    result.reserved_space = pop_int64(&bytes);
    result.metric_data_format = pop_int16(&bytes);
    result.number_of_h_metrics = pop_uint16(&bytes);

    return result;
}

/* htmx table */

LongHorMetricRecord pop_long_hor_metric_record(uint8_t **bytes) {
    LongHorMetricRecord result = {};

    result.advance_width = pop_uint16(bytes);
    result.lsb = pop_int16(bytes);

    return result;
}

HmtxTable ttf::get_hmtx_table(uint8_t *bytes, uint16_t h_metrics_count, uint16_t glyph_count) {
    HmtxTable result = {};

    result.h_metrics = (LongHorMetricRecord *)malloc(sizeof(LongHorMetricRecord) * h_metrics_count);
    for(uint16_t i = 0; i < h_metrics_count; ++i) {
        result.h_metrics[i] = pop_long_hor_metric_record(&bytes);
    }

    uint16_t left_side_bearings_count = glyph_count - h_metrics_count;
    result.left_side_bearings = (int16_t *)malloc(sizeof(int16_t) * left_side_bearings_count);
    for(uint16_t i = 0; i < left_side_bearings_count; ++i) {
        result.left_side_bearings[i] = pop_int16(&bytes);
    }

    return result;
}

void ttf::release(HmtxTable *hmtx_table) {
    free(hmtx_table->h_metrics);
    free(hmtx_table->left_side_bearings);
}

/* maxp table */

MaxpTable ttf::get_maxp_table(uint8_t *bytes) {
    MaxpTable result = {};

    result.version = pop_uint32(&bytes);
    result.num_glyphs = pop_uint16(&bytes);
    if(result.version != 0x00010000) {
        return result;
    }
    result.max_points = pop_uint16(&bytes);
    result.max_contours = pop_uint16(&bytes);
    result.max_composite_points = pop_uint16(&bytes);
    result.max_composite_contours = pop_uint16(&bytes);
    result.max_zones = pop_uint16(&bytes);
    result.max_twilight_points = pop_uint16(&bytes);
    result.max_storage = pop_uint16(&bytes);
    result.max_function_defs = pop_uint16(&bytes);
    result.max_instruction_defs = pop_uint16(&bytes);
    result.max_stack_elements = pop_uint16(&bytes);
    result.max_size_of_instructions = pop_uint16(&bytes);
    result.max_component_elements = pop_uint16(&bytes);
    result.max_component_depth = pop_uint16(&bytes);

    return result;
}

/* loca table */

LocaTable ttf::get_loca_table(uint8_t *bytes, uint16_t glyph_count, bool long_offsets) {
    LocaTable result = {};

    result.offsets = (uint32_t *)malloc(sizeof(uint32_t) * (glyph_count + 1));
    for(uint16_t i = 0; i < glyph_count + 1; ++i) {
        if(long_offsets) {
            result.offsets[i] = pop_uint32(&bytes);
        } else {
            result.offsets[i] = uint32_t(pop_uint16(&bytes)) * 2;
        }
    }

    return result;
}

void ttf::release(LocaTable *loca_table) {
    free(loca_table->offsets);
}

/* kern table */

KernPair pop_kern_pair(uint8_t **bytes) {
    KernPair result = {};

    result.left = pop_uint16(bytes);
    result.right = pop_uint16(bytes);
    result.value = pop_int16(bytes);

    return result;
}

KernSubTable pop_kern_subtable(uint8_t **bytes) {
    KernSubTable result = {};

    result.version = pop_uint16(bytes);
    result.length = pop_uint16(bytes);
    result.coverage = pop_uint16(bytes);

    uint8_t format = (result.coverage & 0xFF00) >> 8;
    if(format > 0) {
        // Move to the end of table.
        *bytes += result.length - 3 * sizeof(uint16_t);
        return result;
    }

    result.n_pairs = pop_uint16(bytes);
    result.search_range = pop_uint16(bytes);
    result.entry_selector = pop_uint16(bytes);
    result.range_shift = pop_uint16(bytes);

    result.pairs = (KernPair *)malloc(sizeof(KernPair) * result.n_pairs);
    for(uint16_t i = 0; i < result.n_pairs; ++i) {
        result.pairs[i] = pop_kern_pair(bytes);
    }

    return result;
}

void ttf::release(KernSubTable *kern_subtable) {
    uint8_t format = (kern_subtable->coverage & 0xFF00) >> 8;
    if(format == 0) {
        free(kern_subtable->pairs);
    }
}

KernTable ttf::get_kern_table(uint8_t *bytes) {
    KernTable result = {};

    result.version = pop_uint16(&bytes);
    result.n_tables = pop_uint16(&bytes);
    result.subtables = (KernSubTable *)malloc(sizeof(KernSubTable) * result.n_tables);
    for(uint16_t i = 0; i < result.n_tables; ++i) {
        result.subtables[i] = pop_kern_subtable(&bytes);
    }

    return result;
}

void ttf::release(KernTable *kern_table) {
    for(uint16_t i = 0; i < kern_table->n_tables; ++i) {
        release(&kern_table->subtables[i]);
    }
    free(kern_table->subtables);
}

/* glyph table */

TTFGlyph get_simple_glyph(uint8_t *bytes) {
    TTFGlyph result;

    // Get the basic glyph info.
    result.number_of_contours = pop_int16(&bytes);
    result.x_min = pop_int16(&bytes);
    result.y_min = pop_int16(&bytes);
    result.x_max = pop_int16(&bytes);
    result.y_max = pop_int16(&bytes);

    // Get all the end point indices.
    result.end_points = (uint16_t *)malloc(sizeof(uint16_t) * result.number_of_contours);
    for(uint16_t i = 0; i < result.number_of_contours; ++i) {
        result.end_points[i] = pop_uint16(&bytes);
    }

    // Get all the instructions.
    result.instruction_length = pop_uint16(&bytes);
    result.instructions = (uint8_t *)malloc(sizeof(uint8_t) * result.instruction_length);
    for(uint16_t i = 0; i < result.instruction_length; ++i) {
        result.instructions[i] = pop_uint8(&bytes);
    }
    
    uint16_t point_count = result.end_points[result.number_of_contours - 1] + 1;
    
    // Parse all the flags.
    result.flags = (uint8_t *)malloc(sizeof(uint8_t) * point_count);
    uint8_t repeat_flag_count = 0;
    for(uint16_t i = 0; i < point_count; ++i) {
        if(repeat_flag_count) {
            result.flags[i] = result.flags[i - 1];
            repeat_flag_count--;
        } else {
            uint8_t flag = pop_uint8(&bytes);
            if(flag & REPEAT_FLAG) {
                repeat_flag_count = pop_uint8(&bytes);
            }
            result.flags[i] = flag;
        }
    }

    // Get all the points' x coordinates.
    result.x_coordinates = (int16_t *)malloc(sizeof(int16_t) * point_count);
    for(uint16_t i = 0; i < point_count; ++i) {
        uint8_t flag = result.flags[i];
        if(flag & X_SHORT_VECTOR) {
            int16_t x = int16_t(pop_uint8(&bytes));
            result.x_coordinates[i] = flag & X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR ? x : -x;
            if(i > 0) {
                result.x_coordinates[i] += result.x_coordinates[i - 1];
            }
        } else {
            if(flag & X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR) {
                if(i > 0) {
                    result.x_coordinates[i] = result.x_coordinates[i - 1];
                } else {
                    result.x_coordinates[i] = 0;
                }
            } else {
                result.x_coordinates[i] = pop_int16(&bytes);
                if(i > 0) {
                    result.x_coordinates[i] += result.x_coordinates[i - 1];
                }
            }
        }
    }
    
    // Get all the points' y coordinates.
    result.y_coordinates = (int16_t *)malloc(sizeof(int16_t) * point_count);
    for(uint16_t i = 0; i < point_count; ++i) {
        uint8_t flag = result.flags[i];
        if(flag & Y_SHORT_VECTOR) {
            int16_t y = int16_t(pop_uint8(&bytes));
            result.y_coordinates[i] = flag & Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR ? y : -y;
            if(i > 0) {
                result.y_coordinates[i] += result.y_coordinates[i - 1];
            }
        } else {
            if(flag & Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR) {
                if(i > 0) {
                    result.y_coordinates[i] = result.y_coordinates[i - 1];
                } else {
                    result.y_coordinates[i] = 0;
                }
            } else {
                result.y_coordinates[i] = pop_int16(&bytes);
                if(i > 0) {
                    result.y_coordinates[i] += result.y_coordinates[i - 1];
                }
            }
        }
    }

    return result;
}

TTFGlyph get_composite_glyph(uint8_t *bytes) {
    TTFGlyph result;

    // Get the basic glyph info.
    result.number_of_contours = pop_int16(&bytes);
    result.x_min = pop_int16(&bytes);
    result.y_min = pop_int16(&bytes);
    result.x_max = pop_int16(&bytes);
    result.y_max = pop_int16(&bytes);

    // Parse all the components. Note that we don't know how many of them are there in advance.
    result.number_of_components = 0;
    // TODO: This should be more flexible.
    const int MAX_COMPONENT_COUNT = 20;
    result.components = (GlyphComponent *)malloc(sizeof(GlyphComponent) * MAX_COMPONENT_COUNT);
    uint16_t flags;
    do {
        GlyphComponent *component = &result.components[result.number_of_components++];
        
        // Parse flags first.
        flags = pop_uint16(&bytes);
        
        // Set up "simple" glyph information.
        component->glyph_index = pop_uint16(&bytes);
        component->flags = flags;
        component->use_metrics = flags & USE_MY_METRICS;

        // Parse the argument values - either offsets or point indices.
        if(flags & ARGS_ARE_XY_VALUES) {
            component->offsets_are_matching_points = false;
            if(flags & ARG_1_AND_2_ARE_WORDS) {
                component->offset_x = pop_int16(&bytes);
                component->offset_y = pop_int16(&bytes);
            } else {
                component->offset_x = (int16_t)pop_int8(&bytes);
                component->offset_y = (int16_t)pop_int8(&bytes);
            }
        } else {
            component->offsets_are_matching_points = true;
            if(flags & ARG_1_AND_2_ARE_WORDS) {
                component->src_point = pop_uint16(&bytes);
                component->dst_point = pop_uint16(&bytes);
            } else {
                component->src_point = (uint16_t)pop_uint8(&bytes);
                component->dst_point = (uint16_t)pop_uint8(&bytes);
            }
        }

        // Parse the transformation/scaling data.
        if(flags & WE_HAVE_A_SCALE) {
            float scale = float(pop_int16(&bytes)) / 16384.0f;
            component->transform_matrix[0] = scale;
            component->transform_matrix[1] = 0.0f;
            component->transform_matrix[2] = 0.0f;
            component->transform_matrix[3] = scale;
        } else if(flags & WE_HAVE_AN_X_AND_Y_SCALE) {
            float scale_x = float(pop_int16(&bytes)) / 16384.0f;
            float scale_y = float(pop_int16(&bytes)) / 16384.0f;
            component->transform_matrix[0] = scale_x;
            component->transform_matrix[1] = 0.0f;
            component->transform_matrix[2] = 0.0f;
            component->transform_matrix[3] = scale_y;
        } else if(flags & WE_HAVE_A_TWO_BY_TWO) {
            component->transform_matrix[0] = float(pop_int16(&bytes)) / 16384.0f;
            component->transform_matrix[1] = float(pop_int16(&bytes)) / 16384.0f;
            component->transform_matrix[2] = float(pop_int16(&bytes)) / 16384.0f;
            component->transform_matrix[3] = float(pop_int16(&bytes)) / 16384.0f;
        } else {
            component->transform_matrix[0] = 1.0f;
            component->transform_matrix[1] = 0.0f;
            component->transform_matrix[2] = 0.0f;
            component->transform_matrix[3] = 1.0f;
        }

    } while(flags & MORE_COMPONENTS && result.number_of_components < MAX_COMPONENT_COUNT);

    return result;
}

TTFGlyph ttf::get_glyph(uint8_t *bytes) {
    // Peek at the next two bytes to see the number of contours.
    // If the number is negative, the glyph is composite, otherwise it's a simple glyph.
    uint8_t *temp_bytes = bytes;
    int16_t number_of_contours = pop_int16(&temp_bytes);
    
    if(number_of_contours < 0) {
        return get_composite_glyph(bytes);
    }
    return get_simple_glyph(bytes);
}

void ttf::release(TTFGlyph *glyph) {
    if(glyph->number_of_contours < 0) {
        free(glyph->components);
    } else {
        free(glyph->end_points);
        free(glyph->instructions);
        free(glyph->flags);
        free(glyph->x_coordinates);
        free(glyph->y_coordinates);
    }
}

/* "Higher level" API */

uint8_t *ttf::get_table_ptr(TableDirectory *table_directory, uint8_t *ttf_file_ptr, const char *table_tag) {
    uint32_t table_tag_num = *(uint32_t *)table_tag;
    for(int t = 0; t < table_directory->num_tables; ++t) {
        TableRecord table_record = table_directory->table_records[t];
        if(*(uint32_t *)table_record.tag.bytes == table_tag_num) {
            return ttf_file_ptr + table_record.offset;
        }
    }
    return NULL;
}

uint16_t ttf::get_glyph_index(uint8_t character_code, CmapTable *cmap_table) {
    // TODO: This should find a suitable table.
    Format4SubTable subtable = cmap_table->encoding_records[0].subtable;

    // Find the first segment for which end code is higher than or equal to the character code.
    int16_t segment_id = -1;
    for(uint16_t i = 0; i < subtable.seg_count; ++i) {
        if(uint16_t(character_code) <= subtable.end_code[i]) {
            segment_id = i;
            break;
        }
    }

    // Check that the start code for the segment is lower than or equal to the character code,
    // if not, return glyph 0 - missing glyph.
    if(subtable.start_code[segment_id] > uint16_t(character_code)) {
        return 0;
    }

    // Get delta and range_offset values.
    int16_t id_delta = subtable.id_delta[segment_id];
    uint16_t id_range_offset = subtable.id_range_offset[segment_id];

    // By default, glyph id is just the character code with delta offset.
    uint16_t glyph_id = character_code + id_delta;

    // In case range_offset is specified, we have to do fancy mapping.
    // See docs for explanation:
    // https://docs.microsoft.com/en-us/typography/opentype/spec/cmap#format-4-segment-mapping-to-delta-values
    if(id_range_offset > 0) {
        uint16_t *glyph_id_array_ptr = &subtable.id_range_offset[segment_id]
            + id_range_offset / 2
            + (character_code - subtable.start_code[segment_id]);
        glyph_id = *glyph_id_array_ptr + id_delta;
    }
    
    return glyph_id;
}
