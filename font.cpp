#include "font.h"
#include "maths.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

#ifdef DEBUG
#include<stdio.h>
#define PRINT_DEBUG(message, ...) {printf("ERROR in file %s on line %d: ", __FILE__, __LINE__); printf(message, __VA_ARGS__); printf("\n");}
#else
#define PRINT_DEBUG(message, ...)
#endif

// TODO: Remove math.h

/*

Helper functions 

*/

// Simple affine transformation on a 2D point.
Vector2 transform_point(Vector2 p, float transform_matrix[4], Vector2 translation) {
    Vector2 result = Vector2(
        roundf(p.x * transform_matrix[0] + p.y * transform_matrix[2]),
        roundf(p.x * transform_matrix[1] + p.y * transform_matrix[3])
    ) + translation;
    return result;
}

/*

SDF functions.

*/

// From https://www.iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm
float sdf_line(Vector2 p, Vector2 a, Vector2 b ) {
    Vector2 pa = p - a, ba = b - a;
    float h = math::clamp(math::dot(pa, ba) / math::dot(ba, ba), 0.0f, 1.0f);
    return math::length(pa - ba * h);
}

// From https://www.iquilezles.org/www/articles/distfunctions2d/distfunctions2d.htm
float sdf_bezier(Vector2 pos, Vector2 A, Vector2 B, Vector2 C) {    
    Vector2 a = B - A;
    Vector2 b = A - B * 2.0f + C;
    Vector2 c = a * 2.0f;
    Vector2 d = A - pos;
    float kk = 1.0f / math::dot(b, b);
    float kx = kk * math::dot(a, b);
    float ky = kk * (2.0f * math::dot(a, a) + math::dot(d, b)) / 3.0f;
    float kz = kk * math::dot(d, a);      
    float res = 0.0;
    float p = ky - kx * kx;
    float p3 = p * p * p;
    float q = kx * (2.0f * kx * kx - 3.0f * ky) + kz;
    float h = q * q + 4.0f * p3;
    if(h >= 0.0f) { 
        h = math::sqrt(h);
        Vector2 x = (Vector2(h, -h) - q) / 2.0f;
        float uvx = math::sign(x.x) * math::pow(math::abs(x.x), 1.0f / 3.0f);
        float uvy = math::sign(x.y) * math::pow(math::abs(x.y), 1.0f / 3.0f);
        Vector2 uv = Vector2(uvx, uvy);
        float t = math::clamp(uv.x + uv.y - kx, 0.0f, 1.0f);
        Vector2 temp = d + (c + b * t) * t;
        res = math::dot(temp, temp);
    } else {
        float z = math::sqrt(-p);
        float v = math::acos(q / (p * z * 2.0f)) / 3.0f;
        float m = math::cos(v);
        float n = math::sin(v) * 1.732050808f;
        Vector3 t = Vector3(m + m, -n -m, n-m) * z - kx;
        t.x = math::clamp(t.x, 0.0f, 1.0f);
        t.y = math::clamp(t.y, 0.0f, 1.0f);
        t.z = math::clamp(t.z, 0.0f, 1.0f);
        res = math::min(math::dot2(d + (c + b * t.x) * t.x), math::dot2(d + (c + b * t.y) * t.y));
        // the third root cannot be the closest
        // res = min(res,dot2(d+(c+b*t.z)*t.z));
    }
    return math::sqrt(res);
}

/*

Functions to compute winding number.

*/

