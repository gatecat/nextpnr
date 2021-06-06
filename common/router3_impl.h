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

template <uint8_t bits> struct PackedArray
{
    using Tchunk = uint64_t;
    using Tvalue = uint8_t;
    static constexpr Tchunk vals_per_chunk = (64 / bits);
    static constexpr Tchunk mask = ((1 << bits) - 1);

    PackedArray() = default;
    PackedArray(size_t size) { resize(size); };
    Tvalue get(size_t index) const
    {
        Tchunk ch = chunks.at(index / vals_per_chunk);
        return (ch >> ((index % vals_per_chunk) * bits)) & mask;
    }
    void set(size_t index, Tvalue value)
    {
        Tchunk &ch = chunks.at(index / vals_per_chunk);
        size_t bit_offset = (index % vals_per_chunk) * bits;
        ch &= ~(mask << (bit_offset));
        ch |= ((value & mask) << bit_offset);
    }
    void resize(size_t new_size) { chunks.resize((new_size + vals_per_chunk - 1) / vals_per_chunk); }
    std::vector<Tchunk> chunks;
};

struct WireStatus
{
    WireStatus() = default;
    WireStatus(int curr_cong, int hist_cong, IdString reserved = {})
            : curr_cong(curr_cong), hist_cong(hist_cong), reserved(reserved){};
    int curr_cong;
    int hist_cong;
    IdString reserved;
};

struct WireStatusStore
{
    PackedArray<3> flat_data;
    // TODO: faster hash maps?
    dict<uint32_t, WireStatus> ext_data;
    WireStatus get(uint32_t wire_idx) const
    {
        uint8_t f = flat_data.get(wire_idx);
        if (f == 0x7) // wire is in extended array
            return ext_data.at(wire_idx);
        else
            return WireStatus((f & 0x3), (f >> 2));
    }
    void set(uint32_t wire_idx, const WireStatus &st)
    {
        // Conditions to use extended data
        if (st.curr_cong > 2 || st.hist_cong > 1 || st.reserved != IdString()) {
            ext_data[wire_idx] = st;
            flat_data.set(wire_idx, 0x7);
        } else {
            flat_data.set(wire_idx, (st.hist_cong << 2) | st.curr_cong);
        }
    }
};

struct NetData
{
    // Wire -> (uphill pip, bound arc count)
    dict<uint32_t, std::pair<PipId, int>> wires;
    // ....
};

struct Router3Types
{
    using WireLocT = DefaultWireLocation;
    using FlatWireT = DefaultFlatWireIndexer;
    using WireSegT = DefaultWireSegmenter;
};

template <typename Types> struct Router3
{

    Router3(Context *ctx) : ctx(ctx), wire_loc(ctx), flat_wires(ctx), wire_seg(ctx){};

    dict<IdString, NetData> nets;
    WireStatusStore wires;

    void bind_wire(NetData &net, uint32_t wire, PipId uphill)
    {
        auto found = net.wires.find(wire);
        if (found == net.wires.end()) {
            // Not yet used for any arcs of this net, add to list
            net.wires.emplace(wire, std::make_pair(uphill, 1));
            // Increase bound count of wire by 1
            WireStatus st = wires.get(wire);
            ++st.curr_cong;
            wires.set(wire, st);
        } else {
            // Already used for at least one other arc of this net
            // Don't allow two uphill PIPs for the same net and wire
            NPNR_ASSERT(found->second.first == uphill);
            // Increase the count of bound arcs
            ++found->second.second;
        }
    }

    void unbind_wire(NetData &net, uint32_t wire)
    {
        auto &b = net.wires.at(wire);
        --b.second;
        if (b.second == 0) {
            // No remaining arcs of this net bound to this wire
            WireStatus st = wires.get(wire);
            --st.curr_cong;
            wires.set(wire, st);
            net.wires.erase(wire);
        }
    }

    Context *ctx;
    typename Types::WireLocT wire_loc;
    typename Types::FlatWireT flat_wires;
    typename Types::WireSegT wire_seg;
};

NEXTPNR_NAMESPACE_END