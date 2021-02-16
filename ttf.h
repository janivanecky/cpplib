#pragma once
#include <stdint.h>

struct Tag {
    uint8_t bytes[4];
};

/* Table directory */

struct TableRecord {
    Tag tag;
    uint32_t checksum;
    uint32_t offset;
    uint32_t length;
};

struct TableDirectory {
    uint32_t sfnt_version;
    uint16_t num_tables;
    uint16_t search_range;
    uint16_t entry_selector;
    uint16_t range_shift;
    TableRecord *table_records;
};

/* OS2Table */

struct OS2Table {
    uint16_t version;
    int16_t x_avg_char_width;
    uint16_t us_weight_class;
    uint16_t us_width_class;
    uint16_t fs_type;
    int16_t y_subscript_x_size;
    int16_t y_subscript_y_size;
    int16_t y_subscript_x_offset;
    int16_t y_subscript_y_offset;
    int16_t y_superscript_x_size;
    int16_t y_superscript_y_size;
    int16_t y_superscript_x_offset;
    int16_t y_superscript_y_offset;
    int16_t y_strikeout_size;
    int16_t y_strikeout_position;
    int16_t s_family_class;
    uint8_t panose[10];
    uint32_t ul_unicode_range_1;
    uint32_t ul_unicode_range_2;
    uint32_t ul_unicode_range_3;
    uint32_t ul_unicode_range_4;
    Tag ach_vend_id;
    uint16_t fs_selection;
    uint16_t us_first_char_index;
    uint16_t us_last_char_index;
    int16_t s_typo_ascender;
    int16_t s_typo_descender;
    int16_t s_typo_line_gap;
    uint16_t us_win_ascent;
    uint16_t us_win_descent;
    uint32_t ul_code_page_range_1;
    uint32_t ul_code_page_range_2;
    int16_t sx_height;
    int16_t s_cap_height;
    uint16_t us_default_char;
    uint16_t us_break_char;
    uint16_t us_max_context;
    uint16_t us_lower_optical_point_size;
    uint16_t us_upper_optical_point_size;
};

/* cmap table */

struct Format4SubTable {
    uint16_t length;
    uint16_t language;
    uint16_t seg_count;
    uint16_t search_range;
    uint16_t entry_selector;
    uint16_t range_shift;
    uint16_t *end_code;
    uint16_t reserve_pad;
    uint16_t *start_code;
    int16_t  *id_delta;
    uint16_t *id_range_offset;
    uint16_t *glyph_id_array;
};

struct EncodingRecord {
    // TODO: Map this to names
    uint16_t platform_id;
    uint16_t encoding_id;
    uint16_t subtable_format;
    Format4SubTable subtable;
};

struct CmapTable {
    uint16_t version;
    uint16_t num_tables;
    EncodingRecord *encoding_records;
};

/* head table */

struct HeadTable {
    uint16_t major_version;
    uint16_t minor_version;
    int32_t revision; // NOTE: This is actually fixed 16.16
    uint32_t checksum_adjustment;
    uint32_t magic_number;
    uint16_t flags;
    uint16_t units_per_em;
    int64_t created;
    int64_t modified;
    int16_t x_min;
    int16_t y_min;
    int16_t x_max;
    int16_t y_max;
    uint16_t mac_style;
    uint16_t lowest_rec_ppem;
    int16_t font_direction_hint;
    int16_t index_to_loc_format;
    int16_t glyph_data_format;
};

/* hhea table */

struct HheaTable {
    uint16_t major_version;
    uint16_t minor_version;
    int16_t ascender;
    int16_t descender;
    int16_t line_gap;
    uint16_t advance_width_max;
    int16_t min_left_side_bearing;
    int16_t min_right_side_bearing;
    int16_t x_max_extent;
    int16_t caret_slop_rise;
    int16_t caret_slop_run;
    int16_t caret_offset;
    int64_t reserved_space;
    int16_t metric_data_format;
    uint16_t number_of_h_metrics;
};

/* htmx table */

struct LongHorMetricRecord {
    uint16_t advance_width;
    int16_t lsb;
};

struct HmtxTable {
    LongHorMetricRecord *h_metrics;
    int16_t *left_side_bearings;
};

/* maxp table */