int winding_number_line(Vector2 p, Vector2 line_start, Vector2 line_end) {
    // It's essentially finding an intersection of two rays.
    // First ray goes from current point in direction of y-axis.
    Vector2 p1 = p;
    Vector2 v1 = Vector2(0, 1);
    
    // Second ray goes from one line point to the other.
    Vector2 p2 = line_start;
    Vector2 v2 = line_end - line_start;

    // In case the rays are parallel, they're never going to connect, and the denominator in
    // equations below is going to be zero -> undefined behavior. Therefore we'll check for
    // parallel rays and exit early.
    bool are_parallel = v1.x * v2.y == v1.y * v2.x;
    if(are_parallel) return 0;

    // Solve the set of two linear equations for t of both rays.
    float t1 = (v2.x * (p1.y - p2.y) + v2.y * (p2.x - p1.x))  / (v1.x * v2.y - v1.y * v2.x);
    float t2 = (v1.x * (p1.y - p2.y) + v1.y * (p2.x - p1.x))  / (v1.x * v2.y - v1.y * v2.x);

    // We're considering ray line intersection not sucessful if it's either on the negative
    // side of the ray coming from our pixel position (t1 < 0), or if the intersection is
    // outside of line's bounds (t1 < 0 || t1 > 1).
    if(t1 < 0.0f || t2 < 0.0f || t2 > 1.0f){
        return 0;
    }
    
    // Line direction (winding number increment) is positive for lines going to the right.
    int line_direction = v2.x >= 0.0f ? 1 : -1;

    return line_direction;
}

int winding_number_bezier(Vector2 p, Vector2 b1, Vector2 b2, Vector2 b3) {
    // We're looking for intersection of a ray and bezier curve.
    // The ray goes from current point p in direction of y-axis.
    Vector2 v = Vector2(0, 1);

    // We know that the intersection has to happen at x = p.x because the ray
    // goes from p in direction of y-axis, so no change in x. Now we need to find
    // point on bezier curve with the same x value. To find it, we construct a
    // quadratic equation (since we're dealing with quadratic bezier):
    // A * t^2 + B * t + C_ = p.x
    // A * t^2 + B * t + C_ - p.x = 0
    // A * t^2 + B * t + C = 0
    float A = b3.x - 2 * b2.x + b1.x;
    float B = 2 * b2.x - 2 * b1.x;
    float C = b1.x - p.x;
    // The equation has 0-2 solutions.
    float t1, t2;

    bool is_linear_equation = fabs(A) < 0.00001f;
    if(is_linear_equation) {
        // In case the equation is linear, we only have one solution.
        // Solve equation B * t + C = 0 for t.
        t1 = -C / B;
        t2 = -1; // No secondary solution. 
    } else {
        // In case the equation is quadratic, we compute the discriminant.
        float D = B * B - 4 * A * C;
        // In case the discriminant is negative, the equation has no solutions.
        if(D < 0) {
            return 0;
        }

        // Get both solutions.
        t1 = (-B + math::sqrt(D)) / (2 * A);
        t2 = (-B - math::sqrt(D)) / (2 * A);
    }

    // Evaluate bezier curve's y-coordinate from t to get the y-value of the intersection point.
    float intersection_y1 = t1 * t1 * (b3.y - 2.0f * b2.y + b1.y) + t1 * 2 * (b2.y - b1.y) + b1.y;
    float intersection_y2 = t2 * t2 * (b3.y - 2.0f * b2.y + b1.y) + t2 * 2 * (b2.y - b1.y) + b1.y;
    
    // Compute u for ray equation p.y + u * v.y from the intersection point.
    // p.y + u * v.y = intersection_y
    // Since v.y = 1.0, this simplifies to
    // p.y + u = intersection_y
    // u = intersection_y - p.y;
    float u1 = intersection_y1 - p.y;
    float u2 = intersection_y2 - p.y;

    // Line direction (winding number increment) is positive for curves going to the right.
    int line_direction = b3.x >= b1.x ? 1 : -1;

    // If any of the solutions for t resulted in the intersection within 0-1 bounds of
    // the bezier curve and the positive side of the ray, return the line_direction.
    if(u1 >= 0.0f && t1 >= 0.0f && t1 <= 1.0f) {
        return line_direction;
    }
    if(u2 >= 0.0f && t2 >= 0.0f && t2 <= 1.0f) {
        return line_direction;
    }

    // No intersection within bounds of curve and correct side of the ray.
    return 0;
}

/*

Segment Outline section.

*/

enum OutlineSegmentType {
    LINE,
    BEZIER
};

struct OutlineSegment {
    Vector2 points[3];
    OutlineSegmentType type;
};

