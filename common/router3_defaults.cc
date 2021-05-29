/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2021  gatecat <gatecat@ds0.me>
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "nextpnr.h"
#include "router3_api.h"

NEXTPNR_NAMESPACE_BEGIN

Loc DefaultWireLocation::approx_wire_loc(WireId wire) const
{
    ArcBounds bb = ctx->getRouteBoundingBox(wire, wire);
    return Loc((bb.x1 + bb.x0) / 2, (bb.y1 + bb.y0) / 2, 0);
}

ArcBounds DefaultWireLocation::wire_bounds(WireId wire) const { return ctx->getRouteBoundingBox(wire, wire); }

DefaultFlatWireIndexer::DefaultFlatWireIndexer(Context *ctx)
{
    for (WireId wire : ctx->getWires()) {
        wire_to_index[wire] = wire_by_index.size();
        wire_by_index.push_back(wire);
    }
}

uint32_t DefaultFlatWireIndexer::size() const { return wire_by_index.size(); }

uint32_t DefaultFlatWireIndexer::get_index(WireId wire) const { return wire_to_index.at(wire); }

WireId DefaultFlatWireIndexer::get_wire(uint32_t index) const { return wire_by_index.at(index); }

std::vector<WireSegment> DefaultWireSegmenter::segment_net(const NetInfo *net)
{
    std::vector<WireSegment> result;

    if (net->driver.cell == nullptr)
        return result;

    WireId src = ctx->getNetinfoSourceWire(net);

    for (size_t usr = 0; usr < net->users.size(); usr++) {
        for (WireId dst : ctx->getNetinfoSinkWires(net, net->users.at(usr)))
            result.emplace_back(src, dst, usr);
    }

    return result;
}

NEXTPNR_NAMESPACE_END
