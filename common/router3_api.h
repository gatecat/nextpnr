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

NEXTPNR_NAMESPACE_BEGIN

// This provides APIs to get a notional location of a wire; and its entire bounding box
struct WireLocationAPI
{
    virtual Loc approx_wire_loc(WireId wire) const = 0;
    virtual ArcBounds wire_bounds(WireId wire) const = 0;
};

struct DefaultWireLocation : WireLocationAPI
{
    Context *ctx;

    DefaultWireLocation(Context *ctx) : ctx(ctx){};
    virtual Loc approx_wire_loc(WireId wire) const override;
    virtual ArcBounds wire_bounds(WireId wire) const override;
};

// This should convert `WireId`s to/from a flat index (only going to a flat index needs to be fast). The index does not
// need to be strictly contiguous but should not have excessive gaps for memory efficiency
struct FlatWireIndexerAPI
{
    virtual uint32_t size() const = 0;
    virtual uint32_t get_index(WireId wire) const = 0;
    virtual WireId get_wire(uint32_t index) const = 0;
};

struct DefaultFlatWireIndexer : FlatWireIndexerAPI
{
    DefaultFlatWireIndexer(Context *ctx);
    uint32_t size() const override;
    uint32_t get_index(WireId wire) const override;
    WireId get_wire(uint32_t index) const override;

    std::vector<WireId> wire_by_index;
    dict<WireId, uint32_t> wire_to_index;
};

// This allows architectures to split the routing problem of a wire into sections; for example to force the use of
// global resources for some sinks
struct WireSegment
{
    WireSegment(WireId src, WireId dst) : src(src), dst(dst){};
    WireSegment(WireId src, WireId dst, size_t sink_idx) : src(src), dst(dst), logical_sink(sink_idx){};

    WireId src;
    WireId dst;
    // The logical sink reached by this segment, if applicable
    size_t logical_sink = std::numeric_limits<size_t>::max();
};

struct WireSegmenterAPI
{
    virtual std::vector<WireSegment> segment_net(const NetInfo *net) = 0;
};

struct DefaultWireSegmenter : WireSegmenterAPI
{
    Context *ctx;

    DefaultWireSegmenter(Context *ctx) : ctx(ctx){};
    virtual std::vector<WireSegment> segment_net(const NetInfo *net) override;
};

NEXTPNR_NAMESPACE_END