// Decompose the outline into segments. Segment is either a straight line or a quadratic bezier curve.
int get_glyph_segments(TTFGlyph *glyph, OutlineSegment *segs) {
    uint32_t segs_count = 0;

    int current_point = 0;
    for(int i = 0; i < glyph->number_of_contours; ++i) {
        // Set up where current contour starts and ends.
        int contour_start_point = current_point;
        int contour_end_point = glyph->end_points[i] + 1;

        // Get all the segments for the contour.
        while(current_point < contour_end_point) {
            // Get the start and end points for the segment (next on-curve point).
            int segment_start_point = current_point;
            int segment_end_point = current_point;
            int segment_length = 0;
            do {
                segment_end_point++;
                segment_length++;
                // Connect contour start with end.
                if(segment_end_point == contour_end_point) segment_end_point = contour_start_point;
            } while(!(glyph->flags[segment_end_point] & ON_CURVE_POINT));

            // Create a segment based on its length.
            if(segment_length == 1) {
                // Simple line segment.
                Vector2 v1 = Vector2(glyph->x_coordinates[segment_start_point], glyph->y_coordinates[segment_start_point]);
                Vector2 v2 = Vector2(glyph->x_coordinates[segment_end_point], glyph->y_coordinates[segment_end_point]);
                segs[segs_count++] = {
                    {v1, v2, Vector2(0,0)},
                    LINE
                };
            } else if (segment_length == 2) {
                // Simple quadratic bezier curve.
                Vector2 v1 = Vector2(glyph->x_coordinates[segment_start_point], glyph->y_coordinates[segment_start_point]);
                Vector2 v2 = Vector2(glyph->x_coordinates[segment_start_point + 1], glyph->y_coordinates[segment_start_point + 1]);
                Vector2 v3 = Vector2(glyph->x_coordinates[segment_end_point], glyph->y_coordinates[segment_end_point]);
                segs[segs_count++] = {
                    {v1, v2, v3},
                    BEZIER
                };
            } else if (segment_length > 2) {
                // "Compressed" bezier curves.
                // This means that some of the on-curve control points are not explicitly specified, because they
                // can be inferred from surrounding off-curve points. This leads to multiple sub-segments per this segment.
                int segment_count = segment_length - 1;
                for(int s = 0; s < segment_count; ++s) {
                    Vector2 v1, v2, v3;  // Three bezier points.
                    
                    // The middle, off-curve control point is always explicitly specified.
                    int off_curve_point_idx = segment_start_point + s + 1;
                    v2 = Vector2(glyph->x_coordinates[off_curve_point_idx], glyph->y_coordinates[off_curve_point_idx]);

                    if(s == 0) {
                        // For the first sub-segment, the first point is explicitly specified.
                        v1 = Vector2(glyph->x_coordinates[segment_start_point], glyph->y_coordinates[segment_start_point]);
                    } else {
                        // Otherwise we have to infer it from the off-curve point and the point before that.
                        Vector2 v0 = Vector2(glyph->x_coordinates[off_curve_point_idx - 1], glyph->y_coordinates[off_curve_point_idx - 1]);
                        v1 = (v0 + v2) / 2.0f;
                        // NOTE: We want the design/glyph space to be in integer coordinates, but division by two above can cause
                        // coordinates to become .5. Thus we have to round.
                        v1.x = roundf(v1.x);
                        v1.y = roundf(v1.y);
                    }

                    if(s == segment_count - 1) {
                        // For the last sub-segment, the last point is explicitly specified.
                        v3 = Vector2(glyph->x_coordinates[segment_end_point], glyph->y_coordinates[segment_end_point]);
                    } else {
                        // Otherwise we have to infer it from the off-curve point and the point after that.
                        Vector2 v4 = Vector2(glyph->x_coordinates[off_curve_point_idx + 1], glyph->y_coordinates[off_curve_point_idx + 1]);
                        v3 = (v2 + v4) / 2.0f;
                        // NOTE: We want the design/glyph space to be in integer coordinates, but division by two above can cause
                        // coordinates to become .5. Thus we have to round.
                        v3.x = roundf(v3.x);
                        v3.y = roundf(v3.y);
                    }

                    segs[segs_count++] = {
                        {v1, v2, v3},
                        BEZIER
                    };
                }
            }

            // Move to the next segment.
            current_point += segment_length;
        }
    }

    return segs_count;
}