struct MaxpTable {
    uint32_t version;
    uint16_t num_glyphs;
    uint16_t max_points;
    uint16_t max_contours;
    uint16_t max_composite_points;
    uint16_t max_composite_contours;
    uint16_t max_zones;
    uint16_t max_twilight_points;
    uint16_t max_storage;
    uint16_t max_function_defs;
    uint16_t max_instruction_defs;
    uint16_t max_stack_elements;
    uint16_t max_size_of_instructions;
    uint16_t max_component_elements;
    uint16_t max_component_depth;
};

/* loca table */

struct LocaTable {
    uint32_t *offsets;
};

/* kern table */

struct KernPair {
    uint16_t left;
    uint16_t right;
    int16_t value;
};

struct KernSubTable {
    uint16_t version;
    uint16_t length;
    uint16_t coverage;
    uint16_t n_pairs;
    uint16_t search_range;
    uint16_t entry_selector;
    uint16_t range_shift;
    KernPair *pairs;
};

struct KernTable {
    uint16_t version;
    uint16_t n_tables;
    KernSubTable *subtables;
};

/* glyph table */

enum SimpleGlyphFlags {
    ON_CURVE_POINT                          = 0x01,
    X_SHORT_VECTOR                          = 0x02,
    Y_SHORT_VECTOR                          = 0x04,
    REPEAT_FLAG                             = 0x08,
    X_IS_SAME_OR_POSITIVE_X_SHORT_VECTOR    = 0x10,
    Y_IS_SAME_OR_POSITIVE_Y_SHORT_VECTOR    = 0x20,
    OVERLAP_SIMPLE                          = 0x40,
};

enum CompositeGlyphFlags {
  ARG_1_AND_2_ARE_WORDS     = 0x1,
  ARGS_ARE_XY_VALUES        = 0x2,
  ROUND_XY_TO_GRID          = 0x4,
  WE_HAVE_A_SCALE           = 0x8,
  MORE_COMPONENTS           = 0x20,
  WE_HAVE_AN_X_AND_Y_SCALE  = 0x40,
  WE_HAVE_A_TWO_BY_TWO      = 0x80,
  WE_HAVE_INSTRUCTIONS      = 0x100,
  USE_MY_METRICS            = 0x200,
  OVERLAP_COMPOUND          = 0x400,
  SCALED_COMPONENT_OFFSET   = 0x800,
  UNSCALED_COMPONENT_OFFSET = 0x1000
};

struct GlyphComponent {
    uint16_t glyph_index;
    uint16_t flags;
    // Column major matrix.
    float transform_matrix[4]; // TODO: Could be in f2.14?
    union {
        int16_t offset_x;
        uint16_t src_point;
    };
    union {
        int16_t offset_y;
        uint16_t dst_point;
    };
    bool offsets_are_matching_points;
    bool use_metrics;
};

struct TTFGlyph {
    int16_t number_of_contours;
    int16_t x_min;
    int16_t y_min;
    int16_t x_max;
    int16_t y_max;

    union {
        struct {
            uint16_t *end_points;
            uint16_t instruction_length;
            uint8_t  *instructions;
            uint8_t  *flags;
            int16_t  *x_coordinates;
            int16_t  *y_coordinates;
        };
        struct {
            uint16_t number_of_components;
            GlyphComponent *components;
        };
    };
};

namespace ttf {
    TableDirectory get_table_directory(uint8_t *bytes);
    uint8_t *get_table_ptr(TableDirectory *table_directory, uint8_t *ttf_file_ptr, const char *table_tag);

    OS2Table get_os2_table(uint8_t *bytes);
    CmapTable get_cmap_table(uint8_t *bytes);
    HeadTable get_head_table(uint8_t *bytes);
    HheaTable get_hhea_table(uint8_t *bytes);
    HmtxTable get_hmtx_table(uint8_t *bytes, uint16_t h_metrics_count, uint16_t glyph_count);
    MaxpTable get_maxp_table(uint8_t *bytes);
    LocaTable get_loca_table(uint8_t *bytes, uint16_t glyph_count, bool long_offset=false);
    KernTable get_kern_table(uint8_t *bytes);
    TTFGlyph get_glyph(uint8_t *bytes);

    uint16_t get_glyph_index(uint8_t character_code, CmapTable *cmap_table);

    void release(TableDirectory *table_directory);
    void release(CmapTable *cmap_table);
    void release(HmtxTable *hmtx_table);
    void release(LocaTable *loca_table);
    void release(KernSubTable *kern_subtable);
    void release(KernTable *kern_table);
    void release(TTFGlyph *glyph);
}

#ifdef CPPLIB_TTF_IMPL
#include "ttf.cpp"
#endif
