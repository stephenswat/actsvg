// This file is part of the actsvg packge.
//
// Copyright (C) 2022 CERN for the benefit of the ACTS project
//
// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

#pragma once

#include <string>
#include <vector>

#include "actsvg/actsvg.hpp"
#include "actsvg/display/helpers.hpp"
#include "actsvg/proto/surface.hpp"
#include "actsvg/proto/volume.hpp"

namespace actsvg {

using namespace defaults;

namespace display {

enum sheet_type { e_module_info = 0, e_grid_info = 1 };

/** Make a surface sheet
 *
 * @tparam volume3_container is the type of the 3D point container
 *
 * @param s_ is the surface to be displayed
 * @param sh_ is the sheet size for displaying
 *
 * @return a surface sheet svg object
 **/
template <typename point3_container>
svg::object surface_sheet(const proto::surface<point3_container>& s_,
                          const std::array<scalar, 2>& sh_ = {400., 400.}) {
    svg::object so;
    so._tag = "g";
    so._id = "surface_sheet_" + s_._name;

    views::x_y x_y_view;

    // Add the surface
    auto surface_contour = x_y_view(s_._vertices);

    std::vector<views::contour> contours = {surface_contour};

    auto [x_axis, y_axis] = display::view_range(contours);

    scalar s_x = sh_[0] / (x_axis[1] - x_axis[0]);
    scalar s_y = sh_[1] / (y_axis[1] - y_axis[0]);

    // Create the scale transform
    style::transform scale_transform;
    scale_transform._scale = {s_x, s_y};

    auto surface = draw::polygon(surface_contour, s_._name, s_._fill,
                                 s_._stroke, scale_transform);
    so.add_object(surface);

    display::prepare_axes(x_axis, y_axis, s_x, s_y, 30., 30.);

    auto axis_font = __a_font;
    axis_font._size = 10;

    so.add_object(
        draw::x_y_axes(x_axis, y_axis, __a_stroke, "x", "y", axis_font));

    // The measures
    // - Trapezoid
    if (s_._type == proto::surface<point3_container>::e_trapez and
        s_._measures.size() == 3u) {

        scalar hlx_min = s_._measures[0] * s_x;
        scalar hlx_max = s_._measures[1] * s_x;
        scalar hly = s_._measures[2] * s_y;
        scalar ms = 2 * __m_marker._size;

        scalar hly_x = hlx_max + 2 * __m_marker._size;

        auto measure_hlx_min = draw::measure(
            {0, -hly - ms}, {hlx_min, -hly - ms}, __m_stroke, __m_marker,
            "h_x_min = " + std::to_string(s_._measures[0]), __m_font, 1, -2);
        auto measure_hlx_max = draw::measure(
            {0, hly + ms}, {hlx_max, hly + ms}, __m_stroke, __m_marker,
            "h_x_max = " + std::to_string(s_._measures[1]), __m_font, 1, 1);
        auto measure_hly = draw::measure(
            {hly_x, 0}, {hly_x, hly}, __m_stroke, __m_marker,
            "h_y = " + std::to_string(s_._measures[2]), __m_font, 1, 1);
        so.add_object(measure_hlx_min);
        so.add_object(measure_hlx_max);
        so.add_object(measure_hly);
    }

    return so;
}

/** Make a summary sheet for an endcap type volume
 *
 * @tparam volume3_container is the type of the 3D point container
 *
 * @param v_ is the volume to be displayed
 * @param sh_ is the sheet size for displaying
 * @param t_ is the sheet type
 * @param s_sh_ is the surface sheet sub display size
 **/
template <typename point3_container>
svg::object endcap_sheet(const proto::volume<point3_container>& v_,
                         const std::array<scalar, 2>& sh_ = {600., 600.},
                         sheet_type t_ = e_module_info,
                         const std::array<scalar, 2>& s_sh_ = {200., 400.}) {
    svg::object eo;
    eo._tag = "g";
    eo._id = v_._name;

    views::x_y x_y_view;

    // Axis range & pre-loop
    std::vector<views::contour> contours;
    contours.reserve(v_._surfaces.size());
    for (auto [is, s] : utils::enumerate(v_._surfaces)) {
        auto surface_contour = x_y_view(s._vertices);
        contours.push_back(surface_contour);
    }
    // Get the scaling right
    auto [x_axis, y_axis] = display::view_range(contours);
    scalar s_x = sh_[0] / (x_axis[1] - x_axis[0]);
    scalar s_y = sh_[1] / (y_axis[1] - y_axis[0]);

    // Create the scale transform
    style::transform scale_transform;
    scale_transform._scale = {s_x, s_y};

    // Draw the modules and estimate axis ranges
    std::vector<svg::object> modules;
    modules.reserve(contours.size());
    for (auto [ic, c] : utils::enumerate(contours)) {
        const auto& surface = v_._surfaces[ic];
        auto surface_module = draw::polygon(c, surface._name, surface._fill,
                                            surface._stroke, scale_transform);
        modules.push_back(surface_module);
    }

    // Draw the template module surfaces
    if (t_ == e_module_info and not v_._template_surfaces.empty()) {
        // The templates
        std::vector<svg::object> templates;
        for (auto s : v_._template_surfaces) {
            // Use a local copy of the surface to modify color
            style::fill s_fill;
            s_fill._fc._rgb = s._fill._fc._hl_rgb;
            s_fill._fc._opacity = s._fill._fc._opacity;
            s._fill = s_fill;
            // The template sheet
            auto s_sheet = display::surface_sheet(s, s_sh_);
            style::transform({x_axis[1] + 350, 0., 0.})
                .attach_attributes(s_sheet);
            templates.push_back(s_sheet);
        }
        // Add the modules
        for (auto& m : modules) {
            eo.add_object(m);
        }
        // Connect the surface sheets
        if (v_._surfaces.size() == v_._templates.size()) {
            connect_surface_sheets(v_, templates, eo);
        }
    } else if (t_ == e_grid_info and not v_._surface_grid._edges_0.empty() and
               not v_._surface_grid._edges_1.empty()) {
        
        // Draw the grid with the appropriate scale transform
        auto grid_sectors = draw::r_phi_grid(
            v_._surface_grid._edges_0, v_._surface_grid._edges_1, __g_fill,
            __g_stroke, scale_transform);

        connectors::connect_objects(grid_sectors, modules, v_._surface_grid._associations);

        // Add the modules
        for (auto& m : modules) {
            eo.add_object(m);
        }
        // And the grid sectors
        for (auto& gs : grid_sectors) {
            eo.add_object(gs);
        }
    } else {
        // Simply add the modules
        for (auto& m : modules) {
            eo.add_object(m);
        }
    }

    // Prepare the axis for the view range
    display::prepare_axes(x_axis, y_axis, s_x, s_y, 30., 30.);

    // Left upper corner reference
    scalar ref_luc_x = x_axis[0] - 25;
    scalar ref_ruc_x = x_axis[1] + 25;
    scalar ref_luc_y = y_axis[1] + 25;

    auto axis_font = __a_font;
    axis_font._size = 10;

    eo.add_object(
        draw::x_y_axes(x_axis, y_axis, __a_stroke, "x", "y", axis_font));

    //  Add the title text
    auto title_font = __t_font;
    title_font._size = 16;
    auto title = draw::text({ref_luc_x, ref_luc_y}, "sheet_title", {v_._name},
                            title_font);
    eo.add_object(title);

    return eo;
}

}  // namespace display
}  // namespace actsvg