/*

Public API section.

*/

float get_distance(Vector2 p_pixel, OutlineSegment *segments, uint32_t segment_count, float funits_to_pixels_scaling) {
    Vector2 p_funits = p_pixel / funits_to_pixels_scaling;
    // Make sure that the point we're evaluating doesn't hit grid lines exactly. The outline points will always
    // be on the grid lines (because the outlines are defined in integer space) and to compute winding order we
    // want to avoid edge cases where our pixel is directly above/below one of the points of the outline. That would
    // mean that we could potentially count the line coming to and line coming from the point both as lines above
    // the current pixel, so our winding number would be off. Easy way to prevent it is to make sure the point isn't
    // aligned to the grid.
    if(math::fmod(p_funits.x, 1.0f) < 0.01f) {
        p_funits.x += 0.01f;
    }
    if(math::fmod(p_funits.x, 1.0f) > 0.99f) {
        p_funits.x -= 0.01f;
    }
    if(math::fmod(p_funits.y, 1.0f) < 0.01f) {
        p_funits.y += 0.01f;
    }
    if(math::fmod(p_funits.y, 1.0f) > 0.99f) {
        p_funits.y -= 0.01f;
    }

    // Compute minimum distance to the outline and the winding number of current point.
    int winding_number = 0;
    float d = 10000000.0f;  // TODO: Replace with FMAX
    for(uint32_t s = 0; s < segment_count; ++s) {
        OutlineSegment seg = segments[s];
        if (seg.type == LINE) {
            d = math::min(d, sdf_line(
                p_pixel,
                seg.points[0] * funits_to_pixels_scaling,
                seg.points[1] * funits_to_pixels_scaling
            ));
            winding_number += winding_number_line(p_funits, seg.points[0], seg.points[1]);
        } else {
            d = math::min(d, sdf_bezier(
                p_pixel,
                seg.points[0] * funits_to_pixels_scaling,
                seg.points[1] * funits_to_pixels_scaling,
                seg.points[2] * funits_to_pixels_scaling
            ));
            winding_number += winding_number_bezier(p_funits, seg.points[0], seg.points[1], seg.points[2]);
        }
    }

    // If winding number is non-zero, we're inside the glyph.
    if(winding_number != 0) {
        d *= -1;
    }

    return d;
}

// TODO: This function could probably be shorter.
Font font::get(uint8_t *data, int32_t data_size, int32_t size, int32_t bitmap_size) {
    Font font = {};

    // TODO: Handle errors (incorrect check sum etc.)
    // Fetch TTF tables.
    TableDirectory table_directory = ttf::get_table_directory(data);

    uint8_t *hhea_table_ptr = ttf::get_table_ptr(&table_directory, data, "hhea");
    HheaTable hhea_table = ttf::get_hhea_table(hhea_table_ptr);

    uint8_t *head_table_ptr = ttf::get_table_ptr(&table_directory, data, "head");
    HeadTable head_table = ttf::get_head_table(head_table_ptr);

    uint8_t *maxp_table_ptr = ttf::get_table_ptr(&table_directory, data, "maxp");
    MaxpTable maxp_table = ttf::get_maxp_table(maxp_table_ptr);

    uint8_t *hmtx_table_ptr = ttf::get_table_ptr(&table_directory, data, "hmtx");
    uint16_t h_metrics_count = hhea_table.number_of_h_metrics;
    HmtxTable hmtx_table = ttf::get_hmtx_table(hmtx_table_ptr, h_metrics_count, maxp_table.num_glyphs);

    uint8_t *cmap_table_ptr = ttf::get_table_ptr(&table_directory, data, "cmap");
    CmapTable cmap_table = ttf::get_cmap_table(cmap_table_ptr);

    uint8_t *loca_table_ptr = ttf::get_table_ptr(&table_directory, data, "loca");
    LocaTable loca_table = ttf::get_loca_table(loca_table_ptr, maxp_table.num_glyphs, head_table.index_to_loc_format);

    uint8_t *glyf_table_ptr = ttf::get_table_ptr(&table_directory, data, "glyf");

    // Set up kerning subtable.
    uint8_t *kern_table_ptr = ttf::get_table_ptr(&table_directory, data, "kern");
    KernSubTable kerning_table = {};
    if(kern_table_ptr) {
        KernTable kern_table = ttf::get_kern_table(kern_table_ptr);

        // Find and copy over kerning subtable in format 0 with horizontal information.
        for(uint16_t i = 0; i < kern_table.n_tables; ++i) {
            KernSubTable subtable = kern_table.subtables[i];

            uint8_t format = (subtable.coverage & 0xFF00) >> 8;
            bool is_horizontal = subtable.coverage & 0x1;
            if(format == 0 && is_horizontal) {
                // Deep copy the subtable.
                kerning_table = subtable;
                kerning_table.pairs = (KernPair *)malloc(sizeof(KernPair) * kerning_table.n_pairs);
                memcpy(kerning_table.pairs, subtable.pairs, sizeof(KernPair) * kerning_table.n_pairs);
                break;
            }
        }

        // Since we have copied over the whole subtable, we don't need the kern table anymore.
        ttf::release(&kern_table);
    }

    // Store tables needed to fetch kerning infio.
    font.kerning_table = kerning_table;
    font.cmap_table = cmap_table;

    // Get scaling factor from design units (funits) to pixels. This assumes 72 screen DPI.
    float funits_to_pixels_scaling = float(size) / float(head_table.units_per_em);
    font.scale = funits_to_pixels_scaling;
    
    // Get row height and top padding.
    // NOTE: Not 100% sure how this works, e.g. why rounding instead of ceil/floor,
    // but this seems to be how FreeType computes row height.
    font.row_height = roundf((hhea_table.ascender - hhea_table.descender + hhea_table.line_gap) * funits_to_pixels_scaling);
    font.top_pad = font.row_height - (
        roundf(hhea_table.ascender * funits_to_pixels_scaling) - roundf(hhea_table.descender * funits_to_pixels_scaling)
    );

    // Allocate memory for the bitmap
    uint8_t *font_bitmap = (uint8_t *)malloc(bitmap_size * bitmap_size);
    memset(font_bitmap, 255, bitmap_size * bitmap_size);
    if(!font_bitmap) {
        PRINT_DEBUG("Error allocating memory for font bitmap!");
        return Font{};
    }

    // Position in the bitmap where we'll render glyphs.
    int32_t bitmap_x = 0, bitmap_y = 0;

    for (unsigned char c = 32; c < 128; ++c) {
        // Get glyph id for the current char.
        uint16_t glyph_id = ttf::get_glyph_index(c, &cmap_table);
        
        // Get offset of the current glyph. If the following glyph's offset is same, that means
        // that current glyph is empty, so we just fill it with zeroes.
        uint32_t glyph_offset = loca_table.offsets[glyph_id];
        if(loca_table.offsets[glyph_id + 1] - glyph_offset == 0) {
            // The only value we need is the advance.
            // TODO: This is repeated with advance computation below, could they be unified?
            int advance = int(floorf(hmtx_table.h_metrics[glyph_id].advance_width * funits_to_pixels_scaling));
            font.glyphs[c - 32] = {0, 0, 0, 0, 0, 0, advance};
            continue;
        }

        // Get glyph for the current char.
        TTFGlyph glyph = ttf::get_glyph(glyf_table_ptr + glyph_offset);

        // Composite glyphs have -1 contours.
        bool is_composite = glyph.number_of_contours < 0;

        // Get glyph id for computing metrics. For simple glyph that's just the glyph id, for composite glyph
        // we need to find the glyph component which should be used to retrieve the metrics.
        uint16_t metrics_glyph_id = glyph_id;
        if(is_composite) {
            for(uint16_t i = 0; i < glyph.number_of_components; ++i) {
                if(glyph.components[i].use_metrics) {
                    metrics_glyph_id = glyph.components[i].glyph_index;
                    break;
                }
            }
        }

        // Get min/max points in the glyph.
        int x_min = glyph.x_min, x_max = glyph.x_max;
        int y_min = glyph.y_min, y_max = glyph.y_max;

        // Get glyph advance.
        int advance = int(floorf(hmtx_table.h_metrics[metrics_glyph_id].advance_width * funits_to_pixels_scaling));

        // Get bitmap offset.
        int x_offset = int(floorf(hmtx_table.h_metrics[metrics_glyph_id].lsb * funits_to_pixels_scaling));
        int y_offset = int(roundf(hhea_table.ascender * funits_to_pixels_scaling) - ceilf(y_max * funits_to_pixels_scaling));

        // Get glyph's bitmap width and height.
        int bitmap_width = int(ceilf(x_max * funits_to_pixels_scaling) - floorf(x_min * funits_to_pixels_scaling));
        int bitmap_height = int(ceilf(y_max * funits_to_pixels_scaling) - floorf(y_min * funits_to_pixels_scaling));

        // Add padding to the bitmap.
        const int padding = 5; // TODO: Should this be configurable?
        bitmap_width += padding * 2;
        bitmap_height += padding * 2;
        x_offset -= padding;
        y_offset -= padding;

        // In case we don't fit in the row, let's move to next row
        if(bitmap_x > bitmap_size - bitmap_width) {
            bitmap_x = 0;
            bitmap_y += int(font.row_height) + padding * 2;
        }

        // Get min positions in pixel space.
        float x_min_pixel = floorf(x_min * funits_to_pixels_scaling);
        float y_min_pixel = floorf(y_min * funits_to_pixels_scaling);

        // Decompose the outline into segments.
        // TODO: Make this work with more than 1000 segments, or just handle overflow.
        int segment_count = 0;
        OutlineSegment *segments = (OutlineSegment *)malloc(sizeof(OutlineSegment) * 1000);
        if(is_composite) {
            // For composite glyphs we'll have to concatenate segments from all the glyph components.
            for(uint16_t i = 0; i < glyph.number_of_components; ++i) {
                GlyphComponent *component = &glyph.components[i];

                // TODO: Handle this case.
                if(component->offsets_are_matching_points) continue;

                // Get the component glyph.
                uint16_t component_glyph_id = component->glyph_index;
                // NOTE: We're assuming that composite glyphs cannot have empty components.
                uint32_t component_glyph_offset = loca_table.offsets[component_glyph_id];
                TTFGlyph component_glyph = ttf::get_glyph(glyf_table_ptr + component_glyph_offset);

                // Get the segments for this component glyph.
                OutlineSegment *component_segments = segments + segment_count;
                int component_segment_count = get_glyph_segments(&component_glyph, component_segments);
                
                // Transform the points in all the component segments.
                // Scaling and rotation matrix.
                float *transform_matrix = component->transform_matrix;
                Vector2 offset_vector = Vector2(component->offset_x, component->offset_y);
                if(component->flags & ROUND_XY_TO_GRID) {
                    // If ROUND_XY_TO_GRID is specified, we need to round the offsets
                    // to the scaled grid lines. We first need to convert to pixel space from
                    // "funits" space, round to grid there and then convert back from pixel space
                    // to "funits" space. Note that rounding could also be done purely in funits
                    // space, but FreeType does it in the pixel space, so we're just mimicking here.
                    offset_vector.x = roundf(offset_vector.x * funits_to_pixels_scaling) / funits_to_pixels_scaling;
                    offset_vector.y = roundf(offset_vector.y * funits_to_pixels_scaling) / funits_to_pixels_scaling;
                }
                for(int s = 0; s < component_segment_count; ++s) {
                    OutlineSegment *segment = &component_segments[s];
                    segment->points[0] = transform_point(segment->points[0], transform_matrix, offset_vector);
                    segment->points[1] = transform_point(segment->points[1], transform_matrix, offset_vector);
                    segment->points[2] = transform_point(segment->points[2], transform_matrix, offset_vector);
                }

                // Update the total segment count.
                segment_count += component_segment_count;

                // Release the component glyph.
                ttf::release(&component_glyph);
            }
        } else {
            // For simple glyph we can just retrieve the glyph segments.
            segment_count = get_glyph_segments(&glyph, segments);
        }

        // After this point we don't need the glyph anymore.
        ttf::release(&glyph);

        // Create an SDF map.
        for(int y = 0; y < bitmap_height; ++y) {
            for(int x = 0; x < bitmap_width; ++x) {
                float x_pixel = x + x_min_pixel - padding + 0.5f;
                float y_pixel = y + y_min_pixel - padding + 0.5f;
                Vector2 p_pixel = Vector2(x_pixel, y_pixel);
                
                float d = get_distance(p_pixel, segments, segment_count, funits_to_pixels_scaling);
                d /= float(padding); // TODO: Is this correct?

                // Normalize from (-1, 1) to (0, 1) range.
                d = math::clamp(d, -1.0f, 1.0f) * 0.5f + 0.5f;

                int dst_x = x + bitmap_x;
                int dst_y = bitmap_height - 1 - y + bitmap_y;  // "glyph space" has y-axis up, bitmap down.
                font_bitmap[dst_x + dst_y * bitmap_size] = uint8_t(math::clamp(d, 0.0f, 1.0f) * 255.0f);
            }
        }

        font.glyphs[c - 32] = {bitmap_x, bitmap_y, bitmap_width, bitmap_height, x_offset, y_offset, advance};

        // Move in the bitmap
        bitmap_x += bitmap_width;

        // Release allocated segments.
        free(segments);
    }

    // Store the bitmap.
    font.bitmap = font_bitmap;
    font.bitmap_width = bitmap_size;
    font.bitmap_height = bitmap_size;

    // Release unused ttf tables which allocated memory.
    ttf::release(&hmtx_table);
    ttf::release(&loca_table);
    ttf::release(&table_directory);

    return font;
}

float font::get_kerning(Font *font, char c1, char c2) {
    // Get glyph ids for the left and right characters.
    uint16_t left_glyph_id = ttf::get_glyph_index(c1, &font->cmap_table);
    uint16_t right_glyph_id = ttf::get_glyph_index(c2, &font->cmap_table);

    // The kerning table stores the glyph pairs in the ascending order for
    // 32bit uints that are obtained by combining the glyph ids. This means
    // we can do binary search on a sorted list if we just combine the searched
    // value into uint32.
    uint32_t searched_val = left_glyph_id << 16 | right_glyph_id;

    // Left, right bounds.        
    int32_t l = 0;
    int32_t r = font->kerning_table.n_pairs - 1;
    // Binary search.
    while(l <= r) {
        // Middle point.
        int32_t i = (l + r) / 2;

        // Get the middle point's glyph pair's ids as uint32.
        uint32_t v = font->kerning_table.pairs[i].left << 16 | font->kerning_table.pairs[i].right;

        // Update the search or return the found value (after scaling).
        if(v < searched_val) {
            l = i + 1;
        } else if (v > searched_val) {
            r = i - 1;
        } else {
            return roundf(font->kerning_table.pairs[i].value * font->scale);
        }
    }
    return 0;
}

float font::get_string_width(char *string, Font *font) {
    float width = 0;
    while(*string) {
        // Get a glyph for the current character
        char c = *string;
        Glyph glyph = font->glyphs[c - 32];

        // Increment width by character's advance. This should be more precise than taking it's bitmap's width.
        width += glyph.advance;

        // Take kerning into consideration
        if (*(string + 1)) width += font::get_kerning(font, c, *(string + 1));

        // Next letter
        string++;
    }

    return width;
}

float font::get_string_width(char *string, int string_length, Font *font) {
    float width = 0;
    for(int i = 0; i < string_length; ++i) {
        // Get a glyph for the current character
        char c = *string;
        Glyph glyph = font->glyphs[c - 32];

        // Increment width by character's advance. This should be more precise than taking it's bitmap's width.
        width += glyph.advance;

        // Take kerning into consideration
        if (*(string + 1)) width += font::get_kerning(font, c, *(string + 1));

        // Next letter
        string++;
    }

    return width;
}

float font::get_row_height(Font *font) {
    return font->row_height;
}

void font::release(Font *font) {
    free(font->bitmap);
    font->bitmap = 0;

    ttf::release(&font->cmap_table);
    ttf::release(&font->kerning_table);
}